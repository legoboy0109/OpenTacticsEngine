// SDL2 main loop: window/renderer setup, per-frame step, shutdown.
// Mirrors engine/platform/sdl3/sdl_app.cpp using plain SDL 2.0.x calls.
#include <SDL.h>

#include <memory>
#include <vector>

#include "core/base/log.h"
#include "platform/platform.h"
#include "platform/sdl2/sdl2_gl3d.h"
#include "platform/sdl2/sdl2_input.h"
#include "platform/sdl2/sdl2_renderer.h"

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#endif

namespace ote {
namespace {

// Everything the per-frame function needs. A struct (not locals) so the same
// step can be called from a plain loop or from emscripten_set_main_loop_arg.
struct Loop {
  App* app = nullptr;
  const PlatformConfig* cfg = nullptr;
  SdlRenderer* gfx = nullptr;
  SdlInput input;
  std::vector<InputEvent> events;
  Uint64 lastTicks = 0;
  double tickHz = 1.0;
  long frames = 0;
  bool done = false;
  bool vsync = false;
  // Held while the app handles input, time and drawing; the life-cycle watch takes it too, so a suspend notice that
  // arrives on another thread (Android's UI thread) never runs while a frame is half done. SDL mutexes are recursive:
  // on platforms that deliver the notice from inside SDL_PollEvent on this thread (UWP) it simply re-enters. Null on the web.
  SDL_mutex* lock = nullptr;
};

// The OS may freeze the process right after APP_WILLENTERBACKGROUND / APP_TERMINATING returns, so these are handled
// here, at once, and not from the event queue (docs/PORTING.md "Life cycle").
int SDLCALL lifecycleWatch(void* user, SDL_Event* e) {
  Loop* L = static_cast<Loop*>(user);
  Lifecycle notice;
  switch (e->type) {
    case SDL_APP_WILLENTERBACKGROUND: notice = Lifecycle::WillEnterBackground; break;
    case SDL_APP_DIDENTERFOREGROUND: notice = Lifecycle::DidEnterForeground; break;
    case SDL_APP_LOWMEMORY: notice = Lifecycle::LowMemory; break;
    case SDL_APP_TERMINATING: notice = Lifecycle::Terminating; break;
    default: return 0;
  }
  if (L->lock) SDL_LockMutex(L->lock);
  L->app->onLifecycle(notice);
  if (L->lock) SDL_UnlockMutex(L->lock);
  return 0;
}

void frame(Loop& L) {
  if (L.lock) SDL_LockMutex(L.lock);
  // 1. Input.
  L.events.clear();
  SDL_Event e;
  while (SDL_PollEvent(&e)) L.input.translate(e, L.events);
  for (const InputEvent& ev : L.events) L.app->onInput(ev);

  // 2. Time. Clamped so a stall (window drag, breakpoint) never makes a huge step.
  // SDL_GetPerformanceCounter (not the 32-bit millisecond SDL_GetTicks) for smooth dt.
  Uint64 now = SDL_GetPerformanceCounter();
  double dt = (double)(now - L.lastTicks) / L.tickHz;
  L.lastTicks = now;
  if (dt > 0.1) dt = 0.1;
  if (L.cfg->fixedDt > 0) dt = L.cfg->fixedDt;
  L.app->update(dt);

  // 3. Draw. The final frame is captured before present (the back buffer is
  // undefined after present on some drivers).
  ++L.frames;
  bool last = L.app->wantsQuit() || (L.cfg->maxFrames > 0 && L.frames >= L.cfg->maxFrames);
  L.app->draw(*L.gfx);
  if (last && !L.cfg->screenshotPath.empty()) L.gfx->saveScreenshot(L.cfg->screenshotPath);
  if (L.lock) SDL_UnlockMutex(L.lock);  // present / vsync wait / delay below are the window in which a suspend notice gets in
  SDL_RenderPresent(L.gfx->handle());
  // Without working vsync a plain loop would spin a core at 100%.
  if (!L.cfg->headless && !L.vsync) SDL_Delay(1);
  if (last) L.done = true;
}

#ifdef __EMSCRIPTEN__
void emFrame(void* arg) {
  Loop& L = *static_cast<Loop*>(arg);
  frame(L);
  if (L.done) emscripten_cancel_main_loop();
}
#endif

bool initVideo(bool headless) {
  if (headless) {
    // "offscreen" renders without any display server (SDL 2.0.?+ has it);
    // "dummy" is the fallback. The hint name is the plain string so old
    // headers without SDL_HINT_VIDEODRIVER still compile.
    SDL_SetHint("SDL_VIDEODRIVER", "offscreen");
    if (SDL_Init(SDL_INIT_VIDEO) == 0) return true;
    OTE_WARN("SDL offscreen video driver unavailable (%s); trying dummy", SDL_GetError());
    SDL_SetHint("SDL_VIDEODRIVER", "dummy");
  }
  if (SDL_Init(SDL_INIT_VIDEO) != 0) {
    OTE_ERROR("SDL_Init(video) failed: %s", SDL_GetError());
    return false;
  }
  return true;
}

}  // namespace

int runApp(App& app, const PlatformConfig& config) {
  if (!initVideo(config.headless)) return 2;
  // Gamepads are optional: a failure here must not stop the game.
  if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) OTE_WARN("gamepad support unavailable: %s", SDL_GetError());

  int scale = config.windowScale < 1 ? 1 : config.windowScale;
  // Headless windows are exactly logical size (times headlessScale) so screenshots are 1:1.
  int hs = config.headlessScale < 1 ? 1 : config.headlessScale;
  int winW = config.logicalWidth * (config.headless ? hs : scale);
  int winH = config.logicalHeight * (config.headless ? hs : scale);
  Uint32 flags = config.headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE;
  SDL_Window* window = SDL_CreateWindow(config.title.c_str(), SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                        winW, winH, flags);
  if (!window) {
    OTE_ERROR("SDL_CreateWindow(%dx%d) failed: %s", winW, winH, SDL_GetError());
    SDL_Quit();
    return 2;
  }
  // Nearest filtering for the logical->window scaling (SDL2's default, made explicit).
  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  SDL_Renderer* ren = nullptr;
  if (config.headless) {
    ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
  } else {
    ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
      OTE_WARN("accelerated renderer failed (%s); falling back to software", SDL_GetError());
      ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
  }
  if (!ren) {
    OTE_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 2;
  }
  SDL_RendererInfo info;
  bool vsync = false;
  if (SDL_GetRendererInfo(ren, &info) == 0) {
    vsync = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    OTE_INFO("SDL video driver '%s', renderer '%s'", SDL_GetCurrentVideoDriver(), info.name);
  }

  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  if (SDL_RenderSetLogicalSize(ren, config.logicalWidth, config.logicalHeight) != 0)
    OTE_WARN("logical size failed: %s", SDL_GetError());

  {
    // Scoped: the text atlas textures must be destroyed before the SDL renderer.
    Loop loop;
    loop.app = &app;
    loop.cfg = &config;
    SdlRenderer gfx(window, ren, config.logicalWidth, config.logicalHeight, config.headless, hs);
    loop.gfx = &gfx;
    loop.vsync = !config.headless && vsync;
    loop.tickHz = (double)SDL_GetPerformanceFrequency();
    app.onStart(gfx);  // may load content (slow): take the first time stamp afterwards
    loop.lastTicks = SDL_GetPerformanceCounter();
#ifndef __EMSCRIPTEN__
    loop.lock = SDL_CreateMutex();
    SDL_AddEventWatch(lifecycleWatch, &loop);
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(emFrame, &loop, 0, 1);
#else
    while (!loop.done) frame(loop);
    SDL_DelEventWatch(lifecycleWatch, &loop);
    SDL_DestroyMutex(loop.lock);
#endif
  }

  app.onStop();          // the player lets go of the audio device while SDL is still up
  shutdownHardware3D();  // its GL context must go before SDL does
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

}  // namespace ote
