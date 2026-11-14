#include "ui/item_panel.h"

#include <algorithm>

#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kPad = 6;
const Color kMark(255, 220, 90), kGreyed(120, 124, 148);
}  // namespace

void drawPanelRow(Renderer& r, int x, int y, int w, int h, const PanelRow& row, bool selected) {
  drawRowBox(r, x, y, w, h, selected);
  int lh = r.lineHeight(), ty = y + (h - lh) / 2;
  Color c = row.dim || row.blank ? kGreyed : (selected ? chrome::kTextSel : chrome::kText);
  int s = std::max(3, lh / 3), tx = x + 4;
  if (row.mark) r.fillRect(tx, y + (h - s) / 2, s, s, kMark);  // a small square: this weapon is in hand
  tx += s + 3;  // names line up whether or not a marker is shown
  // The name matters more than the extra: when both do not fit, the name keeps at least half the row.
  int avail = x + w - 4 - tx, gap = 6;
  std::string extra = row.extra, label = row.label;
  if (!extra.empty() && textW(r, label) + gap + textW(r, extra) > avail) {
    int labelMax = std::max(avail / 2, avail - gap - textW(r, extra));
    label = fitText(r, label, labelMax);
    extra = fitText(r, extra, std::max(0, avail - gap - textW(r, label)));
  }
  r.drawText(tx, ty, fitText(r, label, avail).c_str(), c);
  if (!extra.empty()) r.drawText(x + w - 4 - textW(r, extra), ty, extra.c_str(), row.dim ? kGreyed : chrome::kAccent);
}

void ItemPanel::open(const std::string& title, const std::string& head, const std::vector<PanelRow>& rows,
                     const std::string& emptyText, int sel) {
  open_ = true;
  title_ = title;
  head_ = head;
  rows_ = rows;
  empty_ = emptyText;
  list_ = RowList();
  list_.count = (int)rows_.size();
  list_.sel = std::max(0, std::min(sel, list_.count - 1));
}

void ItemPanel::layout(const Renderer& r) {
  const int lh = r.lineHeight(), rowH = lh + 4;
  w_ = std::min(aw_ - 8, std::max(150, std::min(260, aw_ * 3 / 4)));
  int fixedTop = kPad + lh + 4 + (head_.empty() ? 0 : lh + 2);
  int fixedBottom = detailLines_ * lh + 4 + kPad;
  int maxH = ah_ - 8;
  int rows = std::max(1, list_.count);
  int avail = maxH - fixedTop - fixedBottom;
  bool scroll = rows * rowH > avail;
  if (scroll) avail -= 2 * (lh + 1);  // room for the "^" and "v" markers
  int shown = std::max(1, std::min(rows, avail / rowH));
  int mark = scroll ? lh + 1 : 0;
  h_ = fixedTop + mark + shown * rowH + mark + fixedBottom;
  x_ = ax_ + (aw_ - w_) / 2;
  y_ = ay_ + (ah_ - h_) / 2;
  list_.rowH = rowH;
  list_.x = x_ + kPad;
  list_.w = w_ - 2 * kPad;
  list_.y = y_ + fixedTop + mark;
  list_.h = shown * rowH;
  detailY_ = y_ + h_ - fixedBottom + 4;
}

void ItemPanel::selectedBox(const Renderer& r, int& x, int& y, int& w, int& h) {
  layout(r);
  x = list_.x;
  y = list_.rowY(list_.sel);
  w = list_.w;
  h = list_.rowH;
}

ItemPanel::Result ItemPanel::onInput(const InputEvent& e, const Renderer& r) {
  if (!open_) return None;
  layout(r);
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) {
      bool again = row == list_.sel;
      list_.sel = row;
      return again ? Picked : None;  // a first tap only selects (a touch screen never hovers)
    }
    if (e.x < x_ || e.y < y_ || e.x >= x_ + w_ || e.y >= y_ + h_) return Back;
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return None;
    if (e.button == Button::Confirm && list_.count > 0) return Picked;
    if (e.button == Button::Cancel || e.button == Button::Menu) return Back;
  }
  return None;
}

void ItemPanel::draw(Renderer& r) {
  if (!open_) return;
  layout(r);
  const int lh = r.lineHeight();
  r.fillRect(x_ + 2, y_ + 2, w_, h_, Color(0, 0, 0, 110));
  r.fillRect(x_, y_, w_, h_, chrome::kBg);
  r.drawRect(x_, y_, w_, h_, Color(200, 205, 230));
  int ty = y_ + kPad;
  r.drawText(x_ + kPad, ty, fitText(r, title_, w_ - 2 * kPad).c_str(), chrome::kAccent);
  ty += lh + 4;
  if (!head_.empty()) r.drawText(x_ + kPad, ty, fitText(r, head_, w_ - 2 * kPad).c_str(), chrome::kText);
  if (rows_.empty()) {
    drawTextCentered(r, x_ + w_ / 2, list_.y + (list_.h - lh) / 2, fitText(r, empty_, w_ - 2 * kPad), chrome::kDim);
    return;
  }
  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < list_.count; ++i) {
    int row = first + i;
    drawPanelRow(r, list_.x, list_.rowY(row), list_.w, list_.rowH - 1, rows_[(size_t)row], row == list_.sel);
  }
  list_.drawMore(r, chrome::kAccent);
  const std::string& d = rows_[(size_t)list_.sel].detail;
  if (!d.empty()) {
    std::vector<std::string> lines = wrapText(r, d, w_ - 2 * kPad);
    for (int i = 0; i < detailLines_ && i < (int)lines.size(); ++i)
      r.drawText(x_ + kPad, detailY_ + i * lh, lines[(size_t)i].c_str(), chrome::kDim);
  }
}

}  // namespace ote
