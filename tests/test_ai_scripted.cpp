// Computer opponent: the scripted-style options (docs/AI.md "Scripted-style computer players"). Rules on the generated game of
// ai_scripted_fixture.h: acting order, attack weights, caution none, retreat, idle turns, patrol, ignores, guardArea measure.
#include "ai_scripted_fixture.h"

using namespace ote;
using namespace scripted;

namespace {

const char* kOrder = R"({ "turnOrder": "order" })";
// One set with every weight on, so the hand-computed table below touches each term. Defaults are the documented ones.
const char* kWeights = R"({ "weights": {
  "w1":   { "dmg": 2, "left": 3, "friends": 4, "class": 1, "turn": 1, "counter": 2, "wounds": 1, "priority": { "archer": 10, "soldier": 0 } },
  "w2":   { "dmg": 2, "left": 3, "friends": 4, "class": 1, "turn": 1, "counter": 2, "wounds": 1, "danger": 8, "priority": { "archer": 10 } },
  "flat": { "dmg": 0 },
  "flat0": { "dmg": 0, "attackAtZero": true } } })";

Options withAi(const char* rules) {
  Options o;
  o.aiRules = rules;
  return o;
}

// The weighted attack the unit would choose standing where it is (hold: it attacks only from its own square); score without the base of 200.
bool pick(SField& f, const char* who, ai::Order& out) {
  const Unit& u = f.unit(who);
  Reach reach;
  f.b().reachable(u.id, reach);
  ai::Stance st;
  std::vector<Cell> stops(1, u.cell);
  const bool ok = ai::weightedAttack(f.b(), u, st, reach, stops, out);
  out.score -= 200;
  return ok;
}

}  // namespace

// ---- 1. acting order ----

TEST(ai_scripted_order_makes_units_act_one_at_a_time_in_the_order_of_their_numbers) {
  const std::string units = blue("lord", 0, 2) + ", " + red("a", 9, 1, beh(R"({ "order": 3 })")) + ", " + red("b", 9, 2, beh(R"({ "order": 1 })")) + ", " +
                            red("c", 9, 3, beh(R"({ "order": 2 })"));
  SField f("ais_order", units, withAi(kOrder));
  CHECK(f.loaded);
  const std::vector<std::string> seq = f.redSequence();
  CHECK_EQ((int)seq.size(), 3);
  CHECK_STR(seq[0], "b");
  CHECK_STR(seq[1], "c");
  CHECK_STR(seq[2], "a");
  // The same map with the default turn order (the numbers are still accepted): the best action goes first, so b (nearest) then a, c.
  SField g("ais_order_best", units, Options());
  CHECK(g.loaded);
  const std::vector<std::string> best = g.redSequence();
  CHECK_EQ((int)best.size(), 3);
  CHECK_STR(best[0], "b");
  CHECK_STR(best[1], "a");
  CHECK_STR(best[2], "c");
}

TEST(ai_scripted_order_ties_go_to_the_unit_placed_first_and_negative_numbers_go_first) {
  const std::string units = blue("lord", 0, 2) + ", " + red("p", 9, 1) + ", " + red("q", 9, 2) + ", " + red("z", 9, 3, beh(R"({ "order": -5 })"));
  SField f("ais_order_ties", units, withAi(kOrder));
  const std::vector<std::string> seq = f.redSequence();
  CHECK_EQ((int)seq.size(), 3);
  CHECK_STR(seq[0], "z");  // -5 before 0
  CHECK_STR(seq[1], "p");  // both 0: the one placed first
  CHECK_STR(seq[2], "q");
}

TEST(ai_scripted_order_a_unit_acts_by_its_own_best_order_whatever_the_others_could_do) {
  // z (order 9) could kill the weak blue unit; y (order 1) acts first and only walks. Acting order beats scores.
  const std::string units = blue("lord", 0, 2) + ", " + blue("weak", 8, 0) + ", " + red("z", 8, 1, beh(R"({ "order": 9 })")) + ", " +
                            red("y", 12, 4, beh(R"({ "order": 1 })"));
  SField f("ais_order_own", units, withAi(kOrder));
  f.unit("weak").hp = 3;
  const std::vector<std::string> seq = f.redSequence();
  CHECK_EQ((int)seq.size(), 2);
  CHECK_STR(seq[0], "y");
  CHECK_STR(seq[1], "z");
  CHECK(!f.unit("weak").alive);
}

// ---- 2. attack weights: a hand-computed table ----

// R (a red soldier, 15 of 20 hit points, holding (6,2)) next to A, a blue soldier (7,2), and B, a blue archer (5,2) that has a red friend at (5,1).
//   A: dmg 6+5-2 = 9 of 20 -> dmgTerm 9*2 = 18;  left (20 - 11) * 3 = 27;  friends 0;  class 0;  turn 1*1 = 1;
//      counter: A strikes back 9 -> 9*2 = 18;  wounds: 25 % missing -> 1*25/5 = 5.   score 18+27+0+0+1-18-5 = 23
//   B: dmg 6+5-1 = 10 of 16 -> 20;  left (20 - 6) * 3 = 42;  friends 1*4 = 4;  class 10*1 = 10;  turn 1;
//      B cannot strike back at range 1 (bow 2..2) -> +10 (noCounterBonus);  wounds 5.   score 20+42+4+10+1+10-5 = 82
TEST(ai_scripted_weights_score_every_term_as_documented_and_pick_the_best) {
  const std::string both = red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "w1" })")) + ", " + blue("a", 7, 2) + ", " + blue("b", 5, 2, "archer") + ", " +
                           red("f", 5, 1, beh(R"("hold")"));
  SField f("ais_weights_both", both, withAi(kWeights));
  CHECK(f.loaded);
  f.unit("r").hp = 15;
  ai::Order o;
  CHECK(pick(f, "r", o));
  CHECK_EQ(o.target, f.id("b"));
  CHECK_EQ((int)o.score, 82);
  CHECK(o.moveTo == kNoCell);
  // A alone.
  SField g("ais_weights_a", red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "w1" })")) + ", " + blue("a", 7, 2), withAi(kWeights));
  g.unit("r").hp = 15;
  CHECK(pick(g, "r", o));
  CHECK_EQ(o.target, g.id("a"));
  CHECK_EQ((int)o.score, 23);
  // A that this blow kills (9 hit points left, 9 damage): killValue replaces the damage; the hit points left are 0; a dead unit does not strike back.
  //   50 + left (20 - 0) * 3 = 60 + turn 1 - wounds 5 = 106
  g.unit("a").hp = 9;
  CHECK(pick(g, "r", o));
  CHECK_EQ((int)o.score, 106);
  // The real turn: it takes the better target (B) and hits it.
  CHECK(f.redTurn());
  CHECK(f.hp("b") < 16);
  CHECK_EQ(f.hp("a"), 20);
}

TEST(ai_scripted_weights_danger_counts_what_the_enemies_that_can_reach_the_square_could_deal) {
  // The same fight with set w2 (danger 8, dangerDiv 8): both blue units can reach R's square. A hits it for 9; B (archer: 5+4-2 = 7) walks to range.
  // danger term = min(20, 8 * 16 / 8) = 16.  A: 23 - 16 = 7;  B: 82 - 16 = 66.
  const std::string both = red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "w2" })")) + ", " + blue("a", 7, 2) + ", " + blue("b", 5, 2, "archer") + ", " +
                           red("f", 5, 1, beh(R"("hold")"));
  SField f("ais_weights_danger", both, withAi(kWeights));
  f.unit("r").hp = 15;
  ai::Order o;
  CHECK(pick(f, "r", o));
  CHECK_EQ(o.target, f.id("b"));
  CHECK_EQ((int)o.score, 66);
  SField g("ais_weights_danger_a", red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "w2" })")) + ", " + blue("a", 7, 2), withAi(kWeights));
  g.unit("r").hp = 15;
  CHECK(pick(g, "r", o));
  CHECK_EQ((int)o.score, 14);  // 23 - 9
}

TEST(ai_scripted_weights_include_the_hit_chance_and_never_use_randomness) {
  // Hit 50 %: A is expected to take 9 * 50 / 100 = 4.5: dmgTerm 4.5*2 = 9; left (20 - 15.5) * 3 = 13.5; counter 4.5*2 = 9; turn 1; wounds 5 -> 9+13.5+1-9-5 = 9.5
  Options o = withAi(kWeights);
  o.hit = "50";
  SField f("ais_weights_hit", red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "w1" })")) + ", " + blue("a", 7, 2), o);
  f.unit("r").hp = 15;
  ai::Order order;
  CHECK(pick(f, "r", order));
  CHECK_EQ((int)(order.score * 10 + 0.5), 95);
}

TEST(ai_scripted_weights_ties_go_to_the_later_target_and_the_later_weapon_and_zero_needs_attackAtZero) {
  // Every term 0: the target inner loop is in placement order, the later wins: a2 (placed after a1).
  const std::string units = red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "flat0" })")) + ", " + blue("a1", 7, 2) + ", " + blue("a2", 6, 3);
  SField f("ais_weights_tie", units, withAi(kWeights));
  ai::Order o;
  CHECK(pick(f, "r", o));
  CHECK_EQ(o.target, f.id("a2"));
  CHECK_EQ((int)o.score, 0);
  // Without attackAtZero a score of 0 is no reason to attack: nothing is chosen and the turn is a plain wait.
  SField g("ais_weights_zero", red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "flat" })")) + ", " + blue("a1", 7, 2), withAi(kWeights));
  CHECK(!pick(g, "r", o));
  CHECK(g.redTurn());
  CHECK_EQ(g.hp("a1"), 20);
  // Two weapons with equal scores: the later weapon of the inventory (sword2) wins.
  SField h("ais_weights_weapon", red("t", 6, 2, beh(R"({ "mode": "hold", "weights": "flat0" })"), "twin") + ", " + blue("a", 7, 2), withAi(kWeights));
  CHECK(pick(h, "t", o));
  CHECK_EQ(o.weapon, h.rig.db.findWeapon("sword2"));
}

TEST(ai_scripted_weights_attack_from_the_square_the_target_cannot_answer_then_the_better_terrain_then_the_cheaper_walk) {
  // The archer at (3,2) cannot answer a sword from any square next to it. The fort (3,1) is the best defence among the squares R can stand on.
  const std::string units = red("r", 6, 2, beh(R"({ "weights": "flat0" })")) + ", " + blue("b", 3, 2, "archer");
  Options o = withAi(kWeights);
  o.rows = "............../...F........../............../............../..............";
  SField f("ais_weights_square", units, o);
  CHECK(f.loaded);
  CHECK(f.redTurn());
  CHECK(f.hp("b") < 16);
  CHECK_EQ(f.x("r"), 3);
  CHECK_EQ(f.y("r"), 1);
  // No terrain to choose: the cheapest walk, (4,2).
  SField g("ais_weights_square2", units, withAi(kWeights));
  CHECK(g.redTurn());
  CHECK(g.hp("b") < 16);
  CHECK_EQ(g.x("r"), 4);
  CHECK_EQ(g.y("r"), 2);
}

TEST(ai_scripted_weights_a_unit_with_a_set_ignores_the_default_scoring_and_hold_does_not_move) {
  // Hold: a target two squares away is not attacked (no walking), whatever the weights say.
  SField f("ais_weights_hold", red("r", 6, 2, beh(R"({ "mode": "hold", "weights": "flat0" })")) + ", " + blue("a", 8, 2), withAi(kWeights));
  CHECK(f.redTurn());
  CHECK_EQ(f.x("r"), 6);
  CHECK_EQ(f.hp("a"), 20);
}

// ---- 3. caution none ----

TEST(ai_scripted_caution_none_walks_into_a_square_the_normal_advance_avoids) {
  // Three blue soldiers in a block at x=2: (6,2) is within reach of all three (outnumbered); the normal advance stops at x=7, caution none at x=6.
  const std::string cluster = blue("l", 2, 2) + ", " + blue("m", 2, 1) + ", " + blue("n", 2, 3);
  SField n("ais_caution_normal", cluster + ", " + red("r", 10, 2), Options());
  CHECK(n.redTurn());
  CHECK_EQ(n.x("r"), 7);
  SField c("ais_caution_none", cluster + ", " + red("r", 10, 2, beh(R"({ "caution": "none" })")), Options());
  CHECK(c.redTurn());
  CHECK_EQ(c.x("r"), 6);
  CHECK_EQ(c.y("r"), 2);
}

// ---- 4. retreat ----

namespace {
const char* kFortRow = "............../............../...........F../............../..............";
const char* kRetreat = R"({ "retreat": { "below": 30, "until": 80, "to": { "terrain": ["fort"] } } })";
}  // namespace

TEST(ai_scripted_retreat_enters_below_walks_to_the_fort_waits_there_and_leaves_at_until) {
  Options o;
  o.rows = kFortRow;
  SField f("ais_retreat", blue("lord", 6, 2) + ", " + red("r", 8, 2, beh(kRetreat)), o);
  CHECK(f.loaded);
  f.unit("r").hp = 5;  // 25 % < 30
  CHECK(f.redTurn());
  CHECK(f.unit("r").retreating);
  CHECK_EQ(f.x("r"), 11);
  CHECK_EQ(f.y("r"), 2);
  CHECK_EQ(f.hp("lord"), 20);  // it did not strike the lord that stood two squares away
  // The lord follows and stands next to it: still no attack, it waits on the fort.
  Unit& lord = f.unit("lord");
  f.b().occupant[(size_t)lord.cell] = -1;
  lord.cell = f.at(10, 2);
  f.b().occupant[(size_t)lord.cell] = lord.id;
  CHECK(f.redTurn());
  CHECK_EQ(f.x("r"), 11);
  CHECK_EQ(f.hp("lord"), 20);
  f.unit("r").hp = 15;  // 75 %: not yet 80
  CHECK(f.redTurn());
  CHECK(f.unit("r").retreating);
  CHECK_EQ(f.hp("lord"), 20);
  f.unit("r").hp = 16;  // 80 %: leaves retreat at the start of its turn and fights
  CHECK(f.redTurn());
  CHECK(!f.unit("r").retreating);
  CHECK(f.hp("lord") < 20);
}

TEST(ai_scripted_retreat_drinks_a_healing_item_first_and_walks_when_useItems_is_off) {
  Options o;
  o.rows = kFortRow;
  SField f("ais_retreat_item", blue("lord", 0, 2) + ", " + red("r", 8, 0, beh(kRetreat) + ", \"items\": [\"potion\"]"), o);
  CHECK(f.loaded);
  f.unit("r").hp = 4;
  CHECK(f.redTurn());
  CHECK_EQ(f.hp("r"), 14);  // the potion, in place
  CHECK_EQ(f.x("r"), 8);
  CHECK_EQ(f.y("r"), 0);
  CHECK(f.unit("r").retreating);
  CHECK(f.redTurn());  // 70 %: still retreating, the second use
  CHECK_EQ(f.hp("r"), 20);
  CHECK(f.redTurn());  // healed: it left retreat
  CHECK(!f.unit("r").retreating);
  // useItems off: it keeps the potion and walks to the fort.
  const std::string noItems = R"({ "retreat": { "below": 30, "until": 80, "to": { "terrain": ["fort"] }, "useItems": false } })";
  SField g("ais_retreat_noitems", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(noItems) + ", \"items\": [\"potion\"]"), o);
  CHECK(g.loaded);
  g.unit("r").hp = 4;
  CHECK(g.redTurn());
  CHECK_EQ(g.hp("r"), 4);
  CHECK_EQ(g.x("r"), 11);
}

TEST(ai_scripted_retreat_without_terrain_goes_where_the_fewest_enemies_can_hit_it_and_hold_does_not_move) {
  SField f("ais_retreat_away", blue("lord", 6, 2) + ", " + red("r", 8, 2, beh(R"({ "retreat": { "below": 30 } })")), Options());
  CHECK(f.loaded);
  f.unit("r").hp = 5;
  CHECK(f.redTurn());
  CHECK(f.b().grid.distance(f.unit("r").cell, f.unit("lord").cell) >= 6);  // outside the lord's walk (4) plus sword (1)
  CHECK_EQ(f.hp("lord"), 20);
  SField h("ais_retreat_hold", blue("lord", 6, 2) + ", " + red("r", 8, 2, beh(R"({ "mode": "hold", "retreat": { "below": 30 } })")), Options());
  h.unit("r").hp = 5;
  CHECK(h.redTurn());
  CHECK_EQ(h.x("r"), 8);
  CHECK_EQ(h.hp("lord"), 20);
}

// ---- 5. idle turns ----

TEST(ai_scripted_idleTurns_makes_no_approach_move_for_its_first_turns_but_still_strikes_what_it_can_reach) {
  SField f("ais_idle", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(R"({ "idleTurns": 2 })")), Options());
  CHECK(f.loaded);
  CHECK(f.redTurn());
  CHECK_EQ(f.x("r"), 8);
  CHECK(f.redTurn());
  CHECK_EQ(f.x("r"), 8);
  CHECK(f.redTurn());  // its third turn: it behaves by its mode again
  CHECK(f.x("r") < 8);
  // Something in reach is attacked (with a move to do it) even in the idle turns.
  SField g("ais_idle_reach", blue("lord", 4, 2) + ", " + red("r", 8, 2, beh(R"({ "idleTurns": 2 })")), Options());
  CHECK(g.redTurn());
  CHECK(g.hp("lord") < 20);
  CHECK(g.x("r") < 8);
}

// ---- 6. patrol ----

TEST(ai_scripted_patrol_walks_its_route_in_a_loop_and_attacks_what_comes_within_reach) {
  const char* route = R"({ "mode": "patrol", "route": [ { "x": 12, "y": 2 }, { "x": 8, "y": 2 } ] })";
  SField f("ais_patrol", blue("lord", 0, 2) + ", " + red("r", 10, 2, beh(route)), Options());
  CHECK(f.loaded);
  const int expect[] = {12, 8, 12, 8};
  for (int i = 0; i < 4; ++i) {
    CHECK(f.redTurn());
    CHECK_EQ(f.x("r"), expect[i]);
    CHECK_EQ(f.y("r"), 2);
  }
  // Someone in reach is attacked instead of walking on.
  Unit& lord = f.unit("lord");
  f.b().occupant[(size_t)lord.cell] = -1;
  lord.cell = f.at(5, 2);
  f.b().occupant[(size_t)lord.cell] = lord.id;
  CHECK(f.redTurn());
  CHECK(f.hp("lord") < 20);
}

// ---- 7. ignores ----

TEST(ai_scripted_ignores_are_not_attacked_or_chased_unless_they_are_the_only_hostile_left) {
  // A wounded decoy that the default scoring would kill, and the lord, both next to R. Without the key the decoy dies.
  SField c("ais_ignores_control", blue("lord", 7, 2) + ", " + blue("decoy", 9, 2) + ", " + red("r", 8, 2), Options());
  c.unit("decoy").hp = 5;
  CHECK(c.redTurn());
  CHECK(!c.unit("decoy").alive);
  SField f("ais_ignores", blue("lord", 7, 2) + ", " + blue("decoy", 9, 2) + ", " + red("r", 8, 2, beh(R"({ "ignores": ["decoy"] })")), Options());
  f.unit("decoy").hp = 5;
  CHECK(f.redTurn());
  CHECK(f.unit("decoy").alive);
  CHECK_EQ(f.hp("decoy"), 5);
  CHECK(f.hp("lord") < 20);
  // Only the decoy is left: it is attacked after all.
  SField g("ais_ignores_last", blue("decoy", 9, 2) + ", " + red("r", 8, 2, beh(R"({ "ignores": ["decoy"] })")), Options());
  CHECK(g.redTurn());
  CHECK(g.hp("decoy") < 20);
  // Approach: the decoy is the nearer by path (6 against 7); an ignoring unit walks toward the lord, the other toward the decoy.
  SField a("ais_ignores_walk", blue("lord", 0, 2) + ", " + blue("decoy", 13, 2) + ", " + red("r", 7, 2, beh(R"({ "ignores": ["decoy"] })")), Options());
  CHECK(a.redTurn());
  CHECK_EQ(a.x("r"), 3);
  SField d("ais_ignores_walk2", blue("lord", 0, 2) + ", " + blue("decoy", 13, 2) + ", " + red("r", 7, 2), Options());
  CHECK(d.redTurn());
  CHECK_EQ(d.x("r"), 11);
}

// ---- 8. guardArea measured by movement cost ----

TEST(ai_scripted_guardArea_within_can_be_a_movement_cost) {
  Options o;
  o.rows = "......ff....../......ff....../......ff....../......ff....../......ff......";
  const std::string steps = blue("lord", 5, 2) + ", " + red("g", 8, 2, beh(R"({ "mode": "guardArea", "within": 4 })"));
  SField s("ais_measure_steps", steps, o);  // the lord is 3 squares away: it wakes
  CHECK(s.redTurn());
  CHECK(s.unit("g").awake);
  // The walk to it costs 2 + 2 + 1 = 5 through the forest: within 4 it stays asleep, within 5 it wakes.
  const std::string cost4 = blue("lord", 5, 2) + ", " + red("g", 8, 2, beh(R"({ "mode": "guardArea", "within": 4, "measure": "moveCost" })"));
  SField c4("ais_measure_cost4", cost4, o);
  CHECK(c4.redTurn());
  CHECK(!c4.unit("g").awake);
  CHECK_EQ(c4.x("g"), 8);
  const std::string cost5 = blue("lord", 5, 2) + ", " + red("g", 8, 2, beh(R"({ "mode": "guardArea", "within": 5, "measure": "moveCost" })"));
  SField c5("ais_measure_cost5", cost5, o);
  CHECK(c5.redTurn());
  CHECK(c5.unit("g").awake);
}
