#include "ui/item_icon.h"

#include "ui/text.h"

namespace ote {
namespace itemicon {
namespace {

const Sprites* g_sprites = nullptr;

IconRef refOf(const Database& db, const Look* look) {
  IconRef ref;
  if (!look || look->cells.empty() || look->sheet < 0 || (size_t)look->sheet >= db.graphics.sheets.size()) return ref;
  const SheetDef& sh = db.graphics.sheets[(size_t)look->sheet];
  ref.sheet = look->sheet;
  ref.col = look->cells[0].col;
  ref.row = look->cells[0].row;
  ref.w = sh.cellW;
  ref.h = sh.cellH;
  return ref;
}

}  // namespace

IconRef ofWeapon(const Database& db, int weapon) { return refOf(db, db.graphics.weaponIcon(weapon)); }

IconRef ofItem(const Database& db, int item) {
  IconRef ref = refOf(db, db.graphics.itemIcon(item));
  if (!ref.any() && item >= 0 && (size_t)item < db.items.size() && db.items[(size_t)item].kind == ItemKind::Weapon)
    ref = ofWeapon(db, db.items[(size_t)item].weapon);
  return ref;
}

IconRef ofUnitWeapon(const Battle& b, const Unit& unit, int weapon) {
  const int slot = b.db->rules.items.carriedWeapons ? b.weaponSlot(unit, weapon) : -1;
  if (slot >= 0 && slot < (int)unit.items.size()) {
    const IconRef ref = ofItem(*b.db, unit.items[(size_t)slot].item);
    if (ref.any()) return ref;
  }
  return ofWeapon(*b.db, weapon);
}

void useSprites(const Sprites* sprites) { g_sprites = sprites; }

void draw(Renderer& r, const IconRef& icon, int x, int y, bool dim, uint8_t alpha) {
  if (!icon.any() || !g_sprites) return;
  const TextureId tex = g_sprites->texture(icon.sheet);
  if (!tex) return;
  // A greyed row's text is (120, 124, 148); the picture is multiplied by the same grey, so it dims the way the text does.
  r.drawTexture(tex, icon.col * icon.w, icon.row * icon.h, icon.w, icon.h, x, y, icon.w, icon.h, dim ? Color(120, 124, 148, alpha) : Color(255, 255, 255, alpha));
}

void drawInRow(Renderer& r, const IconRef& icon, int x, int y, int h, bool dim) { draw(r, icon, x, y + (h - icon.h) / 2, dim); }

void drawBoxLine(Renderer& r, int x, int y, int rowH, int lineH, const IconColumn& col, const IconRef& pic, bool itemLine, bool dim,
                 const std::string& text, int maxW, Color color) {
  const int off = (rowH - lineH) / 2;  // 0 unless an icon made the rows taller than the text needs
  int tx = x;
  if (itemLine) {
    drawInRow(r, pic, x, y, rowH, dim);
    tx += col.indent();
  }
  r.drawText(tx, y + off, fitText(r, text, maxW - (tx - x)).c_str(), color);
}

}  // namespace itemicon
}  // namespace ote
