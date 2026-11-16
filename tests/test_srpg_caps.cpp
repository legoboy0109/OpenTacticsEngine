// Stat caps (docs/SRPG.md section 7): where a permanent stat stops growing, and that bonuses go on top.
#include "srpg_ranks_fixture.h"

using namespace ote;
using namespace rankstest;

TEST(caps_sources_class_over_game_plus_character) {
  World w("caps_sources", "duel");
  const int str = w.stat("str"), def = w.stat("def"), hp = w.stat("hp");
  CHECK_EQ(w.b().statCap(w.unit("kell"), str), 10);  // the game-wide cap
  CHECK_EQ(w.b().statCap(w.unit("mara"), str), 12);  // 10 + her adjustment of 2
  CHECK_EQ(w.b().statCap(w.unit("kell"), hp), 40);
  CHECK_EQ(w.b().statCap(w.unit("kell"), def), -1);  // no cap anywhere: uncapped
  // A knight's class cap (14) overrides the game's 10.
  w.unit("kell").type = w.rig.db.findUnitType("knight");
  CHECK_EQ(w.b().statCap(w.unit("kell"), str), 14);
}

TEST(caps_a_level_up_stops_at_the_cap) {
  World w("caps_levelup", "duel", [](Copy& c) { c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"100\""); });
  const int str = w.stat("str");
  w.unit("kell").stats[(size_t)str] = 9;
  w.unit("kell").xp = 99;
  CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("bronze_sword"))));
  const Event* up = w.first(EventType::LevelUp);
  CHECK(up != nullptr);
  if (up) CHECK_EQ(up->data[(size_t)str], 1);  // growth 100: +1, room for it (9 -> 10)
  CHECK_EQ(w.unit("kell").stats[(size_t)str], 10);
  // At the cap the same level-up gives nothing; the event says so.
  w.unit("kell").done = w.unit("kell").moved = false;  // the next turn
  w.unit("kell").xp = 99;
  CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("bronze_sword"))));
  up = w.first(EventType::LevelUp);
  CHECK(up != nullptr);
  if (up) CHECK_EQ(up->data[(size_t)str], 0);
  CHECK_EQ(w.unit("kell").stats[(size_t)str], 10);
}

TEST(caps_an_item_is_cut_and_refused_at_the_cap) {
  World w("caps_item", "duel");
  const int str = w.stat("str");
  const int slot = w.slotOf("kell", "might_draught");
  w.unit("kell").stats[(size_t)str] = 8;  // +3 would be 11: cut to 10
  CHECK(w.run(Action::useItem(w.id("kell"), slot, w.id("kell"))));
  const Event* raised = w.first(EventType::StatsRaised);
  CHECK(raised != nullptr);
  if (raised) CHECK_EQ(raised->data[(size_t)str], 2);
  CHECK_EQ(w.unit("kell").stats[(size_t)str], 10);
  // At the cap the item would do nothing, so it is refused.
  World full("caps_item2", "duel");
  full.unit("kell").stats[(size_t)str] = 10;
  const std::string why = full.why(Action::useItem(full.id("kell"), full.slotOf("kell", "might_draught"), full.id("kell")));
  CHECK(saysIn(why, "would do nothing"));
}

TEST(caps_promotion_judges_the_new_class_and_never_lowers) {
  World w("caps_promo", "duel");
  const int str = w.stat("str");
  w.unit("kell").stats[(size_t)str] = 13;  // fighter 6 -> knight 8: +2, the knight's cap is 14: +1
  CHECK(w.run(Action::promote(w.id("kell"))));
  const Event* p = w.first(EventType::UnitPromoted);
  CHECK(p != nullptr);
  if (p) CHECK_EQ(p->data[(size_t)str], 1);
  CHECK_EQ(w.unit("kell").stats[(size_t)str], 14);
  World above("caps_promo2", "duel");
  above.unit("kell").stats[(size_t)str] = 16;  // placed above its cap: stays
  CHECK(above.run(Action::promote(above.id("kell"))));
  CHECK_EQ(above.unit("kell").stats[(size_t)str], 16);
}

TEST(caps_bonuses_go_on_top_of_the_permanent_stat) {
  World w("caps_bonus", "duel");
  const int str = w.stat("str");
  Unit& kell = w.unit("kell");
  kell.stats[(size_t)str] = 10;  // at the cap
  ItemStack ring;
  ring.item = w.item("power_ring");  // +3 while carried
  kell.items.push_back(ring);
  CHECK_EQ(w.b().stat(kell, str), 13);  // the rules see 13: caps limit growing, not being
  CHECK_EQ(kell.stats[(size_t)str], 10);
}

TEST(caps_a_unit_without_caps_anywhere_grows_freely) {
  World w("caps_none", "duel", [](Copy& c) {
    c.drop("data/caps.json");
    c.swap("data/units.json", ", \"caps\": { \"str\": 14 }", "");
  });
  CHECK_EQ(w.b().statCap(w.unit("kell"), w.stat("str")), -1);
  w.unit("kell").stats[(size_t)w.stat("str")] = 30;
  const int slot = w.slotOf("kell", "might_draught");
  CHECK(w.run(Action::useItem(w.id("kell"), slot, w.id("kell"))));
  CHECK_EQ(w.unit("kell").stats[(size_t)w.stat("str")], 33);
}
