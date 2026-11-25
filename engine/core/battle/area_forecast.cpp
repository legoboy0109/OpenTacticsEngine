// The forecast of an area attack and the attacker's options with area weapons (docs/SRPG.md "Area attacks and line fire").
// No randomness is used here.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/grid/area.h"
#include "core/script/formulas.h"

namespace ote {

bool Battle::areaForecast(int attacker, int weapon, Cell from, Cell target, AreaForecast& out) const {
  out = AreaForecast();
  if (!isAreaWeapon(weapon) || attacker < 0 || attacker >= (int)units.size() || !grid.valid(from) || !grid.valid(target)) return false;
  const Unit& a = units[(size_t)attacker];
  if (!a.alive) return false;
  // The same checks a single-target attack makes: the weapon is one the unit has from there, and the move rule.
  std::vector<int> owned;
  attackWeaponsAt(a, from, owned);
  if (std::find(owned.begin(), owned.end(), weapon) == owned.end()) return false;
  // (a strike ability: the ability's own "afterMove" counts, checked with its state; ability_strike.cpp)
  if (strikeWeapon_ < 0 && (a.moved || from != a.cell) && !db->weapons[(size_t)weapon].afterMove) return false;
  if (!weaponAims(weapon, from, target, attacker)) return false;

  out.cell = target;
  areaCover(weapon, from, target, out.cells);
  std::vector<int> ids;
  for (Cell c : out.cells) {
    const int id = occupant[(size_t)c];
    if (id >= 0 && areaHits(a, weapon, units[(size_t)id])) ids.push_back(id);
  }
  std::sort(ids.begin(), ids.end());  // unit-id order: the order they are struck in

  Formulas::BatchScope formulaBatch(formulas);  // as in forecastFight: nothing changes between the formula calls below
  const int team = teamOf(a);
  bool enemySeen = false;
  const int aimed = occupant[(size_t)target];
  for (int id : ids) {
    const Unit& u = units[(size_t)id];
    AreaVictim v;
    v.unit = id;
    v.enemy = isEnemy(a, u);
    v.seen = !v.enemy || unitVisibleTo(team, u);
    Forecast f;
    CombatSetup setup;
    // The unit on the aimed square may answer (when the normal rules let it); everyone else is just struck.
    if (!forecastFight(attacker, id, weapon, from, f, setup, false, id == aimed && mayFight(a, u) ? 2 : 1)) continue;
    v.hit = f.attack;
    if (id == aimed) {
      out.primary = (int)out.victims.size();
      out.fight = f;
    }
    enemySeen = enemySeen || (v.enemy && v.seen);
    out.victims.push_back(v);
  }
  return enemySeen;  // an attack that would hit no enemy the attacker can see is not offered
}

// Area weapons add one option per aimable square that would hit an enemy the attacker's team can see. Called by attackOptions.
void Battle::areaOptions(int unit, Cell from, int weapon, bool mapWeapon, std::vector<AttackOption>& out) const {
  const Unit& u = units[(size_t)unit];
  const int team = teamOf(u);
  std::vector<Cell> aim, covered;
  aimCells(weapon, from, unit, aim);
  for (Cell c : aim) {
    areaCover(weapon, from, c, covered);
    bool enemy = false;
    for (Cell k : covered) {
      const int id = occupant[(size_t)k];
      if (id < 0 || !areaHits(u, weapon, units[(size_t)id])) continue;
      if (isEnemy(u, units[(size_t)id]) && unitVisibleTo(team, units[(size_t)id])) {
        enemy = true;
        break;
      }
    }
    if (!enemy) continue;
    AttackOption opt;
    const int there = occupant[(size_t)c];
    // The unit on the aimed square is the target only when the area covers that square (a "ring" leaves it out).
    const bool coversAim = std::binary_search(covered.begin(), covered.end(), c);
    opt.target = there >= 0 && coversAim && areaHits(u, weapon, units[(size_t)there]) ? there : -1;
    opt.weapon = weapon;
    opt.cell = c;
    opt.area = true;
    opt.mapWeapon = mapWeapon;
    out.push_back(opt);
  }
}

}  // namespace ote
