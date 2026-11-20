#include "ui/item_panel.h"

#include <algorithm>

#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/input_hints.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kPad = 6;
const Color kMark(255, 220, 90), kGreyed(120, 124, 148);
constexpr int kTabGap = 10;
}  // namespace

void drawPanelRow(Renderer& r, int x, int y, int w, int h, const PanelRow& row, bool selected) {
  if (row.header) {
    const int hy = y + (h - r.lineHeight()) / 2;
    r.drawText(x + 4, hy, fitText(r, row.label, w - 8).c_str(), chrome::kAccent);
    r.fillRect(x + 2, y + h - 1, w - 4, 1, kGreyed);
    return;
  }
  drawRowBox(r, x, y, w, h, selected);
  int lh = r.lineHeight(), ty = y + (h - lh) / 2;
  const bool twoLines = !row.sub.empty() && h >= 2 * lh + 2;
  if (twoLines) ty = y + (h - 2 * lh) / 2;
  Color c = row.dim || row.blank ? kGreyed : (selected ? chrome::kTextSel : chrome::kText);
  int s = std::max(3, lh / 3), tx = x + 4;
  if (row.mark) r.fillRect(tx, y + (h - s) / 2, s, s, kMark);  // a small square: this weapon is in hand
  tx += s + 3;  // names line up whether or not a marker is shown
  if (row.icon && row.iconSize > 0) {
    row.icon(r, tx, y + (h - row.iconSize) / 2, row.iconSize);
    tx += row.iconSize + 4;
  }
  // The name matters more than the extra: when both do not fit, the name keeps at least half the row.
  int avail = x + w - 4 - tx, gap = 6;
  std::string extra = row.extra;
  // The name with its count: fitName(n) is the longest "Name  x3" within n pixels (the count stays whole).
  auto fitName = [&](int n) { return fitNameCount(r, row.label, row.count, n); };
  std::string label = row.count > 1 ? row.label + "  x" + intStr(row.count) : row.label;
  const Color extraColor = row.dim ? kGreyed : chrome::kAccent, subColor = row.dim ? kGreyed : chrome::kDim;
  if (twoLines && !extra.empty() && textW(r, label) + gap + textW(r, extra) > avail) {
    // A narrow list with a picture: the extra ("Slot 2") moves down to the second line, right-aligned; the dimmer text on the left
    // shrinks to its first word ("Lv2") or goes when even that does not fit (the selected row's explanation says the rest).
    std::string sub = row.sub;
    const int room = avail - gap - textW(r, extra);
    if (textW(r, sub) > room) sub = sub.substr(0, sub.find(' '));
    if (textW(r, sub) > room) sub.clear();
    r.drawText(tx, ty, fitName(avail).c_str(), c);
    r.drawText(tx, ty + lh, sub.c_str(), subColor);
    const std::string shown = fitText(r, extra, avail);
    r.drawText(x + w - 4 - textW(r, shown), ty + lh, shown.c_str(), extraColor);
    return;
  }
  if (!extra.empty() && textW(r, label) + gap + textW(r, extra) > avail) {
    // The name keeps half the row; a stacked row lets it shrink to "P.. x3" so the count and the uses both show.
    int keep = avail / 2;
    if (row.count > 1) keep = std::min(keep, textW(r, "P.. x" + intStr(row.count)));
    int labelMax = std::max(keep, avail - gap - textW(r, extra));
    label = fitName(labelMax);
    extra = fitText(r, extra, std::max(0, avail - gap - textW(r, label)));
  }
  if (textW(r, label) > avail) label = fitName(avail);
  r.drawText(tx, ty, label.c_str(), c);
  if (!extra.empty()) r.drawText(x + w - 4 - textW(r, extra), ty, extra.c_str(), extraColor);
  if (twoLines) r.drawText(tx, ty + lh, fitText(r, row.sub, avail).c_str(), subColor);
}

int panelRowWidth(const Renderer& r, const PanelRow& row) {
  int s = std::max(3, r.lineHeight() / 3);
  int w = 4 + s + 3 + textW(r, row.count > 1 ? row.label + "  x" + intStr(row.count) : row.label) + 4;
  if (!row.extra.empty()) w += 6 + textW(r, row.extra);
  return w;
}

void ItemPanel::open(const std::string& title, const std::string& head, const std::vector<PanelRow>& rows,
                     const std::string& emptyText, int sel) {
  open_ = true;
  title_ = title;
  head_ = head;
  rows_ = rows;
  empty_ = emptyText;
  tabs_.clear();
  tab_ = 0;
  list_ = RowList();
  list_.count = (int)rows_.size();
  list_.sel = std::max(0, std::min(sel, list_.count - 1));
  skipHeaders(Button::Down);
}

// A heading cannot be selected: step on in the direction of the key (wraps; stops after one lap if everything is a heading).
void ItemPanel::skipHeaders(Button dir) {
  for (int n = 0; n < list_.count && list_.sel >= 0 && list_.sel < list_.count && rows_[(size_t)list_.sel].header; ++n) list_.step(dir);
}

void ItemPanel::layout(const Renderer& r) {
  const int lh = r.lineHeight();
  int rowH = lh + 4;
  for (const PanelRow& row : rows_) {  // rows with a picture or a second line are taller (one height for all, as RowList wants)
    if (!row.sub.empty()) rowH = std::max(rowH, 2 * lh + 4);
    if (row.icon) rowH = std::max(rowH, row.iconSize + 3);
  }
  w_ = std::min(aw_ - 8, std::max(150, maxW_ > 0 ? maxW_ : std::min(260, aw_ * 3 / 4)));
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
  // Tabs sit at the right end of the title row, the last one flush with the panel's padding.
  tabX_.assign(tabs_.size(), 0);
  tabW_.assign(tabs_.size(), 0);
  int tx = x_ + w_ - kPad;
  for (size_t i = tabs_.size(); i-- > 0;) {
    tabW_[i] = textW(r, tabs_[i]);
    tx -= tabW_[i];
    tabX_[i] = tx;
    tx -= kTabGap;
  }
}

void ItemPanel::selectedBox(const Renderer& r, int& x, int& y, int& w, int& h) {
  layout(r);
  x = list_.x;
  y = list_.rowY(list_.sel);
  w = list_.w;
  h = list_.rowH;
}

bool ItemPanel::tabCentre(const Renderer& r, int i, int& x, int& y) {
  layout(r);
  if (i < 0 || i >= (int)tabs_.size()) return false;
  x = tabX_[(size_t)i] + tabW_[(size_t)i] / 2;
  y = y_ + kPad + r.lineHeight() / 2;
  return true;
}

ItemPanel::Result ItemPanel::onInput(const InputEvent& e, const Renderer& r) {
  if (!open_) return None;
  layout(r);
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0 && !rows_[(size_t)row].header) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    // A tab's name, with a generous target (the whole title row, half a gap each side): a finger is wider than a letter.
    for (size_t i = 0; i < tabs_.size(); ++i)
      if (e.x >= tabX_[i] - kTabGap / 2 && e.x < tabX_[i] + tabW_[i] + kTabGap / 2 && e.y >= y_ && e.y < list_.y - 1) {
        if ((int)i == tab_) return None;
        tab_ = (int)i;
        uiCue(Cue::Cursor);
        return Tab;
      }
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0 && rows_[(size_t)row].header) return None;
    if (row >= 0) {
      bool again = row == list_.sel;
      list_.sel = row;
      if (again) uiCue(Cue::Confirm);
      return again ? Picked : None;  // a first tap only selects (a touch screen never hovers)
    }
    if (e.x < x_ || e.y < y_ || e.x >= x_ + w_ || e.y >= y_ + h_) { uiCue(Cue::Cancel); return Back; }
  } else if (e.type == InputEvent::ButtonDown) {
    if (tabs_.size() > 1 && (e.button == Button::Left || e.button == Button::Right)) {
      const int n = (int)tabs_.size();
      tab_ = (tab_ + (e.button == Button::Right ? 1 : n - 1)) % n;
      uiCue(Cue::Cursor);
      return Tab;
    }
    if (list_.step(e.button)) {
      skipHeaders(e.button == Button::Up || e.button == Button::Prev ? Button::Up : Button::Down);
      return None;
    }
    if (e.button == Button::Confirm && list_.count > 0) { uiCue(Cue::Confirm); return Picked; }
    if (e.button == Button::Cancel || e.button == Button::Menu) { uiCue(Cue::Cancel); return Back; }
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
  int titleW = w_ - 2 * kPad;
  if (!tabs_.empty()) titleW = std::max(0, tabX_[0] - kTabGap - (x_ + kPad));
  r.drawText(x_ + kPad, ty, fitText(r, title_, titleW).c_str(), chrome::kAccent);
  for (size_t i = 0; i < tabs_.size(); ++i) {  // the current tab is bright and underlined, the others dim
    const bool on = (int)i == tab_;
    r.drawText(tabX_[i], ty, tabs_[i].c_str(), on ? chrome::kTextSel : chrome::kDim);
    if (on) r.fillRect(tabX_[i], ty + lh, tabW_[i], 1, chrome::kAccent);
  }
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
  const std::string d = hintText(rows_[(size_t)list_.sel].detail);
  if (!d.empty()) {
    std::vector<std::string> lines = wrapText(r, d, w_ - 2 * kPad);
    for (int i = 0; i < detailLines_ && i < (int)lines.size(); ++i)
      r.drawText(x_ + kPad, detailY_ + i * lh, lines[(size_t)i].c_str(), chrome::kDim);
  }
}

}  // namespace ote
