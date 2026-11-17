// Staves, stage two, in the player's words and event hooks (docs/SRPG.md section 8): the new reasons in their three lengths, the
// effect texts, the forecast lines for a move / a repair / a light, the reasons given for a tapped unit, and the view playing
// ItemRepaired and AreaLit. Game: tests/fixtures/srpg-staves2.
#include "battle/battle_view.h"
#include "battle/srpg_menu_ids.h"
#include "battle/staff_text.h"
#include "srpg_staves2_fixture.h"

using namespace ote;
using namespace staves2test;

namespace {
bool floated(const BattleView& v, const std::string& text) {
  for (const FloatText& f : v.floats)
    if (f.text == text) return true;
  return false;
}
bool plays(Yard& w, const std::vector<Event>& events, const std::string& text) {
  BattleView view;
  view.init(w.b(), 24, false);
  view.push(events);
  bool seen = false;
  for (int i = 0; i < 200 && (i == 0 || view.busy()); ++i) {
    view.update(0.05, w.b());
    seen |= floated(view, text);
  }
  CHECK(!view.busy());
  return seen;
}
StaffForecast forecast(Yard& w, const char* staff, int target, Cell dest = kNoCell, int itemSlot = -1) {
  StaffForecast f;
  CHECK(w.b().staffForecast(w.id("sage"), w.slot("sage", staff), target, w.cellOf("sage"), f, dest, itemSlot));
  return f;
}
}  // namespace

TEST(player_staves2_the_new_reasons_come_in_three_lengths) {
  CHECK_STR(staffReasonText("nobody in reach can be moved", 0), "Nobody in reach can be moved");
  CHECK_STR(staffReasonText("nobody in reach can be moved", 1), "Nobody to move");
  CHECK_STR(staffReasonText("nobody in reach can be moved", 2), "No one");
  CHECK_STR(staffReasonText("nobody in reach has an item to repair", 0), "Nothing in reach to repair");
  CHECK_STR(staffReasonText("nobody in reach has an item to repair", 1), "Nothing to repair");
  CHECK_STR(staffReasonText("this map has no fog of war", 0), "No fog on this map");
  CHECK_STR(staffReasonText("this map has no fog of war", 2), "No fog");
  CHECK_STR(staffReasonText("nothing in reach is hidden", 1), "Nothing hidden");
}

TEST(player_staves2_a_tapped_unit_that_cannot_be_picked_gets_a_reason_in_the_players_words) {
  CHECK_STR(staffWhyText("that unit is out of range"), "Out of range");
  CHECK_STR(staffWhyText("that unit is already next to the user"), "Already next to you");
  CHECK_STR(staffWhyText("there is no free square next to the user that it could stand on"), "No room next to you");
  CHECK_STR(staffWhyText("there is no free square in reach of the user that it could stand on"), "No free square in reach");
  CHECK_STR(staffWhyText("they could not stand on each other's squares"), "Cannot stand there");
  CHECK_STR(staffWhyText("that unit carries nothing that needs repair"), "Nothing to repair");
  CHECK_STR(staffWhyText(""), "");
  Yard w("player_s2_why");
  CHECK_STR(staffWhyText(w.b().staffTargetWhy(w.id("sage"), w.slot("sage", "recall_staff"), w.id("wisp"), w.cellOf("sage"))), "Already next to you");
  CHECK_STR(staffWhyText(w.b().staffTargetWhy(w.id("sage"), w.slot("sage", "swap_staff"), w.id("wisp"), w.cellOf("sage"))), "Cannot stand there");
  CHECK_STR(staffWhyText(w.b().staffTargetWhy(w.id("sage"), w.slot("sage", "fix_staff"), w.id("bram"), w.cellOf("sage"))), "Nothing to repair");
  CHECK_STR(staffWhyText(w.b().staffTargetWhy(w.id("sage"), w.slot("sage", "warp_staff"), w.id("ren"), w.cellOf("sage"))), "Out of range");
  CHECK_STR(w.b().staffTargetWhy(w.id("sage"), w.slot("sage", "warp_staff"), w.id("bram"), w.cellOf("sage")), "");  // valid: no reason
}

TEST(player_staves2_effect_texts_and_forecast_lines) {
  Yard w("player_s2_text");
  const Database& db = w.rig.db;
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("warp_staff")]), "Warps a unit up to 4 squares away");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("recall_staff")]), "Pulls a unit next to you");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("swap_staff")]), "Swaps places");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("fix_staff")]), "Repairs an item fully");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("patch_staff")]), "Repairs 2 uses");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("lantern_staff")]), "Lights an area");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("kindly_pull_staff")]), "Heals 6 HP");  // the first effect
  // A warp: first "pick a square", then where it goes.
  StaffForecast f = forecast(w, "warp_staff", w.id("bram"));
  CHECK(f.needsDest && f.hits.size() == 1);
  CHECK_STR(staffMoveLines(w.b(), f, f.hits[0])[0], "Bram: pick a square");
  f = forecast(w, "warp_staff", w.id("bram"), w.at(1, 1));
  CHECK(!f.needsDest && f.hits[0].moveTo == w.at(1, 1));
  CHECK_STR(staffMoveLines(w.b(), f, f.hits[0])[0], "Bram warps there");
  f = forecast(w, "recall_staff", w.id("ren"));
  CHECK(f.hits[0].moveTo == w.at(2, 1));
  CHECK_STR(staffMoveLines(w.b(), f, f.hits[0])[0], "Ren is pulled next to you");
  f = forecast(w, "swap_staff", w.id("bram"));
  CHECK(f.hits[0].moveTo == w.at(2, 2) && f.hits[0].userTo == w.at(2, 4));
  CHECK_STR(staffMoveLines(w.b(), f, f.hits[0])[0], "Bram and Sage swap places");
  // A repair: pick an item, then "Iron Blade  3 > 8".
  w.place("ren", 2, 3);
  w.setUses("ren", "iron_blade", 3);
  f = forecast(w, "fix_staff", w.id("ren"));
  CHECK(f.needsItem);
  CHECK_STR(staffMoveLines(w.b(), f, f.hits[0])[0], "Pick an item to repair");
  f = forecast(w, "fix_staff", w.id("ren"), kNoCell, w.slotOf("ren", "iron_blade"));
  CHECK(!f.needsItem);
  CHECK_STR(staffMoveLines(w.b(), f, f.hits[0])[0], "Iron Blade  3 > 8");
}

TEST(player_staves2_a_light_forecast_says_how_many_squares_and_for_how_long) {
  Yard w("player_s2_light", "mist");
  StaffForecast f;
  CHECK(w.b().staffForecast(w.id("sage"), w.slot("sage", "lantern_staff"), (int)w.at(7, 2), w.cellOf("sage"), f));
  CHECK(f.isLight && f.hits.empty() && f.lightRadius == 2 && f.lightTurns == 2 && f.lightCell == w.at(7, 2));
  CHECK_EQ(f.area.size(), size_t(13));  // radius 2 on a square grid
  CHECK_EQ(f.newSquares, 13);           // all of it is out of sight now
  const std::vector<std::string> lines = staffLightLines(f);
  CHECK_EQ((int)lines.size(), 2);
  CHECK_STR(lines[0], "Lights 13 new squares");
  CHECK_STR(lines[1], "2 turns");
  CHECK(!w.b().staffForecast(w.id("sage"), w.slot("sage", "spark_staff"), (int)w.at(1, 1), w.cellOf("sage"), f));  // radius 1 there: all in sight already
}

TEST(player_staves2_the_view_plays_a_repair_and_a_light) {
  Yard w("player_s2_view");
  w.place("ren", 2, 3);
  w.setUses("ren", "iron_blade", 3);
  CHECK(w.use("sage", "fix_staff", "ren", -1, -1, w.slotOf("ren", "iron_blade")));
  CHECK(plays(w, w.events, "Iron Blade +5"));  // the floating text over the unit
  Yard m("player_s2_view_light", "mist");
  CHECK(m.use("sage", "lantern_staff", nullptr, 7, 2));
  CHECK(plays(m, m.events, "Lit"));
  CHECK(StaffWords::kLit == std::string("Lit"));
  Yard f("player_s2_view_blocked", "mist");  // a warp into the brush where an enemy hides unseen
  CHECK(f.use("sage", "warp_staff", "ola", 4, 3));
  CHECK(plays(f, f.events, "Blocked!"));
  CHECK(srpgGroupOf(kStaffItemBase + 3) == SrpgGroup::Staff && kStaffItemBase > kStaffBase + 99);
}
