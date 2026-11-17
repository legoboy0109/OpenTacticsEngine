// Unit details (docs/SRPG.md "Player UI > Unit details"): the overlay of Mode::UnitInfo. A row of page tabs and the
// page under it, turned with Left / Right; Cancel closes.
//
// U0 provides the frame and the page list; the pages are separate steps' own files so they run in parallel:
//   Stats, Skills, Items   unit_info_stats.cpp / unit_info_skills.cpp / unit_info_items.cpp   (U1)
//   Abilities              unit_info_abilities.cpp                                             (U3)
//   Supports               unit_info_supports.cpp                                              (U5)
// A page file defines its `add...Page` function (declared below, empty now): it pushes a UnitInfoPage when the page
// applies to the unit (a game without supports adds no Supports page) and does nothing otherwise. The order of the
// tabs is fixed in buildUnitInfoPages (unit_info.cpp), so no page file edits another.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "platform/platform.h"

namespace ote {

struct UnitInfoPage {
  std::string title;  // the tab
  // Draws the page inside the box (x, y, w, h); lineH is the height of a text row. The page keeps whatever it
  // needs from the Battle by capturing it (copy values, never the Battle reference: the page lives while the
  // overlay is open, and the battle is only read-only there anyway).
  std::function<void(Renderer& r, int x, int y, int w, int h, int lineH)> draw;
};
using UnitInfoPages = std::vector<UnitInfoPage>;

void addStatsPage(UnitInfoPages& pages, const Battle& b, int unit);      // U1
void addSkillsPage(UnitInfoPages& pages, const Battle& b, int unit);     // U1
void addAbilitiesPage(UnitInfoPages& pages, const Battle& b, int unit);  // U3
void addSupportsPage(UnitInfoPages& pages, const Battle& b, int unit);   // U5
void addItemsPage(UnitInfoPages& pages, const Battle& b, int unit);      // U1
void addConditionsPage(UnitInfoPages& pages, const Battle& b, int unit); // docs/SRPG.md section 9: "Status" (only for a unit that has a condition)

// All pages of a unit, in the order Stats, Skills, Abilities, Supports, Items.
UnitInfoPages buildUnitInfoPages(const Battle& b, int unit);

class UnitInfoView {
 public:
  enum Result { None, Closed };

  void open(const Battle& b, int unit);  // builds the pages with buildUnitInfoPages
  void openPages(int unit, UnitInfoPages pages);  // the same with the pages given (tests use fakes)
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  int unit() const { return unit_; }
  int page() const { return page_; }
  int pageCount() const { return (int)pages_.size(); }
  // Left / Prev and Right / Next turn the page (wrapping); Cancel closes. A tap outside the box closes, a tap on
  // its left / right half turns the page (the box is the one the last draw() used).
  Result onInput(const InputEvent& e);
  void draw(Renderer& r, const Battle& b, int lineH) const;

 private:
  void turn(int dir);
  bool open_ = false;
  int unit_ = -1, page_ = 0;
  UnitInfoPages pages_;
  mutable int bx_ = 0, by_ = 0, bw_ = 0, bh_ = 0;  // last drawn box
};

}  // namespace ote
