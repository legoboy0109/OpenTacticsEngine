// Small popup menu: keyboard/pad/pointer navigable list of items.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "platform/platform.h"
#include "ui/item_icon.h"

namespace ote {

struct MenuItem {
  std::string label;
  std::string extra;   // right-aligned (e.g. a cost)
  bool enabled = true;  // disabled items are greyed and cannot be chosen
  int id = 0;           // caller-defined meaning
  IconRef pic;          // an item's icon (graphics.json "icons", ui/item_icon.h) for a list of items; the menu keeps one column for it

  MenuItem() = default;
  // (A constructor, not an aggregate: the many `{label, extra, enabled, id}` lists leave `pic` alone without a compiler warning.)
  MenuItem(std::string l, std::string e = std::string(), bool en = true, int i = 0, IconRef p = IconRef())
      : label(std::move(l)), extra(std::move(e)), enabled(en), id(i), pic(p) {}
};

class Menu {
 public:
  enum Result { None, Chosen, Cancelled };

  // Places the menu near (anchorX, anchorY) and keeps it inside the screen.
  // Opens with the first enabled item selected.
  void open(Renderer& r, const std::string& title, const std::vector<MenuItem>& items, int anchorX, int anchorY,
            int screenW, int screenH);
  // Moves an open menu (e.g. to follow a unit while the camera eases). If it
  // would run off the right edge it is placed to the left of altX instead.
  void reanchor(int anchorX, int anchorY, int altX, int screenW, int screenH);
  // Places the top-left at (x, y), clamped to the screen.
  void place(int x, int y, int screenW, int screenH);
  int x() const { return x_; }
  int y() const { return y_; }
  int w() const { return w_; }
  int h() const { return h_; }
  // The middle of entry `index` on screen (for tests that tap it; the player logs them).
  void rowCentre(int index, int& x, int& y) const { x = x_ + w_ / 2; y = y_ + 4 + titleH_ + index * rowH_ + rowH_ / 2; }
  void close() { open_ = false; }
  bool isOpen() const { return open_; }

  Result onInput(const InputEvent& e);
  int chosenId() const { return chosen_; }
  int selectedId() const { return open_ && sel_ >= 0 ? items_[(size_t)sel_].id : -1; }  // the highlighted entry (-1 = none)
  void draw(Renderer& r) const;

 private:
  void step(int dir);
  int itemAt(int x, int y) const;  // -1 = none
  bool inside(int x, int y) const { return x >= x_ && y >= y_ && x < x_ + w_ && y < y_ + h_; }

  bool open_ = false;
  std::string title_;
  std::vector<MenuItem> items_;
  int sel_ = -1, chosen_ = -1;
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0;
  int titleH_ = 0, rowH_ = 12, arrowW_ = 12;
  IconColumn icons_;  // the widest and tallest icon of the entries: every label moves right by its indent
};

}  // namespace ote
