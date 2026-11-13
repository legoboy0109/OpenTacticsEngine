// SDL3 main loop: window/renderer setup, per-frame step, shutdown.
#include <SDL3/SDL.h>

#include <memory>
#include <vector>

#include "core/base/log.h"
#include "platform/platform.h"
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
  bool done = false;
  bool vsync = false;
};

void frame(Loop& L) {
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
  // Headless windows are exactly logical size so screenshots are 1:1.
  int winW = config.logicalWidth * (config.headless ? 1 : scale);
  int winH = config.logicalHeight * (config.headless ? 1 : scale);
  SDL_WindowFlags flags = config.headless ? SDL_WINDOW_HIDDEN : SDL_WINDOW_RESIZABLE;
  SDL_Window* window = SDL_CreateWindow(config.title.c_str(), winW, winH, flags);
  if (!window) {
    OTE_ERROR("SDL_CreateWindow(%dx%d) failed: %s", winW, winH, SDL_GetError());
    SDL_Quit();
    return 2;
  }
  SDL_Renderer* ren = SDL_CreateRenderer(window, config.headless ? SDL_SOFTWARE_RENDERER : nullptr);
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

  SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
  if (!SDL_SetRenderLogicalPresentation(ren, config.logicalWidth, config.logicalHeight,
                                        SDL_LOGICAL_PRESENTATION_LETTERBOX))
    OTE_WARN("logical presentation failed: %s", SDL_GetError());

  Loop loop;
  loop.app = &app;
  loop.cfg = &config;
  SdlRenderer gfx(window, ren, config.logicalWidth, config.logicalHeight, config.headless);
  loop.gfx = &gfx;
  loop.vsync = !config.headless && SDL_SetRenderVSync(ren, 1);
  app.onStart(gfx);  // may load content (slow): take the first time stamp afterwards
  loop.lastNs = SDL_GetTicksNS();

#ifdef __EMSCRIPTEN__
  emscripten_set_main_loop_arg(emFrame, &loop, 0, 1);
#else
  while (!loop.done) frame(loop);
#endif

  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}

}  // namespace ote
