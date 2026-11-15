// The computer side of repair and supply (docs/ECONOMY.md "AI"): retreating to repair, going for supplies,
// suppliers using Supply, never planning a move the fuel does not allow, and staying out of the way when the
// game has none of this.
#include "core/ai/ai.h"
#include "save_helpers.h"
#include "supply_fixture.h"

using namespace ote;
using namespace suptest;

namespace {

// Red (the AI) to move; blue's units stand still. Runs the AI until red's turn ends (or `steps` decisions).
void redTurn(SupplyWorld& w, int steps = 20) {
  for (int i = 0; i < steps && !w.battle.over && w.battle.activeSide == 1; ++i) aiTakeStep(w.battle, w.events);
}

struct Red : SupplyWorld {
  Red(const std::vector<fixture::UnitSpec>& units) : SupplyWorld(field(), units) { map.sides[1].ai = true; }
  bool start() { return begin() && act(Action::endTurn()); }  // blue passes: it is red's turn
  Cell lastMoveEnd(int unit) const {
    Cell end = kNoCell;
    for (const Event& e : events)
      if (e.type == EventType::UnitMoved && e.a == unit && !e.data.empty()) end = e.data.back();
    return end;
  }
};

}  // namespace

TEST(ai_a_badly_hurt_unit_goes_to_a_repairing_structure_of_its_side) {
  // Red soldier (4,1) at 3/10 HP, the red hospital two squares behind it, the enemy far away.
  Red w({{"soldier", 1, 4, 1, false}, {"soldier", 0, 0, 1, false}});
  w.place("hospital", 6, 1, 1);
  CHECK(w.start());
  w.battle.units[0].hp = 3;
  w.events.clear();
  redTurn(w);
  CHECK_EQ(w.battle.units[0].cell, w.at(6, 1));
  CHECK(w.battle.over == false);
  // At red's next turn it is repaired.
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK(w.battle.units[0].hp > 3);
}

TEST(ai_without_the_repair_rule_it_does_not_retreat) {
  Red w({{"soldier", 1, 4, 1, false}, {"soldier", 0, 0, 1, false}});
  w.place("hospital", 6, 1, 1);
  w.db.rules.supply = SupplyRules();
  CHECK(w.start());
  w.battle.units[0].hp = 3;
  redTurn(w);
  CHECK(w.battle.units[0].cell != w.at(6, 1));
}

TEST(ai_a_hurt_unit_with_a_kill_available_attacks_instead) {
  Red kill({{"soldier", 1, 4, 1, false}, {"soldier", 0, 3, 1, false}});
  kill.place("hospital", 6, 1, 1);
  CHECK(kill.start());
  kill.battle.units[0].hp = 3;
  kill.battle.units[1].hp = 1;                    // one blow kills it
  kill.events.clear();
  redTurn(kill);
  CHECK(!kill.battle.units[1].alive);
  CHECK(kill.battle.units[0].cell == kill.at(4, 1));  // it stayed to fight
  Red noKill({{"soldier", 1, 4, 1, false}, {"soldier", 0, 3, 1, false}});
  noKill.place("hospital", 6, 1, 1);
  CHECK(noKill.start());
  noKill.battle.units[0].hp = 3;
  redTurn(noKill);
  CHECK_EQ(noKill.battle.units[0].cell, noKill.at(6, 1));  // no kill: it withdraws
  CHECK(noKill.battle.units[1].alive);
}

TEST(ai_a_unit_that_cannot_pay_for_any_repair_does_not_go_there) {
  Red w({{"soldier", 1, 4, 1, false}, {"soldier", 0, 0, 1, false}});
  w.place("hospital", 6, 1, 1);
  w.map.sides[1].resources = {{0, 0}};
  CHECK(w.start());
  w.battle.units[0].hp = 3;
  redTurn(w);
  CHECK(w.battle.units[0].cell != w.at(6, 1));
}

TEST(ai_a_hurt_unit_ignores_a_structure_that_does_not_repair_its_kind_or_is_too_far) {
  Red w({{"soldier", 1, 4, 1, false}, {"soldier", 0, 0, 1, false}});
  w.place("workshop", 6, 1, 1);                   // tanks only
  CHECK(w.start());
  w.battle.units[0].hp = 3;
  redTurn(w);
  CHECK(w.battle.units[0].cell != w.at(6, 1));
  Red far({{"soldier", 1, 3, 1, false}, {"soldier", 0, 0, 1, false}});
  far.place("hospital", 6, 2, 1);                 // 4 squares: out of reach (3) this turn
  CHECK(far.start());
  far.battle.units[0].hp = 3;
  redTurn(far);
  CHECK(far.battle.units[0].cell != far.at(6, 2));
}

TEST(ai_a_unit_with_little_fuel_heads_for_its_own_structure) {
  Red w({{"plane", 1, 3, 1, false}, {"soldier", 0, 0, 1, false}});
  w.place("hospital", 6, 1, 1);
  CHECK(w.start());
  w.battle.units[0].fuel = 4;                     // under a quarter of 20
  redTurn(w);
  CHECK_EQ(w.battle.units[0].cell, w.at(6, 1));
  CHECK(w.battle.units[0].fuel >= 0);
  // A plane with plenty of fuel does not.
  Red full({{"plane", 1, 3, 1, false}, {"soldier", 0, 0, 1, false}});
  full.place("hospital", 6, 1, 1);
  CHECK(full.start());
  redTurn(full);
  CHECK(full.battle.units[0].cell != full.at(6, 1));
}

TEST(ai_a_unit_out_of_ammunition_goes_for_supplies) {
  Red w({{"tank", 1, 4, 1, false}, {"soldier", 0, 0, 1, false}});
  w.place("hospital", 6, 1, 1);
  CHECK(w.start());
  w.battle.units[0].ammo[0] = 0;
  redTurn(w);
  CHECK_EQ(w.battle.units[0].cell, w.at(6, 1));
  // A ranger with an empty gun still has its sword: not "out of ammunition".
  Red r({{"ranger", 1, 4, 1, false}, {"soldier", 0, 0, 1, false}});
  r.place("hospital", 6, 1, 1);
  CHECK(r.start());
  r.battle.units[0].ammo[0] = 0;
  redTurn(r);
  CHECK(r.battle.units[0].cell != r.at(6, 1));
}

TEST(ai_with_no_depot_a_low_unit_walks_to_a_supplier_and_the_supplier_fills_it) {
  // The truck cannot move (empty tank), so the tank has to come to it. The enemy is out of everybody's reach.
  Red w({{"tank", 1, 3, 2, false}, {"truck", 1, 6, 0, false}, {"soldier", 0, 0, 0, false}});
  w.db.rules.supply.structures = false;
  CHECK(w.start());
  w.battle.units[0].ammo[0] = 0;
  w.battle.units[1].fuel = 0;
  w.events.clear();
  redTurn(w);
  CHECK_EQ(w.battle.grid.distance(w.battle.units[0].cell, w.battle.units[1].cell), 1);  // next to the truck
  CHECK_EQ(w.battle.ammo(0, 2), 2);                                                      // and filled up by Supply
  CHECK_EQ(w.count(EventType::UnitResupplied), 1);
  CHECK(w.battle.units[1].done);
}

TEST(ai_a_supplier_does_not_supply_when_nobody_needs_it) {
  Red w({{"truck", 1, 5, 2, false}, {"ranger", 1, 6, 2, false}, {"soldier", 0, 0, 0, false}});
  CHECK(w.start());
  w.events.clear();
  redTurn(w);
  CHECK_EQ(w.count(EventType::UnitResupplied), 0);
}

TEST(ai_a_supplier_moves_next_to_a_unit_in_need_and_supplies_it) {
  Red w({{"truck", 1, 3, 2, false}, {"ranger", 1, 6, 2, false}, {"soldier", 0, 0, 0, false}});
  CHECK(w.start());
  w.battle.units[1].ammo[0] = 0;                  // only the gun; it keeps its sword
  w.events.clear();
  redTurn(w);
  CHECK_EQ(w.battle.ammo(1, Gun), 3);
  CHECK(w.battle.units[0].done);
}

namespace {

// Two AI sides, planes, rangers, trucks and hospitals: plays to the end and checks the books at every step.
struct Skirmish {
  SupplyWorld w;
  Skirmish()
      : w(field(), {{"plane", 0, 0, 0, false}, {"ranger", 0, 0, 1, false}, {"truck", 0, 1, 2, false}, {"tank", 0, 1, 1, false},
                    {"plane", 1, 6, 2, false}, {"ranger", 1, 6, 1, false}, {"truck", 1, 5, 0, false}, {"tank", 1, 5, 1, false}}) {
    w.map.sides[0].ai = w.map.sides[1].ai = true;
    w.place("hospital", 0, 2, 0);
    w.place("hospital", 6, 0, 1);
  }
  void play(int steps, bool check) {
    if (!w.begin(5)) return;
    for (int i = 0; i < steps && !w.battle.over; ++i) {
      aiTakeStep(w.battle, w.events);
      if (!check) continue;
      for (const Unit& u : w.battle.units) {
        if (!u.alive) continue;
        const UnitTypeDef& t = w.battle.typeOf(u);
        if (u.fuel < -1 || u.fuel > t.fuel) CHECK(false);
        if (t.fuel > 0 && u.fuel < 0) CHECK(false);
        for (size_t k = 0; k < u.ammo.size(); ++k)
          if (u.ammo[k] < -1 || u.ammo[k] > w.battle.db->weapons[(size_t)t.weapons[k]].ammo) CHECK(false);
        if (u.hp > w.battle.maxHp(u)) CHECK(false);
      }
    }
  }
};

}  // namespace

TEST(ai_plays_whole_battles_with_fuel_ammunition_and_repair_and_stays_consistent) {
  Skirmish a;
  a.play(1500, true);
  CHECK(a.w.battle.over || a.w.battle.round > 3);  // it ended, or at least it kept going without a stall of the books
  CHECK(a.w.count(EventType::UnitRepaired) + a.w.count(EventType::UnitResupplied) > 0);
}

TEST(ai_with_supply_rules_is_deterministic) {
  Skirmish a, b;
  a.play(1500, false);
  b.play(1500, false);
  CHECK(savetest::sameEvents(a.w.events, b.w.events));
  CHECK_STR(savetest::stateText(a.w.battle), savetest::stateText(b.w.battle));
  // A copy taken in the middle plays on identically.
  Skirmish c;
  c.play(40, false);
  Battle copy = c.w.battle;
  std::vector<Event> e1, e2;
  for (int i = 0; i < 200 && !c.w.battle.over; ++i) {
    aiTakeStep(c.w.battle, e1);
    aiTakeStep(copy, e2);
  }
  CHECK(savetest::sameEvents(e1, e2));
}
