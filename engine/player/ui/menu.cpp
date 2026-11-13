#include "ui/menu.h"

#include <algorithm>

#include "ui/text.h"

namespace ote {
namespace {
constexpr int kPad = 4, kRowH = 12;
}

void Menu::open(const std::string& title, const std::vector<MenuItem>& items, int anchorX, int anchorY,
                int screenW, int screenH) {
  open_ = true;
  title_ = title;
  items_ = items;
  chosen_ = -1;
  sel_ = -1;
  step(1);

  int chars = (int)title.size();
  for (const MenuItem& it : items) {
    int n = (int)it.label.size() + (it.extra.empty() ? 0 : 2 + (int)it.extra.size());
    chars = std::max(chars, n);
  }
  titleH_ = title.empty() ? 0 : kRowH;
  w_ = chars * kGlyph + kPad * 2 + 10;  // 10 = room for the selection arrow
  h_ = titleH_ + (int)items.size() * kRowH + kPad * 2 - 2;
  reanchor(anchorX, anchorY, anchorX, screenW, screenH);
}

void Menu::reanchor(int anchorX, int anchorY, int altX, int screenW, int screenH) {
  int x = anchorX + w_ > screenW - 2 ? altX - w_ : anchorX;
  x_ = std::max(2, std::min(x, screenW - w_ - 2));
  y_ = std::max(2, std::min(anchorY, screenH - h_ - 2));
}

void Menu::place(int x, int y, int screenW, int screenH) {
  x_ = std::max(2, std::min(x, screenW - w_ - 2));
  y_ = std::max(2, std::min(y, screenH - h_ - 2));
}

void Menu::step(int dir) {
  int n = (int)items_.size();
  if (n == 0) return;
  int start = sel_ < 0 ? (dir > 0 ? -1 : 0) : sel_;
  for (int i = 1; i <= n; ++i) {
    int idx = ((start + dir * i) % n + n) % n;
    if (items_[idx].enabled) { sel_ = idx; return; }
  }
  sel_ = -1;  // nothing selectable
}

int Menu::itemAt(int x, int y) const {
  if (!inside(x, y)) return -1;
  int row = (y - y_ - kPad - titleH_) / kRowH;
  if (y - y_ - kPad - titleH_ < 0 || row < 0 || row >= (int)items_.size()) return -1;
  return row;
}

Menu::Result Menu::onInput(const InputEvent& e) {
  if (!open_) return None;
  switch (e.type) {
    case InputEvent::PointerMove: {
      int i = itemAt(e.x, e.y);
      if (i >= 0 && items_[i].enabled) sel_ = i;
      return None;
    }
    case InputEvent::PointerDown: {
      if (!inside(e.x, e.y)) return Cancelled;  // tap outside closes
      int i = itemAt(e.x, e.y);
      if (i >= 0 && items_[i].enabled) {
        sel_ = i;
        chosen_ = items_[i].id;
        return Chosen;
      }
      return None;
    }
    case InputEvent::ButtonDown:
      if (e.button == Button::Up || e.button == Button::Prev) step(-1);
      else if (e.button == Button::Down || e.button == Button::Next) step(1);
      else if (e.button == Button::Cancel || e.button == Button::Menu) return Cancelled;
      else if (e.button == Button::Confirm && sel_ >= 0) {
        chosen_ = items_[sel_].id;
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
  int ty = y_ + kPad;
  if (titleH_) {
    r.drawText(x_ + kPad + 2, ty + 1, title_.c_str(), Color(150, 160, 200));
    ty += titleH_;
  }
  for (size_t i = 0; i < items_.size(); ++i) {
    const MenuItem& it = items_[i];
    int ry = ty + (int)i * kRowH;
    bool sel = (int)i == sel_;
    if (sel) r.fillRect(x_ + 2, ry - 1, w_ - 4, kRowH - 1, Color(70, 90, 150));
    Color c = it.enabled ? Color(240, 240, 245) : Color(110, 112, 125);
    if (sel) r.drawText(x_ + 3, ry + 1, ">", Color(255, 220, 90));
    r.drawText(x_ + kPad + 8, ry + 1, it.label.c_str(), c);
    if (!it.extra.empty())
      r.drawText(x_ + w_ - kPad - textWidth(it.extra), ry + 1, it.extra.c_str(),
                 it.enabled ? Color(255, 220, 120) : c);
  }
}

}  // namespace ote
