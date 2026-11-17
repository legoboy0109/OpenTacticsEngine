// Experience options, level cap per unit type, stat caps for computer units (docs/SRPG.md "Experience: options", "Level cap per unit
// type", "Stat caps"): rules effects with exact numbers on the miniature game; the data files are in test_fidelity_data.cpp.
#include "abilities_fixture.h"
#include "battle/unit_info_rows.h"
#include "srpg_combat_fixture.h"

using namespace ote;
using namespace combattest;

namespace {

// Soldier (blue) strikes a soldier (red) with a hit chance of 0: both miss, so nobody does damage.
struct Whiff : Duel {
  explicit Whiff(const char* defender = "soldier") : Duel("soldier", defender, 1) {
    w.db.rules.progression = true;
    w.db.rules.fHit = "0";
    w.db.rules.fXpHit = "60";
    w.db.rules.fXpKill = "100";
  }
};

}  // namespace

TEST(xp_no_damage_default_gives_nothing) {
  Whiff d;
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.w.battle.units[0].xp, 0);
  CHECK_EQ(d.w.battle.units[1].xp, 0);
}

TEST(xp_no_damage_gives_a_number_to_everyone_who_struck) {
  Whiff d;
  d.w.db.rules.xpNoDamageExpr = d.w.db.addExpr("7", "test");
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.w.battle.units[0].xp, 7);  // the attacker missed
  CHECK_EQ(d.w.battle.units[1].xp, 7);  // the defender struck back and missed
  CHECK_EQ(d.w.count(EventType::XpGained), 2);
}

TEST(xp_no_damage_defender_that_cannot_answer_gets_none) {
  Whiff d("archer");  // a bow reaches only 2 squares: no answer at 1
  d.w.db.rules.xpNoDamageExpr = d.w.db.addExpr("7", "test");
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.w.battle.units[0].xp, 7);
  CHECK_EQ(d.w.battle.units[1].xp, 0);
}

TEST(xp_no_damage_formula_sees_both_units_and_damage_still_pays_xp_hit) {
  Whiff d;
  d.w.db.rules.xpNoDamageExpr = d.w.db.addExpr("a.level + b.level + 3", "test");
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.w.battle.units[0].xp, 5);
  // The same fight with a sure hit pays xpHit (60), never the no-damage number.
  Duel hit("soldier", "soldier", 1);
  hit.w.db.rules.progression = true;
  hit.w.db.rules.fHit = "100";
  hit.w.db.rules.fXpHit = "60";
  hit.w.db.rules.xpNoDamageExpr = hit.w.db.addExpr("7", "test");
  CHECK(hit.begin());
  CHECK(hit.attack());
  CHECK_EQ(hit.w.battle.units[0].xp, 60);
}

TEST(xp_computer_gains_false_only_human_sides_gain) {
  Duel d("soldier", "soldier", 1);
  d.w.db.rules.progression = true;
  d.w.db.rules.fHit = "100";
  d.w.db.rules.fXpHit = "10";
  d.w.db.rules.computerGains = false;
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.w.battle.units[0].xp, 10);  // blue is human
  CHECK_EQ(d.w.battle.units[1].xp, 0);   // red is the computer's: its counter earns nothing
  Duel e("soldier", "soldier", 1);       // today's behaviour (the default): both gain
  e.w.db.rules.progression = true;
  e.w.db.rules.fHit = "100";
  e.w.db.rules.fXpHit = "10";
  CHECK(e.begin());
  CHECK(e.attack());
  CHECK_EQ(e.w.battle.units[0].xp, 10);
  CHECK_EQ(e.w.battle.units[1].xp, 10);
}

TEST(xp_ability_gives_experience_number_or_formula) {
  abtest::AbWorld w(abtest::Square, false, {{"soldier", 0, 1, 1, false}, {"soldier", 1, 7, 7, false}});
  w.db.rules.progression = true;
  AbilityDef fixed = abtest::ability("mend", AbilityTarget::Self, AbilityEffect::Heal, 3);
  fixed.free = true;
  fixed.xp = 15;
  AbilityDef formula = abtest::ability("mend2", AbilityTarget::Self, AbilityEffect::Heal, 3);
  formula.free = true;
  formula.xpExpr = w.db.addExpr("a.level * 5 + 1", "test");
  AbilityDef none = abtest::ability("mend3", AbilityTarget::Self, AbilityEffect::Heal, 3);
  none.free = true;
  const int a = w.give("soldier", fixed), b = w.give("soldier", formula), c = w.give("soldier", none);
  CHECK(w.begin());
  w.mut(0).hp = 1;
  CHECK(w.use(0, a, 0));
  CHECK_EQ(w.un(0).xp, 15);
  CHECK(w.use(0, b, 0));
  CHECK_EQ(w.un(0).xp, 15 + 6);
  w.events.clear();
  CHECK(w.use(0, c, 0));
  CHECK_EQ(w.un(0).xp, 21);  // an ability without "xp" gives none, as before
  CHECK_EQ(w.count(EventType::XpGained), 0);
}

TEST(xp_ability_experience_can_level_the_user_up) {
  abtest::AbWorld w(abtest::Square, false, {{"soldier", 0, 1, 1, false}, {"soldier", 1, 7, 7, false}});
  w.db.rules.progression = true;
  AbilityDef d = abtest::ability("mend", AbilityTarget::Self, AbilityEffect::Heal, 1);
  d.free = true;
  d.xp = 100;
  const int a = w.give("soldier", d);
  CHECK(w.begin());
  w.mut(0).hp = 5;
  CHECK(w.use(0, a, 0));
  CHECK_EQ(w.un(0).level, 2);
  CHECK_EQ(w.count(EventType::LevelUp), 1);
}

// ---- level cap per unit type ----

TEST(level_cap_type_value_overrides_the_games) {
  Duel d("soldier", "soldier", 1);
  d.w.db.rules.progression = true;
  d.w.db.rules.fHit = "100";
  d.w.db.rules.fXpHit = "100";
  d.w.db.unitTypes[0].maxLevel = 2;  // the soldier tops out at 2 (the game's own cap is 20)
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.w.battle.units[0].level, 2);
  CHECK(d.w.act(Action::endTurn()));
  CHECK(d.w.act(Action::endTurn()));
  CHECK(d.w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(d.w.battle.units[0].level, 2);  // no third level
  CHECK_EQ(d.w.battle.units[0].xp, 0);     // and no experience piling up at the cap
}

TEST(level_cap_zero_never_levels_and_gains_no_xp) {
  Duel d("soldier", "soldier", 1);
  d.w.db.rules.progression = true;
  d.w.db.rules.fHit = "100";
  d.w.db.rules.fXpHit = "100";
  d.w.db.unitTypes[0].maxLevel = 0;
  CHECK(d.begin());
  CHECK(d.attack());
  CHECK_EQ(d.w.battle.units[0].level, 1);
  CHECK_EQ(d.w.battle.units[0].xp, 0);
  CHECK_EQ(d.w.count(EventType::XpGained), 0);  // both soldiers are of that type: neither gains
}

TEST(level_cap_default_is_the_games_and_unit_info_shows_only_an_own_cap) {
  Duel d("soldier", "archer", 2);
  d.w.db.rules.progression = true;
  d.w.db.unitTypes[0].maxLevel = 5;
  CHECK(d.begin());
  auto cap = [&](int unit) -> std::string {
    for (const InfoRow& r : statRows(d.w.battle, d.w.battle.units[(size_t)unit]))
      if (r.label == "Level cap") return r.value;
    return "";
  };
  CHECK_STR(cap(0), "5");
  CHECK_STR(cap(1), "");  // the archer has none of its own: no extra row
  d.w.db.unitTypes[0].maxLevel = 0;
  CHECK_STR(cap(0), "never levels");
}

// ---- stat caps for computer units ----

TEST(caps_computer_units_have_their_own_game_wide_cap) {
  Duel d("soldier", "soldier", 1);
  d.w.db.statCaps = {80, -1, -1, -1};
  d.w.db.computerCaps = {120, -1, -1, -1};
  CHECK(d.begin());
  Battle& b = d.w.battle;
  CHECK_EQ(b.statCap(b.units[0], 0), 80);   // blue (human): the game-wide cap
  CHECK_EQ(b.statCap(b.units[1], 0), 120);  // red (computer): its own
  CHECK_EQ(b.statCap(b.units[1], 1), -1);   // a stat the list leaves out: not capped
}

TEST(caps_computer_cap_applies_to_a_level_up) {
  Duel d("soldier", "soldier", 1);
  d.w.db.rules.progression = true;
  d.w.db.rules.fHit = "100";
  d.w.db.rules.fXpHit = "100";                   // both fighters level up from this one exchange
  d.w.db.unitTypes[0].growths = {0, 0, 0, 5000};  // +50 movement a level (hit points would heal the unit before its foe is paid)
  d.w.db.statCaps = {-1, -1, -1, 80};
  d.w.db.computerCaps = {-1, -1, -1, 120};
  CHECK(d.begin());
  Battle& b = d.w.battle;
  b.units[0].stats[3] = 70;
  b.units[1].stats[3] = 70;
  CHECK(d.attack());
  CHECK_EQ(b.units[0].level, 2);
  CHECK_EQ(b.units[1].level, 2);
  CHECK_EQ(b.units[0].stats[3], 80);   // 70 + 50, cut to the game-wide cap
  CHECK_EQ(b.units[1].stats[3], 120);  // the computer's own cap
}

TEST(caps_a_class_cap_still_wins_and_no_computer_list_changes_nothing) {
  Duel d("soldier", "soldier", 1);
  d.w.db.statCaps = {80, -1, -1, -1};
  d.w.db.computerCaps = {120, -1, -1, -1};
  d.w.db.unitTypes[0].caps = {90, -1, -1, -1};  // the soldier class: 90 for everyone of the type
  CHECK(d.begin());
  CHECK_EQ(d.w.battle.statCap(d.w.battle.units[0], 0), 90);
  CHECK_EQ(d.w.battle.statCap(d.w.battle.units[1], 0), 90);
  Duel e("soldier", "soldier", 1);
  e.w.db.statCaps = {80, -1, -1, -1};
  CHECK(e.begin());
  CHECK_EQ(e.w.battle.statCap(e.w.battle.units[1], 0), 80);  // no computerCaps: the same cap for both
}
