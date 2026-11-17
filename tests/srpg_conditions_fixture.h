// Helpers of the status condition tests (docs/SRPG.md section 9): a loaded battle on tests/fixtures/srpg-conditions.
//
// "field" (12 x 6, no fog; blue human, red ai, but a test drives both): blue kay (fighter, iron blade, antidote, tonic, bad brew) at 2,2
// (leader); mira (mage: fire tome, poison / sleep / hex / rage / cure / purify / blessing staves) at 1,2; bo (knight, skill iron_will:
// immune to poison and sleep) at 2,4; nell (cleric: cure, mend-and-cure, purify, cleanse staves) at 1,3; red r1 (venom blade) at 5,2,
// r2 (iron blade) at 6,2, r3 (lull blade) at 7,4. Hit points: kay 24, mira 18, bo 30, nell 18, raiders 22.
// "mist" (12 x 5, fog; brush at 4,3 hides): blue kay at 1,2 and ola at 2,3; red g1 in the brush at 4,3, g2 at 9,2.
// "skirmish": two sides of AI units with staves and venom blades (the computer opponent).
#pragma once
#include <functional>

#include "srpg_ranks_fixture.h"

namespace condtest {

using namespace rankstest;

struct Field : World {
  explicit Field(const char* name, const char* map = "field", const std::function<void(Copy&)>& tweak = nullptr)
      : World(name, map, tweak, "srpg-conditions") {}
  Cell at(int x, int y) { return b().grid.at(x, y); }
  Cell cellOf(const char* who) { return unit(who).cell; }
  int cond(const char* id) const { return rig.db.findCondition(id); }
  int skill(const char* id) const { return rig.db.findSkill(id); }
  // Arranging the board by hand (as the other fixtures do): a unit on a square, a condition on a unit, a unit gone.
  void place(const char* who, int x, int y) {
    Unit& u = unit(who);
    if (u.cell != kNoCell) b().occupant[(size_t)u.cell] = -1;
    u.cell = at(x, y);
    b().occupant[(size_t)u.cell] = u.id;
    b().refreshSight();
  }
  void remove(const char* who) {
    Unit& u = unit(who);
    if (u.cell != kNoCell) b().occupant[(size_t)u.cell] = -1;
    u.cell = kNoCell;
    u.alive = false;
    u.hp = 0;
    b().refreshSight();
  }
  void give(const char* who, const char* c, int left = -1) {
    ConditionState cs;
    cs.condition = cond(c);
    cs.left = rig.db.conditions[(size_t)cs.condition].until == ConditionUntil::Turns ? (left > 0 ? left : rig.db.conditions[(size_t)cs.condition].turns) : -1;
    unit(who).conditions.push_back(cs);
    b().refreshSight();
  }
  bool has(const char* who, const char* c) { return b().hasCondition(unit(who), cond(c)); }
  int left(const char* who, const char* c) { return b().conditionLeft(unit(who), cond(c)); }
  int hp(const char* who) { return unit(who).hp; }
  // Both sides end their turn: blue's turn starts again (red's start came first).
  void endTurn() { CHECK(run(Action::endTurn())); }
  void round() {
    endTurn();
    endTurn();
  }
  Action staff(const char* who, const char* staff, const char* target) {
    return Action::useStaff(id(who), slotOf(who, staff), target ? id(target) : -1);
  }
  bool useStaff(const char* who, const char* staff, const char* target) { return run(this->staff(who, staff, target)); }
  std::string staffWhy(const char* who, const char* staff, const char* target) { return why(this->staff(who, staff, target)); }
  Action attack(const char* who, const char* target) {
    std::vector<int> weapons;
    b().weaponsOf(unit(who), weapons);
    return Action::attack(id(who), id(target), weapons.empty() ? -1 : weapons[0]);
  }
  bool strike(const char* who, const char* target) { return run(attack(who, target)); }
  StaffOption option(const char* who, const char* staffName) {
    std::vector<StaffOption> opts;
    b().staffOptions(id(who), unit(who).cell, opts);
    for (const StaffOption& o : opts)
      if (o.item == item(staffName)) return o;
    return StaffOption();
  }
  std::vector<int> targets(const char* who, const char* staffName) {
    std::vector<int> out;
    b().staffTargets(id(who), slotOf(who, staffName), unit(who).cell, out);
    return out;
  }
  StaffForecast forecast(const char* who, const char* staffName, const char* target) {
    StaffForecast f;
    b().staffForecast(id(who), slotOf(who, staffName), target ? id(target) : -1, unit(who).cell, f);
    return f;
  }
  // The n-th event of a type in what the last action made.
  const Event* nth(EventType t, int n = 0) const {
    for (const Event& e : events)
      if (e.type == t && n-- == 0) return &e;
    return nullptr;
  }
  // The n-th event of a type about this unit and condition.
  const Event* about(EventType t, const char* who, const char* c) {
    for (const Event& e : events)
      if (e.type == t && e.a == id(who) && e.b == cond(c)) return &e;
    return nullptr;
  }
};

}  // namespace condtest
