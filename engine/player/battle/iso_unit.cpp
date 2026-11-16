#include "battle/iso_unit.h"

#include <algorithm>

#include "battle/carrier_marker.h"
#include "ui/text.h"

namespace ote {
namespace {

Color scaled(Color col, float alpha) { return withAlpha(col, (int)(col.a * alpha)); }
Color shade(float k, int a = 255) { return Color((uint8_t)(255 * k), (uint8_t)(255 * k), (uint8_t)(255 * k), (uint8_t)a); }

}  // namespace

void drawIsoUnitFigure(Renderer& r, const DrawCtx& c, const Unit& u, const UnitView& v, float fade, int cx, int cy,
                       int foot, int uw, const IsoShapeFn& marker) {
  const Battle& b = c.battle;
  float al = v.alpha * fade;
  int a = std::max(0, std::min(255, (int)(al * 255.0f + 0.5f)));
  Color side = Color::rgb(b.sides[(size_t)shownSide(c, u)].color);
  if (u.done) side = mix(side, Color(70, 70, 78), 0.55f);  // dimmed: already acted
  const Look* art = unitArt(c, u);
  int bodyTop;
  if (art) {
    if (b.db->graphics.unitMarker) {
      // A flat shape in the side's colour under the feet, dark rim first.
      int mw = std::max(6, (int)(uw * v.scale)), mh = std::max(3, mw / 2);
      marker(cx - mw / 2 - 1, cy - mh / 2 - 1, mw + 2, mh + 2, Color(20, 20, 28, (uint8_t)(a * 200 / 255)));
      marker(cx - mw / 2, cy - mh / 2, mw, mh, withAlpha(side, a));
    }
    SpriteSrc src = lookSrc(c, *art);
    int w = std::max(1, (int)(uw * v.scale)), h = std::max(1, w * src.h / std::max(1, src.w));
    Color mod = shade(u.done ? 0.55f : 1.0f, a);
    r.drawTexture(src.tex, src.x, src.y, src.w, src.h, cx - w / 2, foot - h, w, h, mod);
    bodyTop = foot - h;
  } else {
    // Placeholder: an upright block in the side's colour with the unit's letter.
    int bw = std::max(2, (int)(uw * 3 / 4 * v.scale)), bh = bw;
    int bx = cx - bw / 2, by = foot - bh;
    r.fillRect(bx - 1, by - 1, bw + 2, bh + 2, scaled(Color(0, 0, 0, 200), al));
    r.fillRect(bx, by, bw, bh, scaled(side, al));
    r.fillRect(bx, by, bw, 1, scaled(mix(side, Color(255, 255, 255), 0.35f), al));
    drawGlyphCentered(r, bx + bw / 2, by + bh / 2, b.db->unitTypes[(size_t)shownType(c, u)].glyph, scaled(readableOn(side), al), std::max(1, bw * 3 / 4));
    bodyTop = by;
  }
  if (u.leader) {
    r.fillRect(cx - uw / 2, bodyTop - 1, 5, 5, scaled(Color(0, 0, 0, 220), al));
    r.fillRect(cx - uw / 2 + 1, bodyTop, 3, 3, scaled(Color(255, 210, 40), al));
  }
  drawCarrierMarker(r, c, u, cx + uw / 2, bodyTop, uw, al);  // top right corner: a unit with someone off the board
  // HP bar under the feet, using the DISPLAYED hp so it drains with the hit.
  int mx = b.maxHp(u), barW = std::max(4, uw * 3 / 4), barX = cx - barW / 2, barY = foot + 1;
  r.fillRect(barX - 1, barY - 1, barW + 2, 4, scaled(Color(0, 0, 0, 200), al));
  int fill = mx > 0 ? std::max(v.hp > 0 ? 1 : 0, barW * std::min(v.hp, mx) / mx) : 0;
  r.fillRect(barX, barY, fill, 2, scaled(hpColor(v.hp, mx), al));
}

}  // namespace ote
