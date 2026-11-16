// Helpers of the staff tests (docs/SRPG.md section 8): a loaded battle on tests/fixtures/srpg-staves.
//
// The "clinic" map (8 x 5, no terrain): blue wen (cleric, level 3, rank C in staves; carries mend, rally, high, kin and chorus
// staves; leader) at 3,2; tarn (fighter) at 5,2; dax (generic fighter) at 5,3; brae (knight) at 3,4; ola (generic fighter) at 2,1;
// mira (generic cleric: kin, mend and rest staves) at 1,3; red g1 (raider) far away at 7,0. Everybody starts unhurt; a test
// hurts the ones it needs. Wen's mend staff: reach 1 + 6/3 = 3, heals 5 + 6/2 = 8, gives 15 experience, rank experience 2.
// Ladder e 0, d 10, c 25, b 45, growth "use". The "skirmish" map is an AI against AI game with healers on both sides.
#pragma once
#include <functional>

#include "srpg_ranks_fixture.h"

namespace stavestest {

using namespace rankstest;

struct Clinic : World {
  explicit Clinic(const char* name, const char* map = "clinic", const std::function<void(Copy&)>& tweak = nullptr)
      : World(name, map, tweak, "srpg-staves") {}
  void hurt(const char* who, int hp) { unit(who).hp = hp; }
  int slot(const char* who, const char* item) { return slotOf(who, item); }
  Action act(const char* who, const char* item, const char* target) {
    return Action::useStaff(id(who), slotOf(who, item), target ? id(target) : -1);
  }
  bool use(const char* who, const char* item, const char* target) { return run(act(who, item, target)); }
  std::string refusal(const char* who, const char* item, const char* target) { return why(act(who, item, target)); }
  // The amount of the UnitHealed event for a unit in the last run (0 = none).
  int healed(const char* who) const {
    for (const Event& e : events)
      if (e.type == EventType::UnitHealed && e.a == rig.battle.unitByStoryId(who)) return e.b;
    return 0;
  }
  // The staff option (from the unit's square) for an item id.
  StaffOption option(const char* who, const char* item) {
    std::vector<StaffOption> opts;
    b().staffOptions(id(who), unit(who).cell, opts);
    for (const StaffOption& o : opts)
      if (o.item == this->item(item)) return o;
    return StaffOption();
  }
};

}  // namespace stavestest
