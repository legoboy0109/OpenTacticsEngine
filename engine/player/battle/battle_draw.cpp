// Map, units, overlays and cursor.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "ui/text.h"

namespace ote {
namespace {

Color scaled(Color c, float alpha) { return withAlpha(c, (int)(c.a * alpha)); }

void drawTerrain(Renderer& r, const DrawCtx& c, Cell cell, int sx, int sy) {
  const Battle& b = c.battle;
  int t = c.layout.tile;
  const CellState& cs = b.cells[cell];
  const TerrainDef& td = b.terrainAt(cell);
  // The visual style may recolor terrain and force the placeholder letters on or off.
  const StyleDef* st = c.style;
  size_t ti = (size_t)b.cells[cell].terrain;
  uint32_t rgb = st && ti < st->terrainColors.size() && st->terrainColors[ti] >= 0 ? (uint32_t)st->terrainColors[ti]
                                                                                    : td.color;
  Color col = Color::rgb(rgb);
  r.fillRect(sx, sy, t, t, col);
  r.drawRect(sx, sy, t, t, Color(0, 0, 0, 28));
  if (td.glyph != ' ' && !(st && st->showGlyphs == 0)) {
    drawGlyphCentered(r, sx + t / 2, sy + t / 2, td.glyph, mix(col, readableOn(col), 0.28f), t * 9 / 20);
  }
  if (cs.owner >= 0) {
    Color oc = Color::rgb(b.sides[cs.owner].color);
    r.fillRect(sx, sy, t, 2, oc);
    r.fillRect(sx, sy + t - 2, t, 2, oc);
    r.fillRect(sx, sy + 2, 2, t - 4, oc);
    r.fillRect(sx + t - 2, sy + 2, 2, t - 4, oc);
  }
}

// Remaining capture points, drawn over the unit that is capturing.
void drawCapture(Renderer& r, const DrawCtx& c, Cell cell, int sx, int sy) {
  const CellState& cs = c.battle.cells[cell];
  int pts = c.battle.terrainAt(cell).capturePoints;
  if (cs.capturer < 0 || pts <= 0 || cs.capture >= pts) return;
  std::string n = intStr(cs.capture);
  int w = textW(r, n), x = sx + c.layout.tile - w - 2, y = sy + c.layout.tile / 6;
  r.fillRect(x - 1, y - 1, w + 2, c.layout.lh + 2, Color(0, 0, 0, 190));
  r.drawText(x, y, n.c_str(), Color(255, 240, 120));
}

void drawUnit(Renderer& r, const DrawCtx& c, const Unit& u, const UnitView& v, int camX, int camY) {
  const Battle& b = c.battle;
  int t = c.layout.tile;
  int sx = (int)std::lround(v.px + v.ox) - camX, sy = (int)std::lround(v.py + v.oy) - camY;
  if (sx + t < 0 || sy + t < 0 || sx > c.layout.vw || sy > c.layout.vh) return;
  float al = v.alpha;
  int inset = std::max(2, t / 8);
  int bx = sx + inset, by = sy + inset, bw = t - 2 * inset, bh = t - 2 * inset - 3;
  if (v.scale < 1) {  // pop-in: grow from the centre
    int nw = std::max(1, (int)(bw * v.scale)), nh = std::max(1, (int)(bh * v.scale));
    bx += (bw - nw) / 2;
    by += (bh - nh) / 2;
    bw = nw;
    bh = nh;
  }
  Color base = Color::rgb(b.sides[u.side].color);
  if (u.done) base = mix(base, Color(70, 70, 78), 0.55f);  // dimmed: already acted
  r.fillRect(bx - 1, by - 1, bw + 2, bh + 2, scaled(Color(0, 0, 0, 200), al));
  r.fillRect(bx, by, bw, bh, scaled(base, al));
  r.fillRect(bx, by, bw, 1, scaled(mix(base, Color(255, 255, 255), 0.35f), al));  // soft top highlight
  drawGlyphCentered(r, bx + bw / 2, by + bh / 2, b.typeOf(u).glyph, scaled(readableOn(base), al),
                    std::max(1, std::min(bw, bh) * 3 / 4));
  if (u.leader) {
    r.fillRect(bx - 1, by - 1, 5, 5, scaled(Color(0, 0, 0, 220), al));
    r.fillRect(bx, by, 3, 3, scaled(Color(255, 210, 40), al));
  }
  // HP bar under the block, using the DISPLAYED hp so it drains with the hit.
  int mx = b.maxHp(u), barY = sy + t - inset - 2, barW = t - 2 * inset;
  r.fillRect(bx - 1, barY - 1, barW + 2, 4, scaled(Color(0, 0, 0, 200), al));
  int fill = mx > 0 ? std::max(v.hp > 0 ? 1 : 0, barW * std::min(v.hp, mx) / mx) : 0;
  r.fillRect(bx, barY, fill, 2, scaled(hpColor(v.hp, mx), al));
}

}  // namespace

void drawMap(Renderer& r, const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  int t = L.tile, cx = c.cam.ix(), cy = c.cam.iy();
  // Behind a map smaller than the viewport (the style may pick the colour).
  r.fillRect(0, 0, L.vw, L.vh, c.style && c.style->hasBackground ? Color::rgb(c.style->background) : Color(10, 10, 16));
  int gw = b.grid.width(), gh = b.grid.height();
  int x0 = std::max(0, cx / t), x1 = std::min(gw - 1, (cx + L.vw) / t);
  int y0 = std::max(0, cy / t), y1 = std::min(gh - 1, (cy + L.vh) / t);
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      Cell cell = b.grid.at(x, y);
      int sx = x * t - cx, sy = y * t - cy;
      drawTerrain(r, c, cell, sx, sy);
      if (ov.any()) {
        if (ov.reach[cell]) r.fillRect(sx, sy, t, t, Color(50, 110, 255, 105));
        else if (ov.attack[cell]) r.fillRect(sx, sy, t, t, Color(255, 50, 50, 105));
      }
    }
  for (const Unit& u : b.units) {
    if (u.id >= (int)c.view.units.size()) continue;
    const UnitView& v = c.view.units[u.id];
    if (v.visible) drawUnit(r, c, u, v, cx, cy);
  }
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) drawCapture(r, c, b.grid.at(x, y), x * t - cx, y * t - cy);
  if (showCursor && b.grid.valid(cursor)) {
    int sx = b.grid.x(cursor) * t - cx, sy = b.grid.y(cursor) * t - cy;
    int a = 200 + (int)(55 * std::sin(c.time * 6.0f));
    r.drawRect(sx, sy, t, t, Color(255, 240, 120, (uint8_t)a));
    r.drawRect(sx + 1, sy + 1, t - 2, t - 2, Color(255, 255, 255, (uint8_t)a));
  }
}

}  // namespace ote
