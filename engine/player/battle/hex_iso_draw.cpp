// A hexagon battle map in the 2D isometric kind of view (docs/HEX.md "2D isometric"). The geometry is
// core/view/hex_iso.h; this draws it like iso_draw.cpp draws squares: hexagons back to front by the pixel row of
// their front point, structures and units right after the hexagons they stand on (so nearer ground covers them), what
// ends up covered shown through again. A hexagon is its terrain picture (top face and front faces in one cell) plus
// repeats of the picture's lower part where the neighbour in front is lower.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "battle/facing_marker.h"
#include "battle/hex_draw.h"
#include "battle/iso_unit.h"
#include "ui/text.h"

namespace ote {
namespace {

struct HI {
  Renderer& r;
  const DrawCtx& c;
  const MapProj& p;
  const HexIsoGeometry& g;
  int camX, camY;

  // Screen top-left of the top-face box of a hexagon at an elevation (levels, may be a structure's height).
  void top(int x, int y, int elev, int& sx, int& sy) const {
    g.topBox(x, y, elev, sx, sy);
    sx -= camX;
    sy -= camY;
  }
  bool offScreen(int x, int y, int w, int h) const { return x + w < 0 || y + h < 0 || x > c.layout.vw || y > c.layout.vh; }
};

Color shade(float k, int a = 255) { return Color((uint8_t)(255 * k), (uint8_t)(255 * k), (uint8_t)(255 * k), (uint8_t)a); }

// Elevation of the neighbour in a direction; off the map -1, so the map's rim shows the side of its lowest level.
int neighbourElev(const HI& I, Cell cell, int dir) {
  Cell n = I.c.battle.grid.neighbor(cell, dir);
  return n == kNoCell ? -1 : I.p.elevation(I.c.battle.grid.x(n), I.c.battle.grid.y(n));
}

// One hexagon: its picture (art or a generated block) at its elevation, and the lower slices of the same picture
// wherever a side face shows below that (only under the faces whose neighbour is lower).
void drawHexagon(const HI& I, Cell cell) {
  const Battle& b = I.c.battle;
  const HexIsoGeometry& g = I.g;
  const int x = b.grid.x(cell), y = b.grid.y(cell), e = I.p.elevation(x, y);
  int bx, by;
  I.top(x, y, e, bx, by);
  const CellState& cs = b.cells[cell];
  const Look* art = terrainArt(I.c, cs.terrain);
  SpriteSrc src;
  if (art) {
    src = terrainSrc(I.c, *art, cell);
  } else {
    IsoPicture pic = I.c.tiles->hexBlock(I.r, g.sq, g.step, terrainRgb(I.c, cs.terrain));
    src.tex = pic.tex;
    src.w = pic.w;
    src.h = pic.h;
  }
  // Width = the box; height keeps the picture's proportions; its bottom edge is the bottom of the side faces, one
  // height step under the top face's box (taller pictures rise above it).
  const int bw = g.boxW(), dh = std::max(1, bw * src.h / std::max(1, src.w));
  const int bottom = by + g.boxH() + g.step;
  if (I.offScreen(bx, bottom - dh, bw, dh + e * g.step)) return;

  const bool dim = I.c.view.dimmed(cell);  // fog: a hexagon the viewer does not see is drawn darker
  const Color mod = dim ? shade(kFogDim) : Color(255, 255, 255);
  const int sliceH = std::min(dh, g.boxH() / 2 + g.step);
  const int srcSlice = std::max(1, sliceH * src.h / dh);
  for (int d = 0; d < 6; ++d) {
    if (!((g.visibleFaces() >> d) & 1u)) continue;
    int l, rr;
    g.faceSpan(d, l, rr);
    const int sx0 = src.x + l * src.w / bw, sw = std::max(1, (rr - l) * src.w / bw);
    for (int level = std::max(neighbourElev(I, cell, d) + 1, 0); level < e; ++level) {
      int sy = bottom + (e - level) * g.step;
      I.r.drawTexture(src.tex, sx0, src.y + src.h - srcSlice, sw, srcSlice, bx + l, sy - sliceH, rr - l, sliceH, mod);
    }
  }
  I.r.drawTexture(src.tex, src.x, src.y, src.w, src.h, bx, bottom - dh, bw, dh, mod);
  if (!art) {
    const TerrainDef& td = b.terrainAt(cell);
    const StyleDef* st = I.c.style;
    if (td.glyph != ' ' && !(st && st->showGlyphs == 0)) {
      Color col = Color::rgb(terrainRgb(I.c, cs.terrain));
      Color gc = mix(col, readableOn(col), 0.28f);
      drawGlyphCentered(I.r, bx + bw / 2, by + g.boxH() / 2, td.glyph, dim ? fogDim(gc) : gc, std::min(g.boxH(), bw / 2) * 9 / 20);
    }
  }
}

// A hexagon over a cell's top face, `lift` pixels above it (a tint, or only its outline).
void tint(const HI& I, Cell cell, Color col, bool outline = false, int lift = 0, int outlineWidth = 2) {
  int sx, sy;
  I.top(I.c.battle.grid.x(cell), I.c.battle.grid.y(cell), I.p.elevation(I.c.battle.grid.x(cell), I.c.battle.grid.y(cell)), sx, sy);
  if (outline) hexEdges(I.r, I.g.sq, sx, sy - lift, col, outlineWidth);
  else fillHex(I.r, I.g.sq, sx, sy - lift, col);
}

void overlayTint(const HI& I, const Overlay& ov, Cell cell, int lift = 0) {
  if (!ov.any()) return;
  const Tone tone = toneOf(ov, cell);
  if (tone != Tone::None) tint(I, cell, toneColor(tone, 120), false, lift);
}

// A structure stands at the height of its highest hexagon.
int structureElev(const HI& I, const std::vector<Cell>& cells) {
  int e = 0;
  for (Cell cell : cells)
    if (cell != kNoCell) e = std::max(e, I.p.elevation(I.c.battle.grid.x(cell), I.c.battle.grid.y(cell)));
  return e;
}

// The sort key of a structure: its nearest (frontmost) hexagon.
int structureKey(const HI& I, int index) {
  std::vector<Cell> cells;
  I.c.battle.structureCells(index, cells);
  int key = 0;
  for (Cell cell : cells)
    if (cell != kNoCell) key = std::max(key, I.g.sortKey(I.c.battle.grid.x(cell), I.c.battle.grid.y(cell)));
  return key;
}

void drawStructureHexIso(const HI& I, int index, const Overlay& ov) {
  const Battle& b = I.c.battle;
  const StructureState st = shownStructure(I.c, index);  // fog: kind and owner as the viewer last saw them
  const StructureDef& def = b.defOf(st);
  const HexIsoGeometry& g = I.g;
  std::vector<Cell> cells;
  b.structureCells(index, cells);  // the middle hexagon first, then the ring
  const int e = structureElev(I, cells);
  int bx, by, bw, bh;
  g.sq.structureBox(st.x, st.y, def.rings, bx, by, bw, bh);
  bx -= I.camX;
  by += (g.levels - e) * g.step + g.headroom - I.camY;
  if (I.offScreen(bx, by - bw, bw, bh + bw)) return;

  const bool dim = structureDimmed(I.c, index);  // fog: nothing of it is in the viewer's sight now
  const Color mod = dim ? shade(kFogDim) : Color(255, 255, 255);
  const int ownerSide = shownOwner(I.c, index);  // fog: the owner the viewer last saw
  Color owner = ownerSide >= 0 ? Color::rgb(b.sides[(size_t)ownerSide].color) : Color(0, 0, 0, 0);
  if (dim) owner = fogDim(owner);
  // The owner's frame on the ground around the covered hexagons: all of it under the structure, then the edges
  // facing the viewer again on top (the body hides the far ones).
  auto frame = [&](unsigned only) {
    for (Cell cell : cells) {
      int sx, sy;
      I.top(b.grid.x(cell), b.grid.y(cell), e, sx, sy);
      hexEdges(I.r, g.sq, sx, sy, owner, 2, outerEdges(b.grid, cell, cells) & only);
    }
  };
  const Look* art = structureArt(I.c, st);
  // (A placeholder block covers the whole ground hexagon, so only its front edges below are of use.)
  if (ownerSide >= 0 && art) frame(63u);

  int lift = 0;  // where the move/attack highlight goes: on a placeholder block's top
  if (art) {
    // Across the box of the covered hexagons, standing on its front edge.
    SpriteSrc src = lookSrc(I.c, *art);
    int dh = std::max(1, bw * src.h / std::max(1, src.w));
    I.r.drawTexture(src.tex, src.x, src.y, src.w, src.h, bx, by + bh - dh, bw, dh, mod);
  } else {
    // Placeholder: every covered hexagon a low block, back to front, the letter on the middle one.
    const int wall = std::max(2, g.boxH() * 3 / 4);
    lift = wall;
    const uint32_t rgb = structureRgb(I.c, st.type);
    IsoPicture pic = I.c.tiles->hexBlock(I.r, g.sq, wall, rgb);
    std::vector<Cell> order = cells;
    std::stable_sort(order.begin(), order.end(), [&](Cell a, Cell c2) {
      int ka = g.sortKey(b.grid.x(a), b.grid.y(a)), kc = g.sortKey(b.grid.x(c2), b.grid.y(c2));
      return ka != kc ? ka < kc : b.grid.x(a) < b.grid.x(c2);
    });
    for (Cell cell : order) {
      int sx, sy;
      I.top(b.grid.x(cell), b.grid.y(cell), e, sx, sy);
      I.r.drawTexture(pic.tex, 0, 0, pic.w, pic.h, sx, sy - wall, pic.w, pic.h, mod);
    }
    const StyleDef* style = I.c.style;
    if (!cells.empty() && def.glyph != ' ' && !(style && style->showGlyphs == 0)) {
      int sx, sy;
      I.top(b.grid.x(cells[0]), b.grid.y(cells[0]), e, sx, sy);
      Color gc = readableOn(Color::rgb(rgb));
      drawGlyphCentered(I.r, sx + g.boxW() / 2, sy - wall + g.boxH() / 2, def.glyph, dim ? fogDim(gc) : gc, g.boxH() * 9 / 20);
    }
  }
  if (ownerSide >= 0) frame(g.visibleFaces());
  for (Cell cell : cells)
    if (cell != kNoCell) overlayTint(I, ov, cell, lift ? lift + (e - I.p.elevation(b.grid.x(cell), b.grid.y(cell))) * g.step : 0);
}

void drawUnitHexIso(const HI& I, const Unit& u, const UnitView& v, float fade) {
  const HexIsoGeometry& g = I.g;
  const int t = I.p.tile(), uw = I.p.unitW();
  // The unit's spot in flat map pixels (its tile box centred on its hexagon's middle), lifted to the ground there.
  const float fx = v.px + v.ox + t * 0.5f, fy = v.py + v.oy + t * 0.5f;
  float wx, wy;
  g.project(fx, fy, I.p.hexElevationAt(fx, fy), wx, wy);
  const int cx = (int)std::lround(wx) - I.camX, cy = (int)std::lround(wy) - I.camY;
  const int foot = cy + g.boxH() / 4;  // the feet stand a little in front of the hexagon's middle
  if (I.offScreen(cx - uw, foot - uw * 3, uw * 2, uw * 3 + 8)) return;
  // The marker under the feet: a small flat hexagon.
  auto marker = [&](int x, int y, int w, int h, Color col) {
    HexGeo m;
    m.pointy = false;
    m.L = std::max(4, w & ~3);
    m.T = std::max(2, h & ~1);
    fillHex(I.r, m, x + (w - m.L) / 2, y + (h - m.T) / 2, col);
  };
  int faceX = kNoFace, faceY = kNoFace;  // the edge of its hexagon toward the way it looks (docs/SRPG.md "Facing")
  const int fdir = shownFacing(I.c, u);
  if (fdir >= 0) {
    float fvx, fvy, ex, ey;
    facingEdgeVector(I.c.battle.grid, fdir, t, fvx, fvy);
    g.project(fx + fvx, fy + fvy, I.p.hexElevationAt(fx, fy), ex, ey);
    faceX = (int)std::lround(ex) - I.camX;
    faceY = (int)std::lround(ey) - I.camY;
  }
  drawIsoUnitFigure(I.r, I.c, u, v, fade, cx, cy, foot, uw, marker, faceX, faceY);
}

void drawCaptureHexIso(const HI& I, const StructureState& st) {
  const Battle& b = I.c.battle;
  int pts = b.defOf(st).capturePoints;
  if (st.capturer < 0 || pts <= 0 || st.capture >= pts) return;
  const Unit& u = b.units[st.capturer];
  if (!u.alive || u.id >= (int)I.c.view.units.size() || !I.c.view.units[u.id].visible) return;
  int sx, sy;
  I.top(b.grid.x(u.cell), b.grid.y(u.cell), I.p.elevation(b.grid.x(u.cell), b.grid.y(u.cell)), sx, sy);
  const int cx = sx + I.g.boxW() / 2, cy = sy + I.g.boxH() / 2;
  std::string n = intStr(st.capture);
  int w = textW(I.r, n), tx = cx + I.p.unitW() / 2 - w / 2, ty = cy - I.p.unitW();
  I.r.fillRect(tx - 1, ty - 1, w + 2, I.c.layout.lh + 2, Color(0, 0, 0, 190));
  I.r.drawText(tx, ty, n.c_str(), Color(255, 240, 120));
}

}  // namespace

void drawMapHexIso(Renderer& r, const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  const MapProj& p = *c.proj;
  HI I{r, c, p, p.hexIso(), c.cam.ix(), c.cam.iy()};
  const HexIsoGeometry& g = I.g;
  r.fillRect(0, 0, L.vw, L.vh, c.style && c.style->hasBackground ? Color::rgb(c.style->background) : Color(10, 10, 16));
  const int pulse = 200 + (int)(55 * std::sin(c.time * 6.0f));
  const bool cursorOn = showCursor && b.grid.valid(cursor);

  // Structures and units, by the pixel row of their front point on the ground (structures first on a tie). A unit
  // on a structure goes after that structure, so it is never hidden by it.
  struct Item { int key, kind, index; };  // kind 0 = structure, 1 = unit
  std::vector<Item> items;
  for (size_t i = 0; i < b.structures.size(); ++i) items.push_back({structureKey(I, (int)i), 0, (int)i});
  for (const Unit& u : b.units) {
    if (u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
    const UnitView& v = c.view.units[u.id];
    const float fy = v.py + v.oy + p.tile() * 0.5f;
    int key = (int)std::ceil(fy * g.boxH() / g.td.boxH() + g.boxH() * 0.5f - 0.001f);  // the hexagon's key when resting
    Cell on = cellNearPos(b.grid, p.tile(), v.px + v.ox, v.py + v.oy);
    int si = b.structureAt[on];
    if (si >= 0) key = std::max(key, structureKey(I, si));
    items.push_back({key, 1, u.id});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b2) {
    return a.key != b2.key ? a.key < b2.key : a.kind < b2.kind;
  });

  size_t next = 0;
  auto flush = [&](int below) {  // draw the items whose key is under `below`
    for (; next < items.size() && items[next].key < below; ++next) {
      const Item& it = items[next];
      if (it.kind == 0) drawStructureHexIso(I, it.index, ov);
      else drawUnitHexIso(I, b.units[it.index], c.view.units[it.index], 1.0f);
    }
  };
  for (Cell cell : p.hexDrawOrder()) {
    flush(g.sortKey(b.grid.x(cell), b.grid.y(cell)));
    drawHexagon(I, cell);
    overlayTint(I, ov, cell);
    if (cursorOn && cell == cursor) tint(I, cell, Color(255, 240, 120, (uint8_t)pulse), true);
  }
  flush(1 << 30);
  // Shown through: units standing behind raised ground, and the cursor.
  for (const Unit& u : b.units) {
    if (u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
    const UnitView& v = c.view.units[u.id];
    Cell on = cellNearPos(b.grid, p.tile(), v.px + v.ox, v.py + v.oy);
    if (g.hidden(b.grid.x(on), b.grid.y(on), p.elevations())) drawUnitHexIso(I, u, v, 0.45f);
  }
  if (cursorOn && g.hidden(b.grid.x(cursor), b.grid.y(cursor), p.elevations()))
    tint(I, cursor, Color(255, 240, 120, (uint8_t)(pulse * 2 / 5)), true);
  for (const StructureState& st : b.structures) drawCaptureHexIso(I, st);
}

}  // namespace ote
