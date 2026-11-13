// Font choice resolution shared by the player and (later) a launcher or
// options screen: built-in font registry (assets/fonts/fonts.json) and the
// "game font / command-line font -> FontRequest" mapping. No SDL, no
// rendering; the result is handed to Renderer::setFont.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"

namespace ote {

// A font as a game or the user asked for it. Mirrors the "font" key of
// game.json (docs/GAME_FORMAT.md); the player fills it from FontConfig or
// from command-line options.
struct FontChoice {
  std::string name = "pixel";  // built-in id from fonts.json, or "pixel"
  std::string file;            // font file (relative to gameRoot); wins over name
  int size = 0;                // logical pixels; <= 0 = the font's default
  int smooth = -1;             // 1 / 0; -1 = the font's default
};

struct BuiltinFont {
  std::string id;    // what games write in font.name
  std::string name;  // display name for menus
  std::string file;  // path to the font file, "" for "pixel"
  int size = 8;      // default size
  bool smooth = false;
};

// "pixel" first, then every entry of <assetsDir>/fonts/fonts.json whose font
// file exists. A missing/broken registry only logs a warning (pixel remains).
std::vector<BuiltinFont> listBuiltinFonts(const std::string& assetsDir);

// Turns a choice into a request. `gameRoot` is the base for choice.file
// (pass "" if the file path is already usable as is). Unknown id or
// unreadable file: logs a warning naming it and returns the "pixel" request.
FontRequest resolveFont(const std::string& assetsDir, const std::string& gameRoot, const FontChoice& choice);

// The built-in pixel font request (what every failure falls back to).
FontRequest pixelFontRequest();

}  // namespace ote
