// The test level (docs/DIFFICULTY.md "Test level"): a maker's testing aid that lifts the human side's units to a level so a late map
// can be tried without playing the earlier ones. It reuses the difficulty's "level-ups at the average growth" (growStatByLevels):
// no dice, the battle's random numbers are never touched. Off (testLevel 0) nothing in here changes anything.
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

int Battle::autoTestLevel() const {
  int first = -1;
  for (size_t i = 0; i < sides.size(); ++i)
    if (!sides[i].ai) {
      first = (int)i;
      break;
    }
  if (first < 0) return 0;
  long long sum = 0;
  int count = 0;
  for (const Unit& u : units) {
    if (!u.alive || u.side < 0 || u.side >= (int)sides.size()) continue;
    const SideState& s = sides[(size_t)u.side];
    if (!s.ai || s.team == sides[(size_t)first].team) continue;
    sum += u.level;
    ++count;
  }
  return count == 0 ? 0 : (int)((sum * 2 + count) / (count * 2));  // rounded, halves up
}

int Battle::levelUnitTo(Unit& u, int target) const {
  // A unit type's own maxLevel (else the game's) stops the climb; a unit placed above the cap is left alone.
  const int to = std::min(target, levelCap(u));
  const int levels = to - u.level;
  if (levels <= 0) return 0;
  for (size_t s = 0; s < u.stats.size(); ++s) growStatByLevels(u, (int)s, levels);
  u.level = to;
  u.xp = 0;
  learnUpTo(u);  // the skills of the levels it now has (no event: it is a start-up, like a placed unit's)
  u.hp = maxHp(u);
  return levels;
}

void Battle::applyTestLevel() {
  testLevelUsed = 0;
  if (testLevel == 0 || !db) return;
  const int target = testLevel > 0 ? testLevel : autoTestLevel();
  if (target <= 0) {
    OTE_INFO("test level: automatic, but no hostile computer units on the map: nothing changed");
    return;
  }
  testLevelUsed = target;
  int raised = 0;
  for (Unit& u : units) {
    if (u.side < 0 || u.side >= (int)sides.size() || sides[(size_t)u.side].ai) continue;  // human sides only
    const int before = u.level;
    if (levelUnitTo(u, target) > 0) {
      ++raised;
      OTE_INFO("test level: %s Lv %d -> %d", u.name.c_str(), before, u.level);
    }
  }
  OTE_INFO("test level: %d%s (%d unit%s raised)", target, testLevel < 0 ? " (auto)" : "", raised, raised == 1 ? "" : "s");
}

}  // namespace ote
