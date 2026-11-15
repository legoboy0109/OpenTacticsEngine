// Action validation and the non-combat actions (move, capture, build).
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

static bool fail(std::string* why, const char* reason) {
  if (why) *why = reason;
  return false;
}

bool Battle::canCaptureAt(int unit, Cell at) const {
  if (!db->rules.capture || unit < 0 || unit >= (int)units.size() || !grid.valid(at)) return false;
  const Unit& u = units[unit];
  if (!u.alive || !typeOf(u).canCapture) return false;
  int si = structureAt[at];
  if (si < 0 || defOf(structures[si]).capturePoints <= 0) return false;
  int owner = structures[si].owner;
  return owner < 0 || sides[owner].team != teamOf(u);
}

bool Battle::canAfford(int side, int unitType) const {
  for (const Amount& c : db->unitTypes[unitType].cost)
    if (sides[side].resources[c.resource] < c.amount) return false;
  return true;
}

void Battle::buildOptions(Cell cell, std::vector<int>& unitTypes, bool includeLocked) const {
  unitTypes.clear();
  if (over || !db->rules.production || !grid.valid(cell)) return;
  int si = structureAt[cell];
  if (si < 0 || structures[si].owner != activeSide || occupant[cell] != -1) return;
  // The new unit appears on this very square, so it must be able to enter it.
  for (int t : defOf(structures[si]).produces)
    if (enterCost(cell, db->unitTypes[t].moveType) > 0 && (includeLocked || unitUnlocked(activeSide, t)))
      unitTypes.push_back(t);
}

bool Battle::canBuild(Cell cell, int unitType, std::string* why) const {
  if (over) return fail(why, "the battle is over");
  if (!db->rules.production) return fail(why, "this game has no unit production");
  if (!grid.valid(cell)) return fail(why, "invalid cell");
  int si = structureAt[cell];
  if (si < 0 || structures[si].owner != activeSide) return fail(why, "you do not own a structure here");
  if (occupant[cell] != -1) return fail(why, "that square is occupied");
  const std::vector<int>& list = defOf(structures[si]).produces;
  if (std::find(list.begin(), list.end(), unitType) == list.end()) return fail(why, "that unit cannot be built here");
  if (enterCost(cell, db->unitTypes[unitType].moveType) <= 0) return fail(why, "that unit could not enter this square");
  if (!unitUnlocked(activeSide, unitType)) {
    std::vector<int> techs;
    unlockedBy(unitType, techs);
    std::string need;
    for (int t : techs) need += (need.empty() ? "" : " or ") + db->research[(size_t)t].name;
    return fail(why, ("needs " + need).c_str());
  }
  if (!canAfford(activeSide, unitType)) return fail(why, "not enough resources");
  return true;
}

bool Battle::validate(const Action& action, std::string* why) const {
  if (over) return fail(why, "the battle is over");

  if (action.type == ActionType::EndTurn) return true;

  if (action.type == ActionType::Build) return canBuild(action.cell, action.unitType, why);
  if (action.type == ActionType::UpgradeStructure) return validateUpgrade(action, why);
  if (action.type == ActionType::StartResearch) return validateResearch(action, why);

  // Everything else is a unit order.
  if (!canAct(action.unit)) return fail(why, "that unit cannot act now");
  const Unit& u = units[action.unit];
  if (u.done) return fail(why, "that unit has already acted");

  switch (action.type) {
    case ActionType::Move: {
      if (u.moved) return fail(why, "that unit has already moved");
      if (action.cell == u.cell) return fail(why, "the unit is already there");
      Reach reach;
      reachable(action.unit, reach);
      if (!reach.canStop(action.cell)) return fail(why, "the unit cannot reach that cell");
      return true;
    }
    case ActionType::Attack: {
      Forecast f;
      if (!forecast(action.unit, action.target, action.weapon, u.cell, f))
        return fail(why, "that attack is not possible");
      return true;
    }
    case ActionType::Capture:
      if (!canCaptureAt(action.unit, u.cell)) return fail(why, "nothing to capture here");
      return true;
    case ActionType::Wait:
      return true;
    case ActionType::Supply:
      if (!typeOf(u).supplies) return fail(why, "that unit cannot supply others");
      if (!canSupply(action.unit)) return fail(why, "there is nobody next to it who needs supplies");
      return true;
    case ActionType::Talk: {
      std::vector<TalkOption> options;
      talkOptions(action.unit, u.cell, options);
      for (const TalkOption& o : options)
        if (o.target == action.target && (action.slot < 0 || action.slot == o.event)) return true;
      return fail(why, "there is nobody here to talk to");
    }
    default:
      return validateItemAction(action, why);
  }
}

bool Battle::apply(const Action& action, std::vector<Event>& events, std::string* why) {
  if (!validate(action, why)) return false;
  EventScope scope(*this, events);  // with fog: stamps every event with the teams that saw it
  switch (action.type) {
    case ActionType::Move: applyMove(action, events); break;
    case ActionType::Attack: applyAttack(action, events); break;
    case ActionType::Capture: applyCapture(action, events); break;
    case ActionType::Wait: units[action.unit].done = true; break;
    case ActionType::Build: applyBuild(action, events); break;
    case ActionType::UpgradeStructure: applyUpgrade(action, events); break;
    case ActionType::StartResearch: applyResearch(action, events); break;
    case ActionType::Supply: applySupply(action, events); break;
    case ActionType::Talk: applyTalk(action, events); break;
    case ActionType::EndTurn:
      advanceTurn(events);
      // Without a story nothing can have changed since the last action's check.
      if (!story) return true;
      break;
    default: applyItemAction(action, events); break;
  }
  settle(events);
  // A counterattack can eliminate the side whose turn it is.
  if (!over && sides[activeSide].defeated) advanceTurn(events);
  return true;
}

void Battle::releaseCapture(int unit, Cell newCell) {
  for (size_t i = 0; i < structures.size(); ++i) {
    StructureState& st = structures[i];
    if (st.capturer != unit) continue;
    // Moving between squares of the same structure keeps the progress.
    if (newCell != kNoCell && structureAt[newCell] == (int)i) continue;
    st.capturer = -1;
    st.capture = defOf(st).capturePoints;
  }
}

void Battle::applyMove(const Action& action, std::vector<Event>& events) {
  Unit& u = units[action.unit];
  Reach reach;
  reachable(action.unit, reach);
  std::vector<Cell> path;
  pathTo(reach, action.cell, path);

  // With fog the path was planned on what the unit's team saw; an enemy it did not see on the way stops the unit
  // in front of it (fog_rules.cpp). Without fog nothing can be in the way and `stop` is the destination.
  int blocker = -1;
  Cell blockedAt = kNoCell;
  const int stop = pathStop(action.unit, path, blocker, blockedAt);
  const Cell dest = path[(size_t)stop];
  if (stop > 0) {
    Event e;
    e.type = EventType::UnitMoved;
    e.a = action.unit;
    e.data.assign(path.begin(), path.begin() + stop + 1);
    events.push_back(e);
  }
  if (blocker >= 0) {
    Event e;
    e.type = EventType::MoveBlocked;
    e.a = action.unit;
    e.b = blocker;
    e.c = dest;
    e.d = blockedAt;
    events.push_back(e);
  }

  releaseCapture(action.unit, dest);  // walking off the structure abandons a capture
  if (u.fuel >= 0) u.fuel -= reach.cost[(size_t)dest];  // the movement points of the squares walked (reach limited it to the tank)
  markSight();  // events so far are stamped with the sight before the unit moved
  if (dest != u.cell) {
    occupant[u.cell] = -1;
    u.cell = dest;
    occupant[u.cell] = action.unit;
  }
  noticeAlong(action.unit, path, stop);
  u.moved = true;
  // Ambushed: by default the unit's turn is over; with "rules.fog.ambush": "canAct" it only counts as having moved
  // (it cannot move again, may do anything else it could do after a normal move to this square).
  if (blocker >= 0 && db->rules.ambush == AmbushRule::EndsTurn) u.done = true;

  if (stop > 0) {  // the unit has ended its move: events waiting for it to arrive here may run
    StoryOccurrence o;
    o.kind = Trigger::Kind::Reach;
    o.unit = action.unit;
    o.cell = dest;
    fireStory(o, events);
  }
}

void Battle::applyCapture(const Action& action, std::vector<Event>& events) {
  Unit& u = units[action.unit];
  int si = structureAt[u.cell];  // validated: a capturable structure
  StructureState& st = structures[si];
  int points = defOf(st).capturePoints;

  int amount = points;  // default: instant (Fire Emblem "seize")
  if (formulas->has(Formulas::CaptureAmount)) {
    FormulaCtx ctx;
    ctx.sides = &sides;
    ctx.a = &u;
    Props scratch;
    ctx.ta = &numbersAt(u.cell, scratch);
    amount = (int)std::floor(formulas->eval(Formulas::CaptureAmount, ctx, points));
  }
  if (amount < 1) amount = 1;

  // Progress belongs to the structure and to one unit: a different unit starts from full.
  if (st.capturer != action.unit) st.capture = points;
  st.capturer = action.unit;
  st.capture -= amount;
  Event e;
  const bool captured = st.capture <= 0;
  if (captured) {
    e.type = EventType::StructureCaptured;
    e.a = si;
    e.b = u.side;
    e.c = st.owner;
  } else {
    e.type = EventType::CaptureProgress;
    e.a = action.unit;
    e.b = u.cell;
    e.c = st.capture;
    e.d = si;
  }
  events.push_back(e);
  if (captured) {
    markSight();  // the event is stamped with the sight before the structure changes hands (it sees for its owner)
    st.owner = u.side;
    st.capture = points;
    st.capturer = -1;
    learnOwner(si, events.back().seen);  // every team that saw it happen now believes the new owner
  }
  u.done = true;
}

void Battle::applyBuild(const Action& action, std::vector<Event>& events) {
  const UnitTypeDef& type = db->unitTypes[action.unitType];
  for (const Amount& c : type.cost) changeResource(activeSide, c.resource, -c.amount, events);

  Unit u;
  u.id = (int)units.size();
  u.type = action.unitType;
  u.side = activeSide;
  u.cell = action.cell;
  u.name = type.name;
  u.stats = type.stats;
  u.stats.resize(db->stats.size(), 0);
  for (const ItemGrant& g : type.items) {
    ItemStack s;
    s.item = g.item;
    s.uses = g.uses;
    s.drop = g.drop;
    u.items.push_back(s);
  }
  u.hp = maxHp(u);
  stockUnit(u);
  u.moved = u.done = !db->rules.builtUnitsCanAct;
  markSight();
  units.push_back(u);
  occupant[action.cell] = u.id;
  sides[activeSide].everHadUnits = true;

  Event e;
  e.type = EventType::UnitBuilt;
  e.a = u.id;
  e.b = action.cell;
  events.push_back(e);
}

}  // namespace ote
