// Step K2 of docs/SRPG.md: timed blocks (turnStart, defeat, afterCombat) and "actAgain". Duel: tests/srpg_skills_duel.h.
#include "srpg_skills_duel.h"

using namespace ote;
using namespace duel;

namespace {

std::string TS(const std::string& effects, const std::string& chance = "", const std::string& cond = "") {
  return block("turnStart", effects, cond, chance);
}
std::string AC(const std::string& effects, const std::string& cond = "") { return block("afterCombat", effects, cond); }
std::string DF(const std::string& effects, const std::string& cond = "") { return block("defeat", effects, cond); }

const Event& firstOf(const Duel& d, EventType t) {
  static const Event none;
  for (const Event& e : d.w.events)
    if (e.type == t) return e;
  return none;
}

// Ends blue's turn; the events of both turns that follow (red's, then the start of blue's) are in `out`.
void endBlueTurn(Duel& d, std::vector<Event>& out) {
  out.clear();
  CHECK(d.w.apply(Action::endTurn()));
  out = d.w.events;
}

}  // namespace

TEST(skills_turn_start_heals_the_active_sides_units_in_id_order) {
  Options o;
  o.aSkills = "\"mend\"";
  o.dSkills = "\"mend\"";
  o.extraUnits = ", { \"type\": \"ua\", \"side\": \"blue\", \"x\": 0, \"y\": 2 }";  // unit 2, blue
  Duel d(skill("mend", TS("{ \"heal\": { \"percent\": 10 } }")), o);
  d.u(0).hp = d.u(1).hp = d.u(2).hp = 20;
  std::vector<Event> ev;
  endBlueTurn(d, ev);  // red's turn starts: only unit 1 heals
  CHECK_EQ(d.hp(1), 23);
  CHECK_EQ(d.hp(0), 20);
  CHECK_STR(d.story(), "sh");
  CHECK_EQ(firstOf(d, EventType::SkillActivated).a, 1);
  CHECK_EQ(firstOf(d, EventType::SkillActivated).b, d.skill("mend"));
  CHECK_EQ(firstOf(d, EventType::UnitHealed).b, 3);
  d.w.events.clear();
  CHECK(d.w.apply(Action::endTurn()));  // blue's turn: units 0 then 2, in id order
  CHECK_STR(d.story(), "shsh");
  std::vector<int> healed;
  for (const Event& e : d.w.events)
    if (e.type == EventType::UnitHealed) healed.push_back(e.a);
  CHECK(healed == (std::vector<int>{0, 2}));
  // Full health: nothing changes and a block without a chance says nothing.
  d.u(0).hp = 30;
  d.u(2).hp = 30;
  CHECK(d.w.apply(Action::endTurn()));
  CHECK_STR(d.story(), "sh");  // red's unit 1 only
}

TEST(skills_turn_start_heal_allies_and_hurt_enemies_within_reach) {
  Options o;
  o.aSkills = "\"aura\"";
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"blue\", \"x\": 0, \"y\": 2 }, { \"type\": \"uc\", \"side\": \"blue\", \"x\": 6, \"y\": 2 }";
  Duel d(skill("aura", TS("{ \"heal\": 5, \"to\": \"allies\", \"within\": 1 }, { \"hurt\": 100, \"to\": \"enemies\", \"within\": 1 }")), o);
  d.u(2).hp = d.u(3).hp = 10;
  d.u(1).hp = 7;
  std::vector<Event> ev;
  endBlueTurn(d, ev);  // red's turn: nothing of blue's
  CHECK_EQ(d.hp(2), 10);
  d.w.events.clear();
  CHECK(d.w.apply(Action::endTurn()));  // blue's turn
  CHECK_EQ(d.hp(2), 15);  // one step away: healed
  CHECK_EQ(d.hp(3), 10);  // far away
  CHECK_EQ(d.hp(1), 1);   // the enemy next to it loses all it can, never the last HP
  bool hurt = false;
  for (const Event& e : d.w.events) hurt |= e.type == EventType::UnitHurt && e.a == 1 && e.b == 6 && e.c == 0;
  CHECK(hurt);
}

TEST(skills_turn_start_activation_rolls_with_the_battle_rng_by_the_usual_rule) {
  Options o;
  o.aSkills = "\"lucky\", \"sure\", \"never\"";
  Duel d(skill("lucky", TS("{ \"heal\": 4 }", "50")) + "," + skill("sure", TS("{ \"heal\": 1 }", "100")) + "," +
         skill("never", TS("{ \"heal\": 9 }", "0")), o);
  d.u(0).hp = 10;
  Rng r = d.b().rng;
  std::vector<Event> ev;
  endBlueTurn(d, ev);  // red's turn (no skills of unit 1)
  d.w.events.clear();
  const bool lucky = r.chance(50);  // exactly one roll: 100 and 0 take none
  CHECK(d.w.apply(Action::endTurn()));
  CHECK(d.b().rng.state == r.state);
  CHECK_EQ(d.hp(0), 10 + (lucky ? 4 : 0) + 1);
}

TEST(skills_after_combat_runs_the_attacker_then_the_defender_and_never_kills) {
  Options o;
  o.aSkills = "\"jab\"";
  o.dSkills = "\"jab\"";
  Duel d(skill("jab", AC("{ \"hurt\": 100 }")), o);
  CHECK(d.attack());
  // A strikes, D answers; then A's block (hurt the foe), then D's. Neither can take the last HP.
  CHECK_STR(d.story(), "ADsxsx");
  CHECK_EQ(d.hp(1), 1);
  CHECK_EQ(d.hp(0), 1);
  CHECK(d.u(0).alive && d.u(1).alive);
  std::vector<int> owners;  // the attacker's activation comes first
  for (const Event& e : d.w.events)
    if (e.type == EventType::SkillActivated) owners.push_back(e.a);
  CHECK(owners == (std::vector<int>{0, 1}));
}

TEST(skills_defeat_runs_only_for_the_one_whose_foe_fell_then_after_combat) {
  Options o;
  o.aSkills = "\"catch\", \"after\"";
  o.dSkills = "\"catch\"";
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"red\", \"x\": 7, \"y\": 2 }";  // so the battle goes on
  Duel d(skill("catch", DF("{ \"heal\": { \"percent\": 25 } }")) + "," + skill("after", AC("{ \"heal\": 1 }")), o);
  d.u(0).hp = 10;
  d.u(1).hp = 30;
  CHECK(d.attack());  // nobody falls: "defeat" does nothing; "after" heals 1
  CHECK_EQ(d.hp(0), 10 - 4 + 1);
  d.u(0).done = false;
  d.u(1).hp = 4;
  d.u(0).hp = 10;
  CHECK(d.attack());  // unit 1 falls: unit 0's "defeat" heals 25 % of 30 = 7, then "after" 1; unit 1 is gone
  CHECK(!d.u(1).alive);
  CHECK_EQ(d.hp(0), 10 + 7 + 1);
  CHECK_STR(d.story(), "Ashsh");  // the strike, then "defeat" (activation, heal), then "afterCombat"
}

TEST(skills_act_again_clears_the_turn_flags_once_per_turn) {
  Options o;
  o.aSkills = "\"charge\"";
  o.extraUnits = ", { \"type\": \"ub\", \"side\": \"red\", \"x\": 0, \"y\": 0 }, { \"type\": \"ub\", \"side\": \"red\", \"x\": 0, \"y\": 2 }";
  Duel d(skill("charge", DF("{ \"actAgain\": true }", "{ \"initiating\": true }")), o);
  d.u(1).hp = 1;
  d.u(2).hp = 1;
  d.u(3).hp = 1;
  CHECK(d.w.apply(Action::attack(0, 1, d.weapon(0))));
  CHECK(!d.u(0).done && !d.u(0).moved && d.u(0).refreshed);
  CHECK_EQ(d.w.count(EventType::SkillActivated), 1);
  CHECK(d.w.apply(Action::attack(0, 2, d.weapon(0))));  // acts again: and this kill gives no second refresh
  CHECK(d.u(0).done);
  CHECK(d.u(0).refreshed);
  CHECK_EQ(d.w.count(EventType::SkillActivated), 0);
  CHECK(!d.w.apply(Action::attack(0, 3, d.weapon(0))));
  // The next turn starts clean.
  std::vector<Event> ev;
  endBlueTurn(d, ev);
  d.w.events.clear();
  CHECK(d.w.apply(Action::endTurn()));
  CHECK(!d.u(0).refreshed && !d.u(0).done);
}

TEST(skills_act_again_after_a_move_gives_back_the_whole_turn) {
  Options o;
  o.aSkills = "\"charge\"";
  o.gap = 2;
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"red\", \"x\": 7, \"y\": 2 }";
  Duel d(skill("charge", DF("{ \"actAgain\": true }")), o);
  d.u(1).hp = 1;
  CHECK(d.w.apply(Action::move(0, d.w.at(1, 1))));
  CHECK(d.u(0).moved && d.u(0).moveSpent > 0);
  CHECK(d.w.apply(Action::attack(0, 1, d.weapon(0))));
  CHECK(!d.u(0).moved && !d.u(0).done && !d.u(0).acted);
  CHECK_EQ(d.u(0).moveSpent, 0);
}

TEST(skills_act_again_is_only_for_the_owners_own_turn) {
  // The defender's "defeat" block fires when its counterattack fells the attacker; it is not its side's turn.
  Options o;
  o.dSkills = "\"charge\"";
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"blue\", \"x\": 0, \"y\": 2 }";
  Duel d(skill("charge", DF("{ \"actAgain\": true }")), o);
  d.u(0).hp = 3;
  CHECK(d.attack());
  CHECK(!d.u(0).alive);
  CHECK(!d.u(1).refreshed);
  CHECK_EQ(d.w.count(EventType::SkillActivated), 0);
}
