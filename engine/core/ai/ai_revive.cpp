// The computer and revives (docs/AI.md "Reviving downed units"; docs/SRPG.md "Downed units and revive").
//
// From each square the unit may stand on, every single-target revive ability and every revive item it carries is tried on each downed unit it
// could stand up from there. A use is worth reviveWorth: (kReviveBase + the share of its maximum it stands up with) times the unit's value
// (ai_score.cpp unitValue), a leader whose defeat ends the game counting double; a foe is never revived (its worth would be negative). So a
// revive is worth between half and one and a half of the unit's value: about what finishing a foe is worth (an attack's kill bonus is 0.6 of the
// victim's value plus its damage), and much more than a plain heal. The best use beats the order already chosen by its worth, minus the danger of
// the square, plus the base an attack gets, as for the other abilities. A priced ability ("spend") is worth 10 % less, so a free revive item or a
// cheaper ability wins a tie. Area revives are scored with the other area abilities (ai_ability_area.cpp), revive staves with the staves
// (ai_staves.cpp); both use reviveWorth. Downed foes are never targets of anything else: every other rule asks for a living unit.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kAttackBase = 200;       // as an attack's (ai_order.cpp)
constexpr double kMoveCostWeight = 0.5;   // as an attack's
constexpr double kPricedFactor = 0.9;     // as a strike's (ai_strike.cpp)

void take(Order& best, double sc, const Unit& u, Cell s, ActionType kind, int ability, int slot, int target) {
  best = Order();
  best.score = sc;
  best.kind = kind;
  best.moveTo = s == u.cell ? kNoCell : s;
  best.ability = ability;
  best.slot = slot;
  best.target = target;
}

}  // namespace

double reviveWorth(const Battle& b, const Unit& user, const Unit& t, int hp) {
  constexpr double kReviveBase = 0.5;  // a unit back in the fight is worth half its value before the HP it gets
  constexpr double kLeader = 2.0;      // as a staff's heal
  double w = (kReviveBase + hp / (double)std::max(1, b.maxHp(t))) * unitValue(b, t);
  if (t.leader && b.db->rules.defeatLeaderDead) w *= kLeader;
  return b.isEnemy(user, t) ? -w : w;
}

void considerRevives(const Battle& b, const Unit& u, Cell s, int travel, Order& best) {
  if (!b.downedGame()) return;
  bool anyDowned = false;  // cheap way out: this runs for every square of every unit
  for (const Unit& t : b.units) anyDowned = anyDowned || (b.revivable(t) && !b.isEnemy(u, t));
  if (!anyDowned) return;
  const double extra = kAttackBase - kMoveCostWeight * travel - exposureCost(b, u, s);

  // Abilities (an area revive is ai_ability_area.cpp's).
  std::vector<AbilityOption> menu;
  b.abilityOptions(u.id, s, menu);
  std::vector<int> targets;
  std::vector<Cell> cells;
  for (const AbilityOption& o : menu) {
    if (!o.ok || b.abilityCharges(o.ability)) continue;
    const AbilityDef& d = b.db->abilities[(size_t)o.ability];
    if (d.effect != AbilityEffect::Revive || d.area.on() || (d.free && s != u.cell)) continue;
    b.abilityTargets(u.id, o.ability, s, targets, cells);
    for (int id : targets) {  // unit-id order: the lowest id wins a tie (strict comparison)
      const Unit& t = b.units[(size_t)id];
      double w = reviveWorth(b, u, t, b.abilityReviveHp(d, u.id, t));
      if (!d.spend.empty()) w *= kPricedFactor;
      if (w <= 0 || w + extra <= best.score) continue;
      take(best, w + extra, u, s, ActionType::Ability, o.ability, -1, id);
      if (d.free) best.moveTo = kNoCell;
    }
  }

  // Revive items: a downed unit of its team next to the square (useTargets asks from where the unit stands now, so the square is judged here).
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& it = b.itemOf(u.items[i]);
    if (it.kind != ItemKind::Usable || !it.revive.on() || b.conditionForbidsKind(u, ItemKind::Usable) >= 0) continue;
    for (const Unit& t : b.units) {
      if (t.id == u.id || !b.revivable(t) || b.teamOf(t) != b.teamOf(u) || b.grid.distance(t.cell, s) != 1) continue;
      const double w = reviveWorth(b, u, t, b.reviveHp(t, it.revive, u.id));
      if (w <= 0 || w + extra <= best.score) continue;
      take(best, w + extra, u, s, ActionType::UseItem, -1, (int)i, t.id);
    }
  }
}

}  // namespace ai
}  // namespace ote
