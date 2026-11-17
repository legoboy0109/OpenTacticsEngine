// Helpers of the stage-two staff tests (docs/SRPG.md section 8: warp, pull, swap, repair, light): a loaded battle on
// tests/fixtures/srpg-staves2.
//
// "yard" (10 x 6; a pond at x 3-4, y 1-2 that only the glider can stand on; a wall at x 3-4, y 3; no fog): blue sage (mystic,
// the user, carries warp, recall, swap, shove, banish, hop, fix, patch and kindly-pull staves) at 2,2; ren (fighter; iron blade
// 8/8, glass blade 4/4 that is "repairable": false, short sword without limit, herb 3/3) at 8,2; wisp (glider, on the pond) at
// 3,2 with an iron blade; bram (knight) at 2,4; dax (fighter) at 6,4; red g1 at 9,0 and g2 at 5,5.
// "mist" (12 x 5, fog; brush at 4,3 hides units): sage at 1,2 with lantern, spark, warp and recall staves; ola at 2,3; red g1 in the
// brush at 4,3 and g2 at 7,2. Vision is 2 (the default), so the red units are out of sight.
// "gate" (6 x 4): sage at 3,2 (warp, swap, recall), bram at 1,3, dax at 4,3, red g1 at 5,0; an event when bram ends a move on 0,0 and
// a victory when dax ends one on 5,3.
#pragma once
#include <functional>

#include "srpg_ranks_fixture.h"

namespace staves2test {

using namespace rankstest;

struct Yard : World {
  explicit Yard(const char* name, const char* map = "yard", const std::function<void(Copy&)>& tweak = nullptr)
      : World(name, map, tweak, "srpg-staves2") {}
  int slot(const char* who, const char* staff) { return slotOf(who, staff); }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  Cell cellOf(const char* who) { return unit(who).cell; }
  // Puts a unit on a square by hand (a test arranging the board).
  void place(const char* who, int x, int y) {
    Unit& u = unit(who);
    if (u.cell != kNoCell) b().occupant[(size_t)u.cell] = -1;
    u.cell = at(x, y);
    b().occupant[(size_t)u.cell] = u.id;
    b().refreshSight();
  }
  // Sets the uses of the unit's first item of this kind.
  void setUses(const char* who, const char* item, int uses) { unit(who).items[(size_t)slotOf(who, item)].uses = uses; }
  Action act(const char* who, const char* staff, const char* target, int x = -1, int y = -1, int itemSlot = -1) {
    return Action::useStaff(id(who), slotOf(who, staff), target ? id(target) : -1, x >= 0 ? at(x, y) : kNoCell, itemSlot);
  }
  bool use(const char* who, const char* staff, const char* target, int x = -1, int y = -1, int itemSlot = -1) {
    return run(act(who, staff, target, x, y, itemSlot));
  }
  std::string refusal(const char* who, const char* staff, const char* target, int x = -1, int y = -1, int itemSlot = -1) {
    return why(act(who, staff, target, x, y, itemSlot));
  }
  StaffOption option(const char* who, const char* staff) {
    std::vector<StaffOption> opts;
    b().staffOptions(id(who), unit(who).cell, opts);
    for (const StaffOption& o : opts)
      if (o.item == item(staff)) return o;
    return StaffOption();
  }
  std::vector<int> targets(const char* who, const char* staff) {
    std::vector<int> out;
    b().staffTargets(id(who), slotOf(who, staff), unit(who).cell, out);
    return out;
  }
  std::vector<Cell> warpCells(const char* who, const char* staff, const char* target) {
    std::vector<Cell> out;
    b().staffWarpCells(id(who), slotOf(who, staff), id(target), unit(who).cell, out);
    return out;
  }
  std::vector<int> repairSlots(const char* who, const char* staff, const char* target) {
    std::vector<int> out;
    b().staffRepairSlots(id(who), slotOf(who, staff), id(target), out);
    return out;
  }
  std::vector<Cell> lightCells(const char* who, const char* staff) {
    std::vector<Cell> out;
    b().staffLightCells(id(who), slotOf(who, staff), unit(who).cell, out);
    return out;
  }
  // Does blue (team 0, the user's side) see the unit?
  bool blueSees(const char* who) { return b().unitVisibleTo(0, unit(who)); }
  bool blueSees(int x, int y) { return b().visibleTo(0, at(x, y)); }
};

}  // namespace staves2test
