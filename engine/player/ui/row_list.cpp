#include "ui/row_list.h"

#include <algorithm>

#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {

int RowList::first() const {
  int vis = visible();
  return std::max(0, std::min(sel - vis / 2, count - vis));
}

int RowList::rowAt(int px, int py) const {
  if (px < x || px >= x + w || py < y || py >= y + h) return -1;
  int row = (py - y) / rowH;
  if (row >= visible()) return -1;
  row += first();
  return row < count ? row : -1;
}

bool RowList::step(Button b) {
  if (count <= 0) return false;
  if (b == Button::Up || b == Button::Prev) sel = (sel + count - 1) % count;
  else if (b == Button::Down || b == Button::Next) sel = (sel + 1) % count;
  else return false;
  uiCue(Cue::Cursor);
  return true;
}

void RowList::drawMore(Renderer& r, Color c) const {
  int lh = r.lineHeight();
  if (first() > 0) drawTextCentered(r, x + w / 2, y - lh - 1, "^", c);
  if (first() + visible() < count) drawTextCentered(r, x + w / 2, y + h, "v", c);
}

}  // namespace ote
