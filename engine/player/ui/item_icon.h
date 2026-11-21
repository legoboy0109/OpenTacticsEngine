// The small picture before an item's or weapon's name in every list (docs/GRAPHICS.md "Icons": data/graphics.json "icons").
// ONE helper for all lists, so they behave the same:
//   - `IconRef` says where the picture is (a cell of a sheet) and how big; the lookup is by weapon or item index.
//   - A list that shows icons shares one `IconColumn`: the widest icon of its rows. Every row of the list, with an icon or not,
//     starts its name `column.indent()` pixels further right, so the names line up. A game without icons has an empty column
//     (indent 0) and every list looks exactly as before.
//   - `draw` puts the icon at its own size (the sheet's cell size), at the given spot; a greyed row passes dim = true.
// Rows keep the height they have unless an icon is taller than the row (the callers use `column.h`).
#pragma once
#include <algorithm>
#include <string>

#include "battle/sprites.h"
#include "core/data/database.h"
#include "platform/platform.h"

namespace ote {

struct IconRef {
  int sheet = -1;
  int col = 0, row = 0;
  int w = 0, h = 0;  // the sheet's cell size
  bool any() const { return sheet >= 0 && w > 0 && h > 0; }
};

namespace itemicon {
constexpr int kGap = 3;  // between the icon column and the name
}

// What a list reserves for its icons: the widest and the tallest icon among its rows (0 = no row has one).
struct IconColumn {
  int w = 0, h = 0;
  void add(const IconRef& i) {
    if (!i.any()) return;
    w = std::max(w, i.w);
    h = std::max(h, i.h);
  }
  bool any() const { return w > 0; }
  int indent() const { return w > 0 ? w + itemicon::kGap : 0; }  // pixels the name moves right: the column plus the gap
};

namespace itemicon {

IconRef ofWeapon(const Database& db, int weapon);
// The item's own icon; a weapon item without one takes its weapon's icon (so a game may name either id).
IconRef ofItem(const Database& db, int item);

// The icon of the weapon `weapon` as `unit` wields it: the item in its pack that gives the weapon (carried weapons) with its own
// icon, else the weapon's.
IconRef ofUnitWeapon(const Battle& b, const Unit& unit, int weapon);

// The textures of the game being shown (null = none): the scene sets them at the start of each frame it draws, before any list.
void useSprites(const Sprites* sprites);

// Draws `icon` with its top-left at (x, y); nothing when it has no texture. `dim` = a greyed row (the picture is darkened like the text).
void draw(Renderer& r, const IconRef& icon, int x, int y, bool dim, uint8_t alpha = 255);
// The same, vertically centred in a row of height `h` that starts at `y` (an icon taller than the row runs over by the rest).
void drawInRow(Renderer& r, const IconRef& icon, int x, int y, int h, bool dim);

// One text line of a box (a forecast): the icon of an item line in the box's icon column, the text after the column. `rowH` is the
// pitch of the box's lines, `lineH` the height a line of text takes in the box when no icon makes the rows taller (the text keeps its
// place then). Lines that are not item lines (`itemLine` false) start at x, as before.
void drawBoxLine(Renderer& r, int x, int y, int rowH, int lineH, const IconColumn& col, const IconRef& pic, bool itemLine, bool dim,
                 const std::string& text, int maxW, Color color);

}  // namespace itemicon
}  // namespace ote
