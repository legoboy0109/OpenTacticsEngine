// Gives the hardware 3D backend (engine/gfx3d/gl) a GL context of its own. (On Windows the Direct3D 11 backend is
// tried first: sdl2_d3d11_3d.cpp.)
//
// A hidden 16 x 16 window with an OpenGL ES 3.0 context, or desktop OpenGL
// 3.3 core where the driver has no ES (macOS). The backend draws into its own
// framebuffer, never into that window. Mirrors engine/platform/sdl3/sdl_gl3d.cpp.
//
// The 2D renderer may be using GL itself (SDL's own), so whatever context
// was current is put back after every use.
#include "platform/sdl2/sdl2_gl3d.h"

#include <SDL.h>

#include "core/base/log.h"
#include "gfx3d/gl/gl_renderer3d.h"
#include "platform/platform3d.h"


namespace ote {
namespace {

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
    return SDL_GL_MakeCurrent(g.window, g.ctx) == 0;
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

#ifndef OTE_UWP  // UWP has no OpenGL: only freeContext() (a no-op there) is used
bool tryContext(bool es) {
  SDL_GL_ResetAttributes();
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, es ? SDL_GL_CONTEXT_PROFILE_ES : SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, es ? 0 : 3);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 0);
  SDL_Window* w = SDL_CreateWindow("OpenTacticsEngine 3D", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 16, 16,
                                   SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
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

#endif

void freeContext() {
  if (g.ctx) {
    if (SDL_GL_GetCurrentContext() == g.ctx) SDL_GL_MakeCurrent(nullptr, nullptr);
    SDL_GL_DeleteContext(g.ctx);
  }
  if (g.window) SDL_DestroyWindow(g.window);
  g.ctx = nullptr;
  g.window = nullptr;
}

#ifndef OTE_UWP
std::unique_ptr<GlHost> makeHost() { return std::unique_ptr<GlHost>(new SdlGlHost()); }
#endif

}  // namespace

#ifdef _WIN32
void attachHardware3D(SDL_Renderer* renderer) { attachD3D11Renderer(renderer); }
#else
void attachHardware3D(SDL_Renderer*) {}
#endif

// Windows: Direct3D 11 first (sdl2_d3d11_3d.cpp), then OpenGL; UWP has no OpenGL, so there it is Direct3D 11 or
// nothing (the player then logs it and draws with the software backend). OTE_3D_API=opengl skips Direct3D 11 on the
// desktop, to try the OpenGL backend there.
#ifdef OTE_UWP
std::unique_ptr<Renderer3D> createHardwareRenderer3D(std::string& why) {
  std::string d3dWhy;
  std::unique_ptr<Renderer3D> d3d = createD3D11HardwareRenderer3D(d3dWhy);
  if (!d3d) why = "Direct3D 11: " + d3dWhy;
  return d3d;
}
#else
std::unique_ptr<Renderer3D> createHardwareRenderer3D(std::string& why) {
#ifdef _WIN32
  std::string d3dWhy;
  const char* api = SDL_getenv("OTE_3D_API");
  if (!(api && !SDL_strcmp(api, "opengl"))) {
    std::unique_ptr<Renderer3D> d3d = createD3D11HardwareRenderer3D(d3dWhy);
    if (d3d) return d3d;
    OTE_INFO("3D (Direct3D 11): not available (%s); trying OpenGL", d3dWhy.c_str());
    d3dWhy = "Direct3D 11: " + d3dWhy + "; OpenGL: ";
  }
#else
  const std::string d3dWhy;
#endif
  if (!makeContext(why)) {
    why = d3dWhy + why;
    return nullptr;
  }
  std::unique_ptr<Renderer3D> r = GlRenderer3D::create(makeHost(), why);
  // A context that cannot do the job is of no use to a second try either.
  if (!r) {
    freeContext();
    why = d3dWhy + why;
  }
  return r;
}
#endif

void shutdownHardware3D() {
#ifdef _WIN32
  shutdownD3D11Hardware3D();
#endif
  freeContext();
}

}  // namespace ote
