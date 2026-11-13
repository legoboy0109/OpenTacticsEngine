#include "ui/text.h"

#include <cstdio>

namespace ote {

std::vector<std::string> wrapText(const std::string& text, int maxChars) {
  std::vector<std::string> lines;
  if (maxChars < 1) maxChars = 1;
  std::string line, word;
  auto flushWord = [&]() {
    while ((int)word.size() > maxChars) {  // split words longer than a line
      if (!line.empty()) { lines.push_back(line); line.clear(); }
      lines.push_back(word.substr(0, maxChars));
      word.erase(0, maxChars);
    }
    if (word.empty()) return;
    if (!line.empty() && (int)(line.size() + 1 + word.size()) > maxChars) {
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

std::string fitText(const std::string& s, int maxChars) {
  if ((int)s.size() <= maxChars) return s;
  if (maxChars <= 2) return s.substr(0, maxChars < 0 ? 0 : maxChars);
  return s.substr(0, maxChars - 2) + "..";
}

std::string intStr(int v) {
  char buf[16];
  snprintf(buf, sizeof buf, "%d", v);
  return buf;
}

void drawTextShadow(Renderer& r, int x, int y, const std::string& s, Color c, int scale) {
  r.drawText(x + scale, y + scale, s.c_str(), Color(0, 0, 0, (uint8_t)(c.a * 180 / 255)), scale);
  r.drawText(x, y, s.c_str(), c, scale);
}

void drawTextCentered(Renderer& r, int cx, int y, const std::string& s, Color c, int scale) {
  r.drawText(cx - textWidth(s, scale) / 2, y, s.c_str(), c, scale);
}

int drawWrapped(Renderer& r, int x, int y, int widthPx, const std::string& s, Color c, int lineH) {
  std::vector<std::string> lines = wrapText(s, widthPx / kGlyph);
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
