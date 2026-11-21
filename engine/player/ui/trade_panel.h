// The trade screen: two units' inventories side by side, unit names as
// headings. Left/Right switch the column, Up/Down move, Confirm picks the
// selected row (the scene decides what that does). Taps work like in
// ItemPanel: first tap selects, tap on the selected row picks, outside = back.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"
#include "ui/item_panel.h"
#include "ui/row_list.h"

namespace ote {

class TradePanel {
 public:
  enum Result { None, Back, Picked };

  // Column 0 is the acting unit, column 1 its partner. Rows include free slots (blank).
  void open(const std::string& nameA, const std::vector<PanelRow>& a, const std::string& nameB,
            const std::vector<PanelRow>& b, int col = 0, int row = 0);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  void setArea(int x, int y, int w, int h) { ax_ = x; ay_ = y; aw_ = w; ah_ = h; }

  Result onInput(const InputEvent& e, const Renderer& r);
  void draw(Renderer& r);

  int col() const { return col_; }
  int row() const { return list_[col_].sel; }
  // The selected row on screen (to put a small menu beside it).
  void selectedBox(const Renderer& r, int& x, int& y, int& w, int& h);

 private:
  void layout(const Renderer& r);

  bool open_ = false;
  std::string name_[2];
  std::vector<PanelRow> rows_[2];
  IconColumn icons_;
  RowList list_[2];
  int col_ = 0;
  int ax_ = 0, ay_ = 0, aw_ = 0, ah_ = 0;
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0, headY_ = 0, detailY_ = 0;
};

}  // namespace ote
