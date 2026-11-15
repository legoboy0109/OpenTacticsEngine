// Small popup menu: keyboard/pad/pointer navigable list of items.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"

namespace ote {

struct MenuItem {
  std::string label;
  std::string extra;   // right-aligned (e.g. a cost)
  bool enabled = true;  // disabled items are greyed and cannot be chosen
  int id = 0;           // caller-defined meaning
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
};

}  // namespace ote
