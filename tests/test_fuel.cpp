// Fuel (docs/ECONOMY.md "Repair and supply"): moving spends the movement points of the path, the tank limits the
// reach (every query that plans a move goes through Battle::reachable), "fuelPerTurn" burns at the start of the
// side's turn, and a unit that burns out is destroyed like any other death.
#include "core/ai/ai.h"
#include "supply_fixture.h"

using namespace ote;
using namespace suptest;

namespace {

int maxCost(const Reach& r) {
  int m = 0;
  for (int c : r.cost) m = c > m ? c : m;
  return m;
}

}  // namespace

TEST(fuel_units_start_full_and_only_types_with_fuel_have_it) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, false}, {"soldier", 0, 1, 1, false}, {"truck", 0, 2, 1, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  const Battle& b = w.battle;
  CHECK_EQ(b.maxFuel(0), 20);
  CHECK_EQ(b.fuel(0), 16);         // the first turn has begun: 4 already burnt
  CHECK_EQ(b.fuel(1), -1);         // a soldier uses none
  CHECK_EQ(b.maxFuel(1), -1);
  CHECK_EQ(b.fuel(2), 30);         // a truck burns nothing by itself
  CHECK_EQ(b.maxFuel(2), 30);
  CHECK_EQ(b.fuel(-1), -1);
  CHECK_EQ(b.fuel(99), -1);
}

TEST(fuel_moving_spends_the_movement_points_of_the_path) {
  SupplyWorld w({".ff....", ".......", "......."}, {{"truck", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  CHECK_EQ(w.battle.fuel(0), 30);
  CHECK(w.act(Action::move(0, w.at(0, 2))));   // two plain squares
  CHECK_EQ(w.battle.fuel(0), 28);
  CHECK(w.nextTurn());
  CHECK(w.act(Action::move(0, w.at(2, 1))));   // three plain squares away
  CHECK_EQ(w.battle.fuel(0), 28 - w.battle.grid.distance(w.at(0, 2), w.at(2, 1)));
}

TEST(fuel_forest_costs_its_movement_points) {
  SupplyWorld w({".ff....", "fff....", "......."}, {{"truck", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  Reach r;
  w.battle.reachable(0, r);
  int cost = r.cost[(size_t)w.at(1, 0)];
  CHECK_EQ(cost, 2);                            // one forest square
  CHECK(w.act(Action::move(0, w.at(1, 0))));
  CHECK_EQ(w.battle.fuel(0), 28);
}

TEST(fuel_limits_the_reach_in_every_movement_query) {
  SupplyWorld w(field(), {{"truck", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  Reach full;
  w.battle.reachable(0, full);
  CHECK_EQ(maxCost(full), 4);                   // movement 4 with plenty of fuel
  w.unit(0, 1).fuel = 2;
  Reach r;
  w.battle.reachable(0, r);
  CHECK_EQ(maxCost(r), 2);
  CHECK(r.stops.size() < full.stops.size());
  for (Cell c : r.stops) CHECK(r.cost[(size_t)c] <= 2);
  // The path planner and the action check agree with it.
  std::vector<Cell> path;
  CHECK(w.battle.pathTo(r, w.at(2, 1), path));
  CHECK(!w.battle.pathTo(r, w.at(4, 1), path));
  CHECK(w.refused(Action::move(0, w.at(4, 1))).size() > 0);
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK_EQ(w.battle.fuel(0), 0);
}

TEST(fuel_empty_tank_cannot_move_but_can_still_act) {
  SupplyWorld w(field(), {{"truck", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  w.unit(0, 1).fuel = 0;
  Reach r;
  w.battle.reachable(0, r);
  CHECK_EQ(r.stops.size(), size_t(1));          // only where it stands
  CHECK(w.refused(Action::move(0, w.at(1, 1))).size() > 0);
  CHECK(w.act(Action::wait(0)));
  // Not destroyed at the next turn start either: a truck has no fuelPerTurn.
  CHECK(w.nextTurn());
  CHECK(w.battle.units[0].alive);
  CHECK_EQ(w.battle.fuel(0), 0);
}

TEST(fuel_attacking_and_waiting_cost_nothing) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  CHECK(w.begin());
  int before = w.battle.fuel(0);
  CHECK(w.act(Action::attack(0, 1, Gun)));
  CHECK_EQ(w.battle.fuel(0), before);
}

TEST(fuel_per_turn_burns_at_the_start_of_the_sides_own_turn_only) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  CHECK_EQ(w.battle.fuel(0), 16);
  CHECK(w.act(Action::endTurn()));              // red's turn begins: blue's plane is untouched
  CHECK_EQ(w.battle.fuel(0), 16);
  CHECK(w.act(Action::endTurn()));
  CHECK_EQ(w.battle.fuel(0), 12);
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.fuel(0), 8);
}

TEST(fuel_burning_and_moving_both_count) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.battle.fuel(0), 13);
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.fuel(0), 9);
}

TEST(fuel_a_unit_that_runs_dry_is_destroyed_with_a_normal_death) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, false}, {"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}}, {"pilot"});
  CHECK(w.load(R"([{"id":"crash","when":{"defeated":{"unit":"pilot"}},"do":[{"say":"went down"}]}])").empty());
  w.db.rules.supply.structures = false;
  CHECK(w.begin());
  w.unit(0, 1).fuel = 4;                        // burns 4 at blue's next turn start
  w.events.clear();
  CHECK(w.nextTurn());
  CHECK(!w.battle.units[0].alive);
  CHECK_EQ(w.count(EventType::UnitDied), 1);
  CHECK_EQ(w.last(EventType::UnitDied)->a, 0);
  CHECK(w.battle.occupant[(size_t)w.at(0, 1)] == -1);
  CHECK(w.saidLine("went down"));               // the story's "defeated" trigger heard of it
  CHECK(!w.battle.over);                        // blue still has its soldier
}

TEST(fuel_the_last_unit_running_dry_loses_the_battle) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  w.db.rules.supply.structures = false;
  CHECK(w.begin());
  w.unit(0, 1).fuel = 4;
  CHECK(w.nextTurn());                          // no story in this battle: the defeat rules still apply
  CHECK(w.battle.over);
  CHECK(w.battle.sides[0].defeated);
  CHECK_EQ(w.battle.winnerTeam, 1);
  CHECK_EQ(w.count(EventType::SideDefeated), 1);
  CHECK_EQ(w.count(EventType::BattleEnded), 1);
}

TEST(fuel_a_leader_dying_of_fuel_loses_the_game) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, true}, {"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});
  w.db.rules.defeatLeaderDead = true;
  w.db.rules.supply.structures = false;
  CHECK(w.begin());
  w.unit(0, 1).fuel = 4;
  CHECK(w.nextTurn());
  CHECK(w.battle.over);
  CHECK_EQ(w.battle.winnerTeam, 1);
}

TEST(fuel_a_depot_resupplies_before_the_burn) {
  SupplyWorld w(field(), {{"plane", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  w.place("hospital", 0, 1, 0);                 // own structure, rules.supply.structures is on
  CHECK(w.begin());
  w.unit(0, 1).fuel = 4;                        // would be destroyed by the burn
  w.events.clear();
  CHECK(w.nextTurn());
  CHECK(w.battle.units[0].alive);
  CHECK_EQ(w.battle.fuel(0), 16);               // filled to 20, then burnt 4
  CHECK_EQ(w.count(EventType::UnitResupplied), 1);
  CHECK_EQ(w.count(EventType::UnitDied), 0);
}

TEST(fuel_the_ai_never_plans_a_move_the_tank_does_not_allow) {
  SupplyWorld w(field(), {{"plane", 1, 5, 1, false}, {"soldier", 0, 0, 1, false}});
  w.map.sides[1].ai = true;
  w.db.rules.supply.structures = false;
  CHECK(w.begin());
  CHECK(w.act(Action::endTurn()));              // red's turn
  w.unit(5, 1).fuel = 3;
  for (int i = 0; i < 6 && w.battle.activeSide == 1; ++i) {
    int before = w.battle.units[0].fuel;
    CHECK(before >= 0);
    aiTakeStep(w.battle, w.events);
    CHECK(w.battle.units[0].fuel >= 0);
    CHECK(w.battle.units[0].fuel >= before - 3);
  }
  CHECK(w.battle.units[0].alive);
}
