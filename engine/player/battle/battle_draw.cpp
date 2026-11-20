// Map, units, overlays and cursor.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "battle/carrier_marker.h"
#include "battle/condition_marker.h"
#include "battle/hex_draw.h"
#include "battle/tile_art.h"
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
  Color col = Color::rgb(terrainRgb(c, cs.terrain));
  r.fillRect(sx, sy, t, t, col);
  SpriteSrc tile;
  if (tileSrc(c, cell, tile)) {  // the map's tile layer replaces the terrain's look (docs/GRAPHICS.md "Tile layer")
    drawTileArt(r, c, tile, sx, sy, t, t);
  } else if (const Look* art = terrainArt(c, cs.terrain)) {
    // Art replaces the faint outline and the letter; transparent pixels show the colour above.
    drawTerrainArt(r, c, *art, cell, sx, sy, t, t);
  } else {
    r.drawRect(sx, sy, t, t, Color(0, 0, 0, 28));
    if (td.glyph != ' ' && !(st && st->showGlyphs == 0)) {
      drawGlyphCentered(r, sx + t / 2, sy + t / 2, td.glyph, mix(col, readableOn(col), 0.28f), t * 9 / 20);
    }
  }
}

// Remaining capture points of a structure, once per structure, drawn over the
// square of the unit that is capturing it.
void drawCapture(Renderer& r, const DrawCtx& c, const StructureState& st, int camX, int camY) {
  int pts = c.battle.defOf(st).capturePoints;
  if (st.capturer < 0 || pts <= 0 || st.capture >= pts) return;
  const Unit& u = c.battle.units[st.capturer];
  if (!u.alive || u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) return;
  float fx, fy;  // the unit's tile box (on a hexagon map: centred on the hexagon's middle)
  cellTopLeft(c.battle.grid, c.layout.tile, u.cell, fx, fy);
  int sx = (int)std::lround(fx) - camX, sy = (int)std::lround(fy) - camY;
  std::string n = intStr(st.capture);
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
  if (const Look* artLook = unitArt(c, u)) {
    drawUnitArt(r, c, *artLook, u, sx, sy, v.scale, al, b.unitSpent(u));
  } else {
    Color base = Color::rgb(b.sides[shownSide(c, u)].color);
    if (b.unitSpent(u)) base = mix(base, Color(70, 70, 78), 0.55f);  // dimmed: already acted
    r.fillRect(bx - 1, by - 1, bw + 2, bh + 2, scaled(Color(0, 0, 0, 200), al));
    r.fillRect(bx, by, bw, bh, scaled(base, al));
    r.fillRect(bx, by, bw, 1, scaled(mix(base, Color(255, 255, 255), 0.35f), al));  // soft top highlight
    drawGlyphCentered(r, bx + bw / 2, by + bh / 2, b.db->unitTypes[(size_t)shownType(c, u)].glyph, scaled(readableOn(base), al),
                      std::max(1, std::min(bw, bh) * 3 / 4));
  }
  if (u.leader) {
    r.fillRect(bx - 1, by - 1, 5, 5, scaled(Color(0, 0, 0, 220), al));
    r.fillRect(bx, by, 3, 3, scaled(Color(255, 210, 40), al));
  }
  drawCarrierMarker(r, c, u, bx + bw, by, bw, al);  // top right corner: a unit with someone off the board (rescue / pair up)
  drawConditionMarker(r, c, u, bx, by + bh, al);     // bottom left corner: its conditions (docs/SRPG.md section 9)
  // HP bar under the block, using the DISPLAYED hp so it drains with the hit.
  int mx = b.maxHp(u), barY = sy + t - inset - 2, barW = t - 2 * inset;
  const Look* art = unitArt(c, u);
  if (art && b.db->graphics.unitMarker) {
    // On the side-colour base the bar sits inside the colour (a dark track, no outline),
    // so the base still shows above, below and beside it.
    r.fillRect(bx, barY, barW, 2, scaled(Color(20, 20, 28, 230), al));
  } else {
    r.fillRect(bx - 1, barY - 1, barW + 2, 4, scaled(Color(0, 0, 0, 200), al));
  }
  int fill = mx > 0 ? std::max(v.hp > 0 ? 1 : 0, barW * std::min(v.hp, mx) / mx) : 0;
  r.fillRect(bx, barY, fill, 2, scaled(hpColor(v.hp, mx), al));
}

// The pixel row of the bottom edge of a structure's footprint or picture box (what it sorts with units by).
int structureBottom(const Battle& b, int t, const StructureState& st) {
  int bx, by, bw, bh;
  structureBox(b.grid, t, st.x, st.y, b.defOf(st), bx, by, bw, bh);
  return by + bh;
}

}  // namespace

void drawMap(Renderer& r, const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor) {
  if (c.view3d()) {
    c.map3d->draw(r, c, ov, cursor, showCursor);
    return;
  }
  if (c.iso()) {
    drawMapIso(r, c, ov, cursor, showCursor);
    return;
  }
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  int t = L.tile, cx = c.cam.ix(), cy = c.cam.iy();
  // Behind a map smaller than the viewport (the style may pick the colour).
  r.fillRect(0, 0, L.vw, L.vh, c.style && c.style->hasBackground ? Color::rgb(c.style->background) : Color(10, 10, 16));
  int gw = b.grid.width(), gh = b.grid.height();
  const CellShape shape = CellShape::of(b.grid, t);  // squares or hexagons (hex_draw.h)
  int x0 = std::max(0, cx / t), x1 = std::min(gw - 1, (cx + L.vw) / t);
  int y0 = std::max(0, cy / t), y1 = std::min(gh - 1, (cy + L.vh) / t);
  if (shape.hex) {  // the cells whose box meets the viewport (a box is at most one step wider than the step)
    const int step = 3 * shape.geo.quarter();
    const int stepX = shape.geo.pointy ? t : step, stepY = shape.geo.pointy ? step : t;
    x0 = std::max(0, cx / stepX - 1);
    x1 = std::min(gw - 1, (cx + L.vw) / stepX + 1);
    y0 = std::max(0, cy / stepY - 1);
    y1 = std::min(gh - 1, (cy + L.vh) / stepY + 1);
  }
  for (int y = y0; y <= y1; ++y)
    for (int x = x0; x <= x1; ++x) {
      Cell cell = b.grid.at(x, y);
      int sx, sy;
      shape.origin(b.grid, cell, cx, cy, sx, sy);
      if (shape.hex) drawTerrainHex(r, c, shape.geo, cell, sx, sy);
      else drawTerrain(r, c, cell, sx, sy);
      if (ov.any()) {
        const Tone tone = toneOf(ov, cell);
        if (tone != Tone::None) shape.fill(r, sx, sy, toneColor(tone, 105));
      }
    }
  // Structures and units share one pass, sorted by the pixel row of their bottom
  // edge (structures first on a tie), so a tall structure covers what stands
  // behind it. A unit standing on a structure square sorts with that structure's
  // bottom edge instead, so it is always drawn over the structure it stands on
  // (also on the upper rows of a multi-square footprint).
  struct Item { int key, kind, index; };  // kind 0 = structure, 1 = unit
  std::vector<Item> items;
  for (size_t i = 0; i < b.structures.size(); ++i) {
    const StructureState& st = b.structures[i];
    items.push_back({structureBottom(b, t, st), 0, (int)i});
  }
  for (const Unit& u : b.units) {
    if (u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
    const UnitView& v = c.view.units[u.id];
    int key = (int)std::lround(v.py) + t;
    Cell under = shape.hex ? cellNearPos(b.grid, t, v.px, v.py)
                           : b.grid.at(((int)std::lround(v.px) + t / 2) / t, ((int)std::lround(v.py) + t / 2) / t);
    int si = under == kNoCell ? -1 : b.structureAt[under];
    if (si >= 0) key = std::max(key, structureBottom(b, t, b.structures[si]));
    items.push_back({key, 1, u.id});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b2) {
    return a.key != b2.key ? a.key < b2.key : a.kind < b2.kind;
  });
  for (const Item& it : items) {
    if (it.kind == 0) drawStructure(r, c, it.index, cx, cy, ov);
    else drawUnit(r, c, b.units[it.index], c.view.units[it.index], cx, cy);
  }
  if (c.view.fogged()) {
    // Squares the viewer does not see: a dark veil over the ground and the structures (no unit the viewer could see
    // stands there, so none is dimmed by mistake); the capture numbers and the cursor come after, on top.
    for (int y = y0; y <= y1; ++y)
      for (int x = x0; x <= x1; ++x)
        if (c.view.dimmed(b.grid.at(x, y))) {
          int sx, sy;
          shape.origin(b.grid, b.grid.at(x, y), cx, cy, sx, sy);
          shape.fill(r, sx, sy, Color(0, 0, 0, (uint8_t)kFogDimAlpha));
        }
  }
  for (const StructureState& st : b.structures) drawCapture(r, c, st, cx, cy);
  if (showCursor && b.grid.valid(cursor)) {
    int sx, sy;
    shape.origin(b.grid, cursor, cx, cy, sx, sy);
    int a = 200 + (int)(55 * std::sin(c.time * 6.0f));
    if (shape.hex) {  // two rings: yellow outside, white inside
      hexEdges(r, shape.geo, sx, sy, Color(255, 255, 255, (uint8_t)a), 2);
      hexEdges(r, shape.geo, sx, sy, Color(255, 240, 120, (uint8_t)a), 1);
    } else {
      r.drawRect(sx, sy, t, t, Color(255, 240, 120, (uint8_t)a));
      r.drawRect(sx + 1, sy + 1, t - 2, t - 2, Color(255, 255, 255, (uint8_t)a));
    }
  }
}

}  // namespace ote
