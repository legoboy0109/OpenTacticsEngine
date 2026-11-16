#include "battle/sprites.h"

#include <algorithm>

#include "battle/battle_draw.h"
#include "core/base/log.h"
#include "platform/common/image.h"
#include "ui/text.h"

namespace ote {

void Sprites::load(Renderer& r, const std::string& gameDir, const Graphics& g) {
  tex_.assign(g.sheets.size(), 0);
  for (size_t i = 0; i < g.sheets.size(); ++i) {
    const SheetDef& s = g.sheets[i];
    Image img;
    std::string err;
    if (!loadPngFile(gameDir + "/" + s.file, img, err)) {
      OTE_ERROR("art sheet \"%s\" (%s): %s; what uses it keeps the placeholder look", s.id.c_str(), s.file.c_str(),
                err.c_str());
      continue;
    }
    if (img.width < s.cellW || img.height < s.cellH) {
      OTE_ERROR("art sheet \"%s\" (%s): the image is %dx%d, smaller than one %dx%d cell; "
                "what uses it keeps the placeholder look",
                s.id.c_str(), s.file.c_str(), img.width, img.height, s.cellW, s.cellH);
      continue;
    }
    tex_[i] = r.createTexture(img.rgba.data(), img.width, img.height);  // logs its own failure
  }
}

void Sprites::release(Renderer& r) {
  for (TextureId t : tex_)
    if (t) r.destroyTexture(t);
  tex_.clear();
}

namespace {

uint32_t timeMs(const DrawCtx& c) { return (uint32_t)(c.time * 1000.0f); }

void blit(Renderer& r, const DrawCtx& c, const Look& look, SpriteCell cell, int dx, int dy, int dw, int dh,
          Color mod) {
  const SheetDef& s = c.battle.db->graphics.sheets[(size_t)look.sheet];
  r.drawTexture(c.sprites->texture(look.sheet), cell.col * s.cellW, cell.row * s.cellH, s.cellW, s.cellH, dx, dy,
                dw, dh, mod);
}

// Scales every channel of `c` by `k` (0..1), keeping alpha.
Color darker(Color c, float k) { return Color((uint8_t)(c.r * k), (uint8_t)(c.g * k), (uint8_t)(c.b * k), c.a); }

}  // namespace

const Look* terrainArt(const DrawCtx& c, int terrain) {
  if (!c.sprites) return nullptr;
  const GraphicsSet* set = c.battle.db->graphics.set(c.styleIndex);
  if (!set || terrain < 0 || (size_t)terrain >= set->terrain.size()) return nullptr;
  const Look& look = set->terrain[(size_t)terrain];
  return c.sprites->has(look) ? &look : nullptr;
}

const Look* unitArt(const DrawCtx& c, const Unit& u) {
  if (!c.sprites) return nullptr;
  const GraphicsSet* set = c.battle.db->graphics.set(c.styleIndex);
  int side = shownSide(c, u);
  const int type = shownType(c, u);
  if (!set || type < 0 || (size_t)type >= set->units.size() || side < 0 || (size_t)side >= c.battle.sides.size())
    return nullptr;
  const Look& look = set->units[(size_t)type].forSide(c.battle.sides[(size_t)side].id);
  return c.sprites->has(look) ? &look : nullptr;
}

const Look* structureArt(const DrawCtx& c, const StructureState& s) {
  if (!c.sprites) return nullptr;
  const GraphicsSet* set = c.battle.db->graphics.set(c.styleIndex);
  if (!set || s.type < 0 || (size_t)s.type >= set->structures.size()) return nullptr;
  const UnitLook& ul = set->structures[(size_t)s.type];
  const Look& look = s.owner >= 0 && (size_t)s.owner < c.battle.sides.size()
                         ? ul.forSide(c.battle.sides[(size_t)s.owner].id)
                         : ul.base;
  return c.sprites->has(look) ? &look : nullptr;
}

namespace {
SpriteSrc srcOf(const DrawCtx& c, const Look& look, SpriteCell cell) {
  const SheetDef& s = c.battle.db->graphics.sheets[(size_t)look.sheet];
  SpriteSrc out;
  out.tex = c.sprites->texture(look.sheet);
  out.x = cell.col * s.cellW;
  out.y = cell.row * s.cellH;
  out.w = s.cellW;
  out.h = s.cellH;
  return out;
}
int joinMaskOf(const DrawCtx& c, const Look& look, int x, int y) {
  if (look.kind != Look::Join) return 0;
  const Grid& g = c.battle.grid;
  // Hexagons: one bit per neighbour in the grid's own direction order (docs/HEX.md "Join pieces on hexagons").
  if (g.isHex())
    return joinMaskHex(look, g, g.at(x, y), [&](Cell nb) { return (int)c.battle.cells[(size_t)nb].terrain; });
  return joinMaskAt(look, x, y, g.width(), g.height(),
                    [&](int nx, int ny) { return (int)c.battle.cells[(size_t)g.at(nx, ny)].terrain; });
}
}  // namespace

SpriteSrc terrainSrc(const DrawCtx& c, const Look& look, Cell cell) {
  int x = c.battle.grid.x(cell), y = c.battle.grid.y(cell);
  return srcOf(c, look, pickCell(look, x, y, joinMaskOf(c, look, x, y), timeMs(c), joinBlockSide(c.battle.grid)));
}

SpriteSrc lookSrc(const DrawCtx& c, const Look& look) { return srcOf(c, look, pickCell(look, 0, 0, 0, timeMs(c))); }

void drawStructureArt(Renderer& r, const DrawCtx& c, const Look& look, int sx, int sy, int fw, int fh) {
  const SheetDef& sh = c.battle.db->graphics.sheets[(size_t)look.sheet];
  int dh = std::max(1, fw * sh.cellH / sh.cellW);
  blit(r, c, look, pickCell(look, 0, 0, 0, timeMs(c)), sx, sy + fh - dh, fw, dh, Color(255, 255, 255));
}

void drawTerrainArt(Renderer& r, const DrawCtx& c, const Look& look, Cell cell, int dx, int dy, int dw, int dh) {
  const Grid& g = c.battle.grid;
  int x = g.x(cell), y = g.y(cell);
  blit(r, c, look, pickCell(look, x, y, joinMaskOf(c, look, x, y), timeMs(c), joinBlockSide(g)), dx, dy, dw, dh,
       Color(255, 255, 255));
}

void drawUnitArt(Renderer& r, const DrawCtx& c, const Look& look, const Unit& u, int sx, int sy, float scale,
                 float alpha, bool done) {
  const int t = c.layout.tile;
  const SheetDef& sh = c.battle.db->graphics.sheets[(size_t)look.sheet];
  int a = std::max(0, std::min(255, (int)(alpha * 255.0f + 0.5f)));
  float k = done ? 0.55f : 1.0f;  // already acted: colour multiply
  // Width = one square, height keeps the cell's proportions, standing on the bottom edge.
  int w = t, h = std::max(1, t * sh.cellH / sh.cellW);
  int x = sx, y = sy + t - h;
  if (scale < 1) {  // pop-in: grow from the middle of the sprite
    int nw = std::max(1, (int)(w * scale)), nh = std::max(1, (int)(h * scale));
    x += (w - nw) / 2;
    y += (h - nh) / 2;
    w = nw;
    h = nh;
  }
  if (c.battle.db->graphics.unitMarker) {
    // Flat base in the side's colour: bar across the bottom of the square, about
    // a sixth high, inset one pixel, one-pixel dark outline.
    int fill = std::max(2, t / 6);
    int bw = std::max(3, t - 2), bh = fill + 2;
    int bx = sx + (t - bw) / 2, by = sy + t - 1 - bh;
    if (scale < 1) {
      bw = std::max(3, (int)(bw * scale));
      bx = sx + (t - bw) / 2;
      by = sy + t - 1 - bh;
    }
    Color side = Color::rgb(c.battle.sides[(size_t)shownSide(c, u)].color);
    r.fillRect(bx, by, bw, bh, Color(20, 20, 28, (uint8_t)(a * 200 / 255)));
    r.fillRect(bx + 1, by + 1, bw - 2, bh - 2, withAlpha(darker(side, k), a));
  }
  Color mod = darker(Color(255, 255, 255), k);
  mod.a = (uint8_t)a;
  SpriteCell cell = pickCell(look, 0, 0, 0, timeMs(c));
  blit(r, c, look, cell, x, y, w, h, mod);
}

}  // namespace ote
