// ote-player: runs a game folder (see docs/GAME_FORMAT.md) with the platform backend chosen at build time (SDL3 or SDL2).
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "app.h"
#include "args.h"
#include "core/base/log.h"
#include "sound/audio_render.h"
#include "platform/platform.h"
#include "platform/platform_main.h"  // SDL_main on the platforms that need it; must stay in this file

using namespace ote;

int main(int argc, char** argv) {
  // OTE_LOG_LEVEL=debug shows the debug lines too (the walk directions; tests read them). Normal runs log info and above only.
  if (const char* lv = std::getenv("OTE_LOG_LEVEL"))
    if (!std::strcmp(lv, "debug")) setLogLevel(LogLevel::Debug);
  Args args = parseArgs(argc, argv);
  if (args.help || !args.error.empty()) {
    if (!args.error.empty()) fprintf(stderr, "error: %s\n", args.error.c_str());
    fputs(usageText(), args.help ? stdout : stderr);
    return args.help ? 0 : 1;
  }

  applyContentRoots(args);

  // Offline mixing needs neither a window nor a sound device.
  if (!args.audioRender.empty()) return runAudioRender(args);

  PlatformConfig cfg;
  cfg.title = "OpenTacticsEngine";
  cfg.windowScale = args.scale;
  cfg.headless = args.headless;
  cfg.headlessScale = args.scale3d;
  cfg.fixedDt = args.fixedDt;
  cfg.maxFrames = args.frames;
  cfg.screenshotPath = args.screenshot;

  PlayerApp app(args);
  std::string err;
  if (!app.start(err)) {
    fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  return runApp(app, cfg);
}
