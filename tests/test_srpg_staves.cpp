// Staves in a battle (docs/SRPG.md section 8): the targets, a single heal and what it costs and earns, areas, the hit roll,
// breaking, experience and ranks, the forecast and the words of the refusals. Game: tests/fixtures/srpg-staves (clinic map;
// the layout and the numbers are in tests/srpg_staves_fixture.h).
#include <algorithm>

#include "srpg_staves_fixture.h"

using namespace ote;
using namespace stavestest;

namespace {
int countOf(const std::vector<Event>& ev, EventType t) {
  int n = 0;
  for (const Event& e : ev) n += e.type == t;
  return n;
}
}  // namespace

TEST(staves_with_everybody_unhurt_nothing_is_usable_and_the_reason_is_plain) {
  Clinic w("staves_unhurt");
  CHECK_STR(w.refusal("wen", "mend_staff", "tarn"), "nobody in reach needs that");
  const StaffOption o = w.option("wen", "mend_staff");
  CHECK(!o.ok);
  CHECK_STR(o.why, "nobody in reach needs that");
  std::vector<StaffOption> all;
  w.b().staffOptions(w.id("wen"), w.unit("wen").cell, all);
  CHECK_EQ((int)all.size(), 5);  // every staff she carries, in order
  CHECK_STR(w.option("wen", "high_staff").why, "Needs rank B");  // the wield reason comes first
  CHECK_STR(w.option("mira", "kin_staff").why, "Only Wen can wield this");
  // Nothing was spent or rolled by asking.
  CHECK_EQ(w.unit("wen").items[0].uses, 6);
}

TEST(staves_a_heal_restores_hp_spends_a_use_ends_the_turn_and_earns_experience) {
  Clinic w("staves_heal");
  w.hurt("tarn", 5);
  const uint64_t dice = w.b().rng.state;
  CHECK(w.use("wen", "mend_staff", "tarn"));
  CHECK_EQ(w.unit("tarn").hp, 13);  // 5 + (power 5 + mag 6 / 2) = 8
  CHECK_EQ(w.unit("wen").items[0].uses, 5);
  CHECK(w.unit("wen").done);
  CHECK_EQ(w.b().rng.state, dice);  // a staff that cannot miss rolls nothing
  // The events, in order: the use, the heal, the experience.
  CHECK_EQ((int)w.events.size(), 3);
  CHECK(w.events[0].type == EventType::StaffUsed && w.events[0].a == w.id("wen") && w.events[0].b == w.item("mend_staff") &&
        w.events[0].c == w.id("tarn") && w.events[0].d == 1);
  CHECK(w.events[1].type == EventType::UnitHealed && w.events[1].a == w.id("tarn") && w.events[1].b == 8);
  CHECK(w.events[2].type == EventType::XpGained && w.events[2].a == w.id("wen") && w.events[2].b == 15);
  CHECK_EQ(w.unit("wen").xp, 15);
  // Rank experience: growth "use", rankXp 2 into the staff group (class d 10, character c 25 -> 27).
  CHECK_EQ(w.unit("wen").weaponXp[(size_t)w.rig.db.ranks.findGroup("staff")], 27);
  // The unit has acted: no second order.
  CHECK_STR(w.refusal("wen", "mend_staff", "tarn"), "that unit has already acted");
}

TEST(staves_a_heal_never_goes_above_the_maximum_and_a_unit_that_is_whole_is_not_a_target) {
  Clinic w("staves_cap");
  w.hurt("tarn", 20);  // max 24: 4 missing
  CHECK(w.use("wen", "mend_staff", "tarn"));
  CHECK_EQ(w.unit("tarn").hp, 24);
  CHECK_EQ(w.healed("tarn"), 4);
  Clinic v("staves_cap2");
  v.hurt("tarn", 5);
  CHECK_STR(v.refusal("wen", "mend_staff", "ola"), "nobody in reach needs that");  // ola is whole
  CHECK_STR(v.refusal("wen", "mend_staff", "wen"), "that staff does not work on that unit");  // an ally staff is not for oneself
  CHECK_STR(v.refusal("wen", "mend_staff", "g1"), "that staff does not work on that unit");  // nor for a foe
  v.hurt("g1", 3);
  CHECK_STR(v.refusal("wen", "mend_staff", "g1"), "that staff does not work on that unit");
}

TEST(staves_out_of_reach_and_unwieldable_are_refused_with_their_reason) {
  Clinic w("staves_refuse");
  w.hurt("tarn", 5);
  w.hurt("ola", 5);
  w.unit("wen").stats[(size_t)w.stat("mag")] = 0;  // reach 1
  CHECK_STR(w.refusal("wen", "mend_staff", "tarn"), "that unit is out of range");
  CHECK_STR(w.refusal("tarn", "short_sword", "ola"), "that item is not a staff");
  w.unit("tarn").items.push_back({w.item("mend_staff"), 6, false});
  CHECK_STR(w.refusal("tarn", "mend_staff", "ola"), "the unit cannot wield that");
  CHECK_STR(w.refusal("wen", "high_staff", "tarn"), "Needs rank B");
  CHECK_STR(w.refusal("mira", "kin_staff", "tarn"), "Only Wen can wield this");
  CHECK(!w.run(Action::useStaff(w.id("wen"), 9, w.id("tarn"))));  // no such slot
  CHECK(!w.run(Action::useStaff(w.id("wen"), w.slot("wen", "mend_staff"), 99)));
}

TEST(staves_it_can_be_used_after_moving_and_from_the_square_it_would_move_to) {
  Clinic w("staves_move");
  w.hurt("dax", 4);
  w.unit("wen").stats[(size_t)w.stat("mag")] = 0;  // reach 1
  std::vector<int> t;
  w.b().staffTargets(w.id("wen"), w.slot("wen", "mend_staff"), w.unit("wen").cell, t);
  CHECK(t.empty());
  w.b().staffTargets(w.id("wen"), w.slot("wen", "mend_staff"), w.b().grid.at(4, 3), t);  // next to dax
  CHECK_EQ((int)t.size(), 1);
  CHECK(w.run(Action::move(w.id("wen"), w.b().grid.at(4, 3))));
  CHECK(w.use("wen", "mend_staff", "dax"));
  CHECK_EQ(w.unit("dax").hp, 4 + 5);  // mag 0: power 5 only
}

TEST(staves_an_area_around_the_picked_unit_heals_each_hurt_unit_in_it) {
  Clinic w("staves_radius");
  w.hurt("tarn", 10);
  w.hurt("dax", 20);
  w.hurt("brae", 5);  // 4 squares from tarn: outside the radius
  CHECK(w.use("wen", "rally_staff", "tarn"));
  CHECK_EQ(w.healed("tarn"), 6);
  CHECK_EQ(w.healed("dax"), 4);  // only 4 missing
  CHECK_EQ(w.healed("brae"), 0);
  CHECK_EQ(w.events[0].d, 2);
  CHECK_EQ(countOf(w.events, EventType::UnitHealed), 2);
  CHECK_EQ(countOf(w.events, EventType::XpGained), 1);  // once per use, not per unit
  CHECK_EQ(w.events[w.events.size() - 1].b, 20);
  CHECK_EQ(w.unit("wen").items[1].uses, 2);
  // The picked unit may be whole when a neighbour is hurt (the use is legal if anyone in the area would change)...
  Clinic v("staves_radius2");
  v.hurt("dax", 10);
  CHECK(v.use("wen", "rally_staff", "tarn"));
  CHECK_EQ(v.healed("dax"), 6);
  CHECK_EQ(v.events[0].d, 1);
  // ...and nobody hurt anywhere near it is refused.
  Clinic u("staves_radius3");
  u.hurt("brae", 10);
  CHECK_STR(u.refusal("wen", "rally_staff", "tarn"), "nobody in reach needs that");
  // An area never touches a foe of an ally staff.
  Clinic x("staves_radius4");
  x.hurt("tarn", 10);
  x.hurt("g1", 3);
  x.unit("g1").cell = x.b().grid.at(6, 2);  // next to tarn (blue's own sight sees it: no fog in this game)
  x.b().refreshSight();
  CHECK(x.use("wen", "rally_staff", "tarn"));
  CHECK_EQ(x.unit("g1").hp, 3);
}

TEST(staves_an_area_of_everyone_in_reach_picks_nobody) {
  Clinic w("staves_all");
  w.hurt("tarn", 1);
  w.hurt("dax", 1);
  w.hurt("brae", 1);
  w.hurt("ola", 1);
  w.hurt("mira", 1);
  w.hurt("wen", 1);  // she is the user: not in her own reach (the reach starts at 1)
  std::vector<int> t;
  w.b().staffTargets(w.id("wen"), w.slot("wen", "chorus_staff"), w.unit("wen").cell, t);
  CHECK(t.size() == 1 && t[0] == w.id("wen"));  // nothing to pick: the user alone means "confirm"
  CHECK(w.use("wen", "chorus_staff", nullptr));
  CHECK_EQ(w.events[0].c, -1);
  CHECK_EQ(w.events[0].d, 5);
  CHECK_EQ(w.healed("tarn"), 6);  // 25 % of 24
  CHECK_EQ(w.healed("brae"), 7);  // 25 % of 30, rounded down
  CHECK_EQ(w.healed("mira"), 4);  // 25 % of 18
  CHECK_EQ(w.healed("wen"), 0);
  CHECK_EQ(w.events[w.events.size() - 1].b, 8);  // "a.level + 5" = 3 + 5 experience
  // Ascending unit id.
  int last = -1;
  for (const Event& e : w.events)
    if (e.type == EventType::UnitHealed) {
      CHECK(e.a > last);
      last = e.a;
    }
  // Out of reach (a unit 5 or more squares away) is simply not in the area.
  Clinic v("staves_all2");
  v.hurt("tarn", 1);
  v.unit("tarn").cell = v.b().grid.at(7, 4);
  v.hurt("ola", 1);
  CHECK(v.use("wen", "chorus_staff", nullptr));
  CHECK_EQ(v.healed("tarn"), 0);
  CHECK_EQ(v.healed("ola"), 6);
  // With nobody hurt in reach it is refused whatever the action carries.
  Clinic u("staves_all3");
  CHECK_STR(u.refusal("wen", "chorus_staff", nullptr), "nobody in reach needs that");
}

TEST(staves_a_staff_on_the_user_alone_takes_no_target_and_no_range) {
  Clinic w("staves_self");
  w.hurt("mira", 5);
  w.hurt("wen", 5);
  CHECK(w.use("mira", "rest_staff", nullptr));
  CHECK_EQ(w.unit("mira").hp, 9);
  CHECK_EQ(w.events[0].c, w.id("mira"));
  CHECK_EQ(w.unit("mira").items[2].uses, -1);  // unlimited
  CHECK_EQ(countOf(w.events, EventType::XpGained), 0);  // no "xp" on that staff
}

TEST(staves_the_hit_chance_is_rolled_per_unit_with_the_battles_dice_and_a_miss_still_costs) {
  int hits = 0, misses = 0;
  for (unsigned seed = 1; seed <= 40; ++seed) {
    Clinic w("staves_hit");
    w.unit("wen").items.push_back({w.item("fortune_staff"), 3, false});
    w.hurt("tarn", 5);
    w.b().rng.seed(seed);
    // The forecast says 60 % and rolls nothing.
    StaffForecast f;
    const uint64_t before = w.b().rng.state;
    CHECK(w.b().staffForecast(w.id("wen"), w.slot("wen", "fortune_staff"), w.id("tarn"), w.unit("wen").cell, f));
    CHECK_EQ(w.b().rng.state, before);
    CHECK(f.canMiss && f.hits.size() == 1 && f.hits[0].hit == 60 && f.hits[0].amount == 19);
    CHECK(w.use("wen", "fortune_staff", "tarn"));
    const Unit& wen = w.unit("wen");
    const int slot = w.slot("wen", "fortune_staff");
    CHECK_EQ(wen.items[(size_t)slot].uses, 2);  // spent either way
    CHECK_EQ(countOf(w.events, EventType::XpGained), 1);  // and earned either way
    if (w.healed("tarn") > 0) {
      ++hits;
      CHECK_EQ(w.unit("tarn").hp, 24);
      CHECK_EQ(countOf(w.events, EventType::StaffMissed), 0);
    } else {
      ++misses;
      CHECK_EQ(w.unit("tarn").hp, 5);
      CHECK_EQ(countOf(w.events, EventType::StaffMissed), 1);
      CHECK(w.events[1].type == EventType::StaffMissed && w.events[1].a == w.id("tarn") && w.events[1].b == w.id("wen") &&
            w.events[1].c == w.item("fortune_staff"));
    }
    // The same seed gives the same result.
    Clinic again("staves_hit2");
    again.unit("wen").items.push_back({again.item("fortune_staff"), 3, false});
    again.hurt("tarn", 5);
    again.b().rng.seed(seed);
    CHECK(again.use("wen", "fortune_staff", "tarn"));
    CHECK(savetest::sameEvents(w.events, again.events));
  }
  CHECK(hits > 10 && misses > 5);  // about 60 / 40
}

TEST(staves_the_last_use_breaks_the_staff) {
  Clinic w("staves_break");
  w.unit("wen").items.push_back({w.item("last_light"), 1, false});
  w.hurt("tarn", 5);
  const int slot = w.slot("wen", "last_light");
  CHECK(w.use("wen", "last_light", "tarn"));
  CHECK_EQ(w.unit("tarn").hp, 15);
  CHECK(w.events[0].type == EventType::StaffUsed);
  CHECK(w.events[1].type == EventType::UnitHealed);
  CHECK(w.events[2].type == EventType::ItemBroke && w.events[2].a == w.id("wen") && w.events[2].b == w.item("last_light"));
  CHECK(w.events[3].type == EventType::XpGained && w.events[3].b == 5);
  CHECK_EQ(w.slot("wen", "last_light"), -1);
  CHECK_EQ((int)w.unit("wen").items.size(), 5);
  (void)slot;
  // The forecast says so beforehand.
  Clinic v("staves_break2");
  v.unit("wen").items.push_back({v.item("last_light"), 1, false});
  v.hurt("tarn", 5);
  StaffForecast f;
  CHECK(v.b().staffForecast(v.id("wen"), v.slot("wen", "last_light"), v.id("tarn"), v.unit("wen").cell, f));
  CHECK(f.breaks && f.usesLeft == 1 && f.xp == 5 && !f.canMiss && f.minRange == 1 && f.maxRange == 2);
  CHECK(f.hits.size() == 1 && f.hits[0].hpBefore == 5 && f.hits[0].hpAfter == 15 && f.hits[0].hit == 100);
}

TEST(staves_rank_experience_grows_with_use_and_ranks_up) {
  Clinic w("staves_rankup");
  const int staff = w.rig.db.ranks.findGroup("staff");
  w.unit("wen").weaponXp[(size_t)staff] = 44;
  w.hurt("tarn", 5);
  CHECK(w.use("wen", "mend_staff", "tarn"));
  CHECK_EQ(w.unit("wen").weaponXp[(size_t)staff], 45);  // stops at the top rank's number
  const Event* up = w.first(EventType::RankUp);
  CHECK(up && up->a == w.id("wen") && up->b == staff && up->c == w.rig.db.ranks.findRank("b") && up->d == w.rig.db.ranks.findRank("c"));
  // At rank B the high staff is wieldable at once.
  CHECK(w.b().canWieldStaff(w.unit("wen"), w.item("high_staff")));
  // The experience stops at the top rank.
  Clinic v("staves_rankup2");
  v.unit("wen").weaponXp[(size_t)staff] = 45;
  v.hurt("tarn", 5);
  CHECK(v.use("wen", "mend_staff", "tarn"));
  CHECK_EQ(v.unit("wen").weaponXp[(size_t)staff], 45);
  CHECK(!v.first(EventType::RankUp));
  // Growth "fixed" trains nothing.
  Clinic f("staves_rankfixed", "clinic", [](Copy& c) { c.swap("data/ranks.json", "\"growth\": \"use\"", "\"growth\": \"fixed\""); });
  f.hurt("tarn", 5);
  CHECK(f.use("wen", "mend_staff", "tarn"));
  CHECK_EQ(f.unit("wen").weaponXp[(size_t)staff], 25);
}

TEST(staves_experience_can_level_the_user_up_and_needs_progression) {
  Clinic w("staves_levelup");
  w.unit("wen").xp = 95;
  w.hurt("tarn", 5);
  CHECK(w.use("wen", "mend_staff", "tarn"));
  CHECK_EQ(w.unit("wen").level, 4);
  CHECK_EQ(w.unit("wen").xp, 10);
  CHECK(w.first(EventType::LevelUp));
  // A staff without "xp" earns none.
  Clinic v("staves_noxp");
  v.hurt("tarn", 5);
  CHECK(v.use("wen", "kin_staff", "tarn"));
  CHECK_EQ(v.unit("wen").xp, 0);
  CHECK_EQ(countOf(v.events, EventType::XpGained), 0);
}

TEST(staves_a_formula_heal_reads_the_users_stats_and_the_staffs_numbers) {
  Clinic w("staves_formula");
  w.hurt("tarn", 1);
  w.unit("wen").stats[(size_t)w.stat("mag")] = 10;  // 5 + 10 / 2 = 10
  StaffForecast f;
  CHECK(w.b().staffForecast(w.id("wen"), w.slot("wen", "mend_staff"), w.id("tarn"), w.unit("wen").cell, f));
  CHECK_EQ(f.hits[0].amount, 10);
  CHECK_EQ(f.hits[0].nominal, 10);
  w.unit("wen").stats[(size_t)w.stat("mag")] = 4;  // 5 + 2 = 7
  CHECK(w.b().staffForecast(w.id("wen"), w.slot("wen", "mend_staff"), w.id("tarn"), w.unit("wen").cell, f));
  CHECK_EQ(f.hits[0].amount, 7);
}

TEST(staves_a_game_without_staves_has_none_of_it) {
  savetest::Rig rig;
  CHECK(rig.loadDemo("demo-srpg", "bridge"));
  CHECK(rig.db.staves.empty());
  rig.start();
  std::vector<StaffOption> out;
  rig.battle.staffOptions(0, rig.battle.units[0].cell, out);
  CHECK(out.empty());
  std::string why;
  CHECK(!rig.battle.validate(Action::useStaff(0, 0, 1), &why));
}
