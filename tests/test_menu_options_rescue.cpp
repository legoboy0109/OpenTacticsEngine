// Menu options, "hide" (docs/SRPG.md "Menu options"): Drop with no free square is marked as having nothing to act on, so "hide" leaves it
// out; Drop with room, and the entries that are only offered when they can be used, are not touched.
#include "battle/menu_hide.h"
#include "battle/rescue_menu.h"
#include "battle/srpg_menu_ids.h"
#include "rescue_fixture.h"

using namespace ote;
using namespace rescuetest;

TEST(menu_options_drop_with_no_room_is_marked_and_hide_leaves_it_out) {
  // The corner carrier (0,0), the carried unit 1 at (1,0), unit 2 at (0,1); unit 3 steps into the freed square.
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 0, 1, false},
                                    {"soldier", 0, 1, 1, false}, {"soldier", 1, 2, 1, false}},
                {"...", "..."});
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  // Room: Drop is a usable entry and is never left out.
  std::vector<RescueEntry> e = rescueEntries(w.battle, 0);
  CHECK(!e.empty() && e[0].id == kDrop && e[0].enabled && !e[0].noTarget);
  CHECK(!rescueEntryHidden(true, e[0]));
  CHECK(w.act(Action::move(3, w.at(1, 0))));  // takes the square the carried unit left
  e = rescueEntries(w.battle, 0);
  CHECK(!e.empty() && e[0].id == kDrop);
  CHECK(!e[0].enabled && e[0].extra == "no room" && e[0].noTarget);
  CHECK(rescueEntryHidden(true, e[0]));    // "hide": gone
  CHECK(!rescueEntryHidden(false, e[0]));  // "grey" (today): greyed with the reason
}
