// Text and colour helpers on top of the 8x8 platform font.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"

namespace ote {

constexpr int kGlyph = Renderer::kGlyphSize;

inline int textWidth(const std::string& s, int scale = 1) { return (int)s.size() * kGlyph * scale; }

// Word-wraps to at most maxChars per line; over-long words are split.
std::vector<std::string> wrapText(const std::string& text, int maxChars);
// Truncates with ".." so the result is at most maxChars long.
std::string fitText(const std::string& s, int maxChars);
std::string intStr(int v);

// Drawing. "Shadow" variants add a 1px dark outline-ish shadow for text over busy maps.
void drawTextShadow(Renderer& r, int x, int y, const std::string& s, Color c, int scale = 1);
void drawTextCentered(Renderer& r, int cx, int y, const std::string& s, Color c, int scale = 1);
// Draws wrapped text; returns the number of lines used.
int drawWrapped(Renderer& r, int x, int y, int widthPx, const std::string& s, Color c, int lineH);

// Colours.
Color mix(Color a, Color b, float t);           // t=0 -> a, t=1 -> b
Color withAlpha(Color c, int alpha);
float luminance(Color c);                       // 0..1
Color readableOn(Color bg);                     // dark or white text for a background
Color hpColor(int hp, int maxHp);               // green -> yellow -> red

}  // namespace ote
