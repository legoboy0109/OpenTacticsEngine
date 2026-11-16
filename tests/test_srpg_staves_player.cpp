// Staves in the player's words and event hooks (docs/SRPG.md section 8 "Player"): the reasons in their three lengths, the
// effect and hit texts, the menu id ranges, and the view playing StaffUsed / StaffMissed with the heal that follows.
#include "battle/battle_view.h"
#include "battle/rank_text.h"
#include "battle/srpg_menu_ids.h"
#include "battle/staff_text.h"
#include "srpg_staves_fixture.h"

using namespace ote;
using namespace stavestest;

namespace {
bool floated(const BattleView& v, const std::string& text) {
  for (const FloatText& f : v.floats)
    if (f.text == text) return true;
  return false;
}
// Plays `events` on a view of the battle and says whether a float with `text` showed up on the way.
bool plays(Clinic& w, const std::vector<Event>& events, const std::string& text) {
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
}  // namespace

TEST(player_staves_reasons_come_in_three_lengths) {
  CHECK_STR(staffReasonText("nobody in reach needs that", 0), "Nobody in reach needs it");
  CHECK_STR(staffReasonText("nobody in reach needs that", 1), "Nobody needs it");
  CHECK_STR(staffReasonText("nobody in reach needs that", 2), "No one");
  CHECK_STR(staffReasonText("the unit cannot wield that", 0), "Cannot wield");
  CHECK_STR(staffReasonText("the unit cannot wield that", 2), "Class");
  CHECK_STR(staffReasonText("Needs rank C", 0), "Needs rank C");
  CHECK_STR(staffReasonText("Needs rank C", 2), "Rank C");
  CHECK_STR(staffReasonText("Only Mara can wield this", 0), "Only Mara can wield this");
  CHECK_STR(staffReasonText("Only Mara can wield this", 1), "Only Mara");
  CHECK_STR(staffReasonText("Only Mara and Ser Ovan can wield this", 1), "Only Mara and Ser Ovan");
  CHECK_STR(staffReasonText("Only Mara can wield this", 2), "Locked");
  CHECK_STR(staffReasonText("something else", 2), "something else");
}

TEST(player_staves_effect_and_hit_texts) {
  Clinic w("player_staves_text");
  const Database& db = w.rig.db;
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("rally_staff")]), "Heals 6 HP");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("chorus_staff")]), "Heals 25%");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("fortune_staff")]), "Heals all HP");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("mend_staff")]), "Heals");  // a formula decides it
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("short_sword")]), "");
  w.hurt("tarn", 5);
  StaffForecast f;
  CHECK(w.b().staffForecast(w.id("wen"), w.slot("wen", "mend_staff"), w.id("tarn"), w.unit("wen").cell, f));
  CHECK_STR(staffHitText(f), "");  // cannot miss: no line
  w.unit("wen").items.push_back({w.item("fortune_staff"), 3, false});
  CHECK(w.b().staffForecast(w.id("wen"), w.slot("wen", "fortune_staff"), w.id("tarn"), w.unit("wen").cell, f));
  CHECK_STR(staffHitText(f), "Hit 60%");
}

TEST(player_staves_menu_ids_have_their_own_group) {
  CHECK(srpgGroupOf(kStaffEntry) == SrpgGroup::Staff);
  CHECK(srpgGroupOf(kStaffBase) == SrpgGroup::Staff);
  CHECK(srpgGroupOf(kStaffBase + 4) == SrpgGroup::Staff);
  CHECK(srpgGroupOf(kStaffBase - 1) == SrpgGroup::Support);
  CHECK(srpgGroupOf(kSrpgRangeEnd) == SrpgGroup::None);
  CHECK(kStaffEntry != kUnitEntry && kStaffEntry < kTalkBase);
}

TEST(player_staves_a_greyed_staff_is_greyed_with_its_reason_on_the_items_screen) {
  Clinic w("player_staves_grey");
  const Unit& wen = w.unit("wen");
  CHECK_STR(weaponBlockReason(w.b(), wen, wen.items[(size_t)w.slot("wen", "high_staff")]), "Needs rank B");
  CHECK_STR(weaponBlockReason(w.b(), wen, wen.items[(size_t)w.slot("wen", "mend_staff")]), "");
  const Unit& mira = w.unit("mira");
  CHECK_STR(weaponBlockReason(w.b(), mira, mira.items[(size_t)w.slot("mira", "kin_staff")]), "Only Wen can wield this");
}

TEST(player_staves_the_view_plays_a_use_the_name_the_heal_and_a_miss) {
  Clinic v("player_staves_view");
  v.hurt("tarn", 5);
  std::vector<Event> events;  // the use on a fresh copy of the same battle, played by a view that starts before it
  BattleView view;
  view.init(v.b(), 24, false);
  CHECK(v.b().apply(v.act("wen", "mend_staff", "tarn"), events));
  view.push(events);
  bool name = false, plus = false, xp = false;
  for (int i = 0; i < 200 && (i == 0 || view.busy()); ++i) {
    view.update(0.05, v.b());
    name |= floated(view, "Mend Staff");
    plus |= floated(view, "+8");
    for (const FloatText& f : view.floats) xp |= f.text.find("15") != std::string::npos;
  }
  CHECK(name && plus && xp);
  CHECK(!view.busy());
  // A miss floats "Missed" over the unit that was missed.
  Event miss;
  miss.type = EventType::StaffMissed;
  miss.a = v.id("tarn");
  miss.b = v.id("wen");
  miss.c = v.item("fortune_staff");
  CHECK(plays(v, {miss}, "Missed"));
  // A staff that breaks says so.
  Clinic c("player_staves_view2");
  c.unit("wen").items.push_back({c.item("last_light"), 1, false});
  c.hurt("tarn", 5);
  std::vector<Event> broke;
  BattleView bv;
  bv.init(c.b(), 24, false);
  CHECK(c.b().apply(c.act("wen", "last_light", "tarn"), broke));
  bv.push(broke);
  bool said = false;
  for (int i = 0; i < 200 && (i == 0 || bv.busy()); ++i) {
    bv.update(0.05, c.b());
    said |= floated(bv, "Last Light broke");
  }
  CHECK(said);
}
