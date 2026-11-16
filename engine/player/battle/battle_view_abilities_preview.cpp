// See ability_preview.h. (Named battle_view_* so the unit tests' source glob picks it up.)
#include "battle/ability_preview.h"

namespace ote {

std::vector<AbilityMovePreview> previewAbilityMoves(const Battle& b, int user, int ability, int target) {
  std::vector<AbilityMovePreview> out;
  if (user < 0 || target < 0 || (size_t)user >= b.units.size() || (size_t)target >= b.units.size()) return out;
  if (ability < 0 || (size_t)ability >= b.db->abilities.size()) return out;
  const AbilityDef& def = b.db->abilities[(size_t)ability];
  const Unit& u = b.units[(size_t)user];
  const Unit& t = b.units[(size_t)target];
  const int team = b.sides[(size_t)u.side].team;
  // A unit in the way that the user's team can see (fog off: every unit).
  auto seenOccupant = [&](Cell c) { return b.unitAt(c) >= 0 && b.visibleTo(team, c); };
  const int dir = b.grid.directionTo(u.cell, t.cell);
  const Cell back = b.grid.neighbor(u.cell, b.grid.opposite(dir));
  switch (def.effect) {
    case AbilityEffect::Push: {
      AbilityMovePreview m;
      m.unit = target;
      Cell at = t.cell;
      for (int step = 0; step < def.amount; ++step) {
        Cell next = b.grid.neighbor(at, dir);
        if (!b.grid.valid(next) || b.enterCost(next, b.typeOf(t).moveType) <= 0 || seenOccupant(next)) break;
        at = next;
        m.path.push_back(at);
      }
      if (!m.path.empty()) out.push_back(m);
      break;
    }
    case AbilityEffect::Pull:
      if (b.grid.valid(back) && !seenOccupant(back)) out = {{target, {u.cell}}, {user, {back}}};
      break;
    case AbilityEffect::Swap: out = {{target, {u.cell}}, {user, {t.cell}}}; break;
    case AbilityEffect::MoveBehind:
      if (b.grid.valid(back) && !seenOccupant(back)) out = {{target, {back}}};
      break;
    default: break;
  }
  return out;
}

}  // namespace ote
