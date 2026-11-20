// Supports in a battle (docs/SRPG.md "Supports"): the state, where points come from, levels, and the combat bonus.
// The Support action and the story's part are in supports_action.cpp; the rules that need no battle (which level is
// ready, ...) are in support_rules.cpp and shared with the camp between chapters.
//
// Nothing here runs in a game without support pairs: every entry point returns at once when db->supports.pairs is
// empty, so such a game keeps its events, random numbers and saves.
#include <algorithm>

#include "core/battle/combat_mods.h"
#include "core/battle/support_rules.h"

namespace ote {

namespace {

Event makeEvent(EventType t, int a, int b, int c, int d, int flags = 0) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  e.flags = flags;
  return e;
}

bool pairHas(const SupportPairDef& p, int character) { return p.a == character || p.b == character; }

}  // namespace

// ---- state ----

void Battle::supportsInit() { supports = startingSupports(*db); }

void Battle::setStartingSupports(const SupportState& carried) {
  // Only a state that fits the game's pairs is taken (a game without pairs: only the empty one).
  if (supportStateFits(*db, carried)) supports = carried;
}

// ---- who is near whom ----

int Battle::supportUnit(int character) const {
  if (character < 0) return -1;
  for (const Unit& u : units)
    if (u.character == character && exists(u)) return u.id;
  return -1;
}

bool Battle::supportNear(const Unit& u, Cell uCell, const Unit& v, int range) const {
  if (u.id == v.id || teamOf(u) != teamOf(v)) return false;
  // The two units of a pair-up pair share a square: they are near each other whatever the range.
  if (db->rules.rescue.mode == RescueMode::PairUp && (u.carriedBy == v.id || v.carriedBy == u.id)) return true;
  if (!u.alive || !v.alive) return false;  // off the board (carried, defeated): not near anybody
  return grid.distance(grid.valid(uCell) ? uCell : u.cell, v.cell) <= range;
}

// ---- points and levels ----

void Battle::supportReached(int pair, int level, std::vector<Event>& events) {
  const SupportPairDef& p = db->supports.pairs[(size_t)pair];
  const int a = supportUnit(p.a), b = supportUnit(p.b);
  events.push_back(makeEvent(EventType::SupportReached, a >= 0 && units[(size_t)a].alive ? a : -1,
                             b >= 0 && units[(size_t)b].alive ? b : -1, pair, level));
  StoryOccurrence o;
  o.kind = Trigger::Kind::Support;
  o.pair = pair;
  o.level = level;
  fireStory(o, events);  // may run story events: nothing below keeps a reference into `units`
}

void Battle::supportSettleLevels(std::vector<Event>& events) {
  std::vector<std::pair<int, int>> gained;
  supportSettle(*db, supports, &gained);
  for (const std::pair<int, int>& g : gained) supportReached(g.first, g.second, events);
}

void Battle::supportGain(int pair, int by, std::vector<Event>& events) {
  const bool talkBefore = supportTalkPending(*db, supports, pair);
  if (!supportAddPoints(*db, supports, pair, by)) return;  // nothing changed (at a limit): no event
  const SupportPairDef& p = db->supports.pairs[(size_t)pair];
  const int a = supportUnit(p.a), b = supportUnit(p.b);
  const bool ready = !talkBefore && supportTalkPending(*db, supports, pair);
  events.push_back(makeEvent(EventType::SupportPoints, a, b, pair, supports.points[(size_t)pair], ready ? kReady : 0));
  supportSettleLevels(events);
}

// A side ends its turn: each pair whose character `a` is on that side and whose two units stand side by side.
void Battle::supportsTurnEnd(std::vector<Event>& events) {
  const SupportRules& rules = db->supports.rules;
  if (db->supports.pairs.empty() || rules.endTurnAdjacent <= 0) return;
  for (size_t i = 0; i < db->supports.pairs.size(); ++i) {
    const SupportPairDef& p = db->supports.pairs[i];
    const int a = supportUnit(p.a), b = supportUnit(p.b);
    if (a < 0 || b < 0 || !ownsTurn(units[(size_t)a])) continue;
    if (supportNear(units[(size_t)a], kNoCell, units[(size_t)b], 1)) supportGain((int)i, rules.endTurnAdjacent, events);
  }
}

// After a combat: each fighter still on the board earns with every partner within range, once per pair.
void Battle::supportsAfterCombat(int attacker, int defender, std::vector<Event>& events) {
  const SupportRules& rules = db->supports.rules;
  if (db->supports.pairs.empty() || rules.fightTogether <= 0) return;
  std::vector<int> paid;
  for (int fighter : {attacker, defender}) {
    for (size_t i = 0; i < db->supports.pairs.size(); ++i) {
      const SupportPairDef& p = db->supports.pairs[i];
      // Re-read the unit each time: a story event started by an earlier gain may have changed the battle.
      const Unit& f = units[(size_t)fighter];
      if (!f.alive || f.character < 0 || !pairHas(p, f.character)) continue;
      if (std::find(paid.begin(), paid.end(), (int)i) != paid.end()) continue;
      const int other = supportUnit(p.a == f.character ? p.b : p.a);
      if (other < 0 || !supportNear(f, kNoCell, units[(size_t)other], rules.range)) continue;
      paid.push_back((int)i);
      supportGain((int)i, rules.fightTogether, events);
    }
  }
}

// `healer` restored HP of `target` (an item on an ally, a heal ability): the pair of the two earns.
void Battle::supportsHealed(int healer, int target, std::vector<Event>& events) {
  if (db->supports.pairs.empty() || db->supports.rules.heal <= 0) return;
  const Unit& h = units[(size_t)healer];
  const Unit& t = units[(size_t)target];
  if (h.character < 0 || t.character < 0 || !exists(h) || !exists(t) || teamOf(h) != teamOf(t)) return;
  const int pair = db->findSupportPair(h.character, t.character);
  if (pair >= 0) supportGain(pair, db->supports.rules.heal, events);
}

// ---- the combat bonus ----

// The bonus of the CURRENT level of each pair whose other character is near; several partners add up.
void Battle::supportBonusFor(const Unit& u, Cell cell, CombatMods& out) const {
  const SupportRules& rules = db->supports.rules;
  for (size_t i = 0; i < db->supports.pairs.size(); ++i) {
    const int level = supports.level[i];
    const SupportPairDef& p = db->supports.pairs[i];
    if (level == 0 || !pairHas(p, u.character)) continue;
    const int other = supportUnit(p.a == u.character ? p.b : p.a);
    if (other < 0 || !supportNear(u, cell, units[(size_t)other], rules.range)) continue;
    const SupportBonus& b = p.levels[(size_t)level - 1].bonus;
    out.hit += b.hit;
    out.avoid += b.avoid;
    out.crit += b.crit;
    out.critAvoid += b.critAvoid;
    out.damage += b.damage;
    out.damageTaken += b.damageTaken;
    for (size_t s = 0; s < b.stats.size(); ++s)
      if (b.stats[s] != 0) out.addStat((int)s, b.stats[s]);
  }
}

// Called for every forecast and before every strike: cheap, no rolls, does not read setup.counters.
void Battle::supportCombatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const {
  if (db->supports.pairs.empty() || supports.level.size() != db->supports.pairs.size()) return;
  const Unit& a = units[(size_t)setup.attacker];
  const Unit& d = units[(size_t)setup.defender];
  if (a.character >= 0) supportBonusFor(a, setup.from, attacker);
  if (d.character >= 0) supportBonusFor(d, kNoCell, defender);
}

}  // namespace ote
