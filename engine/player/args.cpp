#include "args.h"

#include <cstdlib>
#include <cstring>

namespace ote {

const char* usageText() {
  return "usage: ote-player [--games DIR=games] [--assets DIR=assets] [--content-root DIR] [--extra-games DIR] [--game ID] [--map ID] [--autoplay] [--seed N]\n"
         "                  [--headless] [--frames N] [--fixed-dt SECONDS] [--screenshot FILE.bmp]\n"
         "                  [--input \"tok,tok,...\"] [--input-interval FRAMES=6] [--scale N]\n"
         "                  [--font NAME_OR_FILE] [--font-size N] [--font-smooth 0|1] [--style ID] [--ui-scale PERCENT]\n"
         "                  [--settings FILE=<user data folder>/settings.json | --no-settings]\n"
         "                  [--saves DIR=<user data folder>/saves | --no-saving] [--continue | --load-slot N | --campaign] [--test-3d]\n"
         "                  [--3d auto|software|hardware] [--3d-res auto|display|game] [--3d-scale N]\n"
         "                  [--show-all] [--difficulty ID] [--test-level auto|N] [--chapter ID] [--battle-scene scene|map] [--music-volume 0..10] [--sound-volume 0..10] [--no-audio] [--audio-log FILE]\n"
         "                  [--audio-test | --audio-render FILE.wav [--seconds N=4]] [--audio-file FILE.ogg|.wav]\n"
         "roots: games/ and assets/ are looked for in --content-root DIR (default: the working directory); --games / --assets beat it.\n"
         "       --extra-games DIR is a second folder of games (the user's); a game there replaces a built-in one with the same id.\n"
         "fonts: NAME is pixel or an id from <assets>/fonts/fonts.json (pixel-ttf arcade sans serif mono);\n"
         "       otherwise a .ttf/.otf path. Command line beats the game's own font setting.\n"
         "options (font, size, scale, style) are stored in the settings file; these flags override them for one run.\n"
         "saves: Continue save + 5 slots per game (docs/SAVES.md); headless and --no-settings runs save nothing\n"
         "       unless --saves DIR is given; --continue / --load-slot N resume a save of --game directly.\n"
         "campaign: --campaign starts a New game of --game's campaign; --map ID starts that map as a free battle (also in a\n"
         "       campaign game, whatever its freeBattles says) and never touches campaign saves.\n"
         "--scale N (1..16): the window is N times the game's logical size; default: the largest that fits the screen.\n"
         "--test-3d shows the built-in 3D test scene (docs/RENDER3D.md) instead of the launcher.\n"
         "--3d: what draws 3D. hardware = the graphics chip (OpenGL ES 3 / WebGL 2), software = the processor;\n"
         "      auto = hardware where it works (software with --headless). hardware falls back to software.\n"
         "--3d-res: how big the 3D picture is. display = the display's real pixels, game = the game's logical size,\n"
         "      auto = display with the hardware drawer, game size with the software one. Beats the option.\n"
         "--3d-scale N (1..8, with --headless): there is no display, so the 3D picture is N times the logical size and\n"
         "      the screenshot is written at that size too (check detailed art without a window).\n"
         "--show-all: draw a battle with fog of war as if there were none (the rules still apply); for makers testing a map.\n"
         "--difficulty ID: the difficulty of a new campaign or a free battle (the game's own list, docs/DIFFICULTY.md); no choice screen is shown.\n"
         "--test-level auto|N: a testing aid for makers: every human-side unit below that level is levelled up to it at its average growth (a free battle: when it\n"
         "      is built; a campaign: the whole roster, at each chapter). auto = the rounded average level of the computer units hostile to the player at the battle's start, plus 2.\n"
         "      A unit's weapon ranks rise with the levels it gains (each weapon group it wields gains experience for them, up to the top rank).\n"
         "--chapter ID (with --campaign): a NEW test playthrough that starts at that chapter with an approximated roster (docs/SAVES.md); it saves nothing unless --saves is given.\n"
         "--battle-scene scene|map: show fights as the side-view battle scene or on the map for one run (beats the option and the game).\n"
         "sound: --music-volume / --sound-volume beat the options for one run; --no-audio and --headless use no sound device at all.\n"
         "--audio-log FILE writes one line per sound / music request (docs/SOUND.md, Audio log), so tests can see what would be heard.\n"
         "--audio-test plays a synthesized melody and a chime through the real sound device (or --audio-file's music) and exits.\n"
         "--audio-render FILE.wav writes --seconds of what the mixer would play for that same test to a 16-bit stereo WAV, with no device.\n"
         "input tokens: up down left right confirm cancel menu next prev turnleft turnright tiltup tiltdown zoomin zoomout skip background foreground lowmemory terminate wait wait:N tap:X:Y device:keyboard|controller|touch|mouse\n";
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
  bool secondsGiven = false;
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
    else if (!strcmp(arg, "--show-all")) a.showAll = true;
    else if (!strcmp(arg, "--headless")) a.headless = true;
    else if (!strcmp(arg, "--test-3d")) a.test3d = true;
    else if (!strcmp(arg, "--no-settings")) a.noSettings = true;
    else if (!strcmp(arg, "--no-audio")) a.noAudio = true;
    else if (!strcmp(arg, "--audio-test")) a.audioTest = true;
    else if (!strcmp(arg, "--audio-log")) { if (value(v)) a.audioLog = v; }
    else if (!strcmp(arg, "--audio-render")) { if (value(v)) a.audioRender = v; }
    else if (!strcmp(arg, "--audio-file")) { if (value(v)) a.audioFile = v; }
    else if (!strcmp(arg, "--music-volume")) {
      if (value(v)) { if (toLong(v, n) && n >= 0 && n <= 10) a.musicVolume = (int)n; else a.error = "bad --music-volume (0..10)"; }
    }
    else if (!strcmp(arg, "--sound-volume")) {
      if (value(v)) { if (toLong(v, n) && n >= 0 && n <= 10) a.soundVolume = (int)n; else a.error = "bad --sound-volume (0..10)"; }
    }
    else if (!strcmp(arg, "--seconds")) {
      secondsGiven = true;
      if (value(v)) { if (toDouble(v, d) && d >= 0.1 && d <= 600) a.seconds = d; else a.error = "bad --seconds (0.1..600)"; }
    }
    else if (!strcmp(arg, "--no-saving")) a.noSaving = true;
    else if (!strcmp(arg, "--continue")) a.continueSave = true;
    else if (!strcmp(arg, "--campaign")) a.campaign = true;
    else if (!strcmp(arg, "--difficulty")) { if (value(v)) a.difficulty = v; }
    else if (!strcmp(arg, "--chapter")) { if (value(v)) a.chapter = v; }
    else if (!strcmp(arg, "--test-level")) {
      if (value(v)) { if (!strcmp(v, "auto")) a.testLevel = -1; else if (toLong(v, n) && n >= 1 && n <= 99) a.testLevel = (int)n; else a.error = "bad --test-level (auto or 1..99)"; }
    }
    else if (!strcmp(arg, "--battle-scene")) {
      if (value(v)) { a.battleScene = v; if (a.battleScene != "scene" && a.battleScene != "map") a.error = "bad --battle-scene (scene or map)"; }
    }
    else if (!strcmp(arg, "--saves")) { if (value(v)) a.saves = v; }
    else if (!strcmp(arg, "--games")) { if (value(v)) { a.gamesDir = v; a.gamesGiven = true; } }
    else if (!strcmp(arg, "--assets")) { if (value(v)) { a.assetsDir = v; a.assetsGiven = true; } }
    else if (!strcmp(arg, "--content-root")) { if (value(v)) a.contentRoot = v; }
    else if (!strcmp(arg, "--extra-games")) { if (value(v)) a.extraGames = v; }
    else if (!strcmp(arg, "--font")) { if (value(v)) a.font = v; }
    else if (!strcmp(arg, "--style")) { if (value(v)) a.style = v; }
    else if (!strcmp(arg, "--settings")) { if (value(v)) a.settings = v; }
    else if (!strcmp(arg, "--game")) { if (value(v)) a.game = v; }
    else if (!strcmp(arg, "--map")) { if (value(v)) a.map = v; }
    else if (!strcmp(arg, "--screenshot")) { if (value(v)) a.screenshot = v; }
    else if (!strcmp(arg, "--3d")) {
      if (value(v)) {
        a.draw3d = v;
        if (a.draw3d != "auto" && a.draw3d != "software" && a.draw3d != "hardware") a.error = "bad --3d (auto, software or hardware)";
      }
    }
    else if (!strcmp(arg, "--3d-res")) {
      if (value(v)) {
        a.res3d = v;
        if (a.res3d != "auto" && a.res3d != "display" && a.res3d != "game") a.error = "bad --3d-res (auto, display or game)";
      }
    }
    else if (!strcmp(arg, "--3d-scale")) {
      if (value(v)) { if (toLong(v, n) && n >= 1 && n <= 8) a.scale3d = (int)n; else a.error = "bad --3d-scale (1..8)"; }
    }
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
  if (!a.chapter.empty() && !a.campaign) a.error = "--chapter needs --campaign (and --game)";
  else if (a.campaign && (a.game.empty() || !a.map.empty() || a.continueSave || a.loadSlot > 0))
    a.error = "--campaign needs --game and cannot be combined with --map, --continue or --load-slot";
  else if (a.continueSave && a.loadSlot > 0) a.error = "use either --continue or --load-slot, not both";
  else if ((a.continueSave || a.loadSlot > 0) && a.game.empty()) a.error = "--continue and --load-slot need --game";
  if (a.audioTest && !a.audioRender.empty()) a.error = "use either --audio-test or --audio-render, not both";
  else if (secondsGiven && a.audioRender.empty()) a.error = "--seconds only works with --audio-render";
  else if (!a.audioFile.empty() && !a.audioTest && a.audioRender.empty()) a.error = "--audio-file needs --audio-test or --audio-render";
  if (a.scale3d > 1 && !a.headless) a.error = "--3d-scale only works with --headless (a real window has its own size)";
  // Headless runs are tests: deterministic time unless told otherwise.
  if (a.headless && a.fixedDt <= 0) a.fixedDt = 1.0 / 60.0;
  return a;
}

}  // namespace ote
