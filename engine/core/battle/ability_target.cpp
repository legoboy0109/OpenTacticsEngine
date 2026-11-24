// What an ability needs before it can be used (docs/SRPG.md "Legality"): the unit's own state, and a target's
// legality. Both are asked by validation and by the menu queries, so they judge what the unit's team can see and
// never the hidden truth (a square holding an unseen enemy counts as free; carrying the ability out then finds it
// blocked, see ability_apply.cpp).
#include "core/battle/battle.h"

namespace ote {

namespace {

bool refuse(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

}  // namespace

bool Battle::squareFreeFor(Cell c, int moveType, int viewTeam, int ignore) const {
  if (!grid.valid(c) || enterCost(c, moveType) <= 0) return false;  // heights do not matter
  const int occ = occupant[(size_t)c];
  if (occ < 0 || occ == ignore) return true;
  return viewTeam >= 0 && !unitVisibleTo(viewTeam, occ);
}

int Battle::summonCount(int summoner, int ability) const {
  int n = 0;
  for (const Unit& s : units)
    if (s.summoner == summoner && s.summonAbility == ability && exists(s)) ++n;
  return n;
}

bool Battle::abilityStateOk(const Unit& u, int ability, Cell from, std::string* why) const {
  const AbilityDef& d = db->abilities[(size_t)ability];
  if (!d.afterMove && (u.moved || from != u.cell)) return refuse(why, "can only be used before moving");
  const AbilityUse* use = abilityUseOf(u, ability);
  if (use) {
    if (d.uses > 0 && use->used >= d.uses) return refuse(why, "no uses left this battle");
    if (use->wait > 0) return refuse(why, "ready in " + std::to_string(use->wait) + (use->wait == 1 ? " turn" : " turns"));
    if (d.free && use->turn) return refuse(why, "already used this turn");
  }
  if (d.effect == AbilityEffect::Steal && !stealRoomOk(u, why)) return false;
  if (!abilityPriceOk(u, ability, why)) return false;  // "spend": "needs 6 MP" (docs/SRPG.md "Unit resources")
  if (d.effect == AbilityEffect::Summon) {
    const int have = summonCount(u.id, ability);
    if (have >= d.summon.limit && !d.summon.replace) {  // with "replace" the oldest one makes room (summons.cpp)
      const std::string& name = db->unitTypes[(size_t)d.summon.type].name;
      return refuse(why, "already has " + std::to_string(have) + " summoned " + name + (have == 1 ? "" : "s"));
    }
  }
  // Conditions about the user. (Those about the target unit are judged with the target.)
  SkillCtx ctx;
  ctx.unit = u.id;
  ctx.cell = from;
  ctx.weapon = restingWeapon(u);
  for (const SkillCond& cond : d.conds)
    if (!cond.foe && !skillCondHolds(cond, ctx)) return refuse(why, "cannot be used now");
  return true;
}

bool Battle::abilityTargetOk(const Unit& u, int ability, Cell from, int target, Cell cell, std::string* why) const {
  const AbilityDef& d = db->abilities[(size_t)ability];
  const int team = teamOf(u);

  if (d.effect == AbilityEffect::Unlock) return unlockTargetOk(u, ability, from, cell, why);  // doors and chests (docs/SRPG.md section 10)
  if (d.effect == AbilityEffect::Strike) {  // reach and targets of its weapon (ability_strike.cpp)
    if (!strikeTargetOk(u, ability, from, target, cell, why)) return false;
    if (d.target != AbilityTarget::Enemy) return true;  // an area: "who": "foe" conditions are judged for single targets only
    SkillCtx sc;
    sc.unit = u.id;
    sc.cell = from;
    sc.weapon = restingWeapon(u);
    sc.foe = target;
    for (const SkillCond& cond : d.conds)
      if (cond.foe && !skillCondHolds(cond, sc)) return refuse(why, "cannot be used now");
    return true;
  }

  if (d.area.on()) return areaAbilityTargetOk(u, ability, from, cell, why);  // a square whose area holds someone it acts on (ability_area.cpp)
  if (d.target == AbilityTarget::Cell) {
    if (!grid.valid(cell)) return refuse(why, "that square is not on the map");
    const int dist = grid.distance(from, cell);
    if (dist < d.minRange || dist > d.maxRange) return refuse(why, "that square is out of range");
    const UnitTypeDef& type = db->unitTypes[(size_t)d.summon.type];
    if (enterCost(cell, type.moveType) <= 0) return refuse(why, "a " + type.name + " cannot stand there");
    if (!squareFreeFor(cell, type.moveType, team, u.id)) return refuse(why, "that square is occupied");
    return true;
  }

  const Unit* t = &u;
  if (d.target != AbilityTarget::Self) {
    if (target < 0 || target >= (int)units.size() || !units[(size_t)target].alive) return refuse(why, "that is not a unit on the board");
    t = &units[(size_t)target];
    // A "range" from 0 lets it pick the user itself (docs/SRPG.md "An ability on its user"); the user stands on `from`, wherever it is now.
    const bool self = t->id == u.id;
    if (self && d.minRange > 0) return refuse(why, "a unit cannot use that on itself");
    if (!unitVisibleTo(team, *t)) return refuse(why, "you cannot see that unit");
    const bool ally = teamOf(*t) == team;
    if (d.target == AbilityTarget::Ally && !ally) return refuse(why, "that ability works on an ally");
    if (d.target == AbilityTarget::Enemy && ally) return refuse(why, "that ability works on an enemy");
    const int dist = self ? 0 : grid.distance(from, t->cell);
    if (dist < d.minRange || dist > d.maxRange) return refuse(why, "that unit is out of range");
  } else if (target != -1 && target != u.id) {
    return refuse(why, "that ability can only be used on the unit itself");
  }

  // What the effect needs. The user's square is where it would stand (`from`); it counts as free for itself.
  const int dir = t->id == u.id ? -1 : grid.directionTo(from, t->cell);
  const Cell back = dir < 0 ? kNoCell : grid.neighbor(from, grid.opposite(dir));  // the user's opposite side
  switch (d.effect) {
    case AbilityEffect::Push:
      if (!squareFreeFor(grid.neighbor(t->cell, dir), typeOf(*t).moveType, team, u.id))
        return refuse(why, "there is no room to push it");
      break;
    case AbilityEffect::Pull:
      if (!squareFreeFor(back, typeOf(u).moveType, team, u.id)) return refuse(why, "there is no room behind the user");
      if (enterCost(from, typeOf(*t).moveType) <= 0) return refuse(why, "that unit could not stand on the user's square");
      break;
    case AbilityEffect::Swap:
      if (enterCost(t->cell, typeOf(u).moveType) <= 0 || enterCost(from, typeOf(*t).moveType) <= 0)
        return refuse(why, "they cannot take each other's squares");
      break;
    case AbilityEffect::MoveBehind:
      if (!squareFreeFor(back, typeOf(*t).moveType, team, u.id)) return refuse(why, "there is no room behind the user");
      break;
    case AbilityEffect::MovePast:  // the USER goes to the far side of the target
      if (!squareFreeFor(grid.neighbor(t->cell, dir), typeOf(u).moveType, team, u.id)) return refuse(why, "there is no room beyond that unit");
      break;
    case AbilityEffect::Steal:
    case AbilityEffect::Refresh:
    case AbilityEffect::Cure:
    case AbilityEffect::Heal:  // hurt (or, for a heal that also cures, something to cure)
    case AbilityEffect::None:  // only "inflict": refused where none of its conditions could take hold (immune, or already there for as long)
      if (!abilityActsOn(u, d, from, *t, why)) return false;  // ability_area.cpp
      break;
    case AbilityEffect::Summon: break;
    case AbilityEffect::Unlock: break;  // handled above (a square, not a unit)
    case AbilityEffect::Strike: break;  // handled above
  }

  // Conditions about the target unit ("who": "foe").
  SkillCtx ctx;
  ctx.unit = u.id;
  ctx.cell = from;
  ctx.weapon = restingWeapon(u);
  ctx.foe = t->id;
  for (const SkillCond& cond : d.conds)
    if (cond.foe && !skillCondHolds(cond, ctx)) return refuse(why, "cannot be used now");
  return true;
}

}  // namespace ote
