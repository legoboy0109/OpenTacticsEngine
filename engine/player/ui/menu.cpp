#include "ui/menu.h"

#include <algorithm>

#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kPad = 4;
}

void Menu::open(Renderer& r, const std::string& title, const std::vector<MenuItem>& items, int anchorX,
                int anchorY, int screenW, int screenH) {
  open_ = true;
  title_ = title;
  items_ = items;
  chosen_ = -1;
  sel_ = -1;
  step(1);

  // Everything is measured with the active font, so the menu fits any text.
  icons_ = IconColumn();
  for (const MenuItem& it : items) icons_.add(it.pic);
  rowH_ = std::max(r.lineHeight() + 4, icons_.h);  // (an icon that fits the row changes nothing)
  int gap = textW(r, "  "), arrowW = textW(r, ">") + 4;
  int widest = textW(r, title);
  for (const MenuItem& it : items) {
    int n = icons_.indent() + textW(r, it.label) + (it.extra.empty() ? 0 : gap + textW(r, it.extra));
    widest = std::max(widest, n);
  }
  arrowW_ = arrowW;
  titleH_ = title.empty() ? 0 : rowH_;
  w_ = std::min(widest + kPad * 2 + arrowW + 2, std::max(40, screenW - 4));
  const int n = (int)items.size();
  h_ = titleH_ + n * rowH_ + kPad * 2 - 2;
  shown_ = n;
  mark_ = 0;
  if (h_ > screenH - 4 && n > 1) {  // too tall for the screen: the rows that fit, with room for the "^" and "v" markers
    mark_ = r.lineHeight() + 1;
    shown_ = std::max(1, std::min(n, (screenH - 4 - titleH_ - kPad * 2 + 2 - 2 * mark_) / rowH_));
    h_ = titleH_ + 2 * mark_ + shown_ * rowH_ + kPad * 2 - 2;
  }
  list_ = RowList();
  list_.count = n;
  list_.rowH = rowH_;
  reanchor(anchorX, anchorY, anchorX, screenW, screenH);
}

void Menu::reanchor(int anchorX, int anchorY, int altX, int screenW, int screenH) {
  int x = anchorX + w_ > screenW - 2 ? altX - w_ : anchorX;
  x_ = std::max(2, std::min(x, screenW - w_ - 2));
  y_ = std::max(2, std::min(anchorY, screenH - h_ - 2));
  syncList();
}

void Menu::place(int x, int y, int screenW, int screenH) {
  x_ = std::max(2, std::min(x, screenW - w_ - 2));
  y_ = std::max(2, std::min(y, screenH - h_ - 2));
  syncList();
}

void Menu::syncList() {
  list_.x = x_;
  list_.w = w_;
  list_.y = y_ + kPad + titleH_ + mark_;
  list_.h = shown_ * rowH_;
  list_.sel = sel_;
}

void Menu::step(int dir) {
  int n = (int)items_.size();
  if (n == 0) return;
  int start = sel_ < 0 ? (dir > 0 ? -1 : 0) : sel_;
  for (int i = 1; i <= n; ++i) {
    int idx = ((start + dir * i) % n + n) % n;
    if (items_[idx].enabled) {
      sel_ = idx;
      list_.sel = sel_;
      list_.centre();  // a key step keeps the selection in the middle, as in every list
      return;
    }
  }
  sel_ = -1;  // nothing selectable
  list_.sel = sel_;
}

int Menu::itemAt(int x, int y) const {
  if (!inside(x, y) || y < list_.y) return -1;
  int row = (y - list_.y) / rowH_;
  if (row >= shown_) return -1;
  row += list_.first();
  return row < (int)items_.size() ? row : -1;
}

Menu::Result Menu::onInput(const InputEvent& e) {
  if (!open_) return None;
  if (list_.input(e)) return None;  // the wheel, a "^" / "v" marker, a finger's drag
  switch (e.type) {
    case InputEvent::PointerMove: {
      int i = itemAt(e.x, e.y);
      if (i >= 0 && items_[i].enabled) sel_ = list_.sel = i;  // (a shown row: hovering never scrolls)
      return None;
    }
    case InputEvent::PointerDown: {
      if (!inside(e.x, e.y)) { uiCue(Cue::Cancel); return Cancelled; }  // tap outside closes
      int i = itemAt(e.x, e.y);
      if (i >= 0 && items_[i].enabled) {
        sel_ = list_.sel = i;
        chosen_ = items_[i].id;
        uiCue(Cue::Confirm);
        return Chosen;
      }
      if (i >= 0) uiCue(Cue::Error);  // a greyed entry
      return None;
    }
    case InputEvent::ButtonDown:
      if (e.button == Button::Up || e.button == Button::Prev) { step(-1); uiCue(Cue::Cursor); }
      else if (e.button == Button::Down || e.button == Button::Next) { step(1); uiCue(Cue::Cursor); }
      else if (e.button == Button::Cancel || e.button == Button::Menu) { uiCue(Cue::Cancel); return Cancelled; }
      else if (e.button == Button::Confirm && sel_ >= 0) {
        chosen_ = items_[sel_].id;
        uiCue(Cue::Confirm);
        return Chosen;
      }
      return None;
    default: return None;
  }
}

void Menu::draw(Renderer& r) const {
  if (!open_) return;
  r.fillRect(x_ + 2, y_ + 2, w_, h_, Color(0, 0, 0, 110));  // drop shadow
  r.fillRect(x_, y_, w_, h_, Color(24, 28, 44, 245));
  r.drawRect(x_, y_, w_, h_, Color(200, 205, 230));
  if (titleH_) r.drawText(x_ + kPad + 2, y_ + kPad + 1, fitText(r, title_, w_ - 2 * kPad - 2).c_str(), Color(150, 160, 200));
  const int first = list_.first();
  for (size_t i = (size_t)first; i < items_.size() && (int)i < first + shown_; ++i) {
    const MenuItem& it = items_[i];
    int ry = list_.rowY((int)i);
    bool sel = (int)i == sel_;
    if (sel) r.fillRect(x_ + 2, ry - 1, w_ - 4, rowH_ - 1, Color(70, 90, 150));
    Color c = it.enabled ? Color(240, 240, 245) : Color(110, 112, 125);
    if (sel) r.drawText(x_ + 3, ry + 1 + (rowH_ - (r.lineHeight() + 4)) / 2, ">", Color(255, 220, 90));
    int tx = x_ + kPad + arrowW_;
    if (icons_.any()) {
      itemicon::drawInRow(r, it.pic, tx, ry - 1, rowH_, !it.enabled);
      tx += icons_.indent();
    }
    const int ty0 = ry + 1 + (rowH_ - (r.lineHeight() + 4)) / 2;  // (the text keeps its place unless an icon made the rows taller)
    // A long reason ("Only Ser Ovan can wield this") on a narrow screen: the menu is clamped to the screen, so the
    // extra text is shortened to what is left after the label instead of running out of the box on the left.
    std::string extra = it.extra;
    if (!extra.empty()) {
      const int room = w_ - kPad - (tx - x_) - textW(r, it.label) - textW(r, "  ");
      if (textW(r, extra) > room) extra = fitText(r, extra, std::max(0, room));
    }
    int extraW = extra.empty() ? 0 : textW(r, extra);
    r.drawText(tx, ty0, fitText(r, it.label, w_ - kPad - (tx - x_) - (extraW ? extraW + 4 : 0)).c_str(), c);
    if (extraW) r.drawText(x_ + w_ - kPad - extraW, ty0, extra.c_str(), it.enabled ? Color(255, 220, 120) : c);
  }
  if (mark_) list_.drawMore(r, Color(255, 220, 90));
}

}  // namespace ote
