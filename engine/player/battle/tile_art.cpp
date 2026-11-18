#include "battle/tile_art.h"

namespace ote {

namespace {
const TilesetDef* tilesetOf(const DrawCtx& c) {
  const Battle& b = c.battle;
  if (!b.hasTiles() || b.tileset < 0 || (size_t)b.tileset >= b.db->graphics.tilesets.size()) return nullptr;
  return &b.db->graphics.tilesets[(size_t)b.tileset];
}
}  // namespace

int tileSheet(const DrawCtx& c) {
  const TilesetDef* ts = tilesetOf(c);
  return ts ? ts->sheet : -1;
}

bool tileSrc(const DrawCtx& c, Cell cell, SpriteSrc& out) {
  const TilesetDef* ts = tilesetOf(c);
  if (!ts || !c.sprites) return false;
  const int tile = c.battle.tileAt(cell);
  if (tile < 0 || tile >= ts->count()) return false;
  const TextureId tex = c.sprites->texture(ts->sheet);
  if (!tex) return false;
  const SheetDef& sh = c.battle.db->graphics.sheets[(size_t)ts->sheet];
  const SpriteCell sc = ts->cell(ts->frame(tile, (uint32_t)(c.time * 1000.0f)));
  out.tex = tex;
  out.x = sc.col * sh.cellW;
  out.y = sc.row * sh.cellH;
  out.w = sh.cellW;
  out.h = sh.cellH;
  return true;
}

bool hasTileArt(const DrawCtx& c, Cell cell) {
  SpriteSrc s;
  return tileSrc(c, cell, s);
}

void drawTileArt(Renderer& r, const DrawCtx& c, const SpriteSrc& s, int dx, int dy, int dw, int dh) {
  r.drawTexture(s.tex, s.x, s.y, s.w, s.h, dx, dy, dw, dh, Color(255, 255, 255));
}

IsoPicture isoTileBlock(Renderer& r, const DrawCtx& c, const IsoGeometry& g, Cell cell, int sideH, uint32_t sideRgb) {
  SpriteSrc s;
  const TilesetDef* ts = tilesetOf(c);
  if (!ts || !tileSrc(c, cell, s)) return IsoPicture();
  const Image* img = c.sprites->pixels(ts->sheet);
  if (!img) return IsoPicture();
  return c.tiles->pictureBlock(r, g, sideH, sideRgb, {ts->sheet, s.x, s.y, s.w, s.h}, img->rgba.data(), img->width, s.x, s.y, s.w, s.h);
}

}  // namespace ote
