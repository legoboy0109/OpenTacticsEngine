#include "ui/trade_panel.h"

#include <algorithm>

#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kPad = 6, kGap = 6;
}

void TradePanel::open(const std::string& nameA, const std::vector<PanelRow>& a, const std::string& nameB,
                      const std::vector<PanelRow>& b, int col, int row) {
  open_ = true;
  name_[0] = nameA;
  name_[1] = nameB;
  rows_[0] = a;
  rows_[1] = b;
  col_ = col ? 1 : 0;
  for (int c = 0; c < 2; ++c) {
    list_[c] = RowList();
    list_[c].count = (int)rows_[c].size();
    list_[c].sel = 0;
  }
  list_[col_].sel = std::max(0, std::min(row, list_[col_].count - 1));
}

void TradePanel::layout(const Renderer& r) {
  const int lh = r.lineHeight(), rowH = lh + 4;
  w_ = std::min(aw_ - 8, std::max(220, std::min(320, aw_ - 20)));
  int rows = std::max(1, std::max(list_[0].count, list_[1].count));
  int fixedTop = kPad + lh + 4, fixedBottom = 2 * lh + 4 + kPad;
  int avail = ah_ - 8 - fixedTop - fixedBottom;
  bool scroll = rows * rowH > avail;
  if (scroll) avail -= 2 * (lh + 1);
  int shown = std::max(1, std::min(rows, avail / rowH));
  int mark = scroll ? lh + 1 : 0;
  h_ = fixedTop + mark + shown * rowH + mark + fixedBottom;
  x_ = ax_ + (aw_ - w_) / 2;
  y_ = ay_ + (ah_ - h_) / 2;
  int colW = (w_ - 2 * kPad - kGap) / 2;
  headY_ = y_ + kPad;
  for (int c = 0; c < 2; ++c) {
    list_[c].rowH = rowH;
    list_[c].x = x_ + kPad + c * (colW + kGap);
    list_[c].w = colW;
    list_[c].y = y_ + fixedTop + mark;
    list_[c].h = shown * rowH;
  }
  detailY_ = y_ + h_ - fixedBottom + 4;
}

TradePanel::Result TradePanel::onInput(const InputEvent& e, const Renderer& r) {
  if (!open_) return None;
  layout(r);
  if (e.type == InputEvent::PointerMove) {
    for (int c = 0; c < 2; ++c) {
      int row = list_[c].rowAt(e.x, e.y);
      if (row >= 0) { col_ = c; list_[c].sel = row; }
    }
  } else if (e.type == InputEvent::PointerDown) {
    for (int c = 0; c < 2; ++c) {
      int row = list_[c].rowAt(e.x, e.y);
      if (row < 0) continue;
      bool again = c == col_ && row == list_[c].sel;
      col_ = c;
      list_[c].sel = row;
      return again ? Picked : None;
    }
    if (e.x < x_ || e.y < y_ || e.x >= x_ + w_ || e.y >= y_ + h_) return Back;
  } else if (e.type == InputEvent::ButtonDown) {
    if (e.button == Button::Left || e.button == Button::Right) {
      int other = 1 - col_;
      list_[other].sel = std::min(list_[col_].sel, std::max(0, list_[other].count - 1));  // same height
      col_ = other;
    } else if (list_[col_].step(e.button)) {
    } else if (e.button == Button::Confirm) {
      return Picked;
    } else if (e.button == Button::Cancel || e.button == Button::Menu) {
      return Back;
    }
  }
  return None;
}

void TradePanel::draw(Renderer& r) {
  if (!open_) return;
  layout(r);
  const int lh = r.lineHeight();
  r.fillRect(x_ + 2, y_ + 2, w_, h_, Color(0, 0, 0, 110));
  r.fillRect(x_, y_, w_, h_, chrome::kBg);
  r.drawRect(x_, y_, w_, h_, Color(200, 205, 230));
  for (int c = 0; c < 2; ++c) {
    const RowList& l = list_[c];
    r.drawText(l.x, headY_, fitText(r, name_[c], l.w).c_str(), c == col_ ? chrome::kAccent : chrome::kDim);
    int first = l.first();
    for (int i = 0; i < l.visible() && first + i < l.count; ++i) {
      int row = first + i;
      drawPanelRow(r, l.x, l.rowY(row), l.w, l.rowH - 1, rows_[c][(size_t)row], c == col_ && row == l.sel);
    }
  }
  list_[col_].drawMore(r, chrome::kAccent);
  const RowList& cur = list_[col_];
  if (cur.count > 0) {
    std::vector<std::string> lines = wrapText(r, rows_[col_][(size_t)cur.sel].detail, w_ - 2 * kPad);
    for (int i = 0; i < 2 && i < (int)lines.size(); ++i)
      r.drawText(x_ + kPad, detailY_ + i * lh, lines[(size_t)i].c_str(), chrome::kDim);
  }
}

}  // namespace ote
