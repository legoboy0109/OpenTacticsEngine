// Gives the hardware 3D backend (engine/gfx3d/gl) a GL context of its own.
//
// Desktop: a hidden 16 x 16 window with an OpenGL ES 3.0 context, or desktop
// OpenGL 3.3 core where the driver has no ES (macOS). The backend draws into
// its own framebuffer, never into that window.
// Web: a WebGL 2 context on a canvas that is never shown.
// Android: an EGL pbuffer context made directly (sdl_gl3d_android.cpp), no window at all.
//
// The 2D renderer may be using GL itself (SDL's own), so whatever context
// was current is put back after every use.
#include "platform/sdl3/sdl_gl3d.h"

#include <SDL3/SDL.h>

#include "core/base/log.h"
#include "gfx3d/gl/gl_renderer3d.h"
#include "platform/platform3d.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>
#endif

namespace ote {
namespace {

#ifdef __EMSCRIPTEN__

struct Shared {
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE ctx = 0;
  int users = 0;
};
Shared g;

class WebGlHost : public GlHost {
 public:
  bool enter() override {
    if (!g.ctx) return false;
    prev_ = emscripten_webgl_get_current_context();
    return emscripten_webgl_make_context_current(g.ctx) == EMSCRIPTEN_RESULT_SUCCESS;
  }
  void leave() override {
    if (prev_ && prev_ != g.ctx) emscripten_webgl_make_context_current(prev_);
  }
  void* proc(const char* name) override { return emscripten_webgl_get_proc_address(name); }
  bool gles() const override { return true; }

 private:
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE prev_ = 0;
};

bool makeContext(std::string& why) {
  if (g.ctx) return true;
  // A canvas of its own, never shown: the page's canvas belongs to the 2D renderer.
  EM_ASM({
    if (!document.getElementById('ote-gl3d')) {
      var c = document.createElement('canvas');
      c.id = 'ote-gl3d';
      c.width = 16;
      c.height = 16;
      c.style.display = 'none';
      document.body.appendChild(c);
    }
  });
  EmscriptenWebGLContextAttributes a;
  emscripten_webgl_init_context_attributes(&a);
  a.majorVersion = 2;
  a.minorVersion = 0;
  a.alpha = false;
  a.depth = false;
  a.stencil = false;
  a.antialias = false;
  a.enableExtensionsByDefault = false;
  EMSCRIPTEN_WEBGL_CONTEXT_HANDLE c = emscripten_webgl_create_context("#ote-gl3d", &a);
  if (c <= 0) {
    why = "this browser gave no WebGL 2 context";
    return false;
  }
  g.ctx = c;
  return true;
}

void freeContext() {
  if (g.ctx) emscripten_webgl_destroy_context(g.ctx);
  g.ctx = 0;
}

std::unique_ptr<GlHost> makeHost() { return std::unique_ptr<GlHost>(new WebGlHost()); }

#elif !defined(__ANDROID__)  // desktop

struct Shared {
  SDL_Window* window = nullptr;
  SDL_GLContext ctx = nullptr;
  bool es = true;
};
Shared g;

class SdlGlHost : public GlHost {
 public:
  bool enter() override {
    if (!g.ctx) return false;
    prevWindow_ = SDL_GL_GetCurrentWindow();
    prevCtx_ = SDL_GL_GetCurrentContext();
    if (prevCtx_ == g.ctx) return true;
    return SDL_GL_MakeCurrent(g.window, g.ctx);
  }
  void leave() override {
    // SDL keeps track of what is current, so its own GL renderer finds its context again.
    if (g.ctx && prevCtx_ && prevCtx_ != g.ctx && prevWindow_) SDL_GL_MakeCurrent(prevWindow_, prevCtx_);
  }
  void* proc(const char* name) override { return (void*)SDL_GL_GetProcAddress(name); }
  bool gles() const override { return g.es; }

 private:
  SDL_Window* prevWindow_ = nullptr;
  SDL_GLContext prevCtx_ = nullptr;
};

bool tryContext(bool es) {
  SDL_GL_ResetAttributes();
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
  SDL_Window* w = SDL_CreateWindow("OpenTacticsEngine 3D", 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
  if (!w) return false;
  SDL_GLContext c = SDL_GL_CreateContext(w);
  if (!c) {
    SDL_DestroyWindow(w);
    return false;
  }
  g.window = w;
  g.ctx = c;
  g.es = es;
  return true;
}

bool makeContext(std::string& why) {
  if (g.ctx) return true;
  // Creating a context makes it current: remember and put back the 2D renderer's.
  SDL_Window* prevWindow = SDL_GL_GetCurrentWindow();
  SDL_GLContext prevCtx = SDL_GL_GetCurrentContext();
  // OTE_3D_GL=core skips OpenGL ES, to try the desktop OpenGL path on a machine that has both.
  const char* force = SDL_getenv("OTE_3D_GL");
  bool ok = !(force && !SDL_strcmp(force, "core")) && tryContext(true);
  std::string esError = ok ? "" : SDL_GetError();
  if (!ok) ok = tryContext(false);
  if (!ok) why = std::string("no OpenGL ES 3.0 (") + esError + ") and no OpenGL 3.3 (" + SDL_GetError() + ")";
  SDL_GL_ResetAttributes();
  if (prevCtx && prevWindow) SDL_GL_MakeCurrent(prevWindow, prevCtx);
  return ok;
}

void freeContext() {
  if (g.ctx) {
    if (SDL_GL_GetCurrentContext() == g.ctx) SDL_GL_MakeCurrent(nullptr, nullptr);
    SDL_GL_DestroyContext(g.ctx);
  }
  if (g.window) SDL_DestroyWindow(g.window);
  g.ctx = nullptr;
  g.window = nullptr;
}

std::unique_ptr<GlHost> makeHost() { return std::unique_ptr<GlHost>(new SdlGlHost()); }

#endif

}  // namespace

#ifdef __ANDROID__
// Android has no second window: an EGL pbuffer context of its own (sdl_gl3d_android.cpp).
std::unique_ptr<Renderer3D> createHardwareRenderer3D(std::string& why) { return createAndroidHardwareRenderer3D(why); }

void shutdownHardware3D() { shutdownAndroidHardware3D(); }

#else

std::unique_ptr<Renderer3D> createHardwareRenderer3D(std::string& why) {
  if (!makeContext(why)) return nullptr;
  std::unique_ptr<Renderer3D> r = GlRenderer3D::create(makeHost(), why);
  // A context that cannot do the job is of no use to a second try either.
  if (!r) freeContext();
  return r;
}

void shutdownHardware3D() { freeContext(); }

#endif

}  // namespace ote
