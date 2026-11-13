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
  const CellState& cs = cells[at];
  if (terrainAt(at).capturePoints <= 0) return false;
  return cs.owner < 0 || sides[cs.owner].team != teamOf(u);
}

bool Battle::canAfford(int side, int unitType) const {
  for (const Amount& c : db->unitTypes[unitType].cost)
    if (sides[side].resources[c.resource] < c.amount) return false;
  return true;
}

void Battle::buildOptions(Cell cell, std::vector<int>& unitTypes) const {
  unitTypes.clear();
  if (over || !db->rules.production || !grid.valid(cell)) return;
  if (cells[cell].owner != activeSide || occupant[cell] != -1) return;
  unitTypes = terrainAt(cell).produces;
}

bool Battle::validate(const Action& action, std::string* why) const {
  if (over) return fail(why, "the battle is over");

  if (action.type == ActionType::EndTurn) return true;

  if (action.type == ActionType::Build) {
    if (!db->rules.production) return fail(why, "this game has no unit production");
    if (!grid.valid(action.cell)) return fail(why, "invalid cell");
    if (cells[action.cell].owner != activeSide) return fail(why, "you do not own this building");
    if (occupant[action.cell] != -1) return fail(why, "the building is occupied");
    const std::vector<int>& list = terrainAt(action.cell).produces;
    if (std::find(list.begin(), list.end(), action.unitType) == list.end())
      return fail(why, "that unit cannot be built here");
    if (!canAfford(activeSide, action.unitType)) return fail(why, "not enough resources");
    return true;
  }

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
    default:
      return fail(why, "unknown action");
  }
}

bool Battle::apply(const Action& action, std::vector<Event>& events, std::string* why) {
  if (!validate(action, why)) return false;
  switch (action.type) {
    case ActionType::Move: applyMove(action, events); break;
    case ActionType::Attack: applyAttack(action, events); break;
    case ActionType::Capture: applyCapture(action, events); break;
    case ActionType::Wait: units[action.unit].done = true; break;
    case ActionType::Build: applyBuild(action, events); break;
    case ActionType::EndTurn: advanceTurn(events); return true;
  }
  checkDefeat(events);
  // A counterattack can eliminate the side whose turn it is.
  if (!over && sides[activeSide].defeated) advanceTurn(events);
  return true;
}

void Battle::releaseCapture(int unit) {
  Cell c = units[unit].cell;
  if (c == kNoCell || cells[c].capturer != unit) return;
  cells[c].capturer = -1;
  cells[c].capture = terrainAt(c).capturePoints;
}

void Battle::applyMove(const Action& action, std::vector<Event>& events) {
  Unit& u = units[action.unit];
  Reach reach;
  reachable(action.unit, reach);
  Event e;
  e.type = EventType::UnitMoved;
  e.a = action.unit;
  std::vector<Cell> path;
  pathTo(reach, action.cell, path);
  e.data.assign(path.begin(), path.end());
  events.push_back(e);

  releaseCapture(action.unit);  // walking away abandons a capture
  occupant[u.cell] = -1;
  u.cell = action.cell;
  occupant[u.cell] = action.unit;
  u.moved = true;
}

void Battle::applyCapture(const Action& action, std::vector<Event>& events) {
  Unit& u = units[action.unit];
  CellState& cs = cells[u.cell];
  int points = terrainAt(u.cell).capturePoints;

  int amount = points;  // default: instant (Fire Emblem "seize")
  if (formulas->has(Formulas::CaptureAmount)) {
    FormulaCtx ctx;
    ctx.a = &u;
    ctx.ta = &terrainAt(u.cell);
    amount = (int)std::floor(formulas->eval(Formulas::CaptureAmount, ctx, points));
  }
  if (amount < 1) amount = 1;

  cs.capturer = action.unit;
  cs.capture -= amount;
  Event e;
  if (cs.capture <= 0) {
    e.type = EventType::CellCaptured;
    e.a = u.cell;
    e.b = u.side;
    e.c = cs.owner;
    cs.owner = u.side;
    cs.capture = points;
    cs.capturer = -1;
  } else {
    e.type = EventType::CaptureProgress;
    e.a = action.unit;
    e.b = u.cell;
    e.c = cs.capture;
  }
  events.push_back(e);
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
  u.hp = u.stats[db->rules.statHp];
  u.moved = u.done = !db->rules.builtUnitsCanAct;
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
