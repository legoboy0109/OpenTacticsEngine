// Summoned units (docs/SRPG.md "Summoning"): creating them, their limit, lifetime and vanishing, and the experience
// rule. The defeat-rule and filter exceptions are one-liners in turn.cpp, objectives.cpp and story.cpp, using
// isSummoned / summonIgnored (srpg_api.h, srpg_internal.h).
#include "core/battle/battle.h"

namespace ote {

bool Battle::summonUnit(int summoner, int ability, Cell cell, std::vector<Event>& events) {
  const AbilityDef& def = db->abilities[(size_t)ability];
  const SummonDef& sd = def.summon;
  const UnitTypeDef& type = db->unitTypes[(size_t)sd.type];
  if (!grid.valid(cell) || occupant[(size_t)cell] >= 0 || enterCost(cell, type.moveType) <= 0) return false;

  // "replace": at the limit, the oldest summons of this ability by this summoner leave first (lowest unit id = made first).
  if (sd.replace)
    while (summonCount(summoner, ability) >= sd.limit) {
      int oldest = -1;
      for (const Unit& o : units)
        if (o.summoner == summoner && o.summonAbility == ability && exists(o)) { oldest = o.id; break; }
      if (oldest < 0) break;
      removeSummon(oldest, events);
    }

  // An ordinary unit of the type's own base stats and items, on the summoner's side: no story id, no character.
  MapUnitDef md;
  md.type = sd.type;
  md.side = units[(size_t)summoner].side;
  md.level = sd.level > 0 ? sd.level : units[(size_t)summoner].level;
  md.stats = type.stats;
  md.difficultyShift = false;  // a summon is built from the summoner's level, never shifted (docs/DIFFICULTY.md)
  Unit s = makeUnit(md);
  if (!sd.items.empty()) {  // "items": the summon carries one candidate whose "minLevel" the summoner's level reaches (none fits: the type's own items)
    std::vector<int> fits;
    for (size_t i = 0; i < sd.items.size(); ++i)
      if (units[(size_t)summoner].level >= sd.items[i].minLevel &&
          (sd.items[i].maxLevel == 0 || units[(size_t)summoner].level <= sd.items[i].maxLevel))
        fits.push_back((int)i);
    if (!fits.empty()) {
      int pick = fits[0];
      if (sd.pickRandom) {
        if (fits.size() > 1) {  // a roll only when there is a choice; one roll over the weights (all 1: the plain pick among the n)
          int total = 0;
          for (int i : fits) total += sd.items[(size_t)i].weight;
          int roll = rng.range(total);
          for (int i : fits) {
            pick = i;
            if (roll < sd.items[(size_t)i].weight) break;
            roll -= sd.items[(size_t)i].weight;
          }
        }
      } else {
        for (int i : fits)
          if (sd.items[(size_t)i].minLevel > sd.items[(size_t)pick].minLevel) pick = i;  // the first of equals wins
      }
      s.items.clear();
      if (!sd.items[(size_t)pick].nothing) {  // a "nothing" candidate leaves the summon without an item
        ItemStack stack;
        stack.item = sd.items[(size_t)pick].item;
        stack.uses = db->items[(size_t)stack.item].uses;
        s.items.push_back(stack);
      }
    }
  }
  for (ItemStack& item : s.items) item.drop = false;  // its items never drop
  s.summoner = summoner;
  s.summonAbility = ability;
  s.summonLeft = sd.lifetime > 0 ? sd.lifetime : -1;
  s.moved = s.done = !sd.canAct;  // like a built unit that cannot act yet

  markSight();
  s.id = (int)units.size();
  s.cell = cell;
  s.facing = defaultFacing(cell, teamOf(s));  // rules.facing: it looks at the nearest foe (-1 without facing)
  occupant[(size_t)cell] = s.id;
  sides[(size_t)s.side].everHadUnits = true;
  units.push_back(s);  // `units` may move: nothing above is used again
  Event e;
  e.type = EventType::UnitAdded;
  e.a = s.id;
  e.b = cell;
  events.push_back(e);
  return true;
}

// A summon leaves for good: not a defeat (no drops, no XP, no "defeated" events). The unit's own summons follow it.
void Battle::removeSummon(int unit, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  if (u.carriedBy >= 0) {  // taken out of the unit that carries it
    storyRemoveCarried(unit, events);
    return;
  }
  Event e;
  e.type = EventType::UnitRemoved;
  e.a = unit;
  e.b = u.cell;
  events.push_back(e);
  markSight();
  releaseCapture(unit, kNoCell);
  occupant[(size_t)u.cell] = -1;
  u.alive = false;
  u.hp = 0;
  u.cell = kNoCell;
  srpgUnitGone(unit, -1, events);  // `u` is not used after this
}

// The summoner was defeated, removed or lost: its summons of abilities with "vanish" leave at once.
void Battle::summonsUnitGone(int unit, std::vector<Event>& events) {
  if (db->abilities.empty()) return;
  for (size_t i = 0; i < units.size(); ++i) {
    const Unit& s = units[i];
    if (s.summoner != unit || !exists(s)) continue;
    if (s.summonAbility < 0 || !db->abilities[(size_t)s.summonAbility].summon.vanish) continue;
    removeSummon((int)i, events);
  }
}

// Experience for fighting a summon that gives none: the award names no foe, but the fight that earned it is the last
// Attack event of this action (an award only follows the strikes of one fight).
bool Battle::summonGivesNoXp(int unit, const std::vector<Event>& events) const {
  if (db->abilities.empty()) return false;
  for (size_t i = events.size(); i-- > 0;) {
    const Event& e = events[i];
    if (e.type != EventType::Attack) continue;
    const int foe = e.a == unit ? e.b : e.b == unit ? e.a : -1;
    if (foe < 0 || foe >= (int)units.size()) return false;
    const Unit& f = units[(size_t)foe];
    return f.summoner >= 0 && f.summonAbility >= 0 && !db->abilities[(size_t)f.summonAbility].summon.givesXp;
  }
  return false;
}

}  // namespace ote
