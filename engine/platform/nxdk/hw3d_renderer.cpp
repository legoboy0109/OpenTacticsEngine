#include "platform/nxdk/hw3d_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>

#include "core/base/log.h"
#include "platform/nxdk/hw3d_gpu.h"
#include "platform/nxdk/hw3d_texture.h"
#include "platform/nxdk/nxdk_present.h"

namespace ote {

// The vertex arrays read Vertex3D straight from memory (hw3d_gpu.cpp: stride 36, normal at 12, texture at 24, colour at 32).
static_assert(sizeof(Vertex3D) == 36, "Vertex3D is read in place by the chip");
static_assert(offsetof(Vertex3D, nx) == 12 && offsetof(Vertex3D, u) == 24 && offsetof(Vertex3D, r) == 32, "Vertex3D layout");

namespace {

const float kZScale = 16777215.0f;  // the depth buffer is 24 bits (pbkit's pb_ZScale)
const float kPixelBias = 0.53125f;
const size_t kOneTexture = 3u << 20;  // the most one texture (with its mipmaps) may take  // pbkit's half-pixel offset for window positions

size_t pages(size_t bytes) { return (bytes + 4095) & ~(size_t)4095; }

int log2i(int v) {
  int n = 0;
  while ((1 << (n + 1)) <= v) ++n;
  return n;
}

bool powerOfTwo(int v) { return v > 0 && (v & (v - 1)) == 0; }

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

uint32_t texFormat(int w, int h, int levels) {
  // level count | A8R8G8B8 swizzled (6) | 2D, border colour, DMA channel 2 (0x2A, as nxdk's samples) | log2 of the sides
  return ((uint32_t)levels << 16) | (0x06u << 8) | 0x2Au | ((uint32_t)log2i(w) << 20) | ((uint32_t)log2i(h) << 24);
}

}  // namespace

NxdkRenderer3D::NxdkRenderer3D() {
  const bool big = hw3d::width() > 700;  // 720p: its frame buffers take far more of the 64 MB
  texBudget_ = (size_t)(big ? 4 : 6) << 20;  // the 64 MB also hold the 2D art, the chip's buffers and PNG decoding (docs/XBOX_OG.md "3D memory")
  meshBudget_ = (size_t)2 << 20;
  whiteMem_ = hw3d::alloc(4096);
  if (whiteMem_) {
    uint32_t* w = (uint32_t*)whiteMem_;
    w[0] = 0xFFFFFFFFu;
    hw3d::flushWrites();
    whitePhys_ = hw3d::physical(whiteMem_);
  }
  const char* dbg = std::getenv("OTE_3D_DEBUG");
  if (dbg) debugMode_ = std::strcmp(dbg, "cycle") == 0 ? -1 : std::atoi(dbg);
  OTE_INFO("3D drawing: Xbox NV2A; budgets %u MB for textures and %u MB for meshes", (unsigned)(texBudget_ >> 20),
           (unsigned)(meshBudget_ >> 20));
}

NxdkRenderer3D::~NxdkRenderer3D() {
  hw3d::waitIdle();
  for (Mesh& m : meshes_) hw3d::release(m.mem);
  for (Tex& t : textures_) hw3d::release(t.mem);
  hw3d::release(whiteMem_);
}

Caps3D NxdkRenderer3D::caps() const {
  Caps3D c;
  c.maxTextureSize = 1024;  // 4 MB of A8R8G8B8 with mipmaps: the budget (docs/XBOX_OG.md "3D memory") is the real limit
  c.maxMeshVertices = 65535;  // 16-bit indices
  c.fullResolution = true;
  return c;
}

// ---- meshes ------------------------------------------------------------------------------------------------------------

bool NxdkRenderer3D::fill(Mesh& m, const MeshData& data, bool allocate) {
  const size_t vcount = data.vertices.size();
  const size_t icount = data.indices.size() / 3 * 3;
  if (vcount == 0 || icount == 0 || vcount > 65535) {
    OTE_ERROR("3D mesh refused: %u vertices, %u indices (1..65535 vertices and at least one triangle)", (unsigned)vcount,
              (unsigned)data.indices.size());
    return false;
  }
  for (size_t i = 0; i < icount; ++i)
    if (data.indices[i] >= vcount) {
      OTE_ERROR("3D mesh refused: index %u is outside its %u vertices", (unsigned)data.indices[i], (unsigned)vcount);
      return false;
    }
  hw3d::waitIdle();  // the chip may still be reading this mesh's vertices from the frame before
  if (allocate || vcount > m.capacity) {
    const size_t bytes = pages(vcount * sizeof(Vertex3D));
    const size_t had = m.mem ? pages(m.capacity * sizeof(Vertex3D)) : 0;
    if (meshBytes_ - had + bytes > meshBudget_) {
      OTE_ERROR("3D mesh refused: %u vertices would take the meshes over their %u MB budget (%u KB in use)", (unsigned)vcount,
                (unsigned)(meshBudget_ >> 20), (unsigned)(meshBytes_ >> 10));
      return false;
    }
    void* mem = hw3d::alloc(bytes);
    if (!mem) {
      OTE_ERROR("3D mesh refused: no chip memory left for %u vertices", (unsigned)vcount);
      return false;
    }
    hw3d::release(m.mem);
    meshBytes_ = meshBytes_ - had + bytes;
    m.mem = mem;
    m.phys = hw3d::physical(mem);
    m.capacity = vcount;
  }
  memcpy(m.mem, data.vertices.data(), vcount * sizeof(Vertex3D));
  hw3d::flushWrites();
  m.count = vcount;
  m.indices.resize(icount);
  for (size_t i = 0; i < icount; ++i) m.indices[i] = (uint16_t)data.indices[i];
  m.cpu = data.vertices;
  m.lo[0] = m.lo[1] = m.lo[2] = 1e30f;
  m.hi[0] = m.hi[1] = m.hi[2] = -1e30f;
  for (const Vertex3D& v : data.vertices) {
    const float p[3] = {v.x, v.y, v.z};
    for (int k = 0; k < 3; ++k) {
      m.lo[k] = std::min(m.lo[k], p[k]);
      m.hi[k] = std::max(m.hi[k], p[k]);
    }
  }
  m.used = true;
  return true;
}

MeshId NxdkRenderer3D::createMesh(const MeshData& data) {
  size_t slot = 0;
  while (slot < meshes_.size() && meshes_[slot].used) ++slot;
  if (slot == meshes_.size()) meshes_.push_back(Mesh());
  Mesh& m = meshes_[slot];
  if (!fill(m, data, true)) return 0;
  return (MeshId)(slot + 1);
}

void NxdkRenderer3D::updateMesh(MeshId id, const MeshData& data) {
  if (id < 1 || (size_t)id > meshes_.size() || !meshes_[(size_t)id - 1].used) return;
  fill(meshes_[(size_t)id - 1], data, false);
}

void NxdkRenderer3D::destroyMesh(MeshId id) {
  if (id < 1 || (size_t)id > meshes_.size()) return;
  Mesh& m = meshes_[(size_t)id - 1];
  if (!m.used) return;
  hw3d::waitIdle();
  meshBytes_ -= pages(m.capacity * sizeof(Vertex3D));
  hw3d::release(m.mem);
  m = Mesh();
}

// ---- textures ----------------------------------------------------------------------------------------------------------

Texture3DId NxdkRenderer3D::createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps) {
  const int limit = caps().maxTextureSize;
  if (!rgba || !powerOfTwo(width) || !powerOfTwo(height) || width > limit || height > limit) {
    OTE_ERROR("3D texture refused: %d x %d; each side must be a power of two up to %d", width, height, limit);
    return 0;
  }
  // Memory is what limits a texture here, not the chip: one that takes more than kOneTexture, or more than is left of the budget,
  // is held at half the size (and again) until it fits. Texture positions are fractions, so nothing above notices.
  const int askedW = width, askedH = height;
  std::vector<uint8_t> small[2];
  const uint8_t* src = rgba;
  auto bytesFor = [&](int w, int h) { return pages(hw3d::swizzledBytes(w, h, mipmaps ? hw3d::mipLevelCount(w, h) : 1)); };
  int flip = 0;
  while ((bytesFor(width, height) > kOneTexture || texBytes_ + bytesFor(width, height) > texBudget_) && (width > 4 || height > 4)) {
    hw3d::halveImage(src, width, height, small[flip]);
    src = small[flip].data();
    flip ^= 1;
    width = std::max(1, width / 2);
    height = std::max(1, height / 2);
  }
  rgba = src;
  const int levels = mipmaps ? hw3d::mipLevelCount(width, height) : 1;
  const size_t bytes = bytesFor(width, height);
  if (texBytes_ + bytes > texBudget_) {
    OTE_ERROR("3D texture refused: %d x %d would take the textures over their %u MB budget (%u KB in use)", askedW, askedH,
              (unsigned)(texBudget_ >> 20), (unsigned)(texBytes_ >> 10));
    return 0;
  }
  if (width != askedW || height != askedH)
    OTE_INFO("3D texture %d x %d held at %d x %d to stay within the memory budget (%u KB)", askedW, askedH, width, height,
             (unsigned)(bytes >> 10));
  void* mem = hw3d::alloc(bytes);
  if (!mem) {
    OTE_ERROR("3D texture refused: no chip memory left for %d x %d", width, height);
    return 0;
  }
  hw3d::waitIdle();  // (a new texture is not read yet, but the memory may be one the chip just finished with)
  hw3d::buildSwizzled(rgba, width, height, levels, (uint32_t*)mem);
  hw3d::flushWrites();
  size_t slot = 0;
  while (slot < textures_.size() && textures_[slot].used) ++slot;
  if (slot == textures_.size()) textures_.push_back(Tex());
  Tex& t = textures_[slot];
  t.used = true;
  t.mem = mem;
  t.phys = hw3d::physical(mem);
  t.w = width;
  t.h = height;
  t.levels = levels;
  t.repeat = repeat;
  t.mips = levels > 1;
  t.bytes = bytes;
  texBytes_ += bytes;
  OTE_DEBUG("3D texture %d: %d x %d, %d levels, %u KB (textures now %u KB of %u KB)", (int)slot + 1, width, height, levels,
            (unsigned)(bytes >> 10), (unsigned)(texBytes_ >> 10), (unsigned)(texBudget_ >> 10));
  return (Texture3DId)(slot + 1);
}

void NxdkRenderer3D::destroyTexture(Texture3DId id) {
  if (id < 1 || (size_t)id > textures_.size()) return;
  Tex& t = textures_[(size_t)id - 1];
  if (!t.used) return;
  hw3d::waitIdle();
  texBytes_ -= t.bytes;
  hw3d::release(t.mem);
  t = Tex();
}

// ---- drawing -----------------------------------------------------------------------------------------------------------

void NxdkRenderer3D::clearRect() {
  hw3d::VsConstants c;
  hw3d::screenSpaceConstants(c, frame_.clear.r, frame_.clear.g, frame_.clear.b, frame_.clear.a);
  hw3d::Pipe pipe;
  pipe.mix = frame_.clear.a >= 0.999f ? hw3d::Mix::Replace : hw3d::Mix::Alpha;
  pipe.depthTest = false;
  pipe.depthWrite = false;
  pipe.cull = false;
  pipe.edge = hw3d::Edge::Clamp;
  hw3d::HwTexture white;
  white.offset = whitePhys_;
  white.format = texFormat(1, 1, 1);
  hw3d::setConstants(c);
  hw3d::setPipe(pipe, white);
  hw3d::drawQuad((float)rx_, (float)ry_, (float)(rx_ + rw_), (float)(ry_ + rh_), 0, 0, 1, 1, 0xFFFFFFFFu);
}

void NxdkRenderer3D::begin(const Frame3D& frame) {
  frame_ = frame;
  frame_.sunDir = normalize(frame.sunDir);
  inFrame_ = false;
  blended_.clear();
  if (!hw3d::running() || !whiteMem_) return;
  if (debugMode_ < 0 && frames_ % 30 == 0) OTE_INFO("3D debug mode %d", 1 + (frames_ / 120) % 6);
  hw3d::beginFrame();
  hw3d::canvasLayerDown();  // the 2D layer so far goes under the picture; the canvas starts over, transparent
  int sx, sy, sw, sh;
  hw3d::canvasRect(frame.x, frame.y, frame.width, frame.height, sx, sy, sw, sh);
  const int x0 = std::max(0, sx), y0 = std::max(0, sy);
  const int x1 = std::min(hw3d::width(), sx + sw), y1 = std::min(hw3d::height(), sy + sh);
  if (x1 <= x0 || y1 <= y0) return;
  rx_ = sx;
  ry_ = sy;
  rw_ = sw;
  rh_ = sh;
  // The chip copes with positions up to about 2000 pixels outside the picture: that is how far the guard band may reach.
  guard_ = std::min(8.0f, std::max(1.5f, 1800.0f / (0.5f * (float)std::max(sw, sh))));
  hw3d::Rect r;
  r.x = x0;
  r.y = y0;
  r.w = x1 - x0;
  r.h = y1 - y0;
  hw3d::setScissor(r);
  hw3d::clearDepth(r);
  if (frame.clear.a > 0) clearRect();
  drawCalls_ = 0;
  clipped_ = 0;
  inFrame_ = true;
}

void NxdkRenderer3D::draw(MeshId id, const Mat4& model, const Material3D& material) {
  if (!inFrame_ || id < 1 || (size_t)id > meshes_.size() || !meshes_[(size_t)id - 1].used) return;
  if (material.blend == Blend3D::Blend) {
    // See-through things wait until everything solid is in the picture (far to near by the mesh's origin).
    Vec3 origin = (frame_.view * model).point(Vec3(0, 0, 0));
    blended_.push_back(Pending{id, model, material, -origin.z});
    return;
  }
  emit(meshes_[(size_t)id - 1], model, material);
}

void NxdkRenderer3D::end() {
  if (!inFrame_) return;
  std::stable_sort(blended_.begin(), blended_.end(), [](const Pending& a, const Pending& b) { return a.depth > b.depth; });
  for (const Pending& p : blended_)
    if ((size_t)p.mesh <= meshes_.size() && meshes_[(size_t)p.mesh - 1].used) emit(meshes_[(size_t)p.mesh - 1], p.model, p.material);
  blended_.clear();
  inFrame_ = false;
  // One line every 240 pictures: what the 3D layer holds and how busy the frame was (the log is the way to see it on a console).
  if (++frames_ % 240 == 1) {
    OTE_DEBUG("3D: textures %u KB of %u KB, meshes %u KB of %u KB; last picture %d draws (%d cut at the camera), commands %u KB of %u KB",
              (unsigned)(texBytes_ >> 10), (unsigned)(texBudget_ >> 10), (unsigned)(meshBytes_ >> 10), (unsigned)(meshBudget_ >> 10),
              drawCalls_, clipped_, (unsigned)(hw3d::lastFramePushBytes() >> 10), (unsigned)(hw3d::pushBufferBytes() >> 10));
  }
}

void NxdkRenderer3D::emit(const Mesh& mesh, const Mat4& model, const Material3D& material) {
  const Tex* tex = nullptr;
  if (material.texture >= 1 && (size_t)material.texture <= textures_.size() && textures_[(size_t)material.texture - 1].used)
    tex = &textures_[(size_t)material.texture - 1];

  hw3d::HwTexture ht;
  hw3d::Pipe pipe;
  if (tex) {
    ht.offset = tex->phys;
    ht.format = texFormat(tex->w, tex->h, tex->levels);
    pipe.edge = tex->repeat ? hw3d::Edge::Repeat : hw3d::Edge::Clamp;
    pipe.sampling = !material.smooth ? hw3d::Sampling::Nearest : tex->mips ? hw3d::Sampling::Trilinear : hw3d::Sampling::Linear;
  } else {
    ht.offset = whitePhys_;
    ht.format = texFormat(1, 1, 1);
    pipe.edge = hw3d::Edge::Clamp;
  }
  pipe.cull = !material.twoSided;
  pipe.depthTest = material.depthTest;
  // Debug (OTE_3D_DEBUG=N in args.txt, or "cycle": N changes every 120 pictures): 1 no culling, 2 front faces are the other
  // winding, 3 front faces are culled, 4 no depth test, 5 white texture.
  int dbg = debugMode_;
  if (dbg < 0) dbg = 1 + (frames_ / 120) % 6;
  if (dbg == 1) pipe.cull = false;
  if (dbg == 2) pipe.frontCw = true;
  if (dbg == 3) pipe.frontCw = false;
  if (dbg == 4) { pipe.frontCw = true; pipe.cullFront = true; }
  if (dbg == 5) { pipe.frontCw = false; pipe.cullFront = true; }
  if (dbg == 6) { pipe.cull = false; pipe.depthTest = false; }
  if (material.blend == Blend3D::Blend) {
    pipe.mix = hw3d::Mix::Alpha;
    pipe.depthWrite = false;
  } else if (material.blend == Blend3D::Cutout) {
    pipe.alphaRef = std::min(255, std::max(0, (int)std::ceil(material.cutoff * 255.0f - 1e-4f)));
  }

  const Mat4 mv = frame_.view * model, mvp = frame_.proj * mv;
  hw3d::VsConstants c;
  memset(&c, 0, sizeof(c));
  for (int r = 0; r < 4; ++r)
    for (int k = 0; k < 4; ++k) c.clip[r * 4 + k] = mvp.m[k * 4 + r];
  c.scale[0] = (float)rw_ * 0.5f;
  c.scale[1] = (float)rh_ * -0.5f;
  c.scale[2] = kZScale * 0.5f;
  c.scale[3] = 1;
  c.offset[0] = (float)rx_ + (float)rw_ * 0.5f + kPixelBias;
  c.offset[1] = (float)ry_ + (float)rh_ * 0.5f + kPixelBias;
  c.offset[2] = kZScale * 0.5f;
  for (int r = 0; r < 3; ++r) {
    for (int k = 0; k < 3; ++k) c.rot[r * 4 + k] = model.m[k * 4 + r];
    for (int k = 0; k < 4; ++k) c.mv[r * 4 + k] = mv.m[k * 4 + r];
  }
  const Vec3 toSun = frame_.sunDir * -1.0f;
  c.toSun[0] = toSun.x;
  c.toSun[1] = toSun.y;
  c.toSun[2] = toSun.z;
  if (material.lit) {
    c.ambient[0] = frame_.ambient.r;
    c.ambient[1] = frame_.ambient.g;
    c.ambient[2] = frame_.ambient.b;
    c.sun[0] = frame_.sun.r;
    c.sun[1] = frame_.sun.g;
    c.sun[2] = frame_.sun.b;
  } else {
    c.ambient[0] = c.ambient[1] = c.ambient[2] = 1;
  }
  c.ambient[3] = 1;
  c.color[0] = material.color.r;
  c.color[1] = material.color.g;
  c.color[2] = material.color.b;
  c.color[3] = material.color.a;
  if (frame_.fog && frame_.fogEnd > frame_.fogStart) {
    const float k = 1.0f / (frame_.fogEnd - frame_.fogStart);
    c.fog[0] = k;
    c.fog[1] = -frame_.fogStart * k;
    c.fogColor[0] = clamp01(frame_.fogColor.r);
    c.fogColor[1] = clamp01(frame_.fogColor.g);
    c.fogColor[2] = clamp01(frame_.fogColor.b);
  }
  const hw3d::Visibility vis = hw3d::classifyBox(c.clip, mesh.lo, mesh.hi, guard_);
  if (vis == hw3d::Visibility::Outside) return;
  hw3d::setConstants(c);
  hw3d::setPipe(pipe, ht);
  if (vis == hw3d::Visibility::Inside) {
    if (hw3d::drawIndexed(mesh.phys, mesh.indices.data(), mesh.indices.size() / 3)) ++drawCalls_;
    return;
  }
  // Reaches behind the camera or far outside the picture: cut the triangles that cross the edge (hw3d_clip.h).
  ++clipped_;
  cut_.keep.clear();
  cut_.loose.clear();
  hw3d::clipMesh(mesh.cpu.data(), mesh.count, mesh.indices.data(), mesh.indices.size(), c.clip, guard_, cut_);
  if (!cut_.keep.empty() && hw3d::drawIndexed(mesh.phys, cut_.keep.data(), cut_.keep.size() / 3)) ++drawCalls_;
  if (!cut_.loose.empty()) {
    void* mem = hw3d::scratch(cut_.loose.size() * sizeof(Vertex3D));
    if (!mem) {
      OTE_WARN("3D drawing: no room for the triangles cut at the camera (%u vertices); they are left out", (unsigned)cut_.loose.size());
      return;
    }
    memcpy(mem, cut_.loose.data(), cut_.loose.size() * sizeof(Vertex3D));
    hw3d::flushWrites();
    if (hw3d::drawLoose(hw3d::physical(mem), cut_.loose.size())) ++drawCalls_;
  }
}

}  // namespace ote
