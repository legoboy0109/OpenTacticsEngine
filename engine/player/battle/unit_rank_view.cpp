#include "battle/unit_rank_view.h"

#include "battle/battle_view.h"
#include <algorithm>

#include "ui/text.h"

namespace ote {

int shownRank(const DrawCtx& c, const Unit& u) {
  const Battle& b = c.battle;
  if (!b.db->rules.unitRanks.on()) return -1;
  if (u.id >= 0 && (size_t)u.id < c.view.units.size() && c.view.units[(size_t)u.id].rank != kRankOwn) return c.view.units[(size_t)u.id].rank;
  return b.unitRank(u);
}

std::string rankBadgeText(const DrawCtx& c, const Unit& u) {
  const int rank = shownRank(c, u);
  return rank < 0 ? std::string() : c.battle.db->rules.unitRanks.levels[(size_t)rank].badge;
}

namespace {

// A 3x5 pixel font (rows top to bottom, 3 bits each, bit 2 = left) for the badge: the badge must not grow with the UI scale, so it is drawn from rectangles
// sized by the unit's width on screen, not with the text font. Letters (any case), digits and a few marks; anything else is a pip.
struct Glyph { char ch; unsigned short rows; };
constexpr unsigned short R(int a, int b, int c, int d, int e) { return (unsigned short)(a << 12 | b << 9 | c << 6 | d << 3 | e); }
const Glyph kGlyphs[] = {
    {'0', R(7, 5, 5, 5, 7)}, {'1', R(2, 6, 2, 2, 7)}, {'2', R(7, 1, 7, 4, 7)}, {'3', R(7, 1, 7, 1, 7)}, {'4', R(5, 5, 7, 1, 1)},
    {'5', R(7, 4, 7, 1, 7)}, {'6', R(7, 4, 7, 5, 7)}, {'7', R(7, 1, 1, 2, 2)}, {'8', R(7, 5, 7, 5, 7)}, {'9', R(7, 5, 7, 1, 7)},
    {'A', R(2, 5, 7, 5, 5)}, {'B', R(6, 5, 6, 5, 6)}, {'C', R(3, 4, 4, 4, 3)}, {'D', R(6, 5, 5, 5, 6)}, {'E', R(7, 4, 6, 4, 7)},
    {'F', R(7, 4, 6, 4, 4)}, {'G', R(3, 4, 5, 5, 3)}, {'H', R(5, 5, 7, 5, 5)}, {'I', R(7, 2, 2, 2, 7)}, {'J', R(1, 1, 1, 5, 2)},
    {'K', R(5, 5, 6, 5, 5)}, {'L', R(4, 4, 4, 4, 7)}, {'M', R(5, 7, 7, 5, 5)}, {'N', R(6, 5, 5, 5, 5)}, {'O', R(2, 5, 5, 5, 2)},
    {'P', R(6, 5, 6, 4, 4)}, {'Q', R(2, 5, 5, 7, 3)}, {'R', R(6, 5, 6, 5, 5)}, {'S', R(3, 4, 2, 1, 6)}, {'T', R(7, 2, 2, 2, 2)},
    {'U', R(5, 5, 5, 5, 7)}, {'V', R(5, 5, 5, 5, 2)}, {'W', R(5, 5, 7, 7, 5)}, {'X', R(5, 5, 2, 5, 5)}, {'Y', R(5, 5, 2, 2, 2)},
    {'Z', R(7, 1, 2, 4, 7)}, {'*', R(5, 2, 7, 2, 5)}, {'+', R(0, 2, 7, 2, 0)}, {'-', R(0, 0, 7, 0, 0)}, {'^', R(2, 5, 0, 0, 0)}};

int glyphRows(char ch) {
  if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
  for (const Glyph& g : kGlyphs)
    if (g.ch == ch) return g.rows;
  return -1;
}

}  // namespace

// The rule (docs/SRPG.md "Unit ranks"): the badge is at most 4/9 of the unit's width on screen and 5 pixels tall (times p = unitW / 18, at least 1, so it grows with the
// map zoom but never with the UI scale). Up to 3 characters are drawn as letters when they fit (3 pixels wide each, 1 apart); when they do not, one 2x2 pip
// per character is drawn instead, as many as fit (at least one).
void drawRankBadge(Renderer& r, const DrawCtx& c, const Unit& u, int right, int bottom, int unitW, float alpha) {
  const std::string text = rankBadgeText(c, u);
  if (text.empty()) return;
  const int p = std::max(1, unitW / 18), maxW = std::max(5, unitW * 4 / 9);
  const int n = (int)text.size();
  bool pips = n * 3 * p + (n - 1) * p > maxW;
  for (char ch : text) pips = pips || glyphRows(ch) < 0;
  int count = n, w;
  if (pips) {
    count = std::max(1, std::min(n, (maxW + p) / (3 * p)));
    w = count * 3 * p - p;  // pips 2p wide, p apart
  } else {
    w = n * 3 * p + (n - 1) * p;
  }
  const int h = pips ? 2 * p : 5 * p, x0 = right - w, y0 = bottom - h;
  auto a = [&](Color col) { return withAlpha(col, (int)(col.a * alpha)); };
  r.fillRect(x0 - 1, y0 - 1, w + 2, h + 2, a(Color(0, 0, 0, 215)));  // dark rim, so it reads on any colour
  r.fillRect(x0, y0, w, h, a(Color(120, 86, 20, 240)));
  const Color ink = a(Color(255, 232, 130));
  if (pips) {
    for (int i = 0; i < count; ++i) r.fillRect(x0 + i * 3 * p, y0, 2 * p, 2 * p, ink);
    return;
  }
  for (int i = 0; i < n; ++i) {
    const int rows = glyphRows(text[(size_t)i]);
    for (int y = 0; y < 5; ++y)
      for (int x = 0; x < 3; ++x)
        if (rows >> (3 * (4 - y) + (2 - x)) & 1) r.fillRect(x0 + i * 4 * p + x * p, y0 + y * p, p, p, ink);
  }
}

}  // namespace ote
