#include "ui/text.h"

#include <cstdio>

namespace ote {
namespace {

// Byte offsets of every UTF-8 character start (plus the end), so cutting a
// string never splits a multi-byte character.
std::vector<size_t> charStarts(const std::string& s) {
  std::vector<size_t> v;
  for (size_t i = 0; i < s.size(); ++i)
    if (((unsigned char)s[i] & 0xC0) != 0x80) v.push_back(i);
  v.push_back(s.size());
  return v;
}

}  // namespace

std::vector<std::string> wrapText(const Renderer& r, const std::string& text, int maxWidth, int scale) {
  std::vector<std::string> lines;
  if (maxWidth < 1) maxWidth = 1;
  std::string line, word;
  auto fits = [&](const std::string& s) { return textW(r, s, scale) <= maxWidth; };
  auto flushWord = [&]() {
    while (!word.empty() && !fits(word)) {  // split words longer than a line
      if (!line.empty()) { lines.push_back(line); line.clear(); }
      std::vector<size_t> cs = charStarts(word);
      size_t k = 1;  // at least one character per line, so this always ends
      while (k + 1 < cs.size() && fits(word.substr(0, cs[k + 1]))) ++k;
      lines.push_back(word.substr(0, cs[k]));
      word.erase(0, cs[k]);
    }
    if (word.empty()) return;
    if (!line.empty() && !fits(line + " " + word)) {
      lines.push_back(line);
      line.clear();
    }
    if (!line.empty()) line += ' ';
    line += word;
    word.clear();
  };
  for (char ch : text) {
    if (ch == '\n') {
      flushWord();
      lines.push_back(line);
      line.clear();
    } else if (ch == ' ' || ch == '\t' || ch == '\r') {
      flushWord();
    } else {
      word += ch;
    }
  }
  flushWord();
  if (!line.empty()) lines.push_back(line);
  return lines;
}

std::string fitText(const Renderer& r, const std::string& s, int maxWidth, int scale) {
  if (textW(r, s, scale) <= maxWidth) return s;
  std::vector<size_t> cs = charStarts(s);
  for (size_t k = cs.size() - 1; k-- > 0;) {  // longest prefix that leaves room for ".."
    std::string t = s.substr(0, cs[k]) + "..";
    if (textW(r, t, scale) <= maxWidth) return t;
  }
  return textW(r, "..", scale) <= maxWidth ? ".." : "";
}

int fitScale(const Renderer& r, const std::string& s, int maxWidth, int maxScale) {
  int sc = maxScale < 1 ? 1 : maxScale;
  while (sc > 1 && textW(r, s, sc) > maxWidth) --sc;
  return sc;
}

std::string intStr(int v) {
  char buf[16];
  snprintf(buf, sizeof buf, "%d", v);
  return buf;
}

int menuRoomAfter(const Renderer& r, int screenW, const std::string& label) {
  return screenW - 4 - (2 * 4 + textW(r, ">") + 4 + 2) - textW(r, "  ") - textW(r, label);
}

void drawTextShadow(Renderer& r, int x, int y, const std::string& s, Color c, int scale) {
  r.drawText(x + scale, y + scale, s.c_str(), Color(0, 0, 0, (uint8_t)(c.a * 180 / 255)), scale);
  r.drawText(x, y, s.c_str(), c, scale);
}

void drawTextCentered(Renderer& r, int cx, int y, const std::string& s, Color c, int scale) {
  r.drawText(cx - textW(r, s, scale) / 2, y, s.c_str(), c, scale);
}

void drawTextRight(Renderer& r, int x, int y, const std::string& s, Color c, int scale) {
  r.drawText(x - textW(r, s, scale), y, s.c_str(), c, scale);
}

void drawGlyphCentered(Renderer& r, int cx, int cy, char ch, Color c, int maxPx) {
  int sc = maxPx / r.lineHeight();
  if (sc < 1) sc = 1;
  char g[2] = {ch, 0};
  // Letters are mostly capitals: the line box also holds descender space, so
  // nudge down a little to centre the visible ink.
  int lh = r.lineHeight(sc);
  r.drawText(cx - r.textWidth(g, sc) / 2, cy - lh / 2 + lh / 16, g, c, sc);
}

int drawWrapped(Renderer& r, int x, int y, int widthPx, const std::string& s, Color c, int lineH) {
  std::vector<std::string> lines = wrapText(r, s, widthPx);
  for (size_t i = 0; i < lines.size(); ++i) r.drawText(x, y + (int)i * lineH, lines[i].c_str(), c);
  return (int)lines.size();
}

Color mix(Color a, Color b, float t) {
  auto ch = [&](uint8_t x, uint8_t y) { return (uint8_t)(x + (y - x) * t + 0.5f); };
  return Color(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b), ch(a.a, b.a));
}

Color withAlpha(Color c, int alpha) {
  c.a = (uint8_t)(alpha < 0 ? 0 : alpha > 255 ? 255 : alpha);
  return c;
}

float luminance(Color c) { return (0.299f * c.r + 0.587f * c.g + 0.114f * c.b) / 255.0f; }

Color readableOn(Color bg) { return luminance(bg) > 0.55f ? Color(20, 20, 30) : Color(245, 245, 245); }

Color hpColor(int hp, int maxHp) {
  float f = maxHp > 0 ? (float)hp / (float)maxHp : 0;
  if (f < 0) f = 0;
  if (f > 1) f = 1;
  Color red(220, 50, 40), yellow(230, 200, 40), green(70, 200, 70);
  return f < 0.5f ? mix(red, yellow, f * 2) : mix(yellow, green, (f - 0.5f) * 2);
}

}  // namespace ote
