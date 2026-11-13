// ote-player: runs a game folder (see docs/GAME_FORMAT.md) with the SDL3 backend.
#include <cstdio>

#include "app.h"
#include "args.h"
#include "core/base/json.h"
#include "core/base/log.h"
#include "platform/platform.h"

using namespace ote;

// The logical screen size is fixed when the window opens, so peek at the
// game that will be shown first (if known) for its `view` size.
static void pickLogicalSize(const Args& a, PlatformConfig& cfg) {
  std::string game = a.game;
  if (game.empty()) {
    Json index;
    std::string err;
    if (loadJsonFile(a.gamesDir + "/index.json", index, err) && index["games"].size() == 1)
      game = index["games"][0].asString();
  }
  if (game.empty()) return;
  Json gj;
  std::string err;
  if (!loadJsonFile(a.gamesDir + "/" + game + "/game.json", gj, err)) return;  // error shown in-app later
  int w = gj["view"]["width"].asInt(cfg.logicalWidth), h = gj["view"]["height"].asInt(cfg.logicalHeight);
  if (w >= 160 && h >= 120 && w <= 1920 && h <= 1080) { cfg.logicalWidth = w; cfg.logicalHeight = h; }
}

int main(int argc, char** argv) {
  Args args = parseArgs(argc, argv);
  if (args.help || !args.error.empty()) {
    if (!args.error.empty()) fprintf(stderr, "error: %s\n", args.error.c_str());
    fputs(usageText(), args.help ? stdout : stderr);
    return args.help ? 0 : 1;
  }

  PlatformConfig cfg;
  cfg.title = "OpenTacticsEngine";
  cfg.windowScale = args.scale;
  cfg.headless = args.headless;
  cfg.fixedDt = args.fixedDt;
  cfg.maxFrames = args.frames;
  cfg.screenshotPath = args.screenshot;
  pickLogicalSize(args, cfg);

  PlayerApp app(args);
  std::string err;
  if (!app.start(err)) {
    fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  return runApp(app, cfg);
}
