// Android: the hardware 3D backend's GL context, made with EGL directly (docs/ANDROID.md "3D").
//
// SDL on Android has one window, so the hidden second window the desktop host uses (sdl_gl3d.cpp) does not exist. Instead
// this makes an EGL context of its own on a 1 x 1 pbuffer: independent of the config SDL's 2D renderer picked, never shown,
// and the backend draws into its own framebuffer anyway. (Alternative not taken: a second SDL_GL_CreateContext on the main
// window. It would have to share that window's EGL config, which SDL picks for the 2D renderer, and SDL's own bookkeeping
// of "the current context" would be in the way of an exact save/restore.)
//
// Never eglTerminate the display here: SDL's own context lives on the same display, and EGL has no reference counting.
//
// Life cycle. Android may take EGL surfaces away when the app goes to the background; a pbuffer context normally
// survives that, but a driver may still lose it (EGL_CONTEXT_LOST, GPU reset, memory pressure). GlRenderer3D has no
// "recreate everything" path, so ResilientRenderer3D below keeps a CPU copy of every mesh and texture the game made and,
// when the context is gone, makes a new context and backend and replays them (the game's ids stay valid). If even that
// fails, the same copies are replayed into the software backend for the rest of the session. Nothing above this file
// notices, apart from one log line.
//
// Debug: OTE_3D_SIMULATE_LOSS=N destroys the context after N frames, to test the above; with OTE_3D_SIMULATE_NO_REBUILD=1
// as well the rebuild is refused, which tests the software fallback (docs/ANDROID.md).
#ifdef __ANDROID__
#include <EGL/egl.h>
#include <dlfcn.h>

#include <cstdlib>
#include <string>
#include <vector>

#include "core/base/log.h"
#include "gfx3d/gl/gl_renderer3d.h"
#include "gfx3d/soft3d.h"
#include "platform/platform3d.h"
#include "platform/sdl3/sdl_gl3d.h"

#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT 0x00000040
#endif

namespace ote {
namespace {

struct Egl {
  EGLDisplay display = EGL_NO_DISPLAY;
  EGLConfig config = nullptr;
  EGLSurface surface = EGL_NO_SURFACE;
  EGLContext context = EGL_NO_CONTEXT;
  bool lost = false;     // a make-current failed: the context must be remade
  void* gles = nullptr;  // dlopen handle of libGLESv3, for functions eglGetProcAddress does not return
};
Egl g;

const char* eglErrorName(EGLint e) {
  switch (e) {
    case EGL_SUCCESS: return "EGL_SUCCESS";
    case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
    case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
    case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
    case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
    case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
    case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
    case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
    case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
    case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
    case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
    case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
    case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
    case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
    case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
    default: return "EGL error";
  }
}

void destroyEgl() {
  if (g.display != EGL_NO_DISPLAY) {
    if (g.context != EGL_NO_CONTEXT) eglDestroyContext(g.display, g.context);
    if (g.surface != EGL_NO_SURFACE) eglDestroySurface(g.display, g.surface);
  }
  g.context = EGL_NO_CONTEXT;
  g.surface = EGL_NO_SURFACE;
}

class EglHost : public GlHost {
 public:
  bool enter() override {
    if (g.context == EGL_NO_CONTEXT) return false;
    if (depth_++ > 0) return true;  // nested: already ours
    prevDisplay_ = eglGetCurrentDisplay();
    prevDraw_ = eglGetCurrentSurface(EGL_DRAW);
    prevRead_ = eglGetCurrentSurface(EGL_READ);
    prevContext_ = eglGetCurrentContext();
    if (prevContext_ == g.context) return true;
    if (eglMakeCurrent(g.display, g.surface, g.surface, g.context)) return true;
    // CONTEXT_LOST, or the context/surface/display no longer valid: all mean "make a new one".
    EGLint err = eglGetError();
    if (!g.lost) OTE_WARN("3D (EGL): cannot make the 3D context current: %s (0x%x)", eglErrorName(err), (unsigned)err);
    g.lost = true;
    depth_ = 0;
    return false;
  }
  void leave() override {
    if (depth_ == 0 || --depth_ > 0) return;
    if (prevContext_ == g.context) return;
    // Exactly what was current before (SDL's 2D context and window surface, or nothing).
    EGLBoolean ok;
    if (prevContext_ == EGL_NO_CONTEXT)
      ok = eglMakeCurrent(g.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    else
      ok = eglMakeCurrent(prevDisplay_, prevDraw_, prevRead_, prevContext_);
    if (!ok) OTE_WARN("3D (EGL): cannot put SDL's GL context back: %s", eglErrorName(eglGetError()));
  }
  void* proc(const char* name) override {
    void* p = (void*)eglGetProcAddress(name);
    if (!p && g.gles) p = dlsym(g.gles, name);
    return p;
  }
  bool gles() const override { return true; }

 private:
  int depth_ = 0;
  EGLDisplay prevDisplay_ = EGL_NO_DISPLAY;
  EGLSurface prevDraw_ = EGL_NO_SURFACE, prevRead_ = EGL_NO_SURFACE;
  EGLContext prevContext_ = EGL_NO_CONTEXT;
};

// A new pbuffer context (not made current). The display is shared with SDL's, which has initialised it already.
bool makeEgl(std::string& why) {
  destroyEgl();
  g.lost = false;
  if (!g.gles) g.gles = dlopen("libGLESv3.so", RTLD_NOW | RTLD_LOCAL);
  if (g.display == EGL_NO_DISPLAY) g.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  if (g.display == EGL_NO_DISPLAY) {
    why = "no EGL display";
    return false;
  }
  EGLint major = 0, minor = 0;
  if (!eglInitialize(g.display, &major, &minor)) {  // harmless when SDL did it already
    why = std::string("eglInitialize failed: ") + eglErrorName(eglGetError());
    return false;
  }
  const EGLint cfgAttribs[] = {EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
                               EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
                               EGL_DEPTH_SIZE, 0, EGL_NONE};
  EGLint n = 0;
  if (!eglChooseConfig(g.display, cfgAttribs, &g.config, 1, &n) || n < 1) {
    why = "no EGL config with OpenGL ES 3 and pbuffer support";
    return false;
  }
  const EGLint pbAttribs[] = {EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE};
  g.surface = eglCreatePbufferSurface(g.display, g.config, pbAttribs);
  if (g.surface == EGL_NO_SURFACE) {
    why = std::string("eglCreatePbufferSurface failed: ") + eglErrorName(eglGetError());
    return false;
  }
  eglBindAPI(EGL_OPENGL_ES_API);
  const EGLint ctxAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
  g.context = eglCreateContext(g.display, g.config, EGL_NO_CONTEXT, ctxAttribs);
  if (g.context == EGL_NO_CONTEXT) {
    why = std::string("eglCreateContext (OpenGL ES 3.0) failed: ") + eglErrorName(eglGetError());
    destroyEgl();
    return false;
  }
  return true;
}

// A GL backend on a fresh context; null with the reason in why.
std::unique_ptr<Renderer3D> makeGlBackend(std::string& why) {
  if (!makeEgl(why)) return nullptr;
  std::unique_ptr<Renderer3D> r = GlRenderer3D::create(std::unique_ptr<GlHost>(new EglHost()), why);
  if (!r) {
    destroyEgl();
    return nullptr;
  }
  // What the context offers, for the log (GL_VERSION and GL_RENDERER are logged by the backend itself).
  EglHost host;
  if (host.enter()) {
    typedef void (*GetIntegervFn)(unsigned, int*);
    GetIntegervFn gi = (GetIntegervFn)host.proc("glGetIntegerv");
    int samples = -1, tex = -1, rb = -1;
    if (gi) {
      gi(0x8D57, &samples);  // GL_MAX_SAMPLES
      gi(0x0D33, &tex);      // GL_MAX_TEXTURE_SIZE
      gi(0x84E8, &rb);       // GL_MAX_RENDERBUFFER_SIZE
    }
    OTE_INFO("3D (EGL): pbuffer ES 3.0 context; max samples %d, max texture %d, max renderbuffer %d", samples, tex, rb);
    host.leave();
  }
  return r;
}

// What the game made, kept on the CPU so a lost context can be rebuilt.
class ResilientRenderer3D : public Renderer3D {
 public:
  explicit ResilientRenderer3D(std::unique_ptr<Renderer3D> inner) : inner_(std::move(inner)) {
    const char* s = std::getenv("OTE_3D_SIMULATE_LOSS");
    simulateAt_ = s ? std::atoi(s) : 0;
  }
  ~ResilientRenderer3D() override {
    inner_.reset();  // before the EGL objects go
    destroyEgl();
  }

  Caps3D caps() const override { return inner_->caps(); }

  MeshId createMesh(const MeshData& data) override {
    MeshId inner = inner_->createMesh(data);
    if (!inner && recoverIfLost()) inner = inner_->createMesh(data);
    if (!inner) return 0;
    size_t slot = 0;
    while (slot < meshes_.size() && meshes_[slot].used) ++slot;
    if (slot == meshes_.size()) meshes_.push_back(Mesh());
    meshes_[slot].used = true;
    meshes_[slot].data = data;
    meshes_[slot].inner = inner;
    recoverIfLost();
    return (MeshId)slot + 1;
  }
  void updateMesh(MeshId mesh, const MeshData& data) override {
    if (!validMesh(mesh)) return;
    Mesh& m = meshes_[(size_t)mesh - 1];
    m.data = data;
    inner_->updateMesh(m.inner, data);
    recoverIfLost();  // replays m.data, which is already the new one
  }
  void destroyMesh(MeshId mesh) override {
    if (!validMesh(mesh)) return;
    Mesh& m = meshes_[(size_t)mesh - 1];
    inner_->destroyMesh(m.inner);
    m = Mesh();
    recoverIfLost();
  }
  Texture3DId createTexture(const uint8_t* rgba, int width, int height, bool repeat, bool mipmaps) override {
    Texture3DId inner = inner_->createTexture(rgba, width, height, repeat, mipmaps);
    if (!inner && recoverIfLost()) inner = inner_->createTexture(rgba, width, height, repeat, mipmaps);
    if (!inner) return 0;
    size_t slot = 0;
    while (slot < textures_.size() && textures_[slot].used) ++slot;
    if (slot == textures_.size()) textures_.push_back(Texture());
    Texture& t = textures_[slot];
    t.used = true;
    t.rgba.assign(rgba, rgba + (size_t)width * (size_t)height * 4);
    t.w = width;
    t.h = height;
    t.repeat = repeat;
    t.mipmaps = mipmaps;
    t.inner = inner;
    recoverIfLost();
    return (Texture3DId)slot + 1;
  }
  void destroyTexture(Texture3DId texture) override {
    if (texture < 1 || (size_t)texture > textures_.size() || !textures_[(size_t)texture - 1].used) return;
    Texture& t = textures_[(size_t)texture - 1];
    inner_->destroyTexture(t.inner);
    t = Texture();
    recoverIfLost();
  }

  void begin(const Frame3D& frame) override {
    recoverIfLost();
    frame_ = frame;
    draws_.clear();
    inFrame_ = true;
    inner_->begin(frame);
  }
  void draw(MeshId mesh, const Mat4& model, const Material3D& material) override {
    if (!inFrame_ || !validMesh(mesh)) return;
    draws_.push_back(Draw{mesh, model, material});
    inner_->draw(meshes_[(size_t)mesh - 1].inner, model, innerMaterial(material));
  }
  void end() override {
    inner_->end();
    inFrame_ = false;
    if (!software_ && simulateAt_ > 0 && ++frames_ == simulateAt_) {
      OTE_WARN("3D (EGL): OTE_3D_SIMULATE_LOSS: destroying the 3D context now");
      eglDestroyContext(g.display, g.context);  // the next make-current fails like a lost context
      inner_->begin(frame_);                    // one more frame, which finds the context gone (as after a real loss)
      inner_->end();
    }
    // The draws of this frame went nowhere if the context was gone: do them again on the rebuilt backend.
    if (recoverIfLost()) {
      inner_->begin(frame_);
      for (const Draw& d : draws_)
        if (validMesh(d.mesh)) inner_->draw(meshes_[(size_t)d.mesh - 1].inner, d.model, innerMaterial(d.material));
      inner_->end();
    }
  }
  const uint8_t* framePixels(int& width, int& height) const override { return inner_->framePixels(width, height); }

 private:
  struct Mesh {
    bool used = false;
    MeshData data;
    MeshId inner = 0;
  };
  struct Texture {
    bool used = false, repeat = false, mipmaps = false;
    std::vector<uint8_t> rgba;
    int w = 0, h = 0;
    Texture3DId inner = 0;
  };
  struct Draw {
    MeshId mesh;
    Mat4 model;
    Material3D material;
  };

  bool validMesh(MeshId m) const { return m >= 1 && (size_t)m <= meshes_.size() && meshes_[(size_t)m - 1].used; }
  Material3D innerMaterial(Material3D m) const {
    if (m.texture >= 1 && (size_t)m.texture <= textures_.size() && textures_[(size_t)m.texture - 1].used)
      m.texture = textures_[(size_t)m.texture - 1].inner;
    else
      m.texture = 0;
    return m;
  }

  // If the GL context was lost (the software backend never is): rebuild. True when something was rebuilt.
  bool recoverIfLost() {
    if (software_ || !g.lost) return false;
    OTE_WARN("3D (EGL): the 3D context was lost (backgrounded, GPU reset or memory pressure); rebuilding it");
    inner_.reset();  // its GL objects went with the context; the backend sees the failed enter() and frees nothing
    std::string why;
    // OTE_3D_SIMULATE_NO_REBUILD=1 (debug): pretend the new context cannot be made, to try the software fallback.
    const bool refuse = std::getenv("OTE_3D_SIMULATE_NO_REBUILD") != nullptr;
    if (refuse) why = "OTE_3D_SIMULATE_NO_REBUILD";
    std::unique_ptr<Renderer3D> gl = refuse ? nullptr : makeGlBackend(why);
    if (gl) {
      inner_ = std::move(gl);
      replay();
      OTE_INFO("3D (EGL): context rebuilt, %zu meshes and %zu textures uploaded again", countMeshes(), countTextures());
      return true;
    }
    OTE_WARN("3D (EGL): cannot rebuild the context (%s); using the software 3D renderer for the rest of the session",
             why.c_str());
    destroyEgl();
    inner_.reset(new SoftRenderer3D());
    software_ = true;
    replay();
    return true;
  }
  void replay() {
    for (Mesh& m : meshes_)
      if (m.used) m.inner = inner_->createMesh(m.data);
    for (Texture& t : textures_)
      if (t.used) t.inner = inner_->createTexture(t.rgba.data(), t.w, t.h, t.repeat, t.mipmaps);
  }
  size_t countMeshes() const {
    size_t n = 0;
    for (const Mesh& m : meshes_) n += m.used;
    return n;
  }
  size_t countTextures() const {
    size_t n = 0;
    for (const Texture& t : textures_) n += t.used;
    return n;
  }

  std::unique_ptr<Renderer3D> inner_;
  std::vector<Mesh> meshes_;
  std::vector<Texture> textures_;
  Frame3D frame_;
  std::vector<Draw> draws_;
  bool inFrame_ = false, software_ = false;
  int simulateAt_ = 0, frames_ = 0;
};

}  // namespace

std::unique_ptr<Renderer3D> createAndroidHardwareRenderer3D(std::string& why) {
  std::unique_ptr<Renderer3D> gl = makeGlBackend(why);
  if (!gl) return nullptr;
  return std::unique_ptr<Renderer3D>(new ResilientRenderer3D(std::move(gl)));
}

void shutdownAndroidHardware3D() { destroyEgl(); }  // before SDL goes away; a backend that lives on draws nothing

}  // namespace ote
#endif  // __ANDROID__
