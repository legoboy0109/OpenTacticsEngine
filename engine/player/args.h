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
  std::string saves;      // --saves DIR: where save files live (default: <user data folder>/saves; none
                          // at all when --headless or --no-settings unless this is given)
  bool noSaving = false;  // --no-saving: never save or offer saves (the editor's embedded player)
  bool continueSave = false;  // --continue: resume the game's Continue save (needs --game)
  bool campaign = false;  // --campaign: start a New game of the campaign of --game directly (not with --map)
  int loadSlot = 0;       // --load-slot N: resume manual slot N (1..5; needs --game); 0 = no
  std::string game, map;  // empty = ask
  bool test3d = false;    // --test-3d: show the built-in 3D test scene instead of the launcher
  std::string draw3d = "auto";  // --3d auto|software|hardware: which 3D backend (docs/RENDER3D.md). auto =
                                // hardware where it works, but software when --headless (repeatable pictures)
  std::string res3d = "auto";   // --3d-res auto|display|game: how big the 3D picture is (docs/RENDER3D.md "Resolution")
  int scale3d = 1;              // --3d-scale N (1..8, needs --headless): the headless "display" is N times the logical size
  int musicVolume = -1;   // --music-volume 0..10: beats the option; -1 = not given
  int soundVolume = -1;   // --sound-volume 0..10
  bool noAudio = false;   // --no-audio: the null device (headless runs always use it)
  std::string audioLog;   // --audio-log FILE: one line per sound / music request (docs/SOUND.md "Audio log")
  bool audioTest = false; // --audio-test: play a synthesized melody and an effect, then exit
  std::string audioRender;  // --audio-render FILE: write what the mixer would play for --seconds to a WAV; no device
  double seconds = 4;     // --seconds N: length of the --audio-render file
  std::string audioFile;  // --audio-file FILE: --audio-test / --audio-render play this .ogg/.wav as the music
  bool showAll = false;   // --show-all: draw a fog-of-war battle without fog (the rules still apply; for the maker's Play panel)
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
