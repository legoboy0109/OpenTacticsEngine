// SDL3 main loop: window/renderer setup, per-frame step, shutdown.
#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "core/base/log.h"
#include "platform/platform.h"
#include "platform/sdl3/sdl_gl3d.h"
#include "platform/sdl3/sdl_input.h"
#include "platform/sdl3/sdl_renderer.h"

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
  Uint64 lastNs = 0;
  long frames = 0;
  // Debug-level frame timing: the average of "work" (input, update, drawing; before present and the vsync wait) and of the
  // whole frame interval, logged every kTimedFrames frames.
  Uint64 workNs = 0, intervalStartNs = 0;
  int timedFrames = 0;
  bool done = false;
  bool vsync = false;
  // Held while the app handles input, time and drawing; the life-cycle watch takes it too, so a suspend notice that
  // arrives on another thread (Android's UI thread) never runs while a frame is half done. SDL mutexes are recursive:
  // on platforms that deliver the notice from inside SDL_PollEvent on this thread it simply re-enters. Null on the web.
  SDL_Mutex* lock = nullptr;
};

// The OS may freeze the process right after WILL_ENTER_BACKGROUND / TERMINATING returns, so these are handled here, at
// once, and not from the event queue (docs/PORTING.md "Life cycle").
bool SDLCALL lifecycleWatch(void* user, SDL_Event* e) {
  Loop* L = static_cast<Loop*>(user);
  Lifecycle notice;
  switch (e->type) {
    case SDL_EVENT_WILL_ENTER_BACKGROUND: notice = Lifecycle::WillEnterBackground; break;
    case SDL_EVENT_DID_ENTER_FOREGROUND: notice = Lifecycle::DidEnterForeground; break;
    case SDL_EVENT_LOW_MEMORY: notice = Lifecycle::LowMemory; break;
    case SDL_EVENT_TERMINATING: notice = Lifecycle::Terminating; break;
    default: return true;
  }
  if (L->lock) SDL_LockMutex(L->lock);
  L->app->onLifecycle(notice);
  if (L->lock) SDL_UnlockMutex(L->lock);
  return true;
}

constexpr int kTimedFrames = 120;

void frame(Loop& L) {
  const Uint64 workStart = SDL_GetTicksNS();
  if (L.lock) SDL_LockMutex(L.lock);
  // 1. Input.
  L.events.clear();
  SDL_Event e;
  while (SDL_PollEvent(&e)) L.input.translate(e, L.gfx->handle(), L.events);
  for (const InputEvent& ev : L.events) L.app->onInput(ev);

  // 2. Time. Clamped so a stall (window drag, breakpoint) never makes a huge step.
  Uint64 now = SDL_GetTicksNS();
  double dt = (double)(now - L.lastNs) / 1e9;
  L.lastNs = now;
  if (dt > 0.1) dt = 0.1;
  if (L.cfg->fixedDt > 0) dt = L.cfg->fixedDt;
  L.app->update(dt);

  // 3. Draw. The final frame is captured before present (the back buffer is
  // undefined after present on some drivers).
  ++L.frames;
  bool last = L.app->wantsQuit() || (L.cfg->maxFrames > 0 && L.frames >= L.cfg->maxFrames);
  L.app->draw(*L.gfx);
  if (last && !L.cfg->screenshotPath.empty()) L.gfx->saveScreenshot(L.cfg->screenshotPath);
  L.workNs += SDL_GetTicksNS() - workStart;
  if (++L.timedFrames == kTimedFrames) {
    const Uint64 t = SDL_GetTicksNS();
    if (L.intervalStartNs)
      OTE_DEBUG("frame time: work %.1f ms, interval %.1f ms (average of %d frames)", L.workNs / 1e6 / kTimedFrames,
                (t - L.intervalStartNs) / 1e6 / kTimedFrames, kTimedFrames);
    L.intervalStartNs = t;
    L.workNs = 0;
    L.timedFrames = 0;
  }
  if (L.lock) SDL_UnlockMutex(L.lock);  // present / vsync wait / delay below are the window in which a suspend notice gets in
  SDL_RenderPresent(L.gfx->handle());
  // Without working vsync a plain loop would spin a core at 100%.
  // (The browser paces the web loop itself.)
#ifndef __EMSCRIPTEN__
  if (!L.cfg->headless && !L.vsync) SDL_Delay(1);
#endif
  if (last) L.done = true;
}

#ifdef __EMSCRIPTEN__
void emFrame(void* arg) {
  Loop& L = *static_cast<Loop*>(arg);
  frame(L);
  if (L.done) {
    // Reached --frames, the end of an --input script, or a quit request. The page stays up.
    OTE_INFO("main loop ended after %ld frames", L.frames);
    emscripten_cancel_main_loop();
  }
}
#endif

bool initVideo(bool headless) {
#ifdef __ANDROID__
  // The back button arrives as SDL_SCANCODE_AC_BACK (= Cancel) instead of closing the activity; the game is landscape;
  // touch is synthesized into left-button mouse events (the default, set so a later SDL default change cannot break it).
  SDL_SetHint(SDL_HINT_ANDROID_TRAP_BACK_BUTTON, "1");
  SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
#endif
  if (headless) {
    // "offscreen" renders without any display server; "dummy" is the fallback.
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "offscreen");
    if (SDL_Init(SDL_INIT_VIDEO)) return true;
    OTE_WARN("SDL offscreen video driver unavailable (%s); trying dummy", SDL_GetError());
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "dummy");
  }
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    OTE_ERROR("SDL_Init(video) failed: %s", SDL_GetError());
    return false;
  }
  return true;
}

}  // namespace

int runApp(App& app, const PlatformConfig& config) {
  if (!initVideo(config.headless)) return 2;
  // Gamepads are optional: a failure here must not stop the game.
  if (!SDL_InitSubSystem(SDL_INIT_GAMEPAD)) OTE_WARN("gamepad support unavailable: %s", SDL_GetError());

  int scale = config.windowScale < 1 ? 1 : config.windowScale;
  // Headless windows are exactly logical size (times headlessScale) so screenshots are 1:1.
  int hs = config.headlessScale < 1 ? 1 : config.headlessScale;
  int winW = config.logicalWidth * (config.headless ? hs : scale);
  int winH = config.logicalHeight * (config.headless ? hs : scale);
  SDL_WindowFlags flags = config.headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE;
  // Tests of the real window path (its own 2D renderer, the hardware 3D backend drawing into it) without showing
  // anything: OTE_WINDOW_HIDDEN=1 makes the window but never shows it (tests/render3d_window_test.py).
  const bool hiddenWindow = !config.headless && SDL_getenv("OTE_WINDOW_HIDDEN") != nullptr;
  if (hiddenWindow) flags |= SDL_WINDOW_HIDDEN;
#ifdef __ANDROID__
  // Immersive: without it the window sits under the status bar and the top of every screen is cut off.
  flags |= SDL_WINDOW_FULLSCREEN;
#endif
#ifdef __EMSCRIPTEN__
  // The canvas backing store gets devicePixelRatio times its CSS size, so smooth
  // text is rasterized at real screen resolution (the page sizes the canvas via CSS).
  flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
#endif
  SDL_Window* window = SDL_CreateWindow(config.title.c_str(), winW, winH, flags);
  if (!window) {
    OTE_ERROR("SDL_CreateWindow(%dx%d) failed: %s", winW, winH, SDL_GetError());
    SDL_Quit();
    return 2;
  }
  SDL_Renderer* ren = nullptr;
#ifdef _WIN32
  // Direct3D 11 for the 2D layer, so the Direct3D 11 3D backend can share its device and draw straight onto its back
  // buffer (sdl_d3d11_3d.cpp). SDL's own default order starts with it too; asked by name so that stays true. The
  // SDL_RENDER_DRIVER hint (or environment variable) still wins.
  if (!config.headless && !SDL_GetHint(SDL_HINT_RENDER_DRIVER)) {
    ren = SDL_CreateRenderer(window, "direct3d11");
    if (!ren) OTE_INFO("SDL's Direct3D 11 renderer is not available (%s); taking SDL's default", SDL_GetError());
  }
#endif
  if (!ren) ren = SDL_CreateRenderer(window, config.headless ? SDL_SOFTWARE_RENDERER : nullptr);
  if (!ren && !config.headless) {
    OTE_WARN("default renderer failed (%s); falling back to software", SDL_GetError());
    ren = SDL_CreateRenderer(window, SDL_SOFTWARE_RENDERER);
  }
  if (!ren) {
    OTE_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 2;
  }
  OTE_INFO("SDL video driver '%s', renderer '%s'", SDL_GetCurrentVideoDriver(), SDL_GetRendererName(ren));

  attachHardware3D(ren);
  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  if (!SDL_SetRenderLogicalPresentation(ren, config.logicalWidth, config.logicalHeight,
                                        SDL_LOGICAL_PRESENTATION_LETTERBOX))
    OTE_WARN("logical presentation failed: %s", SDL_GetError());

  {
    // Scoped: the text atlas textures must be destroyed before the SDL renderer.
    Loop loop;
    loop.app = &app;
    loop.cfg = &config;
    SdlRenderer gfx(window, ren, config.logicalWidth, config.logicalHeight, config.headless, hs);
    loop.gfx = &gfx;
    loop.vsync = !config.headless && !hiddenWindow && SDL_SetRenderVSync(ren, 1);
    app.onStart(gfx);  // may load content (slow): take the first time stamp afterwards
    loop.lastNs = SDL_GetTicksNS();
#ifndef __EMSCRIPTEN__
    loop.lock = SDL_CreateMutex();
    SDL_AddEventWatch(lifecycleWatch, &loop);
#endif

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop_arg(emFrame, &loop, 0, 1);
#else
    while (!loop.done) frame(loop);
    SDL_RemoveEventWatch(lifecycleWatch, &loop);
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
