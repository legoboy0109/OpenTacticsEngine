// The battle map in the isometric kind of view (docs/VIEWS.md "Drawing").
// Squares are drawn back to front, one diagonal at a time; structures and
// units are drawn right after the diagonal they stand on, so raised ground
// nearer the viewer covers them. What ends up covered is shown through.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "ui/text.h"

namespace ote {
namespace {

struct Iso {
  Renderer& r;
  const DrawCtx& c;
  const MapProj& p;
  const IsoGeometry& g;
  int camX, camY;

  // Screen position of a grid spot at an elevation.
  void at(float gx, float gy, float elev, int& sx, int& sy) const {
    float wx, wy;
    g.project(gx, gy, elev, wx, wy);
    sx = (int)std::lround(wx) - camX;
    sy = (int)std::lround(wy) - camY;
  }
  bool offScreen(int x, int y, int w, int h) const {
    return x + w < 0 || y + h < 0 || x > c.layout.vw || y > c.layout.vh;
  }
};

Color scaled(Color col, float alpha) { return withAlpha(col, (int)(col.a * alpha)); }
Color shade(float k, int a = 255) { return Color((uint8_t)(255 * k), (uint8_t)(255 * k), (uint8_t)(255 * k), (uint8_t)a); }

// Elevation of the square at (x, y); outside the map -1, so the map's rim
// shows the side of its lowest level.
int elevOr(const MapProj& p, const Grid& grid, int x, int y) {
  return x < 0 || y < 0 || x >= grid.width() || y >= grid.height() ? -1 : p.elevation(x, y);
}

// One square: its picture (art or a generated block) at its elevation, and the
// lower slices of the same picture wherever its side faces show below that.
void drawSquare(const Iso& I, Cell cell, int x, int y) {
  const Battle& b = I.c.battle;
  const IsoGeometry& g = I.g;
  int e = I.p.elevation(x, y);
  int nx, ny;
  I.at((float)x, (float)y, (float)e, nx, ny);
  const CellState& cs = b.cells[cell];
  const Look* art = terrainArt(I.c, cs.terrain);
  SpriteSrc src;
  if (art) {
    src = terrainSrc(I.c, *art, cell);
  } else {
    IsoPicture pic = I.c.tiles->block(I.r, g, 1, 1, g.step, terrainRgb(I.c, cs.terrain));
    src.tex = pic.tex;
    src.w = pic.w;
    src.h = pic.h;
  }
  // Width = the diamond; height keeps the picture's proportions; its bottom
  // edge is the bottom of the side faces, one height step under the diamond.
  int dw = g.tileW, dh = std::max(1, g.tileW * src.h / std::max(1, src.w));
  int left = nx - g.tileW / 2, bottom = ny + g.tileH + g.step;
  if (I.offScreen(left, bottom - dh, dw, dh + e * g.step)) return;

  // Side faces down to the tops of the two squares in front of this one.
  int seen = std::min(elevOr(I.p, b.grid, x + 1, y), elevOr(I.p, b.grid, x, y + 1));
  // Fog: a square the viewer does not see is drawn darker, sides and letter included.
  const bool dim = I.c.view.dimmed(cell);
  const Color mod = dim ? shade(kFogDim) : Color(255, 255, 255);
  int sliceH = std::min(dh, g.tileH / 2 + g.step);
  int srcSlice = std::max(1, sliceH * src.h / dh);
  for (int level = std::max(seen + 1, 0); level < e; ++level) {
    int by = bottom + (e - level) * g.step;
    I.r.drawTexture(src.tex, src.x, src.y + src.h - srcSlice, src.w, srcSlice, left, by - sliceH, dw, sliceH, mod);
  }
  I.r.drawTexture(src.tex, src.x, src.y, src.w, src.h, left, bottom - dh, dw, dh, mod);
  if (!art) {
    const TerrainDef& td = b.terrainAt(cell);
    const StyleDef* st = I.c.style;
    if (td.glyph != ' ' && !(st && st->showGlyphs == 0)) {
      Color col = Color::rgb(terrainRgb(I.c, cs.terrain));
      Color gc = mix(col, readableOn(col), 0.28f);
      drawGlyphCentered(I.r, nx, ny + g.tileH / 2, td.glyph, dim ? fogDim(gc) : gc, g.tileH * 9 / 20);
    }
  }
}

// A diamond over a square's top face, `lift` pixels above it.
void tint(const Iso& I, int x, int y, Color col, bool outline = false, int lift = 0) {
  int nx, ny;
  I.at((float)x, (float)y, (float)I.p.elevation(x, y), nx, ny);
  IsoPicture d = I.c.tiles->diamond(I.r, I.g, 1, 1, outline);
  I.r.drawTexture(d.tex, 0, 0, d.w, d.h, nx - I.g.tileW / 2, ny - lift, d.w, d.h, col);
}

void overlayTint(const Iso& I, const Overlay& ov, Cell cell, int x, int y, int lift = 0) {
  if (!ov.any()) return;
  if (ov.reach[cell]) tint(I, x, y, Color(50, 110, 255, 120), false, lift);
  else if (ov.attack[cell]) tint(I, x, y, Color(255, 50, 50, 120), false, lift);
}

// Elevation a structure stands at: the highest of its squares.
int structureElev(const Iso& I, const StructureState& st, const StructureDef& def) {
  int e = 0;
  for (int dy = 0; dy < def.height; ++dy)
    for (int dx = 0; dx < def.width; ++dx)
      if (I.c.battle.grid.at(st.x + dx, st.y + dy) != kNoCell) e = std::max(e, I.p.elevation(st.x + dx, st.y + dy));
  return e;
}

void drawStructureIso(const Iso& I, int index, const Overlay& ov) {
  const Battle& b = I.c.battle;
  const StructureState& st = b.structures[(size_t)index];
  const StructureDef& def = b.defOf(st);
  const IsoGeometry& g = I.g;
  int e = structureElev(I, st, def);
  int nx, ny;
  I.at((float)st.x, (float)st.y, (float)e, nx, ny);
  int fw = (def.width + def.height) * g.tileW / 2, fh = (def.width + def.height) * g.tileH / 2;
  int left = nx - def.height * g.tileW / 2;
  if (I.offScreen(left, ny - fw, fw, fh + fw)) return;

  // The owner's frame around the footprint, on the ground: all of it under the
  // structure, then its two near edges again on top (the body hides the far ones).
  const bool dim = structureDimmed(I.c, index);  // fog: nothing of it is in the viewer's sight now
  const Color mod = dim ? shade(kFogDim) : Color(255, 255, 255);
  const int ownerSide = shownOwner(I.c, index);  // fog: the owner the viewer last saw
  Color owner = ownerSide >= 0 ? Color::rgb(b.sides[(size_t)ownerSide].color) : Color(0, 0, 0, 0);
  if (dim) owner = fogDim(owner);
  if (ownerSide >= 0) {
    IsoPicture ring = I.c.tiles->diamond(I.r, g, def.width, def.height, IsoTiles::Outline);
    I.r.drawTexture(ring.tex, 0, 0, ring.w, ring.h, left, ny, ring.w, ring.h, owner);
  }
  int lift = 0;  // where the move/attack highlight goes: on a placeholder block's top
  if (const Look* art = structureArt(I.c, st)) {
    // Across the footprint's full width, standing on its nearest corner.
    SpriteSrc src = lookSrc(I.c, *art);
    int dh = std::max(1, fw * src.h / std::max(1, src.w));
    I.r.drawTexture(src.tex, src.x, src.y, src.w, src.h, left, ny + fh - dh, fw, dh, mod);
  } else {
    int wall = g.tileH * 3 / 4;  // a low box, so what stands behind it stays visible
    lift = wall + e * g.step;
    IsoPicture pic = I.c.tiles->block(I.r, g, def.width, def.height, wall, structureRgb(I.c, st.type));
    I.r.drawTexture(pic.tex, 0, 0, pic.w, pic.h, left, ny - wall, pic.w, pic.h, mod);
    const StyleDef* style = I.c.style;
    if (def.glyph != ' ' && !(style && style->showGlyphs == 0))
      drawGlyphCentered(I.r, left + fw / 2, ny - wall + fh / 2, def.glyph,
                        dim ? fogDim(readableOn(Color::rgb(structureRgb(I.c, st.type)))) : readableOn(Color::rgb(structureRgb(I.c, st.type))),
                        g.tileH * 9 / 20 * std::min(def.width, def.height));
  }
  if (ownerSide >= 0) {
    IsoPicture ring = I.c.tiles->diamond(I.r, g, def.width, def.height, IsoTiles::FrontOutline);
    I.r.drawTexture(ring.tex, 0, 0, ring.w, ring.h, left, ny, ring.w, ring.h, owner);
  }
  for (int dy = 0; dy < def.height; ++dy)
    for (int dx = 0; dx < def.width; ++dx) {
      Cell cell = b.grid.at(st.x + dx, st.y + dy);
      if (cell != kNoCell) overlayTint(I, ov, cell, st.x + dx, st.y + dy, lift ? lift - I.p.elevation(st.x + dx, st.y + dy) * g.step : 0);
    }
}

void drawUnitIso(const Iso& I, const Unit& u, const UnitView& v, float fade) {
  const Battle& b = I.c.battle;
  const IsoGeometry& g = I.g;
  int t = I.p.tile(), uw = I.p.unitW();
  // The unit's spot in squares (BattleView animates flat map pixels), lifted to the ground there.
  float gx = (v.px + v.ox) / t + 0.5f, gy = (v.py + v.oy) / t + 0.5f;
  int cx, cy;
  I.at(gx, gy, I.p.elevationAt(gx, gy), cx, cy);
  int foot = cy + g.tileH / 4;  // the feet stand a little in front of the square's middle
  if (I.offScreen(cx - uw, foot - uw * 3, uw * 2, uw * 3 + 8)) return;
  float al = v.alpha * fade;
  int a = std::max(0, std::min(255, (int)(al * 255.0f + 0.5f)));
  Color side = Color::rgb(b.sides[(size_t)shownSide(I.c, u)].color);
  if (u.done) side = mix(side, Color(70, 70, 78), 0.55f);  // dimmed: already acted
  const Look* art = unitArt(I.c, u);
  int bodyTop;
  if (art) {
    if (b.db->graphics.unitMarker) {
      // A flat diamond in the side's colour under the feet, dark rim first.
      IsoPicture d = I.c.tiles->diamond(I.r, g, 1, 1, false);
      int mw = std::max(6, (int)(uw * v.scale)), mh = std::max(3, mw / 2);
      I.r.drawTexture(d.tex, 0, 0, d.w, d.h, cx - mw / 2 - 1, cy - mh / 2 - 1, mw + 2, mh + 2, Color(20, 20, 28, (uint8_t)(a * 200 / 255)));
      I.r.drawTexture(d.tex, 0, 0, d.w, d.h, cx - mw / 2, cy - mh / 2, mw, mh, withAlpha(side, a));
    }
    SpriteSrc src = lookSrc(I.c, *art);
    int w = std::max(1, (int)(uw * v.scale)), h = std::max(1, w * src.h / std::max(1, src.w));
    Color mod = shade(u.done ? 0.55f : 1.0f, a);
    I.r.drawTexture(src.tex, src.x, src.y, src.w, src.h, cx - w / 2, foot - h, w, h, mod);
    bodyTop = foot - h;
  } else {
    // Placeholder: an upright block in the side's colour with the unit's letter.
    int bw = std::max(2, (int)(uw * 3 / 4 * v.scale)), bh = bw;
    int bx = cx - bw / 2, by = foot - bh;
    I.r.fillRect(bx - 1, by - 1, bw + 2, bh + 2, scaled(Color(0, 0, 0, 200), al));
    I.r.fillRect(bx, by, bw, bh, scaled(side, al));
    I.r.fillRect(bx, by, bw, 1, scaled(mix(side, Color(255, 255, 255), 0.35f), al));
    drawGlyphCentered(I.r, bx + bw / 2, by + bh / 2, b.typeOf(u).glyph, scaled(readableOn(side), al), std::max(1, bw * 3 / 4));
    bodyTop = by;
  }
  if (u.leader) {
    I.r.fillRect(cx - uw / 2, bodyTop - 1, 5, 5, scaled(Color(0, 0, 0, 220), al));
    I.r.fillRect(cx - uw / 2 + 1, bodyTop, 3, 3, scaled(Color(255, 210, 40), al));
  }
  // HP bar under the feet, using the DISPLAYED hp so it drains with the hit.
  int mx = b.maxHp(u), barW = std::max(4, uw * 3 / 4), barX = cx - barW / 2, barY = foot + 1;
  I.r.fillRect(barX - 1, barY - 1, barW + 2, 4, scaled(Color(0, 0, 0, 200), al));
  int fill = mx > 0 ? std::max(v.hp > 0 ? 1 : 0, barW * std::min(v.hp, mx) / mx) : 0;
  I.r.fillRect(barX, barY, fill, 2, scaled(hpColor(v.hp, mx), al));
}

// The square a unit is drawn on right now (it may be between two while moving).
void unitSquare(const Iso& I, const UnitView& v, int& x, int& y) {
  const Grid& grid = I.c.battle.grid;
  int t = I.p.tile();
  x = std::max(0, std::min((int)std::lround(v.px / t), grid.width() - 1));
  y = std::max(0, std::min((int)std::lround(v.py / t), grid.height() - 1));
}

void drawCaptureIso(const Iso& I, const StructureState& st) {
  const Battle& b = I.c.battle;
  int pts = b.defOf(st).capturePoints;
  if (st.capturer < 0 || pts <= 0 || st.capture >= pts) return;
  const Unit& u = b.units[st.capturer];
  if (!u.alive || u.id >= (int)I.c.view.units.size() || !I.c.view.units[u.id].visible) return;
  int x = b.grid.x(u.cell), y = b.grid.y(u.cell), cx, cy;
  I.at(x + 0.5f, y + 0.5f, (float)I.p.elevation(x, y), cx, cy);
  std::string n = intStr(st.capture);
  int w = textW(I.r, n), tx = cx + I.p.unitW() / 2 - w / 2, ty = cy - I.p.unitW();
  I.r.fillRect(tx - 1, ty - 1, w + 2, I.c.layout.lh + 2, Color(0, 0, 0, 190));
  I.r.drawText(tx, ty, n.c_str(), Color(255, 240, 120));
}

}  // namespace

void drawMapIso(Renderer& r, const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor) {
  const Battle& b = c.battle;
  const Layout& L = c.layout;
  const MapProj& p = *c.proj;
  Iso I{r, c, p, p.geo(), c.cam.ix(), c.cam.iy()};
  r.fillRect(0, 0, L.vw, L.vh, c.style && c.style->hasBackground ? Color::rgb(c.style->background) : Color(10, 10, 16));
  int gw = b.grid.width(), gh = b.grid.height();
  int pulse = 200 + (int)(55 * std::sin(c.time * 6.0f));
  bool cursorOn = showCursor && b.grid.valid(cursor);

  // Structures and units, by the diagonal they are drawn after (structures
  // first on a tie). A structure goes after its nearest square; a unit on a
  // structure goes after that structure, so it is never hidden by it.
  struct Item { int key, kind, index; };  // kind 0 = structure, 1 = unit
  std::vector<Item> items;
  for (size_t i = 0; i < b.structures.size(); ++i) {
    const StructureState& st = b.structures[i];
    const StructureDef& def = b.defOf(st);
    items.push_back({st.x + def.width - 1 + st.y + def.height - 1, 0, (int)i});
  }
  for (const Unit& u : b.units) {
    if (u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
    const UnitView& v = c.view.units[u.id];
    int key = (int)std::ceil((v.px + v.py) / p.tile() - 0.001f);  // between two squares: after the nearer one
    int ux, uy;
    unitSquare(I, v, ux, uy);
    int si = b.structureAt[b.grid.at(ux, uy)];
    if (si >= 0) key = std::max(key, b.structures[si].x + b.defOf(b.structures[si]).width - 1 + b.structures[si].y +
                                         b.defOf(b.structures[si]).height - 1);
    items.push_back({std::min(key, gw + gh - 2), 1, u.id});
  }
  std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b2) {
    return a.key != b2.key ? a.key < b2.key : a.kind < b2.kind;
  });

  size_t next = 0;
  for (int d = 0; d <= gw + gh - 2; ++d) {
    for (int x = std::max(0, d - gh + 1); x <= std::min(gw - 1, d); ++x) {
      int y = d - x;
      Cell cell = b.grid.at(x, y);
      drawSquare(I, cell, x, y);
      overlayTint(I, ov, cell, x, y);
      if (cursorOn && cell == cursor) {
        tint(I, x, y, Color(255, 240, 120, (uint8_t)pulse), true);
      }
    }
    for (; next < items.size() && items[next].key <= d; ++next) {
      const Item& it = items[next];
      if (it.kind == 0) drawStructureIso(I, it.index, ov);
      else drawUnitIso(I, b.units[it.index], c.view.units[it.index], 1.0f);
    }
  }
  // Shown through: units standing behind raised ground, and the cursor.
  for (const Unit& u : b.units) {
    if (u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
    int ux, uy;
    unitSquare(I, c.view.units[u.id], ux, uy);
    if (p.geo().hidden(ux, uy, p.elevations())) drawUnitIso(I, u, c.view.units[u.id], 0.45f);
  }
  if (cursorOn && p.geo().hidden(b.grid.x(cursor), b.grid.y(cursor), p.elevations()))
    tint(I, b.grid.x(cursor), b.grid.y(cursor), Color(255, 240, 120, (uint8_t)(pulse * 2 / 5)), true);
  for (const StructureState& st : b.structures) drawCaptureIso(I, st);
}

}  // namespace ote
