// Text and colour helpers on top of Renderer's text calls. Fonts vary in size
// and width (proportional TrueType), so ALL layout measures through the
// renderer: never assume a glyph size.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"

namespace ote {

inline int textW(const Renderer& r, const std::string& s, int scale = 1) { return r.textWidth(s.c_str(), scale); }
// Vertical space one row of text needs in lists/panels (line + 2px air).
inline int rowHeight(const Renderer& r) { return r.lineHeight() + 2; }

// Word-wraps to lines at most maxWidth logical pixels wide; over-long words are split.
std::vector<std::string> wrapText(const Renderer& r, const std::string& text, int maxWidth, int scale = 1);
// Truncates (on a character boundary) with ".." so the result is at most maxWidth wide.
std::string fitText(const Renderer& r, const std::string& s, int maxWidth, int scale = 1);
// Largest scale in [1, maxScale] at which `s` fits maxWidth (1 if none does).
int fitScale(const Renderer& r, const std::string& s, int maxWidth, int maxScale);
std::string intStr(int v);
// What a row of a popup Menu has left to the right of `label` on a screen screenW wide: the menu is at most the screen less
// 4 px wide, and each row spends Menu::open's padding (2 * 4), arrow (">" + 4), border (2) and the "  " gap before the
// right-hand text. Used by the action menu's lists (staves, steal) to pick the longest extra text that fits. Can be negative.
int menuRoomAfter(const Renderer& r, int screenW, const std::string& label);

// Drawing. "Shadow" variants add a 1px dark shadow for text over busy maps.
void drawTextShadow(Renderer& r, int x, int y, const std::string& s, Color c, int scale = 1);
void drawTextCentered(Renderer& r, int cx, int y, const std::string& s, Color c, int scale = 1);
// Right-aligns the text to x.
void drawTextRight(Renderer& r, int x, int y, const std::string& s, Color c, int scale = 1);
// A single character centred on (cx, cy), as large as fits `maxPx` (unit/terrain letters).
void drawGlyphCentered(Renderer& r, int cx, int cy, char ch, Color c, int maxPx);
// Draws wrapped text; returns the number of lines used.
int drawWrapped(Renderer& r, int x, int y, int widthPx, const std::string& s, Color c, int lineH);

// Colours.
Color mix(Color a, Color b, float t);           // t=0 -> a, t=1 -> b
Color withAlpha(Color c, int alpha);
float luminance(Color c);                       // 0..1
Color readableOn(Color bg);                     // dark or white text for a background
Color hpColor(int hp, int maxHp);               // green -> yellow -> red

}  // namespace ote
