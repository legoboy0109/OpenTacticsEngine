// Turns the layers of settings into the look the player actually uses.
//
// Priority, highest first:
//   command-line args > per-game override > global option >
//   the game's own default (game.json) > engine default (pixel font, 100%).
// Each layer is a LookLayer; a field left at its "unset" value means "this
// layer has no opinion". Option definitions (option_defs.h) fill the layers
// from the settings file; this file is the one place that combines them.
#pragma once
#include <string>
#include <vector>

#include "platform/common/font_registry.h"

namespace ote {

struct LookLayer {
  std::string font;   // built-in font id; args may also give "pixel" or a font file
  int fontSize = 0;   // logical pixels; 0 = unset
  int smooth = -1;    // 1/0; -1 = unset (command line only)
  int uiScale = 0;    // percent; 0 = unset
  std::string style;  // style id; "" = unset
  std::string draw3d; // what draws 3D: "opengl" or "software"; "" = unset
  std::string res3d;  // how big the 3D picture is: "display" or "game"; "" = unset
  std::string battleScene;  // "scene" or "map"; "" = unset (the game decides)
  std::string menuUnavailable;  // "grey" or "hide"; "" = unset (the game decides)
  std::string enemyRanges;      // "both", "attack" or "off"; "" = unset (movement + attack, unless the game switched the display off)
  int musicVolume = -1;  // 0..10; -1 = unset
  int soundVolume = -1;  // 0..10; -1 = unset
  int integerScale = -1; // 1/0: whole-number scaling on / off (docs/GRAPHICS.md); -1 = unset
  int safeArea = 0;      // percent of a TV screen the picture may use (original Xbox only, docs/XBOX_OG.md); 0 = unset
};

// What a game itself asks for. `present` is false on the launcher screens
// (no game): the "game default" level is then skipped.
struct GameLook {
  bool present = false;
  FontChoice font;                 // from game.json "font"
  std::string root;                // game folder (base of font.file)
  std::vector<std::string> styleIds;  // [0] = the default style
  // Per style: can this player draw it? (empty = all). A style that cannot be
  // drawn is never picked; the default is then the first one that can.
  std::vector<char> styleUsable;
  bool battleScene = false;        // game.json "battleScene": the game wants the side-view battle scene by default
  bool menuHide = false;           // game.json "menuUnavailable": "hide": the game wants actions with nothing to act on left out of the menu
  bool enemyRanges = true;         // game.json rules.display.enemyRanges: false = the game switched the enemy-range display off (the player cannot turn it on)
};

struct EffectiveLook {
  FontRequest font;
  int uiScale = 100;
  int styleIndex = 0;  // index into the game's styles: the chosen one, else the first this player can draw
  std::string draw3d;  // "opengl", "software", or "" = the player decides (docs/RENDER3D.md)
  std::string res3d;   // "display", "game", or "" = automatic (docs/RENDER3D.md "Resolution")
  bool battleScene = false;  // fights are shown as the side-view battle scene (docs/BATTLE_SCENE.md "Settings")
  bool menuHide = false;     // the action menu leaves out entries with nothing to act on (docs/SRPG.md "Menu options")
  int enemyRanges = 1;       // the enemy-range display: 0 off, 1 movement + attack, 2 attack only (docs/SRPG.md "Enemy ranges")
  int musicVolume = 7;  // 0..10 (docs/SOUND.md "Options")
  int soundVolume = 7;
  bool integerScale = true;  // the picture is shown at whole multiples of the game's size (docs/GRAPHICS.md "Whole-number scaling")
  int safeArea = 0;  // 0 = the platform's default for the video mode (only the original Xbox backend uses it)
};

// Font size rule: an explicit size setting wins; otherwise the size that
// belongs to the font in use (the game's font.size if the game's own font is
// used, else the font's default); then uiScale multiplies it (rounded).
EffectiveLook resolveLook(const std::string& assetsDir, const LookLayer& args, const LookLayer& perGame,
                          const LookLayer& global, const GameLook& game);

}  // namespace ote
