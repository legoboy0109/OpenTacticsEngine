// Resupply (docs/ECONOMY.md "Repair and supply"): own structures at the start of the turn, supplier units at the
// start of the turn and by the Supply action, and that new units (built, story-added, from a campaign roster)
// start with full fuel and ammunition.
#include "supply_fixture.h"

using namespace ote;
using namespace suptest;

TEST(supply_own_structures_refill_at_the_start_of_the_turn_for_free) {
  SupplyWorld w(field(), {{"ranger", 0, 0, 1, false}, {"ranger", 0, 1, 1, false}, {"soldier", 1, 6, 2, false}});
  w.place("hospital", 0, 1, 0);
  w.place("hospital", 1, 1, 0);
  CHECK(w.begin());
  w.battle.units[0].ammo[0] = 1;
  w.battle.units[1].ammo[0] = 3;               // full: nothing to do
  w.events.clear();
  const int funds = w.funds();
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.ammo(0, Gun), 3);
  CHECK_EQ(w.funds(), funds);                  // free
  std::vector<Event> got = w.all(EventType::UnitResupplied);
  CHECK_EQ(got.size(), size_t(1));
  CHECK_EQ(got[0].a, 0);
}

TEST(supply_structures_only_when_the_rule_says_so_and_only_own_ones) {
  SupplyWorld off(field(), {{"ranger", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  off.place("hospital", 0, 1, 0);
  off.db.rules.supply.structures = false;
  CHECK(off.begin());
  off.battle.units[0].ammo[0] = 0;
  CHECK(off.nextTurn());
  CHECK_EQ(off.battle.ammo(0, Gun), 0);
  CHECK_EQ(off.count(EventType::UnitResupplied), 0);

  SupplyWorld other(field(), {{"ranger", 0, 0, 1, false}, {"ranger", 0, 1, 1, false}, {"soldier", 1, 6, 2, false}});
  other.place("hospital", 0, 1, 1);            // red's
  other.place("hospital", 1, 1, -1);           // neutral
  CHECK(other.begin());
  other.battle.units[0].ammo[0] = 0;
  other.battle.units[1].ammo[0] = 0;
  CHECK(other.nextTurn());
  CHECK_EQ(other.battle.ammo(0, Gun), 0);
  CHECK_EQ(other.battle.ammo(1, Gun), 0);
}

TEST(supply_a_structure_serves_the_side_whose_turn_it_is) {
  SupplyWorld w(field(), {{"ranger", 0, 0, 1, false}, {"ranger", 1, 6, 1, false}});
  w.place("hospital", 6, 1, 1);
  CHECK(w.begin());
  w.battle.units[1].ammo[0] = 0;
  CHECK(w.act(Action::endTurn()));             // red's turn starts
  CHECK_EQ(w.battle.ammo(1, Gun), 3);
}

TEST(supply_a_supplier_fills_adjacent_units_of_its_side_at_the_turn_start) {
  // truck (2,1); plane above it, ranger right of it, another ranger two squares off, red ranger below it.
  SupplyWorld w(field(), {{"truck", 0, 2, 1, false}, {"plane", 0, 2, 0, false}, {"ranger", 0, 3, 1, false},
                          {"ranger", 0, 5, 1, false}, {"ranger", 1, 2, 2, false}});
  CHECK(w.begin());
  w.battle.units[1].fuel = 2;
  w.battle.units[2].ammo[0] = 0;
  w.battle.units[3].ammo[0] = 0;
  w.battle.units[4].ammo[0] = 0;
  w.battle.units[0].fuel = 10;                 // the truck itself: it does not fill itself
  w.events.clear();
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.fuel(1), 16);              // filled to 20 before the burn of 4
  CHECK_EQ(w.battle.ammo(2, Gun), 3);
  CHECK_EQ(w.battle.ammo(3, Gun), 0);          // two squares away
  CHECK_EQ(w.battle.ammo(4, Gun), 0);          // an enemy
  CHECK_EQ(w.battle.fuel(0), 10);
  std::vector<Event> got = w.all(EventType::UnitResupplied);
  CHECK_EQ(got.size(), size_t(2));
  CHECK(got[0].a == 1 && got[1].a == 2);       // unit index order
}

TEST(supply_action_in_detail) {
  SupplyWorld w(field(), {{"truck", 0, 2, 1, false}, {"ranger", 0, 3, 1, false}, {"ranger", 0, 1, 1, false},
                          {"ranger", 0, 2, 0, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  CHECK(!w.battle.canSupply(0));               // nobody lacks anything yet
  CHECK_STR(w.refused(Action::supply(0)), "there is nobody next to it who needs supplies");
  CHECK_STR(w.refused(Action::supply(1)), "that unit cannot supply others");
  w.battle.units[1].ammo[0] = 1;               // right of the truck
  w.battle.units[3].ammo[0] = 0;               // above the truck
  w.battle.units[2].ammo[0] = 2;               // left of it
  CHECK(w.battle.canSupply(0));
  std::vector<int> targets;
  w.battle.supplyTargetsFrom(0, w.battle.units[0].cell, targets);
  CHECK_EQ(targets.size(), size_t(3));
  CHECK(targets[0] == 1 && targets[1] == 2 && targets[2] == 3);
  w.events.clear();
  CHECK(w.act(Action::supply(0)));
  CHECK_EQ(w.battle.ammo(1, Gun), 3);
  CHECK_EQ(w.battle.ammo(2, Gun), 3);
  CHECK_EQ(w.battle.ammo(3, Gun), 3);
  CHECK(w.battle.units[0].done);               // its turn is over
  CHECK_EQ(w.count(EventType::UnitResupplied), 3);
  CHECK(!w.battle.canSupply(0));
  CHECK_STR(w.refused(Action::supply(0)), "that unit has already acted");
  CHECK(w.battle.units[1].done == false);      // the supplied units keep their turn
}

TEST(supply_action_after_moving_next_to_somebody_in_need) {
  SupplyWorld w(field(), {{"truck", 0, 0, 1, false}, {"ranger", 0, 4, 1, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.begin());
  w.battle.units[1].ammo[0] = 0;
  CHECK(!w.battle.canSupply(0));               // too far away
  std::vector<int> targets;
  w.battle.supplyTargetsFrom(0, w.at(3, 1), targets);   // what it would reach from there
  CHECK_EQ(targets.size(), size_t(1));
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.battle.canSupply(0));
  CHECK(w.act(Action::supply(0)));
  CHECK_EQ(w.battle.ammo(1, Gun), 3);
}

TEST(supply_action_is_for_the_active_side_and_never_for_the_enemy) {
  SupplyWorld w(field(), {{"truck", 0, 2, 1, false}, {"ranger", 1, 3, 1, false}, {"soldier", 0, 6, 2, false}});
  CHECK(w.begin());
  w.battle.units[1].ammo[0] = 0;               // an enemy next to the truck
  CHECK(!w.battle.canSupply(0));
  CHECK(w.act(Action::endTurn()));
  CHECK_STR(w.refused(Action::supply(0)), "that unit cannot act now");   // red's turn
  CHECK_EQ(w.battle.ammo(1, Gun), 0);          // a blue truck does not serve red units at red's turn start either
}

TEST(supply_new_units_start_full) {
  // Built.
  SupplyWorld b(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});
  b.db.rules.production = true;
  b.place("factory", 2, 2, 0);
  b.db.structures[(size_t)b.db.findStructure("factory")].produces = {0, Plane, Ranger};
  b.map.sides[0].resources = {{0, 5000}};
  CHECK(b.begin());
  CHECK(b.act(Action::build(b.at(2, 2), Plane)));
  const Unit& plane = b.battle.units.back();
  CHECK_EQ(plane.fuel, 20);                    // not burnt yet: that happens at its side's next turn start
  CHECK(b.nextTurn());
  CHECK_EQ(b.battle.fuel(plane.id), 16);
  CHECK(b.battle.units.back().alive);
  // Built ranger.
  SupplyWorld r(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});
  r.db.rules.production = true;
  r.place("factory", 2, 2, 0);
  r.db.structures[(size_t)r.db.findStructure("factory")].produces = {0, Plane, Ranger};
  r.map.sides[0].resources = {{0, 5000}};
  CHECK(r.begin());
  CHECK(r.act(Action::build(r.at(2, 2), Ranger)));
  CHECK_EQ(r.battle.units.back().ammo.size(), size_t(2));
  CHECK_EQ(r.battle.ammo(r.battle.units.back().id, Gun), 3);
}

TEST(supply_story_added_units_start_full) {
  SupplyWorld w(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});
  CHECK(w.load(R"([{"id":"air","when":{"turn":2,"side":"blue"},"do":[
      {"addUnit":{"type":"plane","side":"blue","x":4,"y":0}},
      {"addUnit":{"type":"ranger","side":"blue","x":4,"y":1}}]}])").empty());
  CHECK(w.begin());
  CHECK(w.nextTurn());
  CHECK_EQ(w.count(EventType::UnitAdded), 2);
  int plane = w.unitIdAt(4, 0), ranger = w.unitIdAt(4, 1);
  CHECK(plane >= 0 && ranger >= 0);
  CHECK_EQ(w.battle.fuel(plane), 20);          // the story runs after the turn's burn: full
  CHECK_EQ(w.battle.ammo(ranger, Gun), 3);
}

TEST(supply_a_campaign_roster_unit_starts_every_chapter_full) {
  SupplyWorld w(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}}, {"hero"});
  std::string err;
  CHECK(w.formulas.init(w.db, err));
  CHECK(w.battle.init(w.db, w.map, w.formulas, 1, err));
  RosterUnit r;
  r.id = "hero";
  r.type = Ranger;      // promoted since the last chapter
  r.name = "Hero";
  r.stats = w.db.unitTypes[Ranger].stats;
  auto roster = std::make_shared<Roster>();
  roster->push_back(r);
  w.battle.setStartingRoster(roster);
  std::vector<Event> ev;
  w.battle.start(ev);
  CHECK_EQ(w.battle.units[0].type, (int)Ranger);
  CHECK_EQ(w.battle.ammo(0, Gun), 3);
  CHECK_EQ(w.battle.units[0].ammo.size(), size_t(2));
  // And a plane as it came from the roster: full tank, minus the first turn's burn.
  SupplyWorld p(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}}, {"pilot"});
  CHECK(p.formulas.init(p.db, err));
  CHECK(p.battle.init(p.db, p.map, p.formulas, 1, err));
  RosterUnit pr;
  pr.id = "pilot";
  pr.type = Plane;
  pr.name = "Pilot";
  pr.stats = p.db.unitTypes[Plane].stats;
  auto roster2 = std::make_shared<Roster>();
  roster2->push_back(pr);
  p.battle.setStartingRoster(roster2);
  CHECK_EQ(p.battle.fuel(0), 20);
  p.battle.start(ev);
  CHECK_EQ(p.battle.fuel(0), 16);
}

TEST(supply_a_game_without_the_rules_changes_nothing) {
  SupplyWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 6, 2, false}});
  w.db.rules.supply = SupplyRules();
  CHECK(w.begin());
  w.battle.units[0].hp = 3;
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.units[0].hp, 3);
  CHECK_EQ(w.count(EventType::UnitRepaired) + w.count(EventType::UnitResupplied), 0);
  CHECK_EQ(w.battle.units[0].fuel, -1);
  CHECK(w.battle.units[0].ammo.empty());
}
