// What stands on the 3D isometric map's blocks: pictures on the tops,
// structures, highlights, the cursor, units, and the 2D things on top.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "battle/carrier_marker.h"
#include "battle/map3d.h"
#include "battle/map3d_sizes.h"
#include "ui/text.h"

namespace ote {

// A structure picture's lower edge sits near the front of its footprint (see map3d_sizes.h): the footprint's size in
// squares (a group of seven hexagons is about 2.5 squares across), times kStructureAhead.
float Map3D::structurePictureAhead(const DrawCtx& c, const StructureDef& def) const {
  const float size = c.proj->hex() ? (def.rings > 0 ? 2.5f : 1.0f) : 0.5f * (float)(def.width + def.height);
  return kStructureAhead * size;
}

namespace {


Tint3D tintOf(Color c, float alpha = 1.0f) { return Tint3D(c.r, c.g, c.b, (uint8_t)(c.a * alpha + 0.5f)); }

Material3D flat(Blend3D blend = Blend3D::Opaque, bool depthTest = true) {
  Material3D m;
  m.lit = false;
  m.twoSided = true;
  m.blend = blend;
  m.depthTest = depthTest;
  return m;
}

// A unit's spot in squares (BattleView animates flat map pixels).
void unitSpot(const DrawCtx& c, const UnitView& v, float& gx, float& gy) {
  int t = c.proj->tile();
  gx = (v.px + v.ox) / t + 0.5f;
  gy = (v.py + v.oy) / t + 0.5f;
}

// Height in pixels of a unit's body as drawn (picture or placeholder block).
int unitBodyPx(const DrawCtx& c, const Unit& u, const UnitView& v) {
  int uw = c.proj->unitW();
  if (const Look* art = unitArt(c, u)) {
    SpriteSrc src = lookSrc(c, *art);
    return std::max(1, (int)(uw * v.scale) * src.h / std::max(1, src.w));
  }
  return std::max(2, (int)(uw * 3 / 4 * v.scale));
}

}  // namespace

// Elevation a structure stands at: the highest of its squares (hexagons).
int structureElev(const DrawCtx& c, const StructureState& st, const StructureDef& def) {
  int e = 0;
  if (c.proj->hex()) {
    std::vector<Cell> cells;
    hexStructureCells(c, st, def, cells);
    for (Cell cell : cells) e = std::max(e, c.proj->elevation(c.battle.grid.x(cell), c.battle.grid.y(cell)));
    return e;
  }
  for (int dy = 0; dy < def.height; ++dy)
    for (int dx = 0; dx < def.width; ++dx)
      if (c.battle.grid.at(st.x + dx, st.y + dy) != kNoCell) e = std::max(e, c.proj->elevation(st.x + dx, st.y + dy));
  return e;
}

// The terrain pictures, laid flat on the tops (north of the picture = grid
// north). Refilled every frame: animated and joining terrain change pieces.
void Map3D::drawTops(const DrawCtx& c) {
  if (c.proj->hex()) {
    drawTopsHex(c);
    return;
  }
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  for (int y = 0; y < g.gridH(); ++y)
    for (int x = 0; x < g.gridW(); ++x) {
      Cell cell = b.grid.at(x, y);
      const Look* art = terrainArt(c, b.cells[cell].terrain);
      if (!art) continue;
      SpriteSrc src = terrainSrc(c, *art, cell);
      Material3D m = flat();
      float uv[4];
      if (!picture(c, *art, src.x, src.y, src.w, src.h, m, uv)) continue;
      // A hair above the block so the picture, not the block, wins where both are.
      groundQuad(batch_.mesh(m), c, x, y, 1, 1, c.proj->elevation(x, y) * g.stepY() + 0.001f, 0, Tint3D(), uv);
    }
}

void Map3D::drawStructures(const DrawCtx& c) {
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  for (int index = 0; index < (int)b.structures.size(); ++index) {
    if (g.hex()) {
      drawStructureHex(c, index);
      continue;
    }
    const StructureState st = shownStructure(c, index);  // fog: kind and owner as the viewer last saw them
    const StructureDef& def = b.defOf(st);
    float Y = structureElev(c, st, def) * g.stepY();
    // A structure out of the viewer's sight is darker (the ground under it gets the veil).
    const int owner = st.owner;
    const bool dim = structureDimmed(c, index);
    const uint8_t k = dim ? (uint8_t)(255 * kFogDim) : 255;
    if (owner >= 0) {
      Color oc = Color::rgb(b.sides[(size_t)owner].color);
      frame(batch_.mesh(flat()), c, st.x, st.y, def.width, def.height, Y + 0.012f, 0.07f, tintOf(dim ? fogDim(oc) : oc));
    }
    const ModelAsset* asset = nullptr;
    if (const ModelLook* ml = structureModel(c, st, asset)) {
      drawStructureModel(c, index, *ml, *asset, Y);
      continue;
    }
    const Look* art = structureArt(c, st);
    Material3D m = flat(Blend3D::Cutout);
    float uv[4];
    SpriteSrc src;
    if (art) src = lookSrc(c, *art);
    if (art && picture(c, *art, src.x, src.y, src.w, src.h, m, uv)) {
      // A standing picture in the middle of the footprint, a little wider than a square's edge per square.
      float wPx = (def.width + def.height) * 0.5f * g.artPixels() * 1.15f;
      Spot feet = spot(c, st.x + def.width * 0.5f, st.y + def.height * 0.5f, 0);
      feet.Y = Y;
      upright(batch_.mesh(m), feet, wPx, wPx * src.h / std::max(1, src.w), structurePictureAhead(c, def), Tint3D(k, k, k, 255), uv);
    } else {
      // Placeholder: a low lit box in the structure's colour.
      Spot a = spot(c, st.x + 0.08f, st.y + 0.08f, 0), z = spot(c, st.x + def.width - 0.08f, st.y + def.height - 0.08f, 0);
      Color base = Color::rgb(structureRgb(c, st.type));
      Tint3D col = tintOf(dim ? fogDim(base) : base);
      Material3D lit;
      addBlock(batch_.mesh(lit), Vec3(a.X, Y, a.Z), Vec3(z.X, Y + kWall, z.Z), col, col);
    }
  }
}

void Map3D::drawHighlights(const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor) {
  if (c.proj->hex()) {
    drawHighlightsHex(c, ov, cursor, showCursor);
    return;
  }
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  // Top of what covers a square: its block, or the placeholder box of a structure on it.
  auto topOf = [&](Cell cell, int x, int y) {
    int si = b.structureAt[cell];
    const ModelAsset* asset = nullptr;
    if (si >= 0) {
      const StructureState st = shownStructure(c, si);
      if (!structureArt(c, st) && !structureModel(c, st, asset)) return structureElev(c, st, b.defOf(st)) * g.stepY() + kWall;
    }
    return c.proj->elevation(x, y) * g.stepY();
  };
  if (c.view.fogged()) {
    // Fog: a dark veil (a blended black quad in the same batch as the highlights) over the top and the lower side
    // faces of every square the viewer does not see. Nothing is rebuilt when the sight changes.
    MeshData& m = batch_.mesh(flat(Blend3D::Blend));
    const Tint3D veil(0, 0, 0, (uint8_t)kFogDimAlpha);
    const float eps = 0.004f;  // the side veils stand a hair outside the block
    auto lowOf = [&](int nx, int ny) {
      return (nx < 0 || ny < 0 || nx >= g.gridW() || ny >= g.gridH() ? -1 : c.proj->elevation(nx, ny)) * g.stepY();
    };
    for (int y = 0; y < g.gridH(); ++y)
      for (int x = 0; x < g.gridW(); ++x) {
        if (!c.view.dimmed(b.grid.at(x, y))) continue;
        const int e = c.proj->elevation(x, y);
        groundQuad(m, c, x, y, 1, 1, e * g.stepY() + 0.010f, 0, veil);
        Spot a = spot(c, (float)x, (float)y, (float)e), z = spot(c, (float)(x + 1), (float)(y + 1), (float)e);
        const float hi = a.Y;
        float lo;
        if ((lo = lowOf(x, y + 1)) < hi) addQuad(m, Vec3(a.X, lo, z.Z + eps), Vec3(z.X, lo, z.Z + eps), Vec3(z.X, hi, z.Z + eps), Vec3(a.X, hi, z.Z + eps), veil);
        if ((lo = lowOf(x, y - 1)) < hi) addQuad(m, Vec3(z.X, lo, a.Z - eps), Vec3(a.X, lo, a.Z - eps), Vec3(a.X, hi, a.Z - eps), Vec3(z.X, hi, a.Z - eps), veil);
        if ((lo = lowOf(x + 1, y)) < hi) addQuad(m, Vec3(z.X + eps, lo, z.Z), Vec3(z.X + eps, lo, a.Z), Vec3(z.X + eps, hi, a.Z), Vec3(z.X + eps, hi, z.Z), veil);
        if ((lo = lowOf(x - 1, y)) < hi) addQuad(m, Vec3(a.X - eps, lo, a.Z), Vec3(a.X - eps, lo, z.Z), Vec3(a.X - eps, hi, z.Z), Vec3(a.X - eps, hi, a.Z), veil);
      }
  }
  if (ov.any()) {
    MeshData& m = batch_.mesh(flat(Blend3D::Blend));
    for (int y = 0; y < g.gridH(); ++y)
      for (int x = 0; x < g.gridW(); ++x) {
        Cell cell = b.grid.at(x, y);
        if (ov.reach[cell]) groundQuad(m, c, x, y, 1, 1, topOf(cell, x, y) + 0.016f, 0.03f, Tint3D(50, 110, 255, 120));
        else if (ov.attack[cell]) groundQuad(m, c, x, y, 1, 1, topOf(cell, x, y) + 0.016f, 0.03f, Tint3D(255, 50, 50, 120));
      }
  }
  if (showCursor && b.grid.valid(cursor)) {
    int x = b.grid.x(cursor), y = b.grid.y(cursor);
    int pulse = 200 + (int)(55 * std::sin(c.time * 6.0f));
    float Y = topOf(cursor, x, y) + 0.024f;
    frame(batch_.mesh(flat(Blend3D::Blend)), c, x, y, 1, 1, Y, 0.09f, Tint3D(255, 240, 120, (uint8_t)pulse));
    // Behind higher blocks: again, faintly, over everything.
    if (hidden_[(size_t)cursor] || standAt(cursor) || standInFront(c, x, y))
      frame(batch_.mesh(flat(Blend3D::Blend, false)), c, x, y, 1, 1, Y, 0.09f, Tint3D(255, 240, 120, (uint8_t)(pulse * 2 / 5)));
  }
}

void Map3D::drawUnit(const DrawCtx& c, const Unit& u, const UnitView& v, float fade, bool through) {
  const Battle& b = c.battle;
  int uw = c.proj->unitW();
  float gx, gy;
  unitSpot(c, v, gx, gy);
  Spot feet = spot(c, gx, gy, c.proj->elevationAt(gx, gy));
  // On a structure that is a standing picture: stand just in front of that picture, never inside it.
  float towards = 0;
  Cell under = c.proj->hex() ? cellNearPos(b.grid, c.proj->tile(), v.px + v.ox, v.py + v.oy)
                             : b.grid.at(std::max(0, std::min((int)gx, b.grid.width() - 1)), std::max(0, std::min((int)gy, b.grid.height() - 1)));
  int si = under == kNoCell ? -1 : b.structureAt[under];
  if (si >= 0) {
    const StructureState st = shownStructure(c, si);
    const StructureDef& def = b.defOf(st);
    const Look* sa = structureArt(c, st);
    const ModelAsset* sm = nullptr;
    if (structureModel(c, st, sm)) {
      towards = 0.45f * std::max(def.width, def.height);  // in front of the building, not inside it
    } else if (sa && sheets_.texture(r3d_, sa->sheet)) {
      float mgx, mgy;
      if (c.proj->hex()) c.proj->cellMid(st.x, st.y, mgx, mgy);
      else { mgx = st.x + def.width * 0.5f; mgy = st.y + def.height * 0.5f; }
      Spot mid = spot(c, mgx, mgy, 0);
      float ahead = (feet.X - mid.X) * cx_ + (feet.Z - mid.Z) * cz_;  // how far in front of the picture's plane
      const float plane = structurePictureAhead(c, def);  // where the picture stands, in front of the middle
      if (ahead < plane + 0.06f) towards = plane + 0.06f - ahead;
    }
  }
  towards = std::max(towards, standAhead(c, under));  // in front of a standing terrain model (the 2D overlays follow: drawOnTop)
  float al = v.alpha * fade;
  // Solid things are cut out against the depth picture; fading ones are blended in afterwards.
  Blend3D solid = al < 0.999f ? Blend3D::Blend : Blend3D::Opaque;
  Color side = Color::rgb(b.sides[(size_t)shownSide(c, u)].color);
  if (u.done) side = mix(side, Color(70, 70, 78), 0.55f);  // dimmed: already acted
  const ModelAsset* asset = nullptr;
  const ModelLook* model = unitModel(c, u, asset);
  const Look* art = model ? nullptr : unitArt(c, u);
  Material3D pm = flat(al < 0.999f ? Blend3D::Blend : Blend3D::Cutout, !through);
  float uv[4];
  SpriteSrc src;
  if (art) src = lookSrc(c, *art);
  if (art && picture(c, *art, src.x, src.y, src.w, src.h, pm, uv)) {
    if (b.db->graphics.unitMarker && c.proj->hex()) {
      // A flat hexagon in the side's colour under the feet, dark rim first.
      float half = std::max(0.12f, 0.3f * v.scale);
      MeshData& m = batch_.mesh(flat(solid, !through));
      hexMarker(m, c, feet, half + 0.04f, 0.018f, Tint3D(20, 20, 28, (uint8_t)(200 * al)));
      hexMarker(m, c, feet, half, 0.02f, tintOf(side, al));
    } else if (b.db->graphics.unitMarker) {
      // A flat square in the side's colour under the feet, dark rim first.
      float half = std::max(0.12f, 0.3f * v.scale);
      MeshData& m = batch_.mesh(flat(solid, !through));
      addQuad(m, Vec3(feet.X - half - 0.04f, feet.Y + 0.018f, feet.Z + half + 0.04f), Vec3(feet.X + half + 0.04f, feet.Y + 0.018f, feet.Z + half + 0.04f),
              Vec3(feet.X + half + 0.04f, feet.Y + 0.018f, feet.Z - half - 0.04f), Vec3(feet.X - half - 0.04f, feet.Y + 0.018f, feet.Z - half - 0.04f),
              Tint3D(20, 20, 28, (uint8_t)(200 * al)));
      addQuad(m, Vec3(feet.X - half, feet.Y + 0.02f, feet.Z + half), Vec3(feet.X + half, feet.Y + 0.02f, feet.Z + half),
              Vec3(feet.X + half, feet.Y + 0.02f, feet.Z - half), Vec3(feet.X - half, feet.Y + 0.02f, feet.Z - half), tintOf(side, al));
    }
    float w = std::max(1.0f, uw * v.scale);
    uint8_t k = (uint8_t)(u.done ? 140 : 255);
    upright(batch_.mesh(pm), feet, w, w * src.h / std::max(1, src.w), towards, Tint3D(k, k, k, (uint8_t)(255 * al)), uv);
  } else if (model) {
    if (b.db->graphics.unitMarker && !through && c.proj->hex()) {
      hexMarker(batch_.mesh(flat(solid)), c, feet, std::max(0.12f, 0.3f * v.scale), 0.02f, tintOf(side, al));
    } else if (b.db->graphics.unitMarker && !through) {
      float half = std::max(0.12f, 0.3f * v.scale);
      addQuad(batch_.mesh(flat(solid)), Vec3(feet.X - half, feet.Y + 0.02f, feet.Z + half), Vec3(feet.X + half, feet.Y + 0.02f, feet.Z + half),
              Vec3(feet.X + half, feet.Y + 0.02f, feet.Z - half), Vec3(feet.X - half, feet.Y + 0.02f, feet.Z - half), tintOf(side, al));
    }
    drawUnitModel(c, u, v, *model, *asset, feet, towards, fade, through);
  } else {
    // Placeholder: an upright block in the side's colour (its letter is drawn on top in 2D).
    float bw = std::max(2.0f, uw * 0.75f * v.scale);
    MeshData& m = batch_.mesh(flat(solid, !through));
    upright(m, feet, bw + 2, bw + 1, towards, Tint3D(0, 0, 0, (uint8_t)(200 * al)));
    upright(m, feet, bw, bw, towards + 0.02f, tintOf(side, al));
  }
}

// With the 2D renderer, over the finished 3D picture: letters of things
// without art, leader marks, HP bars and capture numbers.
void Map3D::drawOnTop(Renderer& r, const DrawCtx& c) {
  const Battle& b = c.battle;
  const MapProj& p = *c.proj;
  const MapProj& g = p;
  const Layout& L = c.layout;
  const StyleDef* style = c.style;
  bool letters = !(style && style->showGlyphs == 0);
  // Where a point is on screen; false = not in the picture. `k`: how large
  // things are drawn there compared with their art size (1 without perspective).
  auto screen = [&](float gx, float gy, float Y, int& sx, int& sy, float& k) {
    float X = gx - g.gridW() * 0.5f, Z = gy - g.gridH() * 0.5f, wx, wy;
    if (!g.project3(X, Y, Z, wx, wy)) return false;
    k = g.pixelsAt(X, Y, Z) / g.artPixels();
    sx = (int)std::lround(wx) - c.cam.ix();
    sy = (int)std::lround(wy) - c.cam.iy();
    return !(sx < -40 || sy < -40 || sx > L.vw + 40 || sy > L.vh + 40);
  };
  auto glyphPx = [&](float k) { return std::max(4, (int)(g.artPixels() * k * g.flatSquash() * 0.64f)); };
  float k = 1;

  if (letters) {
    for (int y = 0; y < g.gridH(); ++y)
      for (int x = 0; x < g.gridW(); ++x) {
        Cell cell = b.grid.at(x, y);
        if (hidden_[(size_t)cell] || b.structureAt[cell] >= 0) continue;
        const TerrainDef& td = b.terrainAt(cell);
        const Look* art = terrainArt(c, b.cells[cell].terrain);
        if (td.glyph == ' ' || (art && sheets_.texture(r3d_, art->sheet))) continue;
        int sx, sy;
        float tgx, tgy;
        p.cellMid(x, y, tgx, tgy);
        if (!screen(tgx, tgy, p.elevation(x, y) * g.stepY(), sx, sy, k)) continue;
        Color col = Color::rgb(terrainRgb(c, b.cells[cell].terrain));
        Color gc = mix(col, readableOn(col), 0.28f);
        drawGlyphCentered(r, sx, sy, td.glyph, c.view.dimmed(cell) ? fogDim(gc) : gc, glyphPx(k));
      }
    for (int index = 0; index < (int)b.structures.size(); ++index) {
      const StructureState st = shownStructure(c, index);
      const StructureDef& def = b.defOf(st);
      const Look* art = structureArt(c, st);
      const ModelAsset* asset = nullptr;
      if (def.glyph == ' ' || (art && sheets_.texture(r3d_, art->sheet)) || structureModel(c, st, asset)) continue;
      int sx, sy;
      float sgx = st.x + def.width * 0.5f, sgy = st.y + def.height * 0.5f;
      if (p.hex()) p.cellMid(st.x, st.y, sgx, sgy);
      if (!screen(sgx, sgy, structureElev(c, st, def) * g.stepY() + kWall, sx, sy, k)) continue;
      Color gc = readableOn(Color::rgb(structureRgb(c, st.type)));
      drawGlyphCentered(r, sx, sy, def.glyph, structureDimmed(c, index) ? fogDim(gc) : gc,
                        glyphPx(k) * std::min(def.width, def.height));
    }
  }

  // An upright thing's height on screen, from its height in art pixels.
  float shown = g.uprightShown();
  for (const Unit& u : b.units) {
    if (u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
    const UnitView& v = c.view.units[u.id];
    float gx, gy;
    unitSpot(c, v, gx, gy);
    // A unit in front of a standing terrain model is drawn moved towards the camera (drawUnit); its letter and bar follow.
    const float ahead = standAhead(c, p.hex() ? cellNearPos(b.grid, p.tile(), v.px + v.ox, v.py + v.oy)
                                              : b.grid.at(std::max(0, std::min((int)gx, b.grid.width() - 1)), std::max(0, std::min((int)gy, b.grid.height() - 1))));
    int sx, sy;
    if (!screen(gx + cx_ * ahead, gy + cz_ * ahead, p.elevationAt(gx, gy) * g.stepY(), sx, sy, k)) continue;
    int uw = std::max(2, (int)std::lround(p.unitW() * k));
    float al = v.alpha;
    auto a = [&](Color col) { return withAlpha(col, (int)(col.a * al)); };
    int body = std::max(1, (int)std::lround(unitBodyPx(c, u, v) * k * shown));
    const Look* art = unitArt(c, u);
    const ModelAsset* asset = nullptr;
    const ModelLook* model = unitModel(c, u, asset);
    if (model) body = (int)((model->height > 0 ? model->height : kUnitModelHeight) * v.scale * g.artPixels() * k * g.uprightSquash() * shown);
    if (!model && !(art && sheets_.texture(r3d_, art->sheet))) {
      Color side = Color::rgb(b.sides[(size_t)shownSide(c, u)].color);
      if (u.done) side = mix(side, Color(70, 70, 78), 0.55f);
      drawGlyphCentered(r, sx, sy - body / 2, b.db->unitTypes[(size_t)shownType(c, u)].glyph, a(readableOn(side)), std::max(1, body * 3 / 4));
    }
    if (u.leader) {
      r.fillRect(sx - uw / 2, sy - body - 1, 5, 5, a(Color(0, 0, 0, 220)));
      r.fillRect(sx - uw / 2 + 1, sy - body, 3, 3, a(Color(255, 210, 40)));
    }
    drawCarrierMarker(r, c, u, sx + uw / 2, sy - body, uw, al);  // top right corner: a unit with someone off the board
    // HP bar under the feet, using the DISPLAYED hp so it drains with the hit.
    int mx = b.maxHp(u), barW = std::max(4, uw * 3 / 4), barX = sx - barW / 2, barY = sy + 2;
    r.fillRect(barX - 1, barY - 1, barW + 2, 4, a(Color(0, 0, 0, 200)));
    int fill = mx > 0 ? std::max(v.hp > 0 ? 1 : 0, barW * std::min(v.hp, mx) / mx) : 0;
    r.fillRect(barX, barY, fill, 2, a(hpColor(v.hp, mx)));
  }

  for (const StructureState& st : b.structures) {
    int pts = b.defOf(st).capturePoints;
    if (st.capturer < 0 || pts <= 0 || st.capture >= pts) continue;
    const Unit& u = b.units[st.capturer];
    if (!u.alive || u.id >= (int)c.view.units.size() || !c.view.units[u.id].visible) continue;
    int x = b.grid.x(u.cell), y = b.grid.y(u.cell), sx, sy;
    float cgx, cgy;
    p.cellMid(x, y, cgx, cgy);
    if (!screen(cgx, cgy, p.elevation(x, y) * g.stepY(), sx, sy, k)) continue;
    int uw = std::max(2, (int)std::lround(p.unitW() * k));
    std::string n = intStr(st.capture);
    int w = textW(r, n), tx = sx + uw / 2 - w / 2, ty = sy - uw - L.lh;
    r.fillRect(tx - 1, ty - 1, w + 2, L.lh + 2, Color(0, 0, 0, 190));
    r.drawText(tx, ty, n.c_str(), Color(255, 240, 120));
  }
}

}  // namespace ote
