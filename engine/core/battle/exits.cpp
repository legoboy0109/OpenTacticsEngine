// Map exits (docs/AI.md "Looting and escaping"): a computer unit standing on one of the map's "exits" may leave the map. Only the computer
// opponent uses it (modes "escape" and "loot"); a person's units are never offered it, and the action refuses them.
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

bool Battle::isExit(Cell c) const { return std::find(exits.begin(), exits.end(), c) != exits.end(); }

void Battle::startEscaping(int unit) {
  touch();
  if (unit < 0 || unit >= (int)units.size() || units[(size_t)unit].behaviour.mode != AiMode::Loot) return;  // only a looter keeps this state
  if (!units[(size_t)unit].escaping) OTE_INFO("ai: %s has nothing left to take and makes for an exit", units[(size_t)unit].name.c_str());
  units[(size_t)unit].escaping = true;
}

bool Battle::validateLeaveMap(const Action& action, std::string* why) const {
  const Unit& u = units[(size_t)action.unit];
  if (!sides[(size_t)u.side].ai) {
    if (why) *why = "only a computer unit leaves the map by an exit";
    return false;
  }
  if (u.carriedBy >= 0 || !isExit(u.cell)) {
    if (why) *why = "that unit is not on an exit square";
    return false;
  }
  return true;
}

void Battle::applyLeaveMap(const Action& action, std::vector<Event>& events) {
  const int i = action.unit;
  Unit& u = units[(size_t)i];
  const Cell cell = u.cell;
  OTE_INFO("exit: %s leaves the map at %d,%d", u.name.c_str(), grid.x(cell), grid.y(cell));
  // The same steps as the story's removeUnit (story_command_units.cpp): the player fades it out.
  Event gone;
  gone.type = EventType::UnitRemoved;
  gone.a = i;
  gone.b = cell;
  events.push_back(gone);
  markSight();
  releaseCapture(i, kNoCell);
  occupant[(size_t)cell] = -1;
  u.alive = false;  // gone, but not "defeated": no drops, no defeat events
  u.hp = 0;
  u.cell = kNoCell;
  u.done = true;
  srpgUnitGone(i, -1, events);
}

}  // namespace ote
