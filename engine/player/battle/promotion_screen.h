// The promotion screen (docs/SRPG.md "Player UI > Promotion", step U2): shown when a unit can become one of several
// unit types. A list of the types and, for the highlighted one, its picture, the stat changes, the skills it brings
// and the weapons the unit could no longer wield. Confirm picks, Cancel / a tap outside leaves. Pointer and touch
// work like the item panel: a first tap selects, a tap on the selected row picks. On a narrow screen (a phone held
// upright) the list sits above the details instead of beside them. Everything is measured from the active font.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "battle/promotion_text.h"
#include "platform/platform.h"
#include "ui/row_list.h"

namespace ote {

class PromotionScreen {
 public:
  enum Result { None, Chosen, Cancelled };
  // Draws the picture of unit type `type` inside the box (the scene knows the sprites and the side colour).
  using Picture = std::function<void(Renderer&, int type, int x, int y, int w, int h)>;

  void open(const std::string& title, std::vector<PromotionPreview> options);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  // The rectangle the screen centres itself in (the map view).
  void setArea(int x, int y, int w, int h) { ax_ = x; ay_ = y; aw_ = w; ah_ = h; }

  Result onInput(const InputEvent& e, const Renderer& r);
  void draw(Renderer& r, const Picture& picture);
  // The unit type of the highlighted row.
  int selectedType() const { return options_.empty() ? -1 : options_[(size_t)list_.sel].to; }

 private:
  void layout(const Renderer& r);

  bool open_ = false;
  std::string title_;
  std::vector<PromotionPreview> options_;
  RowList list_;
  int ax_ = 0, ay_ = 0, aw_ = 0, ah_ = 0;
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0;      // the box
  int dx_ = 0, dy_ = 0, dw_ = 0, dh_ = 0;  // the details area (right of the list, or below it)
};

}  // namespace ote
