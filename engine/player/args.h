// Command-line options of ote-player.
#pragma once
#include <string>

namespace ote {

struct Args {
  std::string gamesDir = "games";
  std::string assetsDir = "assets";  // built-in fonts live in <assetsDir>/fonts
  std::string font;       // --font: built-in id, "pixel", or a font file; empty = the game decides
  int fontSize = 0;       // --font-size, logical pixels; 0 = default
  int fontSmooth = -1;    // --font-smooth 0|1; -1 = default
  std::string style;      // --style ID: visual style (a game without it keeps its default)
  int uiScale = 0;        // --ui-scale PERCENT (multiplies the font size); 0 = default
  std::string settings;   // --settings PATH; empty = <user data folder>/settings.json
                          // (none at all when --headless, so test runs never touch real settings)
  bool noSettings = false;  // --no-settings: options live in memory only (nothing read or written);
                            // used by the embedded web player so the editor preview starts from defaults
  std::string game, map;  // empty = ask
  bool autoplay = false;
  unsigned seed = 1;
  bool headless = false;
  int frames = 0;
  double fixedDt = 0;
  std::string screenshot;
  std::string input;      // scripted input, see script_input.h
  int inputInterval = 6;  // frames between script tokens
  int scale = 3;          // window scale
  bool help = false;
  std::string error;      // non-empty = bad command line
};

Args parseArgs(int argc, char** argv);
const char* usageText();

}  // namespace ote
