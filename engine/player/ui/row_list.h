// Geometry and navigation of a vertical list of equally tall rows that
// scrolls when it does not fit: which rows are visible, which row a pointer
// hit, and Up/Down stepping. Drawing the rows themselves is up to the scene.
#pragma once
#include "platform/platform.h"

namespace ote {

struct RowList {
  // Area the rows may occupy (logical pixels) and the height of one row.
  int x = 0, y = 0, w = 0, h = 0, rowH = 1;
  int count = 0, sel = 0;

  int visible() const { return h / rowH < 1 ? 1 : h / rowH; }
  // Index of the first visible row: keeps the selection roughly centred.
  int first() const;
  int rowY(int index) const { return y + (index - first()) * rowH; }
  // Row under a point, or -1.
  int rowAt(int px, int py) const;
  // Up/Down/Prev/Next move the selection (wrapping); returns true if handled.
  bool step(Button b);
  // "^" / "v" markers just outside the area when rows are hidden.
  void drawMore(Renderer& r, Color c) const;
};

}  // namespace ote
