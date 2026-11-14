#include "gfx3d/soft3d.h"

#include <algorithm>
#include <cmath>

#include "core/base/log.h"

namespace ote {

// Clip-space position plus everything that is blended across a triangle.
struct SoftRenderer3D::V {
  float x, y, z, w;
  float u, v;
  float r, g, b, a;  // lit colour, 0..1
  float fog;         // 0 = none, 1 = all fog
};

namespace {

float clamp01(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }
bool powerOfTwo(int v) { return v > 0 && (v & (v - 1)) == 0; }

}  // namespace

// ---- resources ---------------------------------------------------------------

bool SoftRenderer3D::validMesh(const MeshData& d) const {
  if (d.vertices.empty() || d.indices.empty() || d.indices.size() % 3 != 0) {
    OTE_ERROR("3D mesh refused: it needs vertices and whole triangles (%d vertices, %d indices)",
              (int)d.vertices.size(), (int)d.indices.size());
    return false;
  }
  if ((int)d.vertices.size() > caps().maxMeshVertices) {
    OTE_ERROR("3D mesh refused: %d vertices, the limit is %d per mesh", (int)d.vertices.size(), caps().maxMeshVertices);
    return false;
  }
  for (uint32_t i : d.indices)
    if (i >= d.vertices.size()) {
      OTE_ERROR("3D mesh refused: an index points past its %d vertices", (int)d.vertices.size());
      return false;
    }
  return true;
}

MeshId SoftRenderer3D::createMesh(const MeshData& data) {
  if (!validMesh(data)) return 0;
  size_t slot = 0;
  while (slot < meshes_.size() && meshes_[slot].used) ++slot;
  if (slot == meshes_.size()) meshes_.push_back(Mesh());
  meshes_[slot].used = true;
  meshes_[slot].data = data;
  return (MeshId)(slot + 1);
}

void SoftRenderer3D::updateMesh(MeshId mesh, const MeshData& data) {
  if (mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  if (validMesh(data)) meshes_[(size_t)mesh - 1].data = data;
}

void SoftRenderer3D::destroyMesh(MeshId mesh) {
  if (mesh < 1 || (size_t)mesh > meshes_.size()) return;
  meshes_[(size_t)mesh - 1] = Mesh();
}

Texture3DId SoftRenderer3D::createTexture(const uint8_t* rgba, int width, int height, bool repeat) {
  int limit = caps().maxTextureSize;
  if (!rgba || !powerOfTwo(width) || !powerOfTwo(height) || width > limit || height > limit) {
    OTE_ERROR("3D texture refused: %d x %d; each side must be a power of two up to %d", width, height, limit);
    return 0;
  }
  size_t slot = 0;
  while (slot < textures_.size() && textures_[slot].used) ++slot;
  if (slot == textures_.size()) textures_.push_back(Texture());
  Texture& t = textures_[slot];
  t.used = true;
  t.w = width;
  t.h = height;
  t.repeat = repeat;
  t.rgba.assign(rgba, rgba + (size_t)width * height * 4);
  return (Texture3DId)(slot + 1);
}

void SoftRenderer3D::destroyTexture(Texture3DId texture) {
  if (texture < 1 || (size_t)texture > textures_.size()) return;
  textures_[(size_t)texture - 1] = Texture();
}

// ---- frame ---------------------------------------------------------------------

void SoftRenderer3D::begin(const Frame3D& frame) {
  frame_ = frame;
  frame_.sunDir = normalize(frame.sunDir);
  w_ = std::max(0, frame.width) * scale_;
  h_ = std::max(0, frame.height) * scale_;
  color_.resize((size_t)w_ * h_ * 4);
  depth_.assign((size_t)w_ * h_, 1.0f);
  uint8_t c[4] = {(uint8_t)(clamp01(frame.clear.r) * 255.0f + 0.5f), (uint8_t)(clamp01(frame.clear.g) * 255.0f + 0.5f),
                  (uint8_t)(clamp01(frame.clear.b) * 255.0f + 0.5f), (uint8_t)(clamp01(frame.clear.a) * 255.0f + 0.5f)};
  for (size_t i = 0; i < color_.size(); i += 4) {
    color_[i] = c[0];
    color_[i + 1] = c[1];
    color_[i + 2] = c[2];
    color_[i + 3] = c[3];
  }
  blended_.clear();
  triangles_ = 0;
  inFrame_ = true;
}

void SoftRenderer3D::draw(MeshId mesh, const Mat4& model, const Material3D& material) {
  if (!inFrame_ || mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  if (material.blend == Blend3D::Blend) {
    // See-through things wait until everything solid is in the picture.
    Vec3 origin = (frame_.view * model).point(Vec3(0, 0, 0));
    blended_.push_back(Pending{mesh, model, material, -origin.z});
    return;
  }
  drawNow(meshes_[(size_t)mesh - 1], model, material);
}

void SoftRenderer3D::end() {
  if (!inFrame_) return;
  std::stable_sort(blended_.begin(), blended_.end(), [](const Pending& a, const Pending& b) { return a.depth > b.depth; });
  for (const Pending& p : blended_)
    if ((size_t)p.mesh <= meshes_.size() && meshes_[(size_t)p.mesh - 1].used)
      drawNow(meshes_[(size_t)p.mesh - 1], p.model, p.material);
  blended_.clear();
  inFrame_ = false;
}

// ---- drawing -------------------------------------------------------------------

void SoftRenderer3D::drawNow(const Mesh& mesh, const Mat4& model, const Material3D& material) {
  if (w_ == 0 || h_ == 0) return;
  const Texture* tex = nullptr;
  if (material.texture >= 1 && (size_t)material.texture <= textures_.size() && textures_[(size_t)material.texture - 1].used)
    tex = &textures_[(size_t)material.texture - 1];

  // Per vertex: position to clip space, lighting and fog (the way
  // fixed-function hardware does it; the triangle only blends the results).
  const Mat4 mv = frame_.view * model, mvp = frame_.proj * mv;
  const Vec3 toSun = frame_.sunDir * -1.0f;
  const std::vector<Vertex3D>& in = mesh.data.vertices;
  std::vector<V> vs(in.size());
  for (size_t i = 0; i < in.size(); ++i) {
    const Vertex3D& s = in[i];
    Vec4 p = mvp * Vec4(s.x, s.y, s.z, 1);
    V& o = vs[i];
    o.x = p.x;
    o.y = p.y;
    o.z = p.z;
    o.w = p.w;
    o.u = s.u;
    o.v = s.v;
    float lr = 1, lg = 1, lb = 1;
    if (material.lit) {
      Vec3 n = normalize(model.direction(Vec3(s.nx, s.ny, s.nz)));
      float d = std::max(0.0f, dot(n, toSun));
      lr = frame_.ambient.r + frame_.sun.r * d;
      lg = frame_.ambient.g + frame_.sun.g * d;
      lb = frame_.ambient.b + frame_.sun.b * d;
    }
    o.r = clamp01(s.r / 255.0f * material.color.r * lr);
    o.g = clamp01(s.g / 255.0f * material.color.g * lg);
    o.b = clamp01(s.b / 255.0f * material.color.b * lb);
    o.a = clamp01(s.a / 255.0f * material.color.a);
    o.fog = 0;
    if (frame_.fog && frame_.fogEnd > frame_.fogStart) {
      float dist = length(mv.point(Vec3(s.x, s.y, s.z)));
      o.fog = clamp01((dist - frame_.fogStart) / (frame_.fogEnd - frame_.fogStart));
    }
  }

  const std::vector<uint32_t>& idx = mesh.data.indices;
  for (size_t i = 0; i + 2 < idx.size(); i += 3) {
    // Cut the triangle at the near plane (z = -w): what is behind the camera
    // must not reach the perspective divide. At most one extra corner.
    V poly[4];
    int n = 0;
    const V* tri[3] = {&vs[idx[i]], &vs[idx[i + 1]], &vs[idx[i + 2]]};
    for (int k = 0; k < 3; ++k) {
      const V& a = *tri[k];
      const V& b = *tri[(k + 1) % 3];
      float da = a.z + a.w, db = b.z + b.w;
      if (da >= 0) poly[n++] = a;
      if ((da >= 0) != (db >= 0)) {
        float t = da / (da - db);
        V c;
        c.x = a.x + (b.x - a.x) * t;
        c.y = a.y + (b.y - a.y) * t;
        c.z = a.z + (b.z - a.z) * t;
        c.w = a.w + (b.w - a.w) * t;
        c.u = a.u + (b.u - a.u) * t;
        c.v = a.v + (b.v - a.v) * t;
        c.r = a.r + (b.r - a.r) * t;
        c.g = a.g + (b.g - a.g) * t;
        c.b = a.b + (b.b - a.b) * t;
        c.a = a.a + (b.a - a.a) * t;
        c.fog = a.fog + (b.fog - a.fog) * t;
        poly[n++] = c;
      }
    }
    for (int k = 1; k + 1 < n; ++k) triangle(poly[0], poly[k], poly[k + 1], material, tex);
  }
}

void SoftRenderer3D::triangle(const V& a, const V& b, const V& c, const Material3D& material, const Texture* tex) {
  const V* v[3] = {&a, &b, &c};
  float sx[3], sy[3], sz[3], iw[3];
  for (int i = 0; i < 3; ++i) {
    if (v[i]->w <= 1e-6f) return;
    iw[i] = 1.0f / v[i]->w;
    sx[i] = (v[i]->x * iw[i] * 0.5f + 0.5f) * w_;
    sy[i] = (1.0f - (v[i]->y * iw[i] * 0.5f + 0.5f)) * h_;
    sz[i] = v[i]->z * iw[i];
  }
  // Twice the signed area. With y pointing down, a front face
  // (counter-clockwise in the world) comes out negative.
  float area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
  if (area == 0) return;
  if (area > 0 && !material.twoSided) return;
  float inv = 1.0f / area;

  int x0 = std::max(0, (int)std::floor(std::min(sx[0], std::min(sx[1], sx[2]))));
  int x1 = std::min(w_ - 1, (int)std::ceil(std::max(sx[0], std::max(sx[1], sx[2]))));
  int y0 = std::max(0, (int)std::floor(std::min(sy[0], std::min(sy[1], sy[2]))));
  int y1 = std::min(h_ - 1, (int)std::ceil(std::max(sy[0], std::max(sy[1], sy[2]))));
  if (x0 > x1 || y0 > y1) return;
  ++triangles_;

  const bool blend = material.blend == Blend3D::Blend, cutout = material.blend == Blend3D::Cutout;
  const float fr = clamp01(frame_.fogColor.r), fg = clamp01(frame_.fogColor.g), fb = clamp01(frame_.fogColor.b);

  for (int y = y0; y <= y1; ++y) {
    float py = y + 0.5f;
    for (int x = x0; x <= x1; ++x) {
      float px = x + 0.5f;
      // Weights of the three corners at this pixel's middle.
      float w0 = ((sx[1] - px) * (sy[2] - py) - (sx[2] - px) * (sy[1] - py)) * inv;
      float w1 = ((sx[2] - px) * (sy[0] - py) - (sx[0] - px) * (sy[2] - py)) * inv;
      float w2 = 1.0f - w0 - w1;
      if (w0 < 0 || w1 < 0 || w2 < 0) continue;
      float z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
      size_t at = (size_t)y * w_ + x;
      if (z > 1.0f || (material.depthTest && z >= depth_[at])) continue;

      // Perspective-correct blend: weight by 1/w.
      float q0 = w0 * iw[0], q1 = w1 * iw[1], q2 = w2 * iw[2], qi = 1.0f / (q0 + q1 + q2);
      q0 *= qi;
      q1 *= qi;
      q2 *= qi;
      float r = q0 * a.r + q1 * b.r + q2 * c.r, g = q0 * a.g + q1 * b.g + q2 * c.g, bl = q0 * a.b + q1 * b.b + q2 * c.b;
      float al = q0 * a.a + q1 * b.a + q2 * c.a;

      if (tex) {
        float u = q0 * a.u + q1 * b.u + q2 * c.u, tv = q0 * a.v + q1 * b.v + q2 * c.v;
        float tx = u * tex->w, ty = tv * tex->h;
        auto wrap = [](int i, int n, bool repeat) {
          if (repeat) return ((i % n) + n) % n;
          return i < 0 ? 0 : i >= n ? n - 1 : i;
        };
        float tr, tg, tb, ta;
        if (material.smooth) {
          tx -= 0.5f;
          ty -= 0.5f;
          int ix = (int)std::floor(tx), iy = (int)std::floor(ty);
          float fx = tx - ix, fy = ty - iy;
          float acc[4] = {0, 0, 0, 0};
          for (int k = 0; k < 4; ++k) {
            int cx = wrap(ix + (k & 1), tex->w, tex->repeat), cy = wrap(iy + (k >> 1), tex->h, tex->repeat);
            float wgt = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
            const uint8_t* t = &tex->rgba[((size_t)cy * tex->w + cx) * 4];
            for (int ch = 0; ch < 4; ++ch) acc[ch] += t[ch] * wgt;
          }
          tr = acc[0] / 255.0f;
          tg = acc[1] / 255.0f;
          tb = acc[2] / 255.0f;
          ta = acc[3] / 255.0f;
        } else {
          int cx = wrap((int)std::floor(tx), tex->w, tex->repeat), cy = wrap((int)std::floor(ty), tex->h, tex->repeat);
          const uint8_t* t = &tex->rgba[((size_t)cy * tex->w + cx) * 4];
          tr = t[0] / 255.0f;
          tg = t[1] / 255.0f;
          tb = t[2] / 255.0f;
          ta = t[3] / 255.0f;
        }
        r *= tr;
        g *= tg;
        bl *= tb;
        al *= ta;
      }
      if (cutout && al < material.cutoff) continue;
      if (blend && al <= 0) continue;

      if (frame_.fog) {
        float f = q0 * a.fog + q1 * b.fog + q2 * c.fog;
        r += (fr - r) * f;
        g += (fg - g) * f;
        bl += (fb - bl) * f;
      }
      uint8_t* out = &color_[at * 4];
      if (blend) {
        float k = clamp01(al);
        out[0] = (uint8_t)(out[0] + (r * 255.0f - out[0]) * k + 0.5f);
        out[1] = (uint8_t)(out[1] + (g * 255.0f - out[1]) * k + 0.5f);
        out[2] = (uint8_t)(out[2] + (bl * 255.0f - out[2]) * k + 0.5f);
        out[3] = (uint8_t)(out[3] + (255.0f - out[3]) * k + 0.5f);
      } else {
        out[0] = (uint8_t)(clamp01(r) * 255.0f + 0.5f);
        out[1] = (uint8_t)(clamp01(g) * 255.0f + 0.5f);
        out[2] = (uint8_t)(clamp01(bl) * 255.0f + 0.5f);
        out[3] = 255;
        depth_[at] = z;
      }
    }
  }
}

}  // namespace ote
