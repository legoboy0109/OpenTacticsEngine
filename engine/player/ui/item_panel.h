// List panels of the item screens in a battle: a unit's items, the shared
// storage, a shop, a list of names. A titled box over the map with scrolling
// rows, an optional line of text under the title (a shop's funds), and the
// explanation of the selected row at the bottom. Pointer and touch work the
// way Menu and SlotPanel do: tap a row to select it, tap the selected row to
// choose it, tap outside to leave. All measures come from the active font.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"
#include "ui/row_list.h"

namespace ote {

struct PanelRow {
  std::string label;
  std::string extra;   // right-aligned: uses left, a price
  std::string detail;  // shown at the bottom while the row is selected
  bool dim = false;    // cannot be chosen (shown greyed; choosing it is still reported)
  bool mark = false;   // the equipped weapon
  bool blank = false;  // a free slot ("-")
};

// One row inside `x, y, w, h` (shared with the trade panel).
void drawPanelRow(Renderer& r, int x, int y, int w, int h, const PanelRow& row, bool selected);

class ItemPanel {
 public:
  enum Result { None, Back, Picked, Tab };  // Tab: another tab was chosen (Left/Right or a tap on its name): see tab()

  // Optional tabs in the title row (the preparation shop's Buy / Sell). Left/Right and a tap switch between them and
  // report Tab; the rows are the scene's to replace. Empty = no tabs (Left/Right do nothing, as before). Call after open().
  void setTabs(const std::vector<std::string>& names, int current) { tabs_ = names; tab_ = current; }
  int tab() const { return tab_; }

  // `emptyText` is what an empty list says. `sel` = row to start on.
  void open(const std::string& title, const std::string& head, const std::vector<PanelRow>& rows,
            const std::string& emptyText, int sel = 0);
  void close() { open_ = false; }
  // Lines of explanation under the rows (default 2; the research panel wants more).
  void setDetailLines(int n) { detailLines_ = n; }
  // Width wanted (default: three quarters of the area, at most 260; rows with a price and turns want more).
  void setMaxWidth(int w) { maxW_ = w; }
  bool isOpen() const { return open_; }
  // The rectangle the panel centres itself in (the map view).
  void setArea(int x, int y, int w, int h) { ax_ = x; ay_ = y; aw_ = w; ah_ = h; }

  Result onInput(const InputEvent& e, const Renderer& r);
  void draw(Renderer& r);

  int sel() const { return list_.sel; }
  // The selected row on screen (to put a small menu beside it).
  void selectedBox(const Renderer& r, int& x, int& y, int& w, int& h);
  // The middle of tab `i` on screen (for tests that tap it); false when there is no such tab.
  bool tabCentre(const Renderer& r, int i, int& x, int& y);

 private:
  void layout(const Renderer& r);

  bool open_ = false;
  std::string title_, head_, empty_;
  std::vector<PanelRow> rows_;
  RowList list_;
  int maxW_ = 0;  // 0 = the usual width
  std::vector<std::string> tabs_;
  int tab_ = 0;
  std::vector<int> tabX_;  // left edge and width of each tab's name on screen (laid out with the panel), top = y_ + kPad
  std::vector<int> tabW_;
  int ax_ = 0, ay_ = 0, aw_ = 0, ah_ = 0;
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0, detailY_ = 0, detailLines_ = 2;
};

}  // namespace ote
