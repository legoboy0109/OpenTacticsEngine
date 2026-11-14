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
};

struct EffectiveLook {
  FontRequest font;
  int uiScale = 100;
  int styleIndex = 0;  // index into the game's styles: the chosen one, else the first this player can draw
  std::string draw3d;  // "opengl", "software", or "" = the player decides (docs/RENDER3D.md)
};

// Font size rule: an explicit size setting wins; otherwise the size that
// belongs to the font in use (the game's font.size if the game's own font is
// used, else the font's default); then uiScale multiplies it (rounded).
EffectiveLook resolveLook(const std::string& assetsDir, const LookLayer& args, const LookLayer& perGame,
                          const LookLayer& global, const GameLook& game);

}  // namespace ote
