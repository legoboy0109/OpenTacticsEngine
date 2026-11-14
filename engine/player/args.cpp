#include "args.h"

#include <cstdlib>
#include <cstring>

namespace ote {

const char* usageText() {
  return "usage: ote-player [--games DIR=games] [--assets DIR=assets] [--game ID] [--map ID] [--autoplay] [--seed N]\n"
         "                  [--headless] [--frames N] [--fixed-dt SECONDS] [--screenshot FILE.bmp]\n"
         "                  [--input \"tok,tok,...\"] [--input-interval FRAMES=6] [--scale N]\n"
         "                  [--font NAME_OR_FILE] [--font-size N] [--font-smooth 0|1] [--style ID] [--ui-scale PERCENT]\n"
         "                  [--settings FILE=<user data folder>/settings.json | --no-settings]\n"
         "                  [--saves DIR=<user data folder>/saves | --no-saving] [--continue | --load-slot N] [--test-3d]\n"
         "fonts: NAME is pixel or an id from <assets>/fonts/fonts.json (pixel-ttf arcade sans serif mono);\n"
         "       otherwise a .ttf/.otf path. Command line beats the game's own font setting.\n"
         "options (font, size, scale, style) are stored in the settings file; these flags override them for one run.\n"
         "saves: Continue save + 5 slots per game (docs/SAVES.md); headless and --no-settings runs save nothing\n"
         "       unless --saves DIR is given; --continue / --load-slot N resume a save of --game directly.\n"
         "--test-3d shows the built-in 3D test scene (docs/RENDER3D.md) instead of the launcher.\n"
         "input tokens: up down left right confirm cancel menu next prev wait wait:N tap:X:Y\n";
}

namespace {

bool toLong(const char* s, long& out) {
  char* end = nullptr;
  out = strtol(s, &end, 10);
  return end != s && *end == 0;
}

bool toDouble(const char* s, double& out) {
  char* end = nullptr;
  out = strtod(s, &end);
  return end != s && *end == 0;
}

}  // namespace

Args parseArgs(int argc, char** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const char* arg = argv[i];
    auto value = [&](const char*& out) {
      if (i + 1 >= argc) {
        a.error = std::string("missing value for ") + arg;
        return false;
      }
      out = argv[++i];
      return true;
    };
    const char* v = nullptr;
    long n = 0;
    double d = 0;
    if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) a.help = true;
    else if (!strcmp(arg, "--autoplay")) a.autoplay = true;
    else if (!strcmp(arg, "--headless")) a.headless = true;
    else if (!strcmp(arg, "--test-3d")) a.test3d = true;
    else if (!strcmp(arg, "--no-settings")) a.noSettings = true;
    else if (!strcmp(arg, "--no-saving")) a.noSaving = true;
    else if (!strcmp(arg, "--continue")) a.continueSave = true;
    else if (!strcmp(arg, "--saves")) { if (value(v)) a.saves = v; }
    else if (!strcmp(arg, "--games")) { if (value(v)) a.gamesDir = v; }
    else if (!strcmp(arg, "--assets")) { if (value(v)) a.assetsDir = v; }
    else if (!strcmp(arg, "--font")) { if (value(v)) a.font = v; }
    else if (!strcmp(arg, "--style")) { if (value(v)) a.style = v; }
    else if (!strcmp(arg, "--settings")) { if (value(v)) a.settings = v; }
    else if (!strcmp(arg, "--game")) { if (value(v)) a.game = v; }
    else if (!strcmp(arg, "--map")) { if (value(v)) a.map = v; }
    else if (!strcmp(arg, "--screenshot")) { if (value(v)) a.screenshot = v; }
    else if (!strcmp(arg, "--input")) { if (value(v)) a.input = v; }
    else if (!strcmp(arg, "--seed")) {
      if (value(v)) { if (toLong(v, n) && n >= 0) a.seed = (unsigned)n; else a.error = "bad --seed"; }
    } else if (!strcmp(arg, "--load-slot")) {
      if (value(v)) { if (toLong(v, n) && n >= 1 && n <= 5) a.loadSlot = (int)n; else a.error = "bad --load-slot (1..5)"; }
    } else if (!strcmp(arg, "--frames")) {
      if (value(v)) { if (toLong(v, n) && n >= 0) a.frames = (int)n; else a.error = "bad --frames"; }
    } else if (!strcmp(arg, "--input-interval")) {
      if (value(v)) { if (toLong(v, n) && n >= 1) a.inputInterval = (int)n; else a.error = "bad --input-interval"; }
    } else if (!strcmp(arg, "--font-size")) {
      if (value(v)) { if (toLong(v, n) && n >= 4 && n <= 128) a.fontSize = (int)n; else a.error = "bad --font-size (4..128)"; }
    } else if (!strcmp(arg, "--ui-scale")) {
      if (value(v)) { if (toLong(v, n) && n >= 25 && n <= 400) a.uiScale = (int)n; else a.error = "bad --ui-scale (25..400)"; }
    } else if (!strcmp(arg, "--font-smooth")) {
      if (value(v)) { if (toLong(v, n) && (n == 0 || n == 1)) a.fontSmooth = (int)n; else a.error = "bad --font-smooth (0 or 1)"; }
    } else if (!strcmp(arg, "--scale")) {
      if (value(v)) { if (toLong(v, n) && n >= 1 && n <= 16) a.scale = (int)n; else a.error = "bad --scale"; }
    } else if (!strcmp(arg, "--fixed-dt")) {
      if (value(v)) { if (toDouble(v, d) && d > 0) a.fixedDt = d; else a.error = "bad --fixed-dt"; }
    } else {
      a.error = std::string("unknown option ") + arg;
    }
    if (!a.error.empty()) break;
  }
  if (!a.error.empty()) return a;
  if (a.continueSave && a.loadSlot > 0) a.error = "use either --continue or --load-slot, not both";
  else if ((a.continueSave || a.loadSlot > 0) && a.game.empty()) a.error = "--continue and --load-slot need --game";
  // Headless runs are tests: deterministic time unless told otherwise.
  if (a.headless && a.fixedDt <= 0) a.fixedDt = 1.0 / 60.0;
  return a;
}

}  // namespace ote
