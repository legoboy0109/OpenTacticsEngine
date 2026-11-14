// Structures in battle (docs/STRUCTURES.md "Rules"): movement, merged numbers,
// capture (single and multi-square), income, building and defeat.
#include "battle_fixture.h"

using namespace ote;
using fixture::World;

namespace {
void enableCapture(World& w, const char* amount = "10") {
  w.db.rules.capture = true;
  w.db.rules.fCaptureAmount = amount;  // 20-point structures: two actions
}
// Ends blue's and red's turn so blue acts again (blue = side 0 starts).
bool passRound(World& w) { return w.act(Action::endTurn()) && w.act(Action::endTurn()); }
}  // namespace

TEST(structure_movement_override_inherit_and_blocked) {
  // Row 0: forest, forest+gate(2 x 1, sets cost 1), forest+city (inherits 2), mire (cost 4 over plain), block, plain.
  World w({"ffffff..", "........"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 7, 1, false}});
  w.place("gate", 1, 0);   // covers (1,0) and (2,0): both squares cost 1 instead of the forest's 2
  w.place("city", 3, 0);   // sets no cost: the forest underneath (2) applies
  w.place("mire", 4, 0);   // cost 4 although the ground is forest (2)
  w.place("block", 5, 0);  // cannot be entered although the ground is forest
  CHECK(w.begin());
  const Battle& b = w.battle;
  CHECK_EQ(b.enterCost(w.at(0, 0), 0), 2);  // plain forest
  CHECK_EQ(b.enterCost(w.at(1, 0), 0), 1);  // override (cheaper)
  CHECK_EQ(b.enterCost(w.at(2, 0), 0), 1);  // every square of a multi-square structure
  CHECK_EQ(b.enterCost(w.at(3, 0), 0), 2);  // inherit
  CHECK_EQ(b.enterCost(w.at(4, 0), 0), 4);  // override (dearer)
  CHECK_EQ(b.enterCost(w.at(5, 0), 0), 0);  // cannot enter
  CHECK_EQ(b.enterCost(w.at(6, 1), 0), 1);
  // The search follows the same costs: from (0,1) with mov 3: up to (1,0) costs 1 via the gate.
  Reach r;
  w.battle.reachable(0, r);
  CHECK_EQ(r.cost[w.at(1, 1)], 1);
  CHECK_EQ(r.cost[w.at(1, 0)], 2);  // (1,1) plain 1 + gate 1
  CHECK_EQ(r.cost[w.at(2, 0)], 3);
  CHECK(r.canStop(w.at(2, 0)));
  CHECK(!w.act(Action::move(0, w.at(5, 0))));  // blocked square: not reachable
}

TEST(structure_blocked_square_is_impassable_in_the_search) {
  World w({"......"}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 5, 0, false}});
  w.place("block", 2, 0);
  CHECK(w.begin());
  Reach r;
  w.battle.reachable(0, r);
  CHECK_EQ(r.cost[w.at(1, 0)], 1);
  CHECK_EQ(r.cost[w.at(2, 0)], -1);
  CHECK_EQ(r.cost[w.at(3, 0)], -1);  // a one-square corridor, walled off by the structure
}

TEST(structure_movement_cost_is_set_per_move_type) {
  // Two move types: the structure sets only "wheel"; "foot" keeps the terrain's cost.
  World w({"ffff"}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  w.db.moveTypes = {{"foot", "Foot"}, {"wheel", "Wheel"}};
  for (TerrainDef& t : w.db.terrains) t.moveCost = {t.moveCost[0], t.moveCost[0]};
  w.db.structures[w.db.findStructure("mire")].moveCost = {-1, 5};  // foot not set, wheel 5
  w.place("mire", 1, 0);
  CHECK(w.begin());
  CHECK_EQ(w.battle.enterCost(w.at(1, 0), 0), 2);  // foot: the forest's own cost
  CHECK_EQ(w.battle.enterCost(w.at(1, 0), 1), 5);  // wheel: the structure's
}

TEST(structure_numbers_merge_with_terrain_for_formulas) {
  // Forest has defense 1. Castle (defense 3) replaces it; gate adds "cover" and replaces defense 2.
  World w({"fffff", "....."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 4, 1, false}});
  w.place("castle", 1, 0);  // 2 x 2: covers (1..2, 0..1)
  w.place("gate", 3, 0);    // covers (3..4, 0)
  CHECK(w.begin());
  Props scratch;
  double v = 0;
  const Props& forest = w.battle.numbersAt(w.at(0, 0), scratch);
  CHECK(propFind(forest, "defense", v) && v == 1);  // no structure: the terrain's list, untouched
  const Props& castle = w.battle.numbersAt(w.at(1, 0), scratch);
  CHECK(propFind(castle, "defense", v) && v == 3);  // replaced, not added
  CHECK_EQ((int)castle.size(), 1);
  const Props& castleLow = w.battle.numbersAt(w.at(1, 1), scratch);  // plain underneath has no numbers
  CHECK(propFind(castleLow, "defense", v) && v == 3);
  const Props& gate = w.battle.numbersAt(w.at(3, 0), scratch);
  CHECK(propFind(gate, "defense", v) && v == 2);
  CHECK(propFind(gate, "cover", v) && v == 7);  // structure number the terrain lacks is added
  CHECK_EQ((int)gate.size(), 2);

  // And formulas see them as ta / tb: red soldier (on forest) attacks blue standing on the castle.
  World g({"fffff", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 0, 0, false}});
  g.place("castle", 1, 0);
  g.db.rules.fDamage = "a.atk - b.def + (tb.defense or 0) * 10 + (ta.defense or 0)";
  CHECK(g.begin());
  Forecast fc;
  CHECK(g.battle.forecast(1, 0, 0, g.at(0, 0), fc));
  CHECK_EQ(fc.attack.damage, 34);  // 5 - 2 + castle 3 * 10 (not the forest's 1 = 13) + attacker's forest 1
}

TEST(structure_capture_single_square_city) {
  World w({"....c.."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 6, 0, false}});
  enableCapture(w, "20");  // one action takes the 20-point city
  CHECK(w.begin());
  CHECK(!w.battle.canCaptureAt(0, w.at(1, 0)));  // plain ground: nothing to capture
  CHECK(w.act(Action::move(0, w.at(4, 0))));
  CHECK(w.battle.canCaptureAt(0, w.at(4, 0)));
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(4, 0).owner, 0);
  const Event* e = w.last(EventType::StructureCaptured);
  CHECK(e && e->b == 0 && e->c == -1);
  CHECK(!w.battle.canCaptureAt(0, w.at(4, 0)));  // already ours
  // The enemy takes it over later; the previous owner is reported.
  CHECK(passRound(w));
  CHECK(w.act(Action::move(0, w.at(3, 0))));
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(1, w.at(4, 0))));
  CHECK(w.battle.canCaptureAt(1, w.at(4, 0)));
  CHECK(w.act(Action::capture(1)));
  CHECK_EQ(w.structureAt(4, 0).owner, 1);
  e = w.last(EventType::StructureCaptured);
  CHECK(e && e->b == 1 && e->c == 0);
}

TEST(structure_capture_multi_square_keeps_progress_inside_resets_outside) {
  World w({".....", ".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 4, 2, false}});
  enableCapture(w);
  w.place("castle", 1, 0);  // covers (1..2, 0..1)
  CHECK(w.begin());
  CHECK_EQ(w.structureAt(1, 0).capture, 20);
  CHECK(w.battle.canCaptureAt(0, w.at(2, 1)));  // any square of it
  CHECK(w.act(Action::capture(0)));             // starts on (1,0)
  CHECK_EQ(w.structureAt(1, 0).capture, 10);
  CHECK_EQ(w.structureAt(1, 0).capturer, 0);
  const Event* p = w.last(EventType::CaptureProgress);
  CHECK(p && p->a == 0 && p->b == w.at(1, 0) && p->c == 10 && p->d == w.battle.structureAt[w.at(1, 0)]);
  CHECK(passRound(w));
  // Walk to another square of the same structure: progress is kept.
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK_EQ(w.structureAt(1, 0).capture, 10);
  CHECK_EQ(w.structureAt(1, 0).capturer, 0);
  CHECK(w.act(Action::capture(0)));  // finishes: the whole structure changes owner
  CHECK_EQ(w.structureAt(1, 0).owner, 0);
  CHECK_EQ(w.structureAt(1, 0).capture, 20);
  CHECK_EQ(w.structureAt(1, 0).capturer, -1);
  const Event* c = w.last(EventType::StructureCaptured);
  CHECK(c && c->a == w.battle.structureAt[w.at(1, 0)] && c->b == 0 && c->c == -1);
  CHECK_EQ(w.count(EventType::StructureCaptured), 1);
  // Owned by the unit's team now: nothing left to capture on any square.
  CHECK(!w.battle.canCaptureAt(0, w.at(1, 0)));
  CHECK(!w.battle.canCaptureAt(0, w.at(2, 1)));
}

TEST(structure_capture_resets_when_the_unit_leaves_the_structure) {
  World w({".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 4, 1, false}});
  enableCapture(w);
  w.place("castle", 1, 0);
  CHECK(w.begin());
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(1, 0).capture, 10);
  CHECK(passRound(w));
  CHECK(w.act(Action::move(0, w.at(3, 0))));  // off the structure
  CHECK_EQ(w.structureAt(1, 0).capture, 20);
  CHECK_EQ(w.structureAt(1, 0).capturer, -1);
}

TEST(structure_capture_restarts_when_another_unit_takes_over) {
  World w({".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 0, 2, 1, false}, {"soldier", 1, 4, 1, false}});
  enableCapture(w);
  w.place("castle", 1, 0);
  CHECK(w.begin());
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(1, 0).capture, 10);
  CHECK(w.act(Action::capture(1)));  // a different unit: starts again from full
  CHECK_EQ(w.structureAt(1, 0).capture, 10);  // 20 - 10, not 0
  CHECK_EQ(w.structureAt(1, 0).capturer, 1);
  CHECK_EQ(w.structureAt(1, 0).owner, -1);
  CHECK_EQ(w.count(EventType::StructureCaptured), 0);
  CHECK(passRound(w));
  CHECK(w.act(Action::capture(1)));  // the same unit again completes it
  CHECK_EQ(w.structureAt(1, 0).owner, 0);
}

TEST(structure_capture_resets_when_the_capturer_dies) {
  World w({".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 0, 0, false}, {"soldier", 1, 4, 1, false}});
  enableCapture(w);
  w.db.rules.fDamage = "99";
  w.place("castle", 1, 0);
  CHECK(w.begin());
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(1, 0).capture, 10);
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::attack(1, 0, 0)));  // red kills the capturer
  CHECK(!w.battle.units[0].alive);
  CHECK_EQ(w.structureAt(1, 0).capture, 20);
  CHECK_EQ(w.structureAt(1, 0).capturer, -1);
}

TEST(structure_headquarters_loss_defeats_the_side) {
  World w({"H.....", "......"}, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 1, 0, false}, {"archer", 0, 5, 1, false}});
  enableCapture(w, "20");
  w.db.rules.defeatLostHq = true;
  w.db.rules.defeatNoUnits = false;
  w.own(0, 0, 0);  // blue owns the hq
  CHECK(w.begin());
  CHECK(w.battle.sides[0].hadHq && !w.battle.sides[1].hadHq);
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(1, w.at(0, 0))));  // red soldier steps onto the hq
  CHECK(w.act(Action::capture(1)));           // 20 points in one go
  CHECK_EQ(w.structureAt(0, 0).owner, 1);
  CHECK(w.battle.sides[0].defeated);
  CHECK(w.battle.over);
  CHECK_EQ(w.battle.winnerTeam, w.battle.sides[1].team);

  // A structure with several squares tagged hq counts once: owning it is enough.
  World m({"......", "......"}, {{"soldier", 0, 5, 1, false}, {"soldier", 1, 5, 0, false}});
  m.db.rules.defeatLostHq = true;
  m.db.rules.defeatNoUnits = false;
  m.db.structures[m.db.findStructure("castle")].tags = {"hq"};
  m.place("castle", 0, 0, 0);
  CHECK(m.begin());
  CHECK(m.battle.sides[0].hadHq);
  CHECK(!m.battle.over);
}

TEST(structure_income_is_paid_once_per_structure) {
  World w({"......", "......", "c....."}, {{"soldier", 0, 5, 0, false}, {"soldier", 1, 5, 2, false}});
  w.db.rules.economy = true;
  w.place("castle", 0, 0, 0);  // 2 x 2 = four squares, one payment of 100
  w.own(0, 2, 0);              // plus a city (100)
  CHECK(w.begin());
  CHECK_EQ(w.battle.sides[0].resources[0], 200);
  CHECK(passRound(w));
  CHECK_EQ(w.battle.sides[0].resources[0], 400);
  CHECK_EQ(w.battle.sides[1].resources[0], 0);
}

TEST(structure_build_on_every_free_square) {
  World w({"......", "......", "......"}, {{"soldier", 1, 5, 2, false}});
  w.db.rules.production = true;
  w.place("castle", 0, 0, 0);
  w.map.sides[0].resources = {{0, 1000}};
  CHECK(w.begin());
  std::vector<int> opts;
  for (int y = 0; y < 2; ++y)
    for (int x = 0; x < 2; ++x) {
      w.battle.buildOptions(w.at(x, y), opts);
      CHECK_EQ((int)opts.size(), 2);  // soldier + tank on each of the four squares
    }
  w.battle.buildOptions(w.at(2, 0), opts);
  CHECK_EQ((int)opts.size(), 0);  // beside the structure: nothing
  CHECK(w.act(Action::build(w.at(1, 1), 0)));  // the unit appears on the chosen square
  CHECK_EQ(w.battle.unitAt(w.at(1, 1)), 1);
  CHECK_EQ(w.battle.units[1].cell, w.at(1, 1));
  w.battle.buildOptions(w.at(1, 1), opts);
  CHECK_EQ((int)opts.size(), 0);  // occupied now
  CHECK(!w.act(Action::build(w.at(1, 1), 0)));
  CHECK(w.act(Action::build(w.at(0, 0), 2)));  // another square, another unit type
  CHECK(!w.act(Action::build(w.at(2, 2), 0)));  // not a structure square
  CHECK_EQ(w.count(EventType::UnitBuilt), 2);
}

TEST(structure_build_needs_a_square_the_new_unit_can_enter) {
  // Two move types: forest cannot be entered by wheels (the tank's type). A castle over a forest square offers
  // the soldier (foot) there but not the tank; on its plain squares both.
  World w({"f.....", "......"}, {{"soldier", 1, 5, 1, false}});
  w.db.moveTypes = {{"foot", "Foot"}, {"wheel", "Wheel"}};
  for (TerrainDef& t : w.db.terrains) t.moveCost = {t.moveCost[0], t.moveCost[0]};
  w.db.terrains[w.db.findTerrain("forest")].moveCost = {2, 0};
  w.db.unitTypes[w.db.findUnitType("tank")].moveType = 1;
  w.db.rules.production = true;
  w.db.structures[w.db.findStructure("castle")].moveCost = {-1, -1};
  w.map = fixture::makeMap(w.db, {"f.....", "......"}, {{"soldier", 1, 5, 1, false}});
  w.place("castle", 0, 0, 0);
  w.map.sides[0].resources = {{0, 1000}};
  CHECK(w.begin());
  std::vector<int> opts;
  w.battle.buildOptions(w.at(0, 0), opts);  // forest square
  CHECK_EQ((int)opts.size(), 1);
  CHECK_EQ(opts[0], 0);
  CHECK(!w.battle.validate(Action::build(w.at(0, 0), 2)));
  w.battle.buildOptions(w.at(1, 0), opts);  // plain square
  CHECK_EQ((int)opts.size(), 2);
  CHECK(w.act(Action::build(w.at(1, 0), 2)));
}

TEST(structure_defeated_side_leaves_neutral_structures) {
  World w({"c....", "....."}, {{"soldier", 0, 3, 0, false}, {"soldier", 1, 4, 0, false}});
  w.db.rules.fDamage = "99";
  w.own(0, 0, 0);
  World v({"c....", ".....", "....."}, {{"soldier", 0, 3, 0, false}, {"soldier", 1, 4, 0, false}});
  v.db.rules.fDamage = "99";
  v.own(0, 0, 0);
  v.place("castle", 0, 1, 0);
  v.place("castle", 3, 1, 1);
  CHECK(v.begin());
  CHECK(v.act(Action::attack(0, 1, 0)));  // blue kills red's only unit
  CHECK(v.battle.sides[1].defeated);
  CHECK_EQ(v.structureAt(3, 1).owner, -1);  // red's castle is neutral
  CHECK_EQ(v.structureAt(0, 1).owner, 0);   // the winner keeps its own
  CHECK_EQ(v.structureAt(0, 0).owner, 0);
}

TEST(structure_battle_copy_is_independent) {
  World w({".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 4, 1, false}});
  enableCapture(w);
  w.place("castle", 1, 0);
  CHECK(w.begin());
  Battle snapshot = w.battle;  // undo point / AI look-ahead
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(1, 0).capture, 10);
  CHECK_EQ(snapshot.structures[0].capture, 20);  // untouched by the original's capture
  CHECK_EQ(snapshot.structures[0].capturer, -1);
  std::vector<Event> ev;
  CHECK(passRound(w));
  CHECK(snapshot.apply(Action::capture(0), ev));
  CHECK_EQ(snapshot.structures[0].capture, 10);
  CHECK_EQ(snapshot.structureAt[snapshot.grid.at(2, 1)], 0);  // lookup table copied, not shared
  snapshot.structures[0].owner = 1;
  CHECK_EQ(w.structureAt(1, 0).owner, -1);
}

TEST(structure_init_refuses_overlap_and_unknown_types) {
  World w({"....", "...."}, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 3, 0, false}});
  w.place("castle", 0, 0);
  w.place("city", 1, 1);  // inside the castle
  CHECK(!w.begin());
  World o({"....", "...."}, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 3, 0, false}});
  o.place("castle", 0, 1);  // 2 x 2 from row 1: runs off the map
  CHECK(!o.begin());
  World u({"....", "...."}, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 3, 0, false}});
  MapStructureDef bad;
  bad.type = 99;
  u.map.structures.push_back(bad);
  CHECK(!u.begin());
}
