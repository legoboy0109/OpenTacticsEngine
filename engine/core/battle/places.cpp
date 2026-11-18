// Doors, chests and visitable places, the questions (docs/SRPG.md section 10): who may do what to which structure from where.
// Nothing here changes the battle or rolls a die, so menus, forecasts and the computer opponent can ask freely. Carrying an
// action out is places_apply.cpp; unlock abilities and staves are places_unlock.cpp.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/data/loader_places.h"
#include "core/script/formulas.h"

namespace ote {

namespace {

int wholeNumber(double v) {
  if (!(v > -1e9)) return 0;
  if (v > 1e6) return 1000000;
  return (int)std::floor(v);
}

}  // namespace

int Battle::placeByStoryId(const std::string& id) const {
  if (id.empty()) return -1;
  for (size_t i = 0; i < structures.size(); ++i)
    if (structures[i].id == id) return (int)i;
  return -1;
}

bool Battle::isClosedDoor(int structure) const {
  return structure >= 0 && structure < (int)structures.size() && placeOf(structure).isDoor();
}

bool Battle::isClosedChest(int structure) const {
  return structure >= 0 && structure < (int)structures.size() && placeOf(structure).isChest();
}

int Battle::placeDistance(int structure, Cell from) const {
  std::vector<Cell> cells;
  structureCells(structure, cells);
  int best = 1 << 20;
  for (Cell c : cells) best = std::min(best, grid.distance(from, c));
  return best;
}

bool Battle::placeInReach(int structure, Cell from) const {
  if (structure < 0 || structure >= (int)structures.size() || !grid.valid(from)) return false;
  const PlaceDef& p = placeOf(structure);
  if (p.kind == PlaceKind::None) return false;
  const int d = placeDistance(structure, from);
  return (d == 0 && p.fromOn) || (d == 1 && p.fromNext);
}

bool Battle::visitAllowed(const Unit& u, int structure) const {
  const StructureState& st = structures[(size_t)structure];
  if (!st.visitSides.empty() || !st.visitUnits.empty()) {
    if (std::find(st.visitSides.begin(), st.visitSides.end(), u.side) != st.visitSides.end()) return true;
    return !u.storyId.empty() && std::find(st.visitUnits.begin(), st.visitUnits.end(), u.storyId) != st.visitUnits.end();
  }
  if (st.owner >= 0) return sides[(size_t)st.owner].team == teamOf(u);
  return true;
}

bool Battle::destroyAllowed(const Unit& u, int structure) const {
  const StructureState& st = structures[(size_t)structure];
  // Only the enemies of a place's owner destroy it (a neutral place cannot be destroyed, whoever may visit it).
  if (placeOf(structure).destroyedTo < 0 || st.owner < 0) return false;
  return sides[(size_t)st.owner].team != teamOf(u);
}

void Battle::placeKeySlots(int unit, int structure, std::vector<int>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || structure < 0 || structure >= (int)structures.size()) return;
  const Unit& u = units[(size_t)unit];
  const StructureState& st = structures[(size_t)structure];
  const PlaceDef& p = placeOf(structure);
  if ((!p.isDoor() && !p.isChest()) || !p.byKey) return;
  for (size_t i = 0; i < u.items.size(); ++i)
    if (kindAcceptsKey(*db, defOf(st), st.type, u.items[i].item)) out.push_back((int)i);
}

// A chest's items need room: the unit's inventory, or the side's storage. (A key that breaks on this use frees a slot, but
// that is not counted: drop something first.)
bool Battle::roomForLoot(const Unit& u, int structure) const {
  if (db->rules.items.storage != ItemStorage::Off) return true;
  int items = 0;
  for (const PlaceLoot& l : structures[(size_t)structure].loot) items += l.item >= 0;
  return (int)u.items.size() + items <= db->rules.items.inventorySize;
}

namespace {
const char* kNoRoom = "the unit cannot carry more: drop something first";
}

void Battle::placeOptions(int unit, Cell from, std::vector<PlaceOption>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive) return;
  for (size_t s = 0; s < structures.size(); ++s) {
    const int si = (int)s;
    if (!placeInReach(si, from) || !structureVisibleTo(teamOf(u), si)) continue;
    const StructureState& st = structures[s];
    const PlaceDef& p = placeOf(si);
    if (p.isDoor() || p.isChest()) {
      // A wall that only attacks open (by: ["break"]) has no Open entry while it is locked: a greyed entry that can never turn on
      // would only be noise next to "Break wall" (docs/SRPG.md section 10, "Walls that break").
      if (p.isDoor() && st.locked && p.byBreak && !p.byKey && !p.byTrait) continue;
      PlaceOption o;
      o.structure = si;
      o.verb = PlaceOption::Verb::Open;
      o.locked = st.locked;
      placeKeySlots(unit, si, o.keySlots);
      o.ok = true;
      if (st.locked && o.keySlots.empty()) {
        o.ok = false;
        o.why = p.byKey ? "it is locked and needs a key" : "it is locked";
        o.noTarget = true;
      } else if (!st.locked) {
        o.keySlots.clear();  // not locked: nothing to use
      }
      if (o.ok && p.isChest() && !roomForLoot(u, si)) {
        o.ok = false;
        o.why = kNoRoom;
      }
      out.push_back(o);
    } else if (p.isVisit() && (!st.used || p.repeat)) {
      PlaceOption o;
      o.structure = si;
      o.ok = true;
      if (visitAllowed(u, si)) {
        o.verb = PlaceOption::Verb::Visit;
        out.push_back(o);
      } else if (destroyAllowed(u, si)) {
        o.verb = PlaceOption::Verb::Destroy;
        out.push_back(o);
      }
    }
  }
}

// ---- breaking a door down ----

// What one strike of `weapon` does to the door and how many strikes there are: the weapon's damage formula with the door as the
// foe (`b`): its numbers are the kind's "stats", its hit points the door's. A door never dodges. The attacker's stats are its own
// plus accessories and research (no skill bonuses: the door is not a unit in the battle).
bool Battle::placeStrike(const Unit& u, int structure, int weapon, Cell from, int& damage, int& strikes) const {
  const StructureState& st = structures[(size_t)structure];
  const PlaceDef& p = placeOf(structure);
  if (!formulas || db->unitTypes.empty()) return false;
  Unit door;
  door.id = -1;
  door.type = 0;
  door.side = u.side;
  door.alive = true;
  door.stats = p.stats;
  door.stats.resize(db->stats.size(), 0);
  if (db->rules.statHp >= 0) door.stats[(size_t)db->rules.statHp] = std::max(1, p.hp);
  door.hp = std::max(0, st.hp);
  std::vector<Cell> cells;
  structureCells(structure, cells);
  Cell near = cells.empty() ? kNoCell : cells[0];
  for (Cell c : cells)
    if (grid.distance(from, c) < grid.distance(from, near)) near = c;
  door.cell = near;

  FormulaCtx ctx;
  ctx.sides = &sides;
  ctx.a = &u;
  ctx.b = &door;
  ctx.w = &db->weapons[(size_t)weapon];
  ctx.vs = 1;
  Props scratchA, scratchB;
  ctx.ta = &numbersAt(from, scratchA);
  if (near != kNoCell) ctx.tb = &numbersAt(near, scratchB);
  ctx.dist = near == kNoCell ? 0 : grid.distance(from, near);
  damage = std::max(0, std::min(wholeNumber(formulas->evalDamage(weapon, ctx)), 1000000));
  strikes = formulas->has(Formulas::Strikes) ? std::max(1, std::min(8, wholeNumber(formulas->eval(Formulas::Strikes, ctx, 1)))) : 1;
  return true;
}

bool Battle::doorBreakable(int structure) const {
  if (structure < 0 || structure >= (int)structures.size()) return false;
  const PlaceDef& p = placeOf(structure);
  return p.isDoor() && p.byBreak && p.hp > 0 && structures[(size_t)structure].hp > 0;
}

void Battle::placeAttackOptions(int unit, Cell from, std::vector<PlaceAttack>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive) return;
  const bool moved = u.moved || from != u.cell;
  std::vector<int> weapons;
  weaponsOf(u, weapons);
  for (size_t s = 0; s < structures.size(); ++s) {
    if (!doorBreakable((int)s) || !structureVisibleTo(teamOf(u), (int)s)) continue;
    const int dist = placeDistance((int)s, from);
    for (int w : weapons) {
      if (moved && !db->weapons[(size_t)w].afterMove) continue;
      if (!weaponInRange(w, dist)) continue;
      int damage = 0, strikes = 1;
      if (!placeStrike(u, (int)s, w, from, damage, strikes) || damage <= 0) continue;
      PlaceAttack a;
      a.structure = (int)s;
      a.weapon = w;
      out.push_back(a);
    }
  }
}

bool Battle::placeForecast(int unit, int structure, int weapon, Cell from, PlaceForecast& out) const {
  out = PlaceForecast();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return false;
  std::vector<PlaceAttack> options;
  placeAttackOptions(unit, from, options);
  bool legal = false;
  for (const PlaceAttack& a : options) legal = legal || (a.structure == structure && a.weapon == weapon);
  if (!legal) return false;
  const Unit& u = units[(size_t)unit];
  out.structure = structure;
  out.weapon = weapon;
  out.hpBefore = structures[(size_t)structure].hp;
  if (!placeStrike(u, structure, weapon, from, out.damage, out.strikes)) return false;
  out.total = std::min(out.hpBefore, out.damage * out.strikes);
  out.hpAfter = out.hpBefore - out.total;
  out.breaks = out.hpAfter <= 0;
  return true;
}

}  // namespace ote
