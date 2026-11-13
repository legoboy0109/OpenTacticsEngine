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
