// The trade screen: two units' inventories side by side, unit names as
// headings. Left/Right switch the column, Up/Down move, Confirm picks the
// selected row (the scene decides what that does: pick up an item, then put it down in a slot of either column: an item there
// swaps places with it, a free slot takes it). The panel only remembers and draws which row is picked up. Taps work like in
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

  // The picked-up row (drawn outlined). Cleared by open(), so a rebuilt screen starts with nothing picked.
  void hold(int col, int row) { heldCol_ = col; heldRow_ = row; }
  void release() { heldCol_ = -1; }
  bool holding() const { return heldCol_ >= 0; }
  int heldCol() const { return heldCol_; }
  int heldRow() const { return heldRow_; }

  int col() const { return col_; }
  int row() const { return list_[col_].sel; }
  // The selected row on screen (to put a small menu beside it).
  // The middle of a row on screen (the screen tests tap it).
  void rowCentre(const Renderer& r, int col, int row, int& x, int& y);
  void selectedBox(const Renderer& r, int& x, int& y, int& w, int& h);

 private:
  void layout(const Renderer& r);

  bool open_ = false;
  std::string name_[2];
  std::vector<PanelRow> rows_[2];
  IconColumn icons_;
  RowList list_[2];
  int col_ = 0;
  int heldCol_ = -1, heldRow_ = -1;
  int ax_ = 0, ay_ = 0, aw_ = 0, ah_ = 0;
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0, headY_ = 0, detailY_ = 0;
};

}  // namespace ote
