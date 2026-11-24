#include "battle/iso_backdrop.h"

#include <cmath>

#include "battle/sprites.h"

namespace ote {

const MapBackdrop* isoBackdrop(const DrawCtx& c) {
  if (!c.map || !c.iso() || !c.sprites || c.proj->hex()) return nullptr;
  const MapBackdrop& bd = c.map->backdrop;
  if (!bd.any() || !bd.inStyle(c.styleIndex)) return nullptr;
  return c.sprites->texture(bd.sheet) ? &bd : nullptr;
}

void isoBackdropCorner(const DrawCtx& c, const MapBackdrop& bd, int camX, int camY, int& x, int& y) {
  // Elevations count from the map's lowest square, so the map file's height 0 is elevation -lowest.
  float wx, wy;
  c.proj->geo().project(0, 0, (float)-c.proj->lowest(), wx, wy);
  x = (int)std::lround(wx) - bd.originX - camX;
  y = (int)std::lround(wy) - bd.originY - camY;
}

void drawIsoBackdrop(Renderer& r, const DrawCtx& c, const MapBackdrop& bd, int camX, int camY) {
  const SheetDef& sh = c.battle.db->graphics.sheets[(size_t)bd.sheet];
  int x, y;
  isoBackdropCorner(c, bd, camX, camY, x, y);
  r.drawTexture(c.sprites->texture(bd.sheet), 0, 0, sh.width, sh.height, x, y, sh.width, sh.height);
}

void drawIsoOverWhole(Renderer& r, const DrawCtx& c, const MapBackdrop& bd, int camX, int camY) {
  const TextureId tex = bd.over >= 0 ? c.sprites->texture(bd.over) : 0;
  if (!tex) return;
  const SheetDef& sh = c.battle.db->graphics.sheets[(size_t)bd.over];
  int x, y;
  isoBackdropCorner(c, bd, camX, camY, x, y);
  r.drawTexture(tex, 0, 0, sh.width, sh.height, x, y, sh.width, sh.height);
}

void drawIsoOverPiece(Renderer& r, const DrawCtx& c, const MapBackdrop& bd, const BackdropPiece& p, int camX, int camY, Color mod) {
  const TextureId tex = bd.over >= 0 ? c.sprites->texture(bd.over) : 0;
  if (!tex) return;
  int x, y;
  isoBackdropCorner(c, bd, camX, camY, x, y);
  if (x + p.x + p.w < 0 || y + p.y + p.h < 0 || x + p.x > c.layout.vw || y + p.y > c.layout.vh) return;  // off the screen
  r.drawTexture(tex, p.x, p.y, p.w, p.h, x + p.x, y + p.y, p.w, p.h, mod);
}

bool isoOverCovers(const DrawCtx& c, const MapBackdrop& bd, int key, int camX, int camY, int x, int y, int w, int h) {
  if (bd.over < 0) return false;
  int ox, oy;
  isoBackdropCorner(c, bd, camX, camY, ox, oy);
  for (const BackdropPiece& p : bd.pieces) {
    if (p.gx + p.gy <= key) continue;  // drawn before (or with) the diagonal: under what stands there
    const int px = ox + p.x, py = oy + p.y;
    if (px < x + w && x < px + p.w && py < y + h && y < py + p.h) return true;
  }
  return false;
}

}  // namespace ote
