// Computer opponent: waking a waiting unit (docs/AI.md). A guard ("guard" / "guardArea") sleeps until something wakes it: the AI's own
// check at the start of its steps (ai/ai_behaviour.cpp), a hit it takes, a unit of its group waking, or the story's wakeGroup. Waking
// one unit wakes its whole group (same side, same group name).
#include "core/battle/battle.h"

namespace ote {

namespace {
bool sleeper(const Unit& u) { return (u.behaviour.mode == AiMode::Guard && !u.behaviour.react) || u.behaviour.mode == AiMode::GuardArea; }
}  // namespace

void Battle::wakeUnit(int unit) {
  if (unit < 0 || unit >= (int)units.size()) return;
  const Unit& u = units[(size_t)unit];
  if (!sleeper(u)) return;  // only a guard has anything to wake from; others never gain the flag (saves stay as they were)
  const std::string group = u.behaviour.group;
  const int side = u.side;
  units[(size_t)unit].awake = true;
  if (group.empty()) return;
  for (Unit& o : units)
    if (o.alive && o.side == side && sleeper(o) && o.behaviour.group == group) o.awake = true;
}

void Battle::wakeGroup(const std::string& group) {
  if (group.empty()) return;
  for (Unit& o : units)
    if (o.alive && sleeper(o) && o.behaviour.group == group) o.awake = true;
}

}  // namespace ote
