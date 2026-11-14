#include "gfx3d/gl/gl_renderer3d.h"

#include <algorithm>
#include <cstring>

#include "core/base/log.h"

namespace ote {

namespace {

// The buffers take Vertex3D as it is.
static_assert(sizeof(Vertex3D) == 36, "Vertex3D must stay 8 floats + 4 bytes (the GL vertex layout)");

bool powerOfTwo(int v) { return v > 0 && (v & (v - 1)) == 0; }

// Makes the context current for a block and puts the old one back after.
struct Use {
  GlHost& host;
  bool ok;
  explicit Use(GlHost& h) : host(h), ok(h.enter()) {}
  ~Use() {
    if (ok) host.leave();
  }
};

// The same arithmetic as the software backend (soft3d.cpp), which is the
// reference: lighting and fog per vertex, the triangle only blends them.
// The picture is drawn upside down (y negated) so that reading it back gives
// the top row first; front faces are therefore clockwise on screen.
const char* const kVertex = R"(
in vec3 aPos;
in vec3 aNormal;
in vec2 aUv;
in vec4 aColor;
uniform mat4 uMvp;
uniform mat4 uMv;
uniform mat3 uNormalM;
uniform vec4 uColor;
uniform vec3 uToSun;
uniform vec3 uSun;
uniform vec3 uAmbient;
uniform vec4 uParams;   // blend mode (0 solid, 1 cut-out, 2 blended), cutoff, lit, unused
uniform vec3 uFog;      // start, 1 / (end - start), on
out vec4 vColor;
centroid out vec2 vUv;  // centroid: with multisampling the position must stay inside the triangle,
                        // or a tile's edge samples read the transparent padding of its sheet (dark seams)
out float vFog;
void main() {
  vec3 light = vec3(1.0);
  if (uParams.z > 0.5) {
    vec3 n = uNormalM * aNormal;
    float len = length(n);
    if (len > 0.0) n /= len;
    light = uAmbient + uSun * max(0.0, dot(n, uToSun));
  }
  vColor = clamp(vec4(aColor.rgb * uColor.rgb * light, aColor.a * uColor.a), 0.0, 1.0);
  vUv = aUv;
  vFog = 0.0;
  if (uFog.z > 0.5) vFog = clamp((length((uMv * vec4(aPos, 1.0)).xyz) - uFog.x) * uFog.y, 0.0, 1.0);
  vec4 p = uMvp * vec4(aPos, 1.0);
  gl_Position = vec4(p.x, -p.y, p.z, p.w);
}
)";

const char* const kFragment = R"(
in vec4 vColor;
centroid in vec2 vUv;
in float vFog;
uniform sampler2D uTex;
uniform vec4 uParams;
uniform vec4 uFogColor;  // rgb, on
out vec4 oColor;
void main() {
  vec4 c = vColor * texture(uTex, vUv);
  if (uParams.x > 0.5 && uParams.x < 1.5 && c.a < uParams.y) discard;
  if (uFogColor.a > 0.5) c.rgb = mix(c.rgb, uFogColor.rgb, vFog);
  oColor = vec4(c.rgb, uParams.x > 1.5 ? c.a : 1.0);
}
)";

}  // namespace

// ---- start and end -------------------------------------------------------------

std::unique_ptr<Renderer3D> GlRenderer3D::create(std::unique_ptr<GlHost> host, std::string& why) {
  if (!host) {
    why = "the platform gave no GL context";
    return nullptr;
  }
  std::unique_ptr<GlRenderer3D> r(new GlRenderer3D(std::move(host)));
  if (!r->init(why)) return nullptr;
  return std::unique_ptr<Renderer3D>(r.release());
}

bool GlRenderer3D::init(std::string& why) {
  Use use(*host_);
  if (!use.ok) {
    why = "the GL context could not be made current";
    return false;
  }
  const char* missing = nullptr;
  if (!loadGlApi(gl_, *host_, missing)) {
    why = std::string("the GL driver lacks ") + missing;
    return false;
  }
  const char* version = (const char*)gl_.GetString(GL_VERSION_);
  const char* chip = (const char*)gl_.GetString(GL_RENDERER_);

  // Shaders. The two dialects differ in the first lines only.
  const char* head = host_->gles() ? "#version 300 es\nprecision highp float;\n" : "#version 330 core\n";
  auto compile = [&](GLenum_ kind, const char* body) -> GLuint_ {
    GLuint_ s = gl_.CreateShader(kind);
    const char* parts[2] = {head, body};
    gl_.ShaderSource(s, 2, parts, nullptr);
    gl_.CompileShader(s);
    GLint_ ok = 0;
    gl_.GetShaderiv(s, GL_COMPILE_STATUS_, &ok);
    if (!ok) {
      char log[512] = "";
      gl_.GetShaderInfoLog(s, (GLsizei_)sizeof(log) - 1, nullptr, log);
      why = std::string("a shader did not compile: ") + log;
      gl_.DeleteShader(s);
      return 0;
    }
    return s;
  };
  GLuint_ vs = compile(GL_VERTEX_SHADER_, kVertex);
  GLuint_ fs = vs ? compile(GL_FRAGMENT_SHADER_, kFragment) : 0;
  if (!vs || !fs) {
    if (vs) gl_.DeleteShader(vs);
    return false;
  }
  program_ = gl_.CreateProgram();
  gl_.AttachShader(program_, vs);
  gl_.AttachShader(program_, fs);
  gl_.BindAttribLocation(program_, 0, "aPos");
  gl_.BindAttribLocation(program_, 1, "aNormal");
  gl_.BindAttribLocation(program_, 2, "aUv");
  gl_.BindAttribLocation(program_, 3, "aColor");
  gl_.LinkProgram(program_);
  gl_.DeleteShader(vs);
  gl_.DeleteShader(fs);
  GLint_ linked = 0;
  gl_.GetProgramiv(program_, GL_LINK_STATUS_, &linked);
  if (!linked) {
    char log[512] = "";
    gl_.GetProgramInfoLog(program_, (GLsizei_)sizeof(log) - 1, nullptr, log);
    why = std::string("the shaders did not link: ") + log;
    freeAll();
    return false;
  }
  u_.mvp = gl_.GetUniformLocation(program_, "uMvp");
  u_.mv = gl_.GetUniformLocation(program_, "uMv");
  u_.normalM = gl_.GetUniformLocation(program_, "uNormalM");
  u_.color = gl_.GetUniformLocation(program_, "uColor");
  u_.toSun = gl_.GetUniformLocation(program_, "uToSun");
  u_.sun = gl_.GetUniformLocation(program_, "uSun");
  u_.ambient = gl_.GetUniformLocation(program_, "uAmbient");
  u_.lit = gl_.GetUniformLocation(program_, "uParams");
  u_.fog = gl_.GetUniformLocation(program_, "uFog");
  u_.fogColor = gl_.GetUniformLocation(program_, "uFogColor");
  u_.tex = gl_.GetUniformLocation(program_, "uTex");

  // What this context can take: textures up to the driver's limit (at most
  // 4096), pictures as big as its framebuffers allow, and its multisampling.
  GLint_ maxTex = 0, maxRb = 0, maxSamples = 0;
  gl_.GetIntegerv(GL_MAX_TEXTURE_SIZE_, &maxTex);
  gl_.GetIntegerv(GL_MAX_RENDERBUFFER_SIZE_, &maxRb);
  gl_.GetIntegerv(GL_MAX_SAMPLES_, &maxSamples);
  int tex = 1;
  while (tex * 2 <= maxTex && tex * 2 <= 4096) tex *= 2;  // sides are powers of two
  caps_.maxTextureSize = std::max(tex, 1024);              // every GL 3.3 / ES 3 / WebGL 2 context has at least 2048
  caps_.fullResolution = true;
  maxPicture_ = std::max(16, std::min(std::min(maxRb > 0 ? maxRb : 4096, maxTex > 0 ? maxTex : 4096), 4096));
  maxSamples_ = maxSamples;

  // One sampler per way of reading a texture: [mipmapped * 4 + repeat * 2 + smooth]. Hard pixels never
  // use the smaller copies, so they exist for smooth ones only.
  gl_.GenSamplers(8, samplers_);
  for (int i = 0; i < 8; ++i) {
    GLint_ wrap = (i & 2) ? (GLint_)GL_REPEAT_ : (GLint_)GL_CLAMP_TO_EDGE_;
    GLint_ mag = (i & 1) ? (GLint_)GL_LINEAR_ : (GLint_)GL_NEAREST_;
    GLint_ min = (i & 4) ? (GLint_)GL_LINEAR_MIPMAP_LINEAR_ : mag;
    gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_WRAP_S_, wrap);
    gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_WRAP_T_, wrap);
    gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_MIN_FILTER_, min);
    gl_.SamplerParameteri(samplers_[i], GL_TEXTURE_MAG_FILTER_, mag);
  }

  // "No texture" is a white pixel, so the shader always samples.
  const uint8_t white[4] = {255, 255, 255, 255};
  gl_.PixelStorei(GL_UNPACK_ALIGNMENT_, 1);
  gl_.PixelStorei(GL_PACK_ALIGNMENT_, 1);
  gl_.GenTextures(1, &white_);
  gl_.BindTexture(GL_TEXTURE_2D_, white_);
  gl_.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, (GLint_)GL_NEAREST_);
  gl_.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, (GLint_)GL_NEAREST_);
  gl_.TexImage2D(GL_TEXTURE_2D_, 0, (GLint_)GL_RGBA8_, 1, 1, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, white);

  // Proof that drawing into a picture and reading it back works here: clear
  // a small one to a known colour and look at it.
  if (!target(16, 16, 0)) {
    why = "a framebuffer could not be made";
    freeAll();
    return false;
  }
  gl_.Disable(GL_DITHER_);
  gl_.ClearColor(0.2f, 0.4f, 0.6f, 1.0f);
  gl_.Clear(GL_COLOR_BUFFER_BIT_);
  uint8_t px[4] = {0, 0, 0, 0};
  gl_.ReadPixels(3, 3, 1, 1, GL_RGBA_, GL_UNSIGNED_BYTE_, px);
  bool right = px[0] >= 50 && px[0] <= 52 && px[1] >= 101 && px[1] <= 103 && px[2] >= 152 && px[2] <= 154 && px[3] == 255;
  GLenum_ glErr = gl_.GetError();
  if (!right || glErr != 0) {
    why = "a test picture came back wrong";
    OTE_WARN("3D (OpenGL): self-test read %d,%d,%d,%d, GL error 0x%x", px[0], px[1], px[2], px[3], glErr);
    freeAll();
    return false;
  }
  OTE_INFO("3D drawing: OpenGL (%s; %s)", version ? version : "?", chip ? chip : "?");
  return true;
}

void GlRenderer3D::freeAll() {
  for (Mesh& m : meshes_)
    if (m.used) {
      gl_.DeleteVertexArrays(1, &m.vao);
      gl_.DeleteBuffers(1, &m.vbo);
      gl_.DeleteBuffers(1, &m.ibo);
    }
  for (Texture& t : textures_)
    if (t.used) gl_.DeleteTextures(1, &t.id);
  meshes_.clear();
  textures_.clear();
  if (fbo_) gl_.DeleteFramebuffers(1, &fbo_);
  if (msFbo_) gl_.DeleteFramebuffers(1, &msFbo_);
  if (colorTex_) gl_.DeleteTextures(1, &colorTex_);
  if (depthRb_) gl_.DeleteRenderbuffers(1, &depthRb_);
  if (msColorRb_) gl_.DeleteRenderbuffers(1, &msColorRb_);
  if (msDepthRb_) gl_.DeleteRenderbuffers(1, &msDepthRb_);
  if (white_) gl_.DeleteTextures(1, &white_);
  if (samplers_[0]) gl_.DeleteSamplers(8, samplers_);
  if (program_) gl_.DeleteProgram(program_);
  fbo_ = msFbo_ = colorTex_ = depthRb_ = msColorRb_ = msDepthRb_ = white_ = program_ = 0;
  for (GLuint_& smp : samplers_) smp = 0;
  fbW_ = fbH_ = fbSamples_ = 0;
}

GlRenderer3D::~GlRenderer3D() {
  // When the platform has already shut down the context took everything with it.
  Use use(*host_);
  if (use.ok && gl_.DeleteProgram) freeAll();
}

// ---- resources -----------------------------------------------------------------

bool GlRenderer3D::validMesh(const MeshData& d) const {
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

// Context current. Indices fit 16 bits (the vertex limit).
void GlRenderer3D::upload(Mesh& m, const MeshData& data) {
  indexScratch_.resize(data.indices.size());
  for (size_t i = 0; i < data.indices.size(); ++i) indexScratch_[i] = (uint16_t)data.indices[i];
  GLenum_ usage = m.dynamic ? GL_DYNAMIC_DRAW_ : GL_STATIC_DRAW_;
  gl_.BindVertexArray(m.vao);
  gl_.BindBuffer(GL_ARRAY_BUFFER_, m.vbo);
  gl_.BufferData(GL_ARRAY_BUFFER_, (GLsizeiptr_)(data.vertices.size() * sizeof(Vertex3D)), data.vertices.data(), usage);
  gl_.BindBuffer(GL_ELEMENT_ARRAY_BUFFER_, m.ibo);
  gl_.BufferData(GL_ELEMENT_ARRAY_BUFFER_, (GLsizeiptr_)(indexScratch_.size() * sizeof(uint16_t)), indexScratch_.data(), usage);
  gl_.BindVertexArray(0);
  m.indexCount = (int)data.indices.size();
}

MeshId GlRenderer3D::createMesh(const MeshData& data) {
  if (!validMesh(data)) return 0;
  Use use(*host_);
  if (!use.ok) return 0;
  size_t slot = 0;
  while (slot < meshes_.size() && meshes_[slot].used) ++slot;
  if (slot == meshes_.size()) meshes_.push_back(Mesh());
  Mesh& m = meshes_[slot];
  m = Mesh();
  m.used = true;
  gl_.GenVertexArrays(1, &m.vao);
  gl_.GenBuffers(1, &m.vbo);
  gl_.GenBuffers(1, &m.ibo);
  gl_.BindVertexArray(m.vao);
  gl_.BindBuffer(GL_ARRAY_BUFFER_, m.vbo);
  const GLsizei_ stride = (GLsizei_)sizeof(Vertex3D);
  for (GLuint_ a = 0; a < 4; ++a) gl_.EnableVertexAttribArray(a);
  gl_.VertexAttribPointer(0, 3, GL_FLOAT_, 0, stride, (const void*)0);
  gl_.VertexAttribPointer(1, 3, GL_FLOAT_, 0, stride, (const void*)12);
  gl_.VertexAttribPointer(2, 2, GL_FLOAT_, 0, stride, (const void*)24);
  gl_.VertexAttribPointer(3, 4, GL_UNSIGNED_BYTE_, 1, stride, (const void*)32);
  upload(m, data);
  return (MeshId)(slot + 1);
}

void GlRenderer3D::updateMesh(MeshId mesh, const MeshData& data) {
  if (mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  if (!validMesh(data)) return;
  Use use(*host_);
  if (!use.ok) return;
  Mesh& m = meshes_[(size_t)mesh - 1];
  // A solid draw of the old shape is still waiting: carry it out first.
  if (m.queued) flush(false);
  m.dynamic = true;
  upload(m, data);
}

void GlRenderer3D::destroyMesh(MeshId mesh) {
  if (mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  Mesh& m = meshes_[(size_t)mesh - 1];
  Use use(*host_);
  if (use.ok) {
    if (m.queued) flush(false);
    gl_.DeleteVertexArrays(1, &m.vao);
    gl_.DeleteBuffers(1, &m.vbo);
    gl_.DeleteBuffers(1, &m.ibo);
  }
  m = Mesh();
}

Texture3DId GlRenderer3D::createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps) {
  int limit = caps().maxTextureSize;
  if (!rgba || !powerOfTwo(width) || !powerOfTwo(height) || width > limit || height > limit) {
    OTE_ERROR("3D texture refused: %d x %d; each side must be a power of two up to %d", width, height, limit);
    return 0;
  }
  Use use(*host_);
  if (!use.ok) return 0;
  size_t slot = 0;
  while (slot < textures_.size() && textures_[slot].used) ++slot;
  if (slot == textures_.size()) textures_.push_back(Texture());
  Texture& t = textures_[slot];
  t = Texture();
  t.used = true;
  t.repeat = repeat;
  t.mipmaps = mipmaps;
  gl_.GenTextures(1, &t.id);
  gl_.ActiveTexture(GL_TEXTURE0_);
  gl_.BindTexture(GL_TEXTURE_2D_, t.id);
  // The samplers decide how it is read; these only keep it "complete" without mipmaps.
  gl_.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, (GLint_)GL_NEAREST_);
  gl_.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, (GLint_)GL_NEAREST_);
  gl_.TexImage2D(GL_TEXTURE_2D_, 0, (GLint_)GL_RGBA8_, width, height, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, rgba);
  if (mipmaps) gl_.GenerateMipmap(GL_TEXTURE_2D_);  // the smooth samplers then read the smaller copies too
  return (Texture3DId)(slot + 1);
}

void GlRenderer3D::destroyTexture(Texture3DId texture) {
  if (texture < 1 || (size_t)texture > textures_.size() || !textures_[(size_t)texture - 1].used) return;
  Texture& t = textures_[(size_t)texture - 1];
  Use use(*host_);
  if (use.ok) {
    if (t.queued) flush(false);
    gl_.DeleteTextures(1, &t.id);
  }
  t = Texture();
}

// ---- frame ---------------------------------------------------------------------

bool GlRenderer3D::target(int w, int h, int samples) {
  if (samples > maxSamples_) samples = maxSamples_;
  if (samples < 2 || msFailed_) samples = 0;
  if (fbo_ && w == fbW_ && h == fbH_ && (samples == 0) == (fbSamples_ == 0)) {
    gl_.BindFramebuffer(GL_FRAMEBUFFER_, fbSamples_ ? msFbo_ : fbo_);
    return true;
  }
  if (!fbo_) {
    gl_.GenFramebuffers(1, &fbo_);
    gl_.GenTextures(1, &colorTex_);
    gl_.GenRenderbuffers(1, &depthRb_);
  }
  gl_.ActiveTexture(GL_TEXTURE0_);
  gl_.BindTexture(GL_TEXTURE_2D_, colorTex_);
  gl_.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, (GLint_)GL_NEAREST_);
  gl_.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, (GLint_)GL_NEAREST_);
  gl_.TexImage2D(GL_TEXTURE_2D_, 0, (GLint_)GL_RGBA8_, w, h, 0, GL_RGBA_, GL_UNSIGNED_BYTE_, nullptr);
  gl_.BindFramebuffer(GL_FRAMEBUFFER_, fbo_);
  gl_.FramebufferTexture2D(GL_FRAMEBUFFER_, GL_COLOR_ATTACHMENT0_, GL_TEXTURE_2D_, colorTex_, 0);

  fbSamples_ = 0;
  if (samples > 0) {
    // Multisampled colour and depth; the finished frame is resolved into fbo_'s texture in end().
    if (!msFbo_) {
      gl_.GenFramebuffers(1, &msFbo_);
      gl_.GenRenderbuffers(1, &msColorRb_);
      gl_.GenRenderbuffers(1, &msDepthRb_);
    }
    gl_.GetError();  // forget earlier errors: only this block's count
    gl_.BindRenderbuffer(GL_RENDERBUFFER_, msColorRb_);
    gl_.RenderbufferStorageMultisample(GL_RENDERBUFFER_, samples, GL_RGBA8_, w, h);
    gl_.BindRenderbuffer(GL_RENDERBUFFER_, msDepthRb_);
    gl_.RenderbufferStorageMultisample(GL_RENDERBUFFER_, samples, GL_DEPTH_COMPONENT24_, w, h);
    gl_.BindFramebuffer(GL_FRAMEBUFFER_, msFbo_);
    gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER_, GL_COLOR_ATTACHMENT0_, GL_RENDERBUFFER_, msColorRb_);
    gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER_, GL_DEPTH_ATTACHMENT_, GL_RENDERBUFFER_, msDepthRb_);
    if (gl_.CheckFramebufferStatus(GL_FRAMEBUFFER_) == GL_FRAMEBUFFER_COMPLETE_ && gl_.GetError() == 0) {
      fbSamples_ = samples;
    } else {
      msFailed_ = true;  // silently carry on without
      OTE_INFO("3D (OpenGL): %dx multisampling not available here; drawing without", samples);
    }
  }
  if (!fbSamples_) {
    gl_.BindRenderbuffer(GL_RENDERBUFFER_, depthRb_);
    gl_.RenderbufferStorage(GL_RENDERBUFFER_, GL_DEPTH_COMPONENT24_, w, h);
    gl_.BindFramebuffer(GL_FRAMEBUFFER_, fbo_);
    gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER_, GL_DEPTH_ATTACHMENT_, GL_RENDERBUFFER_, depthRb_);
  }
  if (fbSamples_) {  // fbo_ only receives the resolved colour: a depth buffer of an older size would make it incomplete
    gl_.BindFramebuffer(GL_FRAMEBUFFER_, fbo_);
    gl_.FramebufferRenderbuffer(GL_FRAMEBUFFER_, GL_DEPTH_ATTACHMENT_, GL_RENDERBUFFER_, 0);
  }
  gl_.BindFramebuffer(GL_FRAMEBUFFER_, fbSamples_ ? msFbo_ : fbo_);
  fbW_ = w;
  fbH_ = h;
  if (gl_.CheckFramebufferStatus(GL_FRAMEBUFFER_) != GL_FRAMEBUFFER_COMPLETE_) {
    fbW_ = fbH_ = 0;
    return false;
  }
  return true;
}

void GlRenderer3D::begin(const Frame3D& frame) {
  frame_ = frame;
  frame_.sunDir = normalize(frame.sunDir);
  w_ = frame.renderWidth > 0 ? frame.renderWidth : std::max(0, frame.width);
  h_ = frame.renderHeight > 0 ? frame.renderHeight : std::max(0, frame.height);
  const int longer = std::max(w_, h_);
  if (longer > maxPicture_) {  // over what this context's framebuffers (or the 4096 cap) allow: same picture, smaller
    w_ = std::max(1, (int)((long long)w_ * maxPicture_ / longer));
    h_ = std::max(1, (int)((long long)h_ * maxPicture_ / longer));
  }
  queue_.clear();
  blended_.clear();
  for (Mesh& m : meshes_) m.queued = false;
  for (Texture& t : textures_) t.queued = false;
  cleared_ = false;
  inFrame_ = true;
}

void GlRenderer3D::draw(MeshId mesh, const Mat4& model, const Material3D& material) {
  if (!inFrame_ || mesh < 1 || (size_t)mesh > meshes_.size() || !meshes_[(size_t)mesh - 1].used) return;
  Cmd c{mesh, model, material, 0};
  if (material.blend == Blend3D::Blend) {
    // See-through things wait until everything solid is in the picture.
    c.depth = -(frame_.view * model).point(Vec3(0, 0, 0)).z;
    blended_.push_back(c);
    return;
  }
  queue_.push_back(c);
  meshes_[(size_t)mesh - 1].queued = true;
  if (material.texture >= 1 && (size_t)material.texture <= textures_.size()) textures_[(size_t)material.texture - 1].queued = true;
}

// Context current.
void GlRenderer3D::flush(bool withBlended) {
  if (!inFrame_ || w_ == 0 || h_ == 0 || broken_) {
    queue_.clear();
    return;
  }
  if (!target(w_, h_, frame_.samples)) {
    if (!broken_) OTE_ERROR("3D (OpenGL): no framebuffer of %d x %d; the 3D picture is left out", w_, h_);
    broken_ = true;
    queue_.clear();
    return;
  }
  gl_.Viewport(0, 0, w_, h_);
  gl_.Disable(GL_DITHER_);
  gl_.Enable(GL_DEPTH_TEST_);
  gl_.FrontFace(GL_CW_);  // the picture is drawn upside down (see the vertex shader)
  gl_.CullFace(GL_BACK_);
  gl_.BlendFuncSeparate(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_, GL_ONE_, GL_ONE_MINUS_SRC_ALPHA_);
  if (!cleared_) {
    gl_.DepthMask(1);
    gl_.ClearColor(frame_.clear.r, frame_.clear.g, frame_.clear.b, frame_.clear.a);
    gl_.Clear(GL_COLOR_BUFFER_BIT_ | GL_DEPTH_BUFFER_BIT_);
    cleared_ = true;
  }
  gl_.UseProgram(program_);
  gl_.Uniform1i(u_.tex, 0);
  gl_.ActiveTexture(GL_TEXTURE0_);
  const Vec3 toSun = frame_.sunDir * -1.0f;
  gl_.Uniform3f(u_.toSun, toSun.x, toSun.y, toSun.z);
  gl_.Uniform3f(u_.sun, frame_.sun.r, frame_.sun.g, frame_.sun.b);
  gl_.Uniform3f(u_.ambient, frame_.ambient.r, frame_.ambient.g, frame_.ambient.b);
  const bool ranged = frame_.fog && frame_.fogEnd > frame_.fogStart;
  gl_.Uniform3f(u_.fog, frame_.fogStart, ranged ? 1.0f / (frame_.fogEnd - frame_.fogStart) : 0.0f, ranged ? 1.0f : 0.0f);
  gl_.Uniform4f(u_.fogColor, frame_.fogColor.r, frame_.fogColor.g, frame_.fogColor.b, frame_.fog ? 1.0f : 0.0f);

  for (const Cmd& c : queue_) run(c);
  queue_.clear();
  for (Mesh& m : meshes_) m.queued = false;
  for (Texture& t : textures_) t.queued = false;
  if (withBlended) {
    std::stable_sort(blended_.begin(), blended_.end(), [](const Cmd& a, const Cmd& b) { return a.depth > b.depth; });
    for (const Cmd& c : blended_) run(c);
    blended_.clear();
  }
  gl_.BindVertexArray(0);
}

void GlRenderer3D::run(const Cmd& c) {
  if ((size_t)c.mesh > meshes_.size() || !meshes_[(size_t)c.mesh - 1].used) return;
  const Mesh& m = meshes_[(size_t)c.mesh - 1];
  const Material3D& mat = c.material;
  const Texture* tex = nullptr;
  if (mat.texture >= 1 && (size_t)mat.texture <= textures_.size() && textures_[(size_t)mat.texture - 1].used)
    tex = &textures_[(size_t)mat.texture - 1];

  const bool blend = mat.blend == Blend3D::Blend;
  if (blend) gl_.Enable(GL_BLEND_);
  else gl_.Disable(GL_BLEND_);
  // Without the depth test the thing still leaves its depth behind (as the
  // software backend does), so the test stays on and always passes.
  gl_.DepthFunc(mat.depthTest ? GL_LESS_ : GL_ALWAYS_);
  gl_.DepthMask(blend ? 0 : 1);
  if (mat.twoSided) gl_.Disable(GL_CULL_FACE_);
  else gl_.Enable(GL_CULL_FACE_);

  gl_.BindTexture(GL_TEXTURE_2D_, tex ? tex->id : white_);
  gl_.BindSampler(0, samplers_[(tex && tex->repeat ? 2 : 0) + (mat.smooth ? 1 : 0) + (mat.smooth && tex && tex->mipmaps ? 4 : 0)]);

  const Mat4 mv = frame_.view * c.model, mvp = frame_.proj * mv;
  const float* mm = c.model.m;
  const float normalM[9] = {mm[0], mm[1], mm[2], mm[4], mm[5], mm[6], mm[8], mm[9], mm[10]};
  gl_.UniformMatrix4fv(u_.mvp, 1, 0, mvp.m);
  gl_.UniformMatrix4fv(u_.mv, 1, 0, mv.m);
  gl_.UniformMatrix3fv(u_.normalM, 1, 0, normalM);
  gl_.Uniform4f(u_.color, mat.color.r, mat.color.g, mat.color.b, mat.color.a);
  float mode = mat.blend == Blend3D::Cutout ? 1.0f : blend ? 2.0f : 0.0f;
  gl_.Uniform4f(u_.lit, mode, mat.cutoff, mat.lit ? 1.0f : 0.0f, 0.0f);

  gl_.BindVertexArray(m.vao);
  gl_.DrawElements(GL_TRIANGLES_, m.indexCount, GL_UNSIGNED_SHORT_, (const void*)0);
}

void GlRenderer3D::end() {
  if (!inFrame_) return;
  Use use(*host_);
  if (use.ok) flush(true);
  queue_.clear();
  blended_.clear();
  pixels_.resize((size_t)w_ * h_ * 4);
  if (use.ok && !broken_ && w_ > 0 && h_ > 0) {
    if (fbSamples_) {  // resolve the multisampled frame into the texture
      gl_.BindFramebuffer(GL_READ_FRAMEBUFFER_, msFbo_);
      gl_.BindFramebuffer(GL_DRAW_FRAMEBUFFER_, fbo_);
      gl_.BlitFramebuffer(0, 0, w_, h_, 0, 0, w_, h_, GL_COLOR_BUFFER_BIT_, GL_NEAREST_);
    }
    gl_.BindFramebuffer(GL_FRAMEBUFFER_, fbo_);
    gl_.ReadPixels(0, 0, w_, h_, GL_RGBA_, GL_UNSIGNED_BYTE_, pixels_.data());
  } else {
    std::fill(pixels_.begin(), pixels_.end(), (uint8_t)0);
  }
  inFrame_ = false;
}

}  // namespace ote
