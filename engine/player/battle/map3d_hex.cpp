// Hexagon maps in the 3D kinds of view (docs/HEX.md "3D isometric and full 3D", "Implementation notes (3D)").
// The ground is six-sided blocks: a block's top is the terrain's top-down hexagon picture (the sheet cell mapped
// onto the hexagon, join pieces and variants as in the top-down view), its sides the darkened terrain colour.
// Everything here is the hexagon twin of what map3d.cpp / map3d_things.cpp do for squares; positions are in the
// 3D space of core/view/hex3d.h (squares = top-down pixels over tileSize), corners come from HexGeo::corners
// (clockwise seen from above; edge i faces direction i of the neighbour order).
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "battle/hex_draw.h"
#include "battle/map3d.h"
#include "battle/map3d_sizes.h"
#include "ui/text.h"

namespace ote {

namespace {

Tint3D tintOf(Color c, float alpha = 1.0f) { return Tint3D(c.r, c.g, c.b, (uint8_t)(c.a * alpha + 0.5f)); }

Material3D flatMat(Blend3D blend = Blend3D::Opaque, bool depthTest = true) {
  Material3D m;
  m.lit = false;
  m.twoSided = true;
  m.blend = blend;
  m.depthTest = depthTest;
  return m;
}

}  // namespace

// The hexagons a structure covers: its middle one first, then the ring when its kind has one.
void hexStructureCells(const DrawCtx& c, const StructureState& st, const StructureDef& def, std::vector<Cell>& out) {
  out.clear();
  const Grid& g = c.battle.grid;
  const Cell mid = g.at(st.x, st.y);
  if (mid == kNoCell) return;
  out.push_back(mid);
  if (def.rings > 0)
    for (int d = 0; d < g.directionCount(); ++d)
      if (Cell n = g.neighbor(mid, d); n != kNoCell) out.push_back(n);
}

// The six corners of a hexagon's top at height Y, pulled towards (scale < 1) or pushed from its middle.
void Map3D::hexCorners(const DrawCtx& c, int x, int y, float Y, float scale, Vec3 (&p)[6]) const {
  const HexSpace& hs = c.proj->hexSpace();
  float mx, mz;
  hs.middle(x, y, mx, mz);
  for (int i = 0; i < 6; ++i) {
    float X, Z;
    hs.corner(x, y, i, X, Z);
    p[i] = Vec3(mx + (X - mx) * scale, Y, mz + (Z - mz) * scale);
  }
}

// A filled hexagon lying at height Y. With `uvRect` (u0, v0, u1, v1: a sheet cell the size of the hexagon's box) the
// cell is laid over the box, so a top-down picture lands on the hexagon exactly as it does in the top-down view.
void Map3D::hexFill(MeshData& m, const DrawCtx& c, int x, int y, float Y, float scale, Tint3D tint, const float* uvRect) {
  Vec3 p[6];
  hexCorners(c, x, y, Y, scale, p);
  Vec3 ring[6];
  float uv[12];
  const HexSpace& hs = c.proj->hexSpace();
  int bx, by;
  hs.geo.box(x, y, bx, by);
  float X0, Z0;
  hs.toSpace((float)bx, (float)by, X0, Z0);
  const float T = (float)hs.geo.T, bw = hs.geo.boxW() / T, bh = hs.geo.boxH() / T;
  for (int i = 0; i < 6; ++i) {  // counter-clockwise seen from above = the corners backwards
    ring[i] = p[5 - i];
    if (uvRect) {
      uv[2 * i] = uvRect[0] + (ring[i].x - X0) / bw * (uvRect[2] - uvRect[0]);
      uv[2 * i + 1] = uvRect[1] + (ring[i].z - Z0) / bh * (uvRect[3] - uvRect[1]);
    }
  }
  addFan(m, ring, 6, tint, uvRect ? uv : nullptr);
}

// The ring of a hexagon's top `thick` squares wide (inside the hexagon), only along the edges whose bit is set.
void Map3D::hexBand(MeshData& m, const DrawCtx& c, int x, int y, float Y, float thick, Tint3D tint, unsigned edges) {
  Vec3 o[6], in[6];
  hexCorners(c, x, y, Y, 1.0f, o);
  hexCorners(c, x, y, Y, std::max(0.1f, 1.0f - 2.0f * thick), in);  // the hexagon's inradius is half a square
  for (int i = 0; i < 6; ++i)
    if ((edges >> i) & 1u) addQuad(m, o[(i + 1) % 6], o[i], in[i], in[(i + 1) % 6], tint);
}

// The side face under edge `dir` from height `lo` up to `hi`, pushed `eps` squares outwards (veils stand a hair
// outside the block), shrunk about the hexagon's middle by `scale` (placeholder structures).
void Map3D::hexSide(MeshData& m, const DrawCtx& c, int x, int y, int dir, float lo, float hi, float scale, float eps, Tint3D tint) {
  Vec3 p[6];
  hexCorners(c, x, y, 0, scale, p);
  const Vec3 a = p[dir], b = p[(dir + 1) % 6];
  float ox = (a.x + b.x) * 0.5f, oz = (a.z + b.z) * 0.5f;
  float mx, mz;
  c.proj->hexSpace().middle(x, y, mx, mz);
  ox -= mx;
  oz -= mz;
  const float len = std::max(1e-6f, std::sqrt(ox * ox + oz * oz));
  ox = ox / len * eps;
  oz = oz / len * eps;
  // Counter-clockwise seen from outside: from the edge's second corner to its first.
  addQuad(m, Vec3(b.x + ox, lo, b.z + oz), Vec3(a.x + ox, lo, a.z + oz), Vec3(a.x + ox, hi, a.z + oz), Vec3(b.x + ox, hi, b.z + oz), tint);
}

// The blocks: the side faces that show (down to the neighbour's top; the map's rim shows one level below the lowest
// ground) and the tops of hexagons without a usable picture.
void Map3D::buildGroundHex(const DrawCtx& c) {
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  MeshData m;
  for (int y = 0; y < g.gridH(); ++y)
    for (int x = 0; x < g.gridW(); ++x) {
      const Cell cell = b.grid.at(x, y);
      const float hi = g.elevation(x, y) * g.stepY();
      const Color col = Color::rgb(terrainRgb(c, b.cells[cell].terrain));
      const Tint3D side = tintOf(mix(col, Color(40, 30, 24), 0.35f));
      const Look* art = terrainArt(c, b.cells[cell].terrain);
      if (!art || !sheets_.texture(r3d_, art->sheet)) hexFill(m, c, x, y, hi, 1.0f, tintOf(col));
      for (int d = 0; d < 6; ++d) {
        const Cell n = b.grid.neighbor(cell, d);
        const float lo = (n == kNoCell ? -1 : g.elevation(b.grid.x(n), b.grid.y(n))) * g.stepY();
        if (lo < hi) hexSide(m, c, x, y, d, lo, hi, 1.0f, 0, side);
      }
    }
  ground_ = r3d_.createMesh(m);
}

// The terrain pictures laid on the tops (refilled every frame: animated and joining terrain change pieces).
void Map3D::drawTopsHex(const DrawCtx& c) {
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  for (int y = 0; y < g.gridH(); ++y)
    for (int x = 0; x < g.gridW(); ++x) {
      const Cell cell = b.grid.at(x, y);
      const Look* art = terrainArt(c, b.cells[cell].terrain);
      if (!art) continue;
      SpriteSrc src = terrainSrc(c, *art, cell);
      Material3D m = flatMat();
      float uv[4];
      if (!picture(c, *art, src.x, src.y, src.w, src.h, m, uv)) continue;
      hexFill(batch_.mesh(m), c, x, y, g.elevation(x, y) * g.stepY() + 0.001f, 1.0f, Tint3D(), uv);  // a hair above the block
    }
}

// Fog veils, move / attack range and the cursor, all lying on the blocks' tops as hexagons.
void Map3D::drawHighlightsHex(const DrawCtx& c, const Overlay& ov, Cell cursor, bool showCursor) {
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  // Top of what covers a hexagon: its block, or the placeholder blocks of a structure on it.
  auto topOf = [&](Cell cell, int x, int y) {
    int si = b.structureAt[cell];
    const ModelAsset* asset = nullptr;
    if (si >= 0) {
      const StructureState st = shownStructure(c, si);
      if (!structureArt(c, st) && !structureModel(c, st, asset)) return structureElev(c, st, b.defOf(st)) * g.stepY() + kWall;
    }
    return g.elevation(x, y) * g.stepY();
  };
  if (c.view.fogged()) {
    MeshData& m = batch_.mesh(flatMat(Blend3D::Blend));
    const Tint3D veil(0, 0, 0, (uint8_t)kFogDimAlpha);
    for (int y = 0; y < g.gridH(); ++y)
      for (int x = 0; x < g.gridW(); ++x) {
        const Cell cell = b.grid.at(x, y);
        if (!c.view.dimmed(cell)) continue;
        const float hi = g.elevation(x, y) * g.stepY();
        hexFill(m, c, x, y, hi + 0.010f, 1.0f, veil);
        for (int d = 0; d < 6; ++d) {
          const Cell n = b.grid.neighbor(cell, d);
          const float lo = (n == kNoCell ? -1 : g.elevation(b.grid.x(n), b.grid.y(n))) * g.stepY();
          if (lo < hi) hexSide(m, c, x, y, d, lo, hi, 1.0f, 0.004f, veil);
        }
      }
  }
  if (ov.any()) {
    MeshData& m = batch_.mesh(flatMat(Blend3D::Blend));
    for (int y = 0; y < g.gridH(); ++y)
      for (int x = 0; x < g.gridW(); ++x) {
        const Cell cell = b.grid.at(x, y);
        if (ov.reach[cell]) hexFill(m, c, x, y, topOf(cell, x, y) + 0.016f, 1.0f, Tint3D(50, 110, 255, 120));
        else if (ov.attack[cell]) hexFill(m, c, x, y, topOf(cell, x, y) + 0.016f, 1.0f, Tint3D(255, 50, 50, 120));
      }
  }
  if (showCursor && b.grid.valid(cursor)) {
    const int x = b.grid.x(cursor), y = b.grid.y(cursor);
    const int pulse = 200 + (int)(55 * std::sin(c.time * 6.0f));
    const float Y = topOf(cursor, x, y) + 0.024f;
    hexBand(batch_.mesh(flatMat(Blend3D::Blend)), c, x, y, Y, 0.09f, Tint3D(255, 240, 120, (uint8_t)pulse));
    if (hidden_[(size_t)cursor] || standAt(cursor) || standInFront(c, x, y))  // behind higher blocks: again, faintly
      hexBand(batch_.mesh(flatMat(Blend3D::Blend, false)), c, x, y, Y, 0.09f, Tint3D(255, 240, 120, (uint8_t)(pulse * 2 / 5)));
  }
}

// One structure on hexagons: owner frame round the covered hexagons, then a model, a standing picture or placeholder
// blocks (one low prism per covered hexagon, in the structure's colour).
void Map3D::drawStructureHex(const DrawCtx& c, int index) {
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  const StructureState st = shownStructure(c, index);  // fog: kind and owner as the viewer last saw them
  const StructureDef& def = b.defOf(st);
  std::vector<Cell> cells;
  hexStructureCells(c, st, def, cells);
  if (cells.empty()) return;
  const float Y = structureElev(c, st, def) * g.stepY();
  const bool dim = structureDimmed(c, index);
  const uint8_t k = dim ? (uint8_t)(255 * kFogDim) : 255;
  // The owner frame lies on the ground round the covered hexagons; a placeholder group of seven fills its hexagons, so
  // there the frame goes on the blocks' tops (a lone block is pulled in from the edges and leaves the ground band free).
  const ModelAsset* asset = nullptr;
  const bool placeholder = !structureModel(c, st, asset) && !structureArt(c, st);
  if (st.owner >= 0) {
    Color oc = Color::rgb(b.sides[(size_t)st.owner].color);
    MeshData& m = batch_.mesh(flatMat());
    const float frameY = Y + 0.012f + (placeholder && def.rings > 0 ? kWall : 0.0f);
    for (Cell cell : cells)
      hexBand(m, c, b.grid.x(cell), b.grid.y(cell), frameY, 0.07f, tintOf(dim ? fogDim(oc) : oc), outerEdges(b.grid, cell, cells));
  }
  if (const ModelLook* ml = structureModel(c, st, asset)) {
    drawStructureModel(c, index, *ml, *asset, Y);
    return;
  }
  const Look* art = structureArt(c, st);
  Material3D m = flatMat(Blend3D::Cutout);
  float uv[4];
  SpriteSrc src;
  if (art) src = lookSrc(c, *art);
  if (art && picture(c, *art, src.x, src.y, src.w, src.h, m, uv)) {
    // Standing on the middle hexagon, as wide as the picture's box in the top-down view (a little wider for one hexagon).
    int bx, by, bw, bh;
    g.hexGeo().structureBox(st.x, st.y, def.rings, bx, by, bw, bh);
    const float wPx = (float)bw / (float)g.tile() * g.artPixels() * (def.rings > 0 ? 1.0f : 1.15f);
    float mx, mz;
    g.hexSpace().middle(st.x, st.y, mx, mz);
    Spot feet{mx, Y, mz};
    upright(batch_.mesh(m), feet, wPx, wPx * src.h / std::max(1, src.w), structurePictureAhead(c, def), Tint3D(k, k, k, 255), uv);
    return;
  }
  Color base = Color::rgb(structureRgb(c, st.type));
  const Tint3D col = tintOf(dim ? fogDim(base) : base);
  MeshData& lit = batch_.mesh(Material3D());
  const float scale = def.rings > 0 ? 1.0f : 0.84f;  // a lone block leaves a margin (0.08 squares, as on squares), a group is one piece
  for (Cell cell : cells) {
    const int x = b.grid.x(cell), y = b.grid.y(cell);
    hexFill(lit, c, x, y, Y + kWall, scale, col);
    const unsigned outer = outerEdges(b.grid, cell, cells);
    for (int d = 0; d < 6; ++d)
      if ((outer >> d) & 1u) hexSide(lit, c, x, y, d, Y, Y + kWall, scale, 0, col);
  }
}

// How far a unit's marker hexagon reaches from its middle, as the offsets of the corners of any hexagon of the map
// scaled by `half` (the square marker's half edge) * 2.
void Map3D::hexMarker(MeshData& m, const DrawCtx& c, const Spot& feet, float half, float lift, Tint3D tint) const {
  Vec3 p[6];
  hexCorners(c, 0, 0, 0, 1.0f, p);
  float mx, mz;
  c.proj->hexSpace().middle(0, 0, mx, mz);
  Vec3 ring[6];
  for (int i = 0; i < 6; ++i) {  // counter-clockwise from above, centred on the feet
    const Vec3& q = p[5 - i];
    ring[i] = Vec3(feet.X + (q.x - mx) * 2.0f * half, feet.Y + lift, feet.Z + (q.z - mz) * 2.0f * half);
  }
  addFan(m, ring, 6, tint);
}

}  // namespace ote
