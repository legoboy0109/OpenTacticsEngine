// Staves, stage 2 (docs/SRPG.md section 8 "Staves: stage two, as built"): the questions about warp, pull, swap, repair and light.
// Like staves.cpp nothing here changes the battle or rolls a die; applying is staves_apply.cpp.
//
// Where a unit may stand is the one rule the rest of the engine uses: its movement type must be able to enter the terrain
// there (enterCost > 0), nobody stands there, and the square is on the map. A staff never moves a unit onto a square that a
// walk, a drop or an ability would refuse it.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"

namespace ote {

namespace {
const StaffDef* staffDefOf(const Database& db, int item) {
  if (item < 0 || (size_t)item >= db.items.size() || db.items[(size_t)item].kind != ItemKind::Staff) return nullptr;
  return &db.staves[(size_t)db.items[(size_t)item].staff];
}
}  // namespace

void Battle::staffWarpRange(const Unit& user, const StaffDef& d, const StaffEffect& fx, int& minRange, int& maxRange) const {
  if (fx.warpExpr < 0) {
    minRange = fx.warpMin;
    maxRange = fx.warpMax;
    return;
  }
  minRange = 1;
  const double v = staffEval(fx.warpExpr, user, nullptr, d, kNoCell, 0);
  maxRange = !(v > 0) ? 0 : v >= kMaxStaffRange ? kMaxStaffRange : (int)std::floor(v);
}

// Free squares a warp may send `t` to. The user stands on `from` (its current square counts as free when it is somewhere else,
// since it will have left it). With fog the team must SEE the square; a square that looks empty but holds an enemy the team
// does not see is offered anyway, and the warp then fails when it is carried out (the use is still spent), as a drop does.
void Battle::staffWarpCells(int unit, int slot, int target, Cell from, std::vector<Cell>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || target < 0 || target >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  const Unit& t = units[(size_t)target];
  if (!u.alive || !t.alive || t.cell == kNoCell || slot < 0 || slot >= (int)u.items.size()) return;
  const StaffDef* d = staffDefOf(*db, u.items[(size_t)slot].item);
  const StaffEffect* fx = d ? d->find(StaffEffect::Kind::Warp) : nullptr;
  if (!fx) return;
  int minR = 1, maxR = 0;
  staffWarpRange(u, *d, *fx, minR, maxR);
  if (maxR < 1) return;
  const int team = teamOf(u);
  std::vector<Cell> near;
  grid.within(from, maxR, near);
  for (Cell c : near) {
    if (c == from || c == t.cell || grid.distance(from, c) < minR || !staffCanStand(t, c)) continue;
    const int occ = occupant[(size_t)c];
    if (occ >= 0 && occ != unit && unitVisibleTo(team, occ)) continue;
    if (!visibleTo(team, c)) continue;
    out.push_back(c);
  }
}

Cell Battle::staffPullCell(int unit, int target, Cell from) const {
  if (unit < 0 || unit >= (int)units.size() || target < 0 || target >= (int)units.size() || !grid.valid(from)) return kNoCell;
  const Unit& t = units[(size_t)target];
  if (!units[(size_t)unit].alive || !t.alive || t.cell == kNoCell || target == unit) return kNoCell;
  if (grid.distance(from, t.cell) <= 1) return kNoCell;  // already next to the user: nothing to pull
  Cell around[Grid::kMaxNeighbors];
  const int n = grid.neighbors(from, around);
  Cell best = kNoCell;
  int bestDist = 0;
  for (int i = 0; i < n; ++i) {
    const Cell c = around[i];
    if ((occupant[(size_t)c] >= 0 && occupant[(size_t)c] != unit) || !staffCanStand(t, c)) continue;
    // Nearest to where the unit is now; the lower cell number wins a tie (the neighbour order differs between grids).
    const int dist = grid.distance(c, t.cell);
    if (best == kNoCell || dist < bestDist || (dist == bestDist && c < best)) {
      best = c;
      bestDist = dist;
    }
  }
  return best;
}

void Battle::staffRepairSlots(int unit, int slot, int target, std::vector<int>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || target < 0 || target >= (int)units.size()) return;
  const Unit& t = units[(size_t)target];
  for (size_t i = 0; i < t.items.size(); ++i) {
    if (target == unit && (int)i == slot) continue;  // the staff being used is not mended by itself
    const ItemStack& st = t.items[i];
    const ItemDef& def = db->items[(size_t)st.item];
    if (st.uses < 0 || def.uses < 0 || st.uses >= def.uses || !def.repairable) continue;
    out.push_back((int)i);
  }
}

void Battle::staffLightCells(int unit, int slot, Cell from, std::vector<Cell>& out) const {
  out.clear();
  if (!fog || unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive || slot < 0 || slot >= (int)u.items.size()) return;
  const int item = u.items[(size_t)slot].item;
  const StaffDef* d = staffDefOf(*db, item);
  const StaffEffect* fx = d ? d->find(StaffEffect::Kind::Light) : nullptr;
  if (!fx || !canWieldStaff(u, item)) return;
  int minR = 1, maxR = 1;
  staffRange(u, item, minR, maxR);
  const int team = teamOf(u);
  std::vector<Cell> near, area;
  grid.within(from, maxR, near);
  for (Cell c : near) {
    if (grid.distance(from, c) < minR) continue;
    grid.within(c, fx->radius, area);
    for (Cell a : area)
      if (!visibleTo(team, a)) {  // something would come into sight
        out.push_back(c);
        break;
      }
  }
}

// null = the effect would change `t`; else the plain reason it would not.
const char* Battle::staffEffectWhy(const Unit& user, const StaffDef& d, const StaffEffect& fx, int slot, const Unit& t, Cell from) const {
  switch (fx.kind) {
    case StaffEffect::Kind::Heal:
      return t.hp < maxHp(t) && staffHealAmount(user, d, fx, t, from) > 0 ? nullptr : "nobody in reach needs that";
    case StaffEffect::Kind::Warp: {
      std::vector<Cell> cells;
      staffWarpCells(user.id, slot, t.id, from, cells);
      return cells.empty() ? "there is no free square in reach of the user that it could stand on" : nullptr;
    }
    case StaffEffect::Kind::Pull:
      if (t.id == user.id) return "a unit cannot be pulled to itself";
      if (grid.distance(from, t.cell) <= 1) return "that unit is already next to the user";
      return staffPullCell(user.id, t.id, from) == kNoCell ? "there is no free square next to the user that it could stand on" : nullptr;
    case StaffEffect::Kind::Swap:
      if (t.id == user.id) return "a unit cannot swap with itself";
      return staffCanStand(user, t.cell) && staffCanStand(t, from) ? nullptr : "they could not stand on each other's squares";
    case StaffEffect::Kind::Repair: {
      std::vector<int> slots;
      staffRepairSlots(user.id, slot, t.id, slots);
      return slots.empty() ? "that unit carries nothing that needs repair" : nullptr;
    }
    case StaffEffect::Kind::Light: return "a light picks a square";
    case StaffEffect::Kind::Inflict: {
      const int why = inflictWhy(t, fx.conds[0], fx.turns);
      return why == 0 ? nullptr : why == 1 ? "that unit is immune to it" : "that unit has it already";
    }
    case StaffEffect::Kind::Cure:
      return cureWould(t, fx.conds, fx.cureAll) ? nullptr : "that unit has nothing it cures";
  }
  return "nobody in reach needs that";
}

std::string Battle::staffTargetWhy(int unit, int slot, int target, Cell from) const {
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return "that is not a unit on the board";
  const Unit& u = units[(size_t)unit];
  if (slot < 0 || slot >= (int)u.items.size()) return "the unit does not carry that item";
  const StaffDef* d = staffDefOf(*db, u.items[(size_t)slot].item);
  if (!d) return "that item is not a staff";
  if (target < 0 || target >= (int)units.size() || !units[(size_t)target].alive || units[(size_t)target].cell == kNoCell)
    return "that is not a unit on the board";
  const Unit& t = units[(size_t)target];
  if (t.id != u.id && !unitVisibleTo(teamOf(u), t)) return "you cannot see that unit";
  const bool mine = teamOf(t) == teamOf(u);
  if ((d->target == StaffTarget::Ally && (!mine || t.id == u.id)) || (d->target == StaffTarget::Team && !mine) ||
      (d->target == StaffTarget::Foe && mine) || (d->target == StaffTarget::Any && t.id == u.id) ||
      (d->target == StaffTarget::Self && t.id != u.id))
    return "that staff does not work on that unit";
  int minR = 1, maxR = 1;
  staffRange(u, u.items[(size_t)slot].item, minR, maxR);
  const int dist = grid.distance(from, t.id == u.id ? from : t.cell);
  if (dist < minR || dist > maxR) return "that unit is out of range";
  std::vector<int> legal;
  staffTargets(unit, slot, from, legal);
  if (std::find(legal.begin(), legal.end(), target) != legal.end()) return "";
  // Not legal: the first effect's reason. When every effect could work, the staff's "if" is what said no, which reads the
  // same as having nothing to do.
  for (const StaffEffect& fx : d->effects) {
    const char* why = staffEffectWhy(u, *d, fx, slot, t, from);
    if (why) return why;
  }
  return "nobody in reach needs that";
}

}  // namespace ote
