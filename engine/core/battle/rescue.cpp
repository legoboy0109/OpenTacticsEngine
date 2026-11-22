// Rescue (carry) and the parts shared with pair up (docs/SRPG.md "Rescue and pair up"). Pair up's own action is
// in pair_up.cpp.
//
// The model: a unit that is carried (or stands behind a partner) is OFF the board: alive == false, no square, but it
// keeps its HP and `carriedBy` names its carrier, whose `carrying` names it. Everything that loops over units and
// skips !alive therefore ignores it for free (movement, sight, targeting, XP, turn start, drawing); only the few
// places that ask "does it still exist" use Battle::exists (see the audit note in docs/SRPG.md, "R1 as built").
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

namespace {

bool refuse(std::string* why, const char* reason) {
  if (why) *why = reason;
  return false;
}

// Where the unit `unit` last stood, read off the event that took it from its square (killUnit and removeUnit
// push UnitDied / UnitRemoved with the square in `b` just before they call srpgUnitGone).
Cell lastSquare(const std::vector<Event>& events, int unit) {
  for (size_t i = events.size(); i-- > 0;) {
    const Event& e = events[i];
    if ((e.type == EventType::UnitDied || e.type == EventType::UnitRemoved) && e.a == unit) return e.b;
  }
  return kNoCell;
}

}  // namespace

// ---- the canCarry formula and the shared pieces ----

bool Battle::carryAllowed(const Unit& stays, const Unit& leaves) const {
  const int expr = db->rules.rescue.canCarry;
  if (expr < 0) return true;
  FormulaCtx ctx;
  ctx.sides = &sides;
  ctx.battle = this;
  ctx.a = &stays;
  ctx.b = &leaves;
  Props scratchA, scratchB;
  // A unit that is already carried (Give / Take) has no square: its table gets no `tb`.
  if (grid.valid(stays.cell)) { ctx.ta = &numbersAt(stays.cell, scratchA); ctx.ha = heightAt(stays.cell); }
  if (grid.valid(leaves.cell)) { ctx.tb = &numbersAt(leaves.cell, scratchB); ctx.hb = heightAt(leaves.cell); }
  if (grid.valid(stays.cell) && grid.valid(leaves.cell)) ctx.dist = grid.distance(stays.cell, leaves.cell);
  return formulas->evalExpr(expr, ctx, 0) != 0;  // a failing formula counts as false (and is logged once)
}

void Battle::linkCarried(int stays, int leaves) {
  Unit& l = units[(size_t)leaves];
  releaseCapture(leaves, kNoCell);  // a capture it was making is abandoned
  if (l.cell != kNoCell) occupant[(size_t)l.cell] = -1;
  l.cell = kNoCell;
  l.alive = false;  // off the board, hp kept
  l.carriedBy = stays;
  units[(size_t)stays].carrying = leaves;
}

void Battle::unlinkCarried(int carried, Cell cell) {
  Unit& c = units[(size_t)carried];
  if (c.carriedBy >= 0) units[(size_t)c.carriedBy].carrying = -1;
  c.carriedBy = -1;
  c.alive = true;
  c.cell = cell;
  occupant[(size_t)cell] = carried;
  c.moved = true;  // put down: its turn is finished
  c.done = true;
}

// ---- queries ----

void Battle::rescueTargets(int unit, Cell from, std::vector<int>& out) const {
  out.clear();
  if (db->rules.rescue.mode == RescueMode::Off || unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive) return;
  Cell around[Grid::kMaxNeighbors];
  const int n = grid.neighbors(from, around);
  for (int i = 0; i < n; ++i) {
    const int t = occupant[(size_t)around[i]];
    if (t >= 0 && t != unit && !rescueWhy(u, from, t)) out.push_back(t);
  }
}

void Battle::dropCells(int unit, Cell from, std::vector<Cell>& out) const {
  out.clear();
  if (db->rules.rescue.mode == RescueMode::Off || unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive || u.carrying < 0) return;
  Cell around[Grid::kMaxNeighbors];
  const int n = grid.neighbors(from, around);
  for (int i = 0; i < n; ++i)
    if (!dropWhy(u, from, around[i])) out.push_back(around[i]);
}

void Battle::giveTargets(int unit, Cell from, std::vector<int>& out) const {
  out.clear();
  if (db->rules.rescue.mode != RescueMode::Carry || unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive) return;
  Cell around[Grid::kMaxNeighbors];
  const int n = grid.neighbors(from, around);
  for (int i = 0; i < n; ++i) {
    const int t = occupant[(size_t)around[i]];
    if (t >= 0 && t != unit && !handWhy(u, from, t, true)) out.push_back(t);
  }
}

void Battle::takeTargets(int unit, Cell from, std::vector<int>& out) const {
  out.clear();
  if (db->rules.rescue.mode != RescueMode::Carry || unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive) return;
  Cell around[Grid::kMaxNeighbors];
  const int n = grid.neighbors(from, around);
  for (int i = 0; i < n; ++i) {
    const int t = occupant[(size_t)around[i]];
    if (t >= 0 && t != unit && !handWhy(u, from, t, false)) out.push_back(t);
  }
}

// ---- reasons (null = allowed) ----

const char* Battle::rescueWhy(const Unit& u, Cell from, int target) const {
  const bool pair = db->rules.rescue.mode == RescueMode::PairUp;
  if (target < 0 || target >= (int)units.size() || !units[(size_t)target].alive) return "that unit is not on the board";
  const Unit& t = units[(size_t)target];
  if (t.id == u.id) return pair ? "a unit cannot pair up with itself" : "a unit cannot rescue itself";
  if (t.side != u.side) return pair ? "only a unit of the same side can be paired with" : "only a unit of the same side can be rescued";
  if (grid.distance(from, t.cell) != 1) return "that unit is not next to it";
  if (u.carrying >= 0 || u.carriedBy >= 0) return pair ? "that unit is already in a pair" : "that unit already carries somebody";
  if (t.carrying >= 0 || t.carriedBy >= 0) return pair ? "that unit is already in a pair" : "that unit already carries somebody";
  // Rescue: the unit that stays on the board is the actor; pair up: the partner that stays in front is the target.
  if (!(pair ? carryAllowed(t, u) : carryAllowed(u, t))) return pair ? "that unit cannot take it behind" : "it cannot carry that unit";
  return nullptr;
}

const char* Battle::handWhy(const Unit& u, Cell from, int target, bool give) const {
  if (target < 0 || target >= (int)units.size() || !units[(size_t)target].alive) return "that unit is not on the board";
  const Unit& t = units[(size_t)target];
  if (t.id == u.id) return "that is the unit itself";
  if (t.side != u.side) return "only a unit of the same side can be handed over to or taken from";
  if (grid.distance(from, t.cell) != 1) return "that unit is not next to it";
  // GiveUnit: u carries, t must carry nobody. TakeUnit: t carries, u must carry nobody.
  const Unit& giver = give ? u : t;
  const Unit& taker = give ? t : u;
  if (giver.carrying < 0) return "that unit is not carrying anybody";
  if (taker.carrying >= 0) return "that unit already carries somebody";
  if (!carryAllowed(taker, units[(size_t)giver.carrying])) return give ? "that unit cannot carry it" : "it cannot carry that unit";
  return nullptr;
}

const char* Battle::dropWhy(const Unit& u, Cell from, Cell cell) const {
  const bool pair = db->rules.rescue.mode == RescueMode::PairUp;
  if (u.carrying < 0 || u.carrying >= (int)units.size()) return pair ? "that unit has nobody behind it" : "that unit is not carrying anybody";
  if (!grid.valid(cell) || grid.distance(from, cell) != 1) return "that square is not next to the unit";
  const Unit& c = units[(size_t)u.carrying];
  if (enterCost(cell, typeOf(c).moveType) <= 0) return "the unit could not stand on that square";
  // Fog: the team judges by what it sees. A square that looks free but holds an unseen enemy is let through; the
  // drop then fails when it is carried out (UnitDropped with kBlocked).
  const int occ = occupant[(size_t)cell];
  if (occ >= 0 && unitVisibleTo(teamOf(u), occ)) return "that square is occupied";
  return nullptr;
}

const char* Battle::switchWhy(const Unit& u) const {
  if (u.carrying < 0 || u.carrying >= (int)units.size()) return "that unit has nobody behind it";
  if (enterCost(u.cell, typeOf(units[(size_t)u.carrying]).moveType) <= 0) return "the unit behind could not stand on this square";
  return nullptr;
}

// ---- validate / apply ----

bool Battle::validateRescue(const Action& action, std::string* why) const {
  const RescueMode mode = db->rules.rescue.mode;
  if (mode == RescueMode::Off) return refuse(why, "this game has no rescue or pair up");
  const ActionType type = action.type;
  const bool carryOnly = type == ActionType::Rescue || type == ActionType::GiveUnit || type == ActionType::TakeUnit;
  const bool pairOnly = type == ActionType::PairUp || type == ActionType::SwitchPair;
  if (carryOnly && mode != RescueMode::Carry) return refuse(why, "this game uses pair up, not carrying");
  if (pairOnly && mode != RescueMode::PairUp) return refuse(why, "this game uses carrying, not pair up");
  const Unit& u = units[(size_t)action.unit];  // validate() has checked the unit
  const char* reason = nullptr;
  switch (type) {
    case ActionType::Rescue:
    case ActionType::PairUp: reason = rescueWhy(u, u.cell, action.target); break;
    case ActionType::GiveUnit: reason = handWhy(u, u.cell, action.target, true); break;
    case ActionType::TakeUnit: reason = handWhy(u, u.cell, action.target, false); break;
    case ActionType::Drop: reason = dropWhy(u, u.cell, action.cell); break;
    case ActionType::SwitchPair: reason = switchWhy(u); break;
    default: reason = "that is not a rescue order"; break;
  }
  return reason ? refuse(why, reason) : true;
}

void Battle::applyRescue(const Action& action, std::vector<Event>& events) {
  switch (action.type) {
    case ActionType::Rescue:
    case ActionType::PairUp: applyRescueAction(action, events); break;
    case ActionType::Drop: applyDrop(action, events); break;
    case ActionType::GiveUnit:
    case ActionType::TakeUnit: applyHand(action, events); break;
    case ActionType::SwitchPair: applySwitchPair(action, events); break;
    default: break;
  }
}

// Rescue: `unit` stays and takes `target`. PairUp: `unit` goes behind `target`.
void Battle::applyRescueAction(const Action& action, std::vector<Event>& events) {
  const bool pair = action.type == ActionType::PairUp;
  const int stays = pair ? action.target : action.unit;
  const int leaves = pair ? action.unit : action.target;
  Event e;
  e.type = EventType::UnitRescued;
  e.a = stays;
  e.b = leaves;
  e.c = units[(size_t)leaves].cell;
  e.flags = pair ? kPaired : 0;
  events.push_back(e);
  markSight();  // the event is stamped with the sight from before the unit leaves
  linkCarried(stays, leaves);
  endOrder(action.unit);
}

void Battle::applyDrop(const Action& action, std::vector<Event>& events) {
  const int carried = units[(size_t)action.unit].carrying;
  const Cell cell = action.cell;
  Event e;
  e.type = EventType::UnitDropped;
  e.a = action.unit;
  e.b = carried;
  e.c = cell;
  const int blocker = occupant[(size_t)cell];
  if (blocker >= 0) {
    // Only reachable with fog: an enemy the team did not see stands there. Nothing happens, the order is spent.
    e.flags = kBlocked;
    e.d = blocker;
    events.push_back(e);
    endOrder(action.unit);
    return;
  }
  markSight();
  unlinkCarried(carried, cell);
  events.push_back(e);
  endOrder(action.unit);
  // The dropped unit has stopped somewhere: events waiting for a unit to arrive on this square may run.
  StoryOccurrence o;
  o.kind = Trigger::Kind::Reach;
  o.unit = carried;
  o.cell = cell;
  fireStory(o, events);
}

void Battle::applyHand(const Action& action, std::vector<Event>& events) {
  const bool give = action.type == ActionType::GiveUnit;
  const int from = give ? action.unit : action.target;
  const int to = give ? action.target : action.unit;
  const int carried = units[(size_t)from].carrying;
  Event e;
  e.type = EventType::UnitHanded;
  e.a = from;
  e.b = to;
  e.c = carried;
  events.push_back(e);
  markSight();
  units[(size_t)from].carrying = -1;
  units[(size_t)to].carrying = carried;
  units[(size_t)carried].carriedBy = to;
  units[(size_t)action.unit].moved = true;  // free, but it counts as having moved
}

// ---- the carrier falls ----

bool Battle::putDownCarried(int carried, Cell around, std::vector<Event>& events) {
  Unit& c = units[(size_t)carried];
  const Cell dest = grid.valid(around) ? nearestFreeCell(around, typeOf(c).moveType) : kNoCell;
  if (dest == kNoCell) return false;
  Event e;
  e.type = EventType::UnitDropped;
  e.a = c.carriedBy;
  e.b = carried;
  e.c = dest;
  e.flags = kForced;
  markSight();
  unlinkCarried(carried, dest);
  events.push_back(e);
  return true;
}

void Battle::rescueUnitGone(int unit, int by, std::vector<Event>& events) {
  const int carried = units[(size_t)unit].carrying;
  if (carried < 0) return;
  const Cell where = lastSquare(events, unit);
  // The carrier is off the board already (alive == false, its square free): the carried unit takes that square if
  // it may, else the nearest free one.
  if (putDownCarried(carried, where, events)) return;
  // No square for it: lost, defeated by whatever defeated the carrier.
  units[(size_t)unit].carrying = -1;
  Unit& c = units[(size_t)carried];
  c.carriedBy = -1;
  c.alive = true;  // killUnit only acts on a unit on the board
  c.cell = grid.valid(where) ? where : kNoCell;
  std::vector<ItemStack> drops;
  dropsOf(c, drops);  // docs/ITEMS.md "Drops"
  killUnit(carried, events, by);
  if (by >= 0 && units[(size_t)by].alive)
    for (const ItemStack& s : drops) giveTo(by, s, kDropped, events);
  lostCarried_.push_back({carried, by});
}

void Battle::defeatCarried(int carried, std::vector<Event>& events) {
  Unit& c = units[(size_t)carried];
  if (c.carriedBy < 0) return;
  Unit& carrier = units[(size_t)c.carriedBy];
  carrier.carrying = -1;
  c.carriedBy = -1;
  c.alive = true;  // killUnit only acts on a unit on the board: it falls where its carrier stands
  c.cell = carrier.cell;
  killUnit(carried, events);
}

void Battle::flushLostCarried(std::vector<Event>& events) {
  if (lostCarried_.empty()) return;
  std::vector<std::pair<int, int>> list;
  list.swap(lostCarried_);
  for (const std::pair<int, int>& l : list) unitDefeated(l.first, l.second, events);
}

// ---- story commands on units that are carried ----

void Battle::storyRemoveCarried(int carried, std::vector<Event>& events) {
  Unit& c = units[(size_t)carried];
  Event e;
  e.type = EventType::UnitRemoved;
  e.a = carried;
  e.b = units[(size_t)c.carriedBy].cell;  // it leaves from where its carrier stands
  events.push_back(e);
  markSight();
  units[(size_t)c.carriedBy].carrying = -1;
  c.carriedBy = -1;
  c.hp = 0;  // gone, but not "defeated": no drops, no defeat events
  srpgUnitGone(carried, -1, events);
}

// changeSide: a carrier (or a carried unit) first puts the carried unit down, next to the carrier. With no free square
// the command leaves both units as they are.
bool Battle::storySeparate(int unit, std::vector<Event>& events) {
  const Unit& u = units[(size_t)unit];
  const int carried = u.carrying >= 0 ? u.carrying : u.carriedBy >= 0 ? unit : -1;
  if (carried < 0) return true;
  const Cell around = units[(size_t)units[(size_t)carried].carriedBy].cell;
  if (putDownCarried(carried, around, events)) return true;
  OTE_WARN("story: changeSide: no free square to put down the unit '%s' carried; both units stay as they are",
           units[(size_t)carried].storyId.c_str());
  return false;
}

void Battle::storyMoveCarried(int carried, Cell to, std::vector<Event>& events) {
  if (!putDownCarried(carried, to, events)) return;
  StoryOccurrence o;
  o.kind = Trigger::Kind::Reach;
  o.unit = carried;
  o.cell = units[(size_t)carried].cell;
  fireStory(o, events);
}

// ---- stats ----

int Battle::rescueStat(const Unit& u, int stat, int value) const {
  const RescueRules& r = db->rules.rescue;
  if (r.mode == RescueMode::Off || u.carrying < 0 || stat == db->rules.statHp) return value;
  // carrierStats: percent first, then the added number; never below 0 (the loader refuses the health stat).
  // "noCarryPenalty" (a skill): the carrier ignores this (not the pair bonus below).
  if ((size_t)stat < r.carrierStats.size() && !(r.mode == RescueMode::Carry && hasStanding(u, SkillEffect::Kind::NoCarryPenalty))) {
    const StatChange& c = r.carrierStats[(size_t)stat];
    const long long v = (long long)value * c.percent / 100 + c.add;
    value = (int)std::max(0LL, std::min(v, 2000000000LL));
  }
  // Pair up: then what the unit behind gives (its type's pairBonus, which replaces the game's as a whole).
  if (r.mode == RescueMode::PairUp && u.carrying < (int)units.size()) {
    const UnitTypeDef& behind = typeOf(units[(size_t)u.carrying]);
    const std::vector<int>& bonus = behind.pairBonus.empty() ? r.pairBonus : behind.pairBonus;
    if ((size_t)stat < bonus.size()) value += bonus[(size_t)stat];
  }
  return value;
}

}  // namespace ote
