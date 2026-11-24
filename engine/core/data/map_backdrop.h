// A map's prerendered picture for the 2D isometric kind of view (docs/VIEWS.md "Backdrop"): drawn in place of the generated
// squares, with an optional over-layer drawn over the units, whole or cut into pieces that each sort with one square. The core only
// describes it; the player draws it (engine/player/battle/iso_backdrop.cpp). Presentation only: rules, saves and hashes never see it.
#pragma once
#include <vector>

namespace ote {

// A rectangle of the over picture that is drawn in the depth order of one square (after the squares and units behind it, before
// the ones in front), so a tree or a roof hides what stands behind it and not what stands in front.
struct BackdropPiece {
  int x = 0, y = 0, w = 0, h = 0;  // in the over picture's pixels
  int gx = 0, gy = 0;              // the square it stands on
};

struct MapBackdrop {
  int sheet = -1;  // index into Graphics::sheets: the whole image is the picture; -1 = no backdrop
  int over = -1;   // the over picture (same placement), -1 = none
  // The pixel of the pictures where the top corner of square (0, 0) is when that square's height is 0 (the map file's own heights,
  // not counted from the lowest square).
  int originX = 0, originY = 0;
  // Empty: the whole over picture is drawn over every unit and structure. Else only these rectangles, each in its square's order.
  std::vector<BackdropPiece> pieces;
  std::vector<int> styles;  // indices into Database::styles it is drawn in; empty = every style of the 2D isometric kind

  bool any() const { return sheet >= 0; }
  bool inStyle(int style) const {
    if (styles.empty()) return true;
    for (int s : styles)
      if (s == style) return true;
    return false;
  }
};

}  // namespace ote
