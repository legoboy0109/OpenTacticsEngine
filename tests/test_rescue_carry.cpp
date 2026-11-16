// Step R1 of docs/SRPG.md: rescue in "carry" mode. Rescue, Drop, GiveUnit, TakeUnit, the carrier's stat penalty,
// the canCarry formula, the carrier falling, fog, and the rules that count a carried unit as still there.
#include <algorithm>

#include "fog_fixture.h"
#include "rescue_fixture.h"

using namespace ote;
using namespace rescuetest;

// ---- Rescue ----

TEST(rescue_carry_takes_a_neighbour_off_the_board) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  CHECK(w.carriedBy(1, 0));
  CHECK_EQ(w.un(1).hp, 8);  // keeps its hp
  CHECK(w.stands(0, 1, 1));
  CHECK(w.un(0).done);  // ends the carrier's turn
  CHECK_EQ(w.battle.unitAt(w.at(2, 1)), -1);
  const Event* e = w.last(EventType::UnitRescued);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(2, 1) && e->flags == 0);
  // It cannot be ordered, targeted or seen as a fighter.
  CHECK(w.refused(Action::wait(1), "that unit cannot act now"));
  w.act(Action::endTurn());
  Forecast f;
  CHECK(!w.battle.forecast(4, 1, 0, w.un(4).cell, f));
  // Nothing happens to it when the turn comes round: it is still carried, still at full hp.
  w.act(Action::endTurn());
  CHECK(w.carriedBy(1, 0));
  CHECK(!w.un(0).done);
}

TEST(rescue_carry_refuses_with_a_reason) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(w.refused(Action::rescue(0, 0), "a unit cannot rescue itself"));
  CHECK(w.refused(Action::rescue(0, 4), "only a unit of the same side can be rescued"));
  CHECK(w.refused(Action::rescue(0, 3), "that unit is not next to it"));
  CHECK(w.refused(Action::rescue(0, 99), "that unit is not on the board"));
  CHECK(w.refused(Action::rescue(0, -1), "that unit is not on the board"));
  CHECK(w.refused(Action::pairUp(0, 1), "this game uses carrying, not pair up"));
  CHECK(w.refused(Action::switchPair(0), "this game uses carrying, not pair up"));
  CHECK(w.refused(Action::rescue(4, 0), "that unit cannot act now"));  // not its side's turn
  CHECK(w.act(Action::rescue(0, 1)));
  CHECK(w.refused(Action::rescue(0, 2), "that unit has already acted"));
  CHECK(w.refused(Action::rescue(2, 1), "that unit is not on the board"));  // carried already
  CHECK(w.refused(Action::rescue(2, 0), "that unit already carries somebody"));  // the target carries
  w.pass();
  CHECK(w.refused(Action::rescue(0, 2), "that unit already carries somebody"));  // the actor carries
  // A failed order changes nothing.
  CHECK(w.carriedBy(1, 0));
  CHECK(w.stands(2, 1, 2));
}

TEST(rescue_carry_canCarry_decides_who_may_carry_whom) {
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 1, 1, false}, {"archer", 0, 2, 1, false}, {"tank", 0, 0, 1, false},
                                    {"soldier", 1, 4, 1, false}});
  w.canCarry("a.atk >= b.atk and not b.tags.armor");  // soldier 5, archer 4, tank 8 (armor)
  CHECK(w.begin());
  CHECK(w.refused(Action::rescue(0, 2), "it cannot carry that unit"));
  CHECK(w.refused(Action::rescue(1, 0), "it cannot carry that unit"));
  CHECK(w.battle.validate(Action::rescue(0, 1)));
  std::vector<int> out;
  w.battle.rescueTargets(0, w.un(0).cell, out);
  CHECK(out == std::vector<int>{1});
  w.battle.rescueTargets(1, w.un(1).cell, out);
  CHECK(out.empty());
  w.battle.rescueTargets(2, w.un(2).cell, out);  // the tank may carry the soldier
  CHECK(out == std::vector<int>{0});
  // The queries judge the square the unit would stand on.
  w.battle.rescueTargets(0, w.at(3, 1), out);
  CHECK(out == std::vector<int>{1});
}

TEST(rescue_carry_a_failing_canCarry_formula_means_no) {
  RescueWorld w = line();
  w.canCarry("a.nothing.at_all");
  CHECK(w.begin());
  CHECK(w.refused(Action::rescue(0, 1), "it cannot carry that unit"));
}

// ---- the carrier's stats ----

TEST(rescue_carry_carrierStats_change_the_carrier_percent_first) {
  RescueWorld w = line();
  w.carrierStat(Atk, 50, 2);  // 5 -> 2 -> 4 (adding first would give 3)
  w.carrierStat(Def, 100, -5);  // never below 0
  w.carrierStat(Mov, 100, -1);
  CHECK(w.begin());
  CHECK_EQ(w.stat(0, Atk), 5);
  CHECK(w.act(Action::rescue(0, 1)));
  CHECK_EQ(w.stat(0, Atk), 4);
  CHECK_EQ(w.stat(0, Def), 0);
  CHECK_EQ(w.stat(0, Mov), 2);
  CHECK_EQ(w.battle.movePoints(w.un(0)), 2);
  CHECK_EQ(w.stat(0, Hp), 10);  // the health stat is never changed
  CHECK_EQ(w.battle.maxHp(w.un(0)), 10);
  CHECK_EQ(w.stat(1, Atk), 4);  // the carried unit and everybody else are untouched
  CHECK_EQ(w.stat(2, Atk), 5);
  w.pass();
  CHECK(w.act(Action::drop(0, w.at(2, 1))));  // the penalty ends with the carrying
  CHECK_EQ(w.stat(0, Atk), 5);
  CHECK_EQ(w.stat(0, Mov), 3);
}

// ---- Drop ----

TEST(rescue_carry_drop_puts_the_unit_on_a_free_neighbour) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  std::vector<Cell> cells;
  w.battle.dropCells(0, w.un(0).cell, cells);  // (1,2) is taken by a soldier
  std::sort(cells.begin(), cells.end());
  std::vector<Cell> want = {w.at(1, 0), w.at(0, 1), w.at(2, 1)};
  std::sort(want.begin(), want.end());
  CHECK(cells == want);
  CHECK(w.refused(Action::drop(2, w.at(3, 1)), "that unit is not carrying anybody"));
  CHECK(w.refused(Action::drop(0, w.at(3, 1)), "that square is not next to the unit"));
  CHECK(w.refused(Action::drop(0, kNoCell), "that square is not next to the unit"));
  CHECK(w.refused(Action::drop(0, w.at(1, 2)), "that square is occupied"));
  CHECK(w.act(Action::drop(0, w.at(0, 1))));
  CHECK(w.stands(1, 0, 1));
  CHECK_EQ(w.battle.unitAt(w.at(0, 1)), 1);
  CHECK(w.un(1).done && w.un(1).moved);  // its turn is finished
  CHECK_EQ(w.un(1).hp, 8);
  CHECK(w.un(0).carrying == -1 && w.un(1).carriedBy == -1 && w.un(0).done);
  const Event* e = w.last(EventType::UnitDropped);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(0, 1) && e->flags == 0);
  w.battle.dropCells(0, w.un(0).cell, cells);
  CHECK(cells.empty());
}

TEST(rescue_carry_drop_needs_a_square_the_unit_may_take) {
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 0, 1, false}, {"archer", 0, 0, 0, false}, {"soldier", 1, 4, 2, false}},
                {".....", ".#...", "....."});
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  CHECK(w.refused(Action::drop(0, w.at(1, 1)), "the unit could not stand on that square"));
  std::vector<Cell> cells;
  w.battle.dropCells(0, w.un(0).cell, cells);
  CHECK(std::find(cells.begin(), cells.end(), w.at(1, 1)) == cells.end());
  CHECK(std::find(cells.begin(), cells.end(), w.at(0, 0)) != cells.end());
}

// ---- GiveUnit and TakeUnit ----

TEST(rescue_carry_give_and_take_pass_a_unit_along_for_free) {
  RescueWorld w = line();
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  std::vector<int> out;
  w.battle.giveTargets(0, w.un(0).cell, out);
  CHECK(out == std::vector<int>{2});
  w.battle.takeTargets(0, w.un(0).cell, out);
  CHECK(out.empty());
  CHECK(w.refused(Action::giveUnit(2, 3), "that unit is not carrying anybody"));
  CHECK(w.refused(Action::giveUnit(0, 0), "that is the unit itself"));
  CHECK(w.refused(Action::giveUnit(0, 4), "only a unit of the same side can be handed over to or taken from"));
  CHECK(w.refused(Action::giveUnit(0, 3), "that unit is not next to it"));
  CHECK(w.act(Action::giveUnit(0, 2)));
  CHECK(w.carriedBy(1, 2));
  const Event* e = w.last(EventType::UnitHanded);
  CHECK(e && e->a == 0 && e->b == 2 && e->c == 1);
  CHECK(w.un(0).moved && !w.un(0).done);  // free, but it counts as having moved
  CHECK(w.un(2).carrying == 1 && w.un(0).carrying == -1);
  CHECK(w.refused(Action::move(0, w.at(1, 0)), "that unit has already moved"));
  // Chained along the line: 2 hands it to 3.
  CHECK(w.act(Action::giveUnit(2, 3)));
  CHECK(w.carriedBy(1, 3));
  CHECK(!w.un(2).done);
  // 2 takes it back from 3, then puts it down in the same turn: Take is free, Drop ends 2's turn.
  w.battle.takeTargets(2, w.un(2).cell, out);
  CHECK(out == std::vector<int>{3});
  CHECK(w.act(Action::takeUnit(2, 3)));
  CHECK(w.carriedBy(1, 2));
  e = w.last(EventType::UnitHanded);
  CHECK(e && e->a == 3 && e->b == 2 && e->c == 1);
  CHECK(w.act(Action::drop(2, w.at(2, 2))));
  CHECK(w.stands(1, 2, 2) && w.un(1).done && w.un(2).done);
}

TEST(rescue_carry_give_and_take_refuse_with_a_reason) {
  // A (1,1) carries B (2,1); D (0,2) carries C (1,2); E (0,1) is free and next to both carriers.
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 1, 1, false}, {"archer", 0, 2, 1, false}, {"soldier", 0, 0, 2, false},
                                    {"soldier", 0, 1, 2, false}, {"soldier", 0, 0, 1, false}, {"soldier", 1, 4, 0, false}});
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  CHECK(w.act(Action::rescue(2, 3)));
  w.pass();
  CHECK(w.refused(Action::takeUnit(4, 5), "only a unit of the same side can be handed over to or taken from"));
  CHECK(w.refused(Action::takeUnit(4, 1), "that unit is not on the board"));  // carried units cannot be traded with
  CHECK(w.refused(Action::takeUnit(1, 0), "that unit cannot act now"));
  CHECK(w.act(Action::takeUnit(4, 0)));
  CHECK(w.carriedBy(1, 4));
  CHECK(w.refused(Action::takeUnit(4, 2), "that unit already carries somebody"));  // the taker carries
  CHECK(w.refused(Action::giveUnit(4, 2), "that unit already carries somebody"));  // the receiver carries
  CHECK(w.refused(Action::giveUnit(0, 4), "that unit is not carrying anybody"));
  CHECK(w.battle.validate(Action::takeUnit(0, 4)));  // legal: A carries nothing now and E carries
  CHECK(w.battle.validate(Action::giveUnit(4, 0)));
  CHECK(w.refused(Action::rescue(0, 4), "that unit already carries somebody"));
  CHECK(w.refused(Action::pairUp(0, 4), "this game uses carrying, not pair up"));
}

TEST(rescue_carry_give_and_take_respect_canCarry) {
  RescueWorld w(RescueMode::Carry, {{"tank", 0, 0, 1, false}, {"soldier", 0, 1, 1, false}, {"archer", 0, 0, 0, false},
                                    {"soldier", 1, 4, 1, false}});
  w.canCarry("a.atk >= b.atk");  // tank 8, soldier 5, archer 4
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));  // the tank carries the soldier
  w.pass();
  CHECK(w.refused(Action::giveUnit(0, 2), "that unit cannot carry it"));   // the archer (4) cannot carry the soldier (5)
  CHECK(w.refused(Action::takeUnit(2, 0), "it cannot carry that unit"));
  std::vector<int> out;
  w.battle.giveTargets(0, w.un(0).cell, out);
  CHECK(out.empty());
  w.battle.takeTargets(2, w.un(2).cell, out);
  CHECK(out.empty());
}

// ---- the carrier falls ----

TEST(rescue_carry_a_fallen_carrier_puts_its_unit_on_its_square) {
  RescueWorld w(RescueMode::Carry, {{"archer", 0, 1, 1, false}, {"soldier", 0, 2, 1, false}, {"tank", 1, 1, 0, false}});
  CHECK(w.begin());
  w.battle.units[0].hp = 3;
  CHECK(w.act(Action::rescue(0, 1)));
  w.act(Action::endTurn());
  CHECK(w.act(Action::attack(2, 0, 2)));
  const int died = story::find(w.events, EventType::UnitDied);
  const int dropped = story::find(w.events, EventType::UnitDropped);
  CHECK(died >= 0 && dropped > died);
  const Event& e = w.events[(size_t)dropped];
  CHECK(e.a == 0 && e.b == 1 && e.c == w.at(1, 1) && e.flags == kForced);
  CHECK(!w.un(0).alive && w.un(0).carrying == -1);
  CHECK(w.stands(1, 1, 1));
  CHECK(w.un(1).done && w.un(1).hp == 10 && w.un(1).carriedBy == -1);
  CHECK_EQ(w.battle.unitAt(w.at(1, 1)), 1);
  CHECK(!w.battle.sides[0].defeated);
}

TEST(rescue_carry_a_fallen_carrier_puts_its_unit_on_the_nearest_square_it_may_take) {
  // Water at (4,1): a boat cannot stand on the land where its carrier fell, so it goes to the water it left.
  BoatWorld w({{"soldier", 0, 3, 1, false}, {"boat", 0, 4, 1, false}, {"soldier", 1, 3, 0, false}}, {{4, 1}}, {});
  CHECK(w.begin());
  w.battle.units[0].hp = 1;
  CHECK(w.act(Action::rescue(0, 1)));
  w.act(Action::endTurn());
  CHECK(w.act(Action::attack(2, 0, 0)));
  CHECK(!w.un(0).alive);
  CHECK(w.stands(1, 4, 1));
  const Event* e = w.last(EventType::UnitDropped);
  CHECK(e && e->c == w.at(4, 1) && e->flags == kForced);
}

TEST(rescue_carry_a_unit_with_no_square_left_is_lost_with_its_carrier) {
  // The one water square is taken once the carrier falls, and a boat cannot stand on land: it is defeated by whoever
  // defeated the carrier.
  BoatWorld w({{"soldier", 0, 3, 1, false}, {"boat", 0, 4, 1, true}, {"soldier", 0, 4, 0, false}, {"soldier", 1, 3, 0, false}},
              {{4, 1}}, {"a", "b", "d", "r"});
  w.db.rules.defeatLeaderDead = true;  // "b" is the leader: its loss must count (the leader rule)
  CHECK(w.play(R"([{"id":"lost","when":{"defeated":{"unit":"b"},"by":{"unit":"r"}},"do":[{"setSwitch":"alarm","to":true}]}])"));
  w.battle.units[0].hp = 1;
  CHECK(w.act(Action::rescue(0, 1)));
  CHECK(w.act(Action::move(2, w.at(4, 1))));  // a soldier wades into the water square the boat left
  w.act(Action::endTurn());
  CHECK(w.act(Action::attack(3, 0, 0)));
  CHECK(!w.battle.exists(w.un(1)) && w.un(1).hp == 0 && w.un(1).carriedBy == -1);
  CHECK(!w.un(0).alive && w.un(0).carrying == -1);
  // The carrier, then its unit, on the carrier's square (the leader rule then takes the rest of the side).
  const int first = story::find(w.events, EventType::UnitDied);
  CHECK(first >= 0 && w.events[(size_t)first].a == 0);
  const int second = story::find(w.events, EventType::UnitDied, (size_t)first + 1);
  CHECK(second > first && w.events[(size_t)second].a == 1 && w.events[(size_t)second].b == w.at(3, 1));
  CHECK_EQ(w.count(EventType::UnitDropped), 0);
  CHECK(w.fired("lost") && w.sw("alarm"));  // the story hears of it
  CHECK(w.battle.over && w.battle.winnerTeam == 1);  // the leader rule
}

// ---- a carried unit still exists ----

TEST(rescue_carry_a_carried_unit_is_not_defeated_for_the_objectives) {
  // Gorm is carried by a red ally: "defeated gorm" must not hold, the battle goes on.
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}, {"archer", 1, 4, 1, false}},
                open(), {"eirik", "gorm", "grunt"});
  CHECK(w.play("", R"({"win":[{"defeated":{"unit":"gorm"}}]})"));
  w.act(Action::endTurn());
  CHECK(w.act(Action::rescue(2, 1)));
  CHECK(w.carriedBy(1, 2));
  CHECK(!w.battle.over);
}

TEST(rescue_carry_a_defeated_side_takes_the_units_it_carries_along) {
  // Red's leader falls while red's archer carries a soldier: the whole side leaves, carried unit included.
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 4, 1, true}, {"archer", 1, 4, 2, false},
                                    {"soldier", 1, 3, 2, false}});
  w.db.rules.defeatLeaderDead = true;
  CHECK(w.begin());
  w.act(Action::endTurn());
  CHECK(w.act(Action::rescue(2, 3)));  // red's archer picks up a red soldier
  w.act(Action::endTurn());
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(w.battle.sides[1].defeated && w.battle.over && w.battle.winnerTeam == 0);
  for (int id : {1, 2, 3}) CHECK(!w.battle.exists(w.un(id)));
  CHECK(w.un(2).carrying == -1 && w.un(3).carriedBy == -1 && w.un(3).hp == 0);
  int deaths = 0;
  for (const Event& e : w.events)
    if (e.type == EventType::UnitDied) ++deaths;
  CHECK_EQ(deaths, 3);
  CHECK_EQ(w.count(EventType::UnitDropped), 0);  // not put down: it left with its side
}

TEST(rescue_carry_a_side_whose_only_unit_is_carried_is_not_defeated) {
  // A state that only exists for an instant in real play (the carrier is gone, its unit not yet put down), set up
  // by hand: the "no units" rule must count the carried unit.
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 1, 1, false}, {"archer", 0, 2, 1, false}, {"soldier", 1, 4, 1, false}});
  CHECK(w.play(R"([{"id":"e","when":{"turn":9},"do":[{"setSwitch":"alarm","to":true}]}])"));
  CHECK(w.act(Action::rescue(0, 1)));
  Unit& carrier = w.battle.units[0];
  w.battle.occupant[(size_t)carrier.cell] = -1;
  carrier.alive = false;
  carrier.cell = kNoCell;
  carrier.hp = 0;
  w.act(Action::endTurn());  // settle() runs the defeat check
  CHECK(!w.battle.sides[0].defeated);
  w.battle.units[1].carriedBy = -1;  // now it is really gone
  w.battle.units[1].hp = 0;
  w.act(Action::endTurn());
  CHECK(w.battle.sides[0].defeated);
}

// ---- fog, items, hexagons ----

TEST(rescue_carry_a_drop_onto_an_unseen_enemy_is_blocked) {
  fogtest::FogWorld w({".....", ".....", "....."},
                      {{"blind", 0, 1, 1, false}, {"blind", 0, 0, 1, false}, {"soldier", 1, 2, 1, false}});
  w.db.rules.rescue.mode = RescueMode::Carry;
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 2));  // vision 0: the neighbour on plain ground is not seen
  CHECK(w.act(Action::rescue(0, 1)));
  w.act(Action::endTurn());
  w.act(Action::endTurn());
  std::vector<Cell> cells;
  w.battle.dropCells(0, w.battle.units[0].cell, cells);  // the square looks free to the team
  CHECK(std::find(cells.begin(), cells.end(), w.at(2, 1)) != cells.end());
  CHECK(w.act(Action::drop(0, w.at(2, 1))));
  const Event* e = w.last(EventType::UnitDropped);
  CHECK(e && e->flags == kBlocked && e->c == w.at(2, 1) && e->d == 2 && e->b == 1);
  CHECK(w.battle.units[1].carriedBy == 0 && !w.battle.units[1].alive);  // nothing happened to it
  CHECK(w.battle.units[0].done);  // but the order is spent
  CHECK_EQ(w.battle.unitAt(w.at(2, 1)), 2);
}

TEST(rescue_carry_a_carried_unit_keeps_its_items_and_nobody_trades_with_it) {
  RescueWorld w = line();
  fixture::addItems(w);
  fixture::carry(w, 1, {fixture::items::Potion});
  fixture::carry(w, 0, {fixture::items::Ring});
  CHECK(w.begin());
  CHECK(w.act(Action::rescue(0, 1)));
  CHECK_EQ(w.un(1).items.size(), (size_t)1);
  w.pass();
  std::vector<int> partners;
  w.battle.tradePartners(0, partners);
  CHECK(std::find(partners.begin(), partners.end(), 1) == partners.end());
  CHECK(!w.battle.validate(Action::takeItem(0, 1, 0)));
  CHECK(!w.battle.validate(Action::giveItem(0, 0, 1)));
  CHECK_EQ(w.un(0).items.size(), (size_t)1);  // the carrier keeps trading with others
  CHECK(w.battle.validate(Action::giveItem(0, 0, 2)));
}

TEST(rescue_carry_works_on_a_hexagon_grid) {
  RescueWorld w(RescueMode::Carry, {{"soldier", 0, 2, 1, false}, {"archer", 0, 3, 1, false}, {"soldier", 1, 4, 2, false}});
  w.db.gridType = w.map.gridType = GridType::Hex;
  w.db.hexLayout = w.map.hexLayout = HexLayout::Pointy;
  CHECK(w.begin());
  std::vector<int> out;
  w.battle.rescueTargets(0, w.un(0).cell, out);
  CHECK(out == std::vector<int>{1});
  CHECK(w.act(Action::rescue(0, 1)));
  w.pass();
  std::vector<Cell> cells;
  w.battle.dropCells(0, w.un(0).cell, cells);
  CHECK_EQ(cells.size(), (size_t)6);  // all six neighbours of a hexagon
  CHECK(w.act(Action::drop(0, cells[0])));
  CHECK(w.un(1).alive && w.un(1).cell == cells[0]);
}
