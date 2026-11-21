// The test level (docs/DIFFICULTY.md "Test level"): a maker's testing aid that lifts the human side's units to a level so a late map
// can be tried without playing the earlier ones. It reuses the difficulty's "level-ups at the average growth" (growStatByLevels):
// no dice, the battle's random numbers are never touched. Off (testLevel 0) nothing in here changes anything.
#include <algorithm>
#include <string>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

int Battle::enemyAverageLevel() const {
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
    sum += effectiveTestLevel(u);
    ++count;
  }
  return count == 0 ? 0 : (int)((sum * 2 + count) / (count * 2));  // rounded, halves up
}

bool Battle::testPromotedType(int type) const {
  if (db->rules.promotion.testAt <= 0) return false;
  for (const UnitTypeDef& t : db->unitTypes)
    for (const PromotionDef& d : t.promotions)
      if (d.to == type && (!d.items.empty() || d.atLevel != PromoteAtLevel::Off)) return true;
  return false;
}

const PromotionDef* Battle::testPromotionOf(const Unit& u) const {
  if (db->rules.promotion.testAt <= 0 || u.type < 0 || testPromotedType(u.type) || jobBlocksPromotion(u)) return nullptr;
  for (const PromotionDef& d : typeOf(u).promotions)
    if (d.to >= 0 && (!d.items.empty() || d.atLevel != PromoteAtLevel::Off)) return &d;  // the first one a player could trigger
  return nullptr;
}

int Battle::effectiveTestLevel(const Unit& u) const {
  return testPromotedType(u.type) ? u.level + db->rules.promotion.testAt : u.level;
}

Battle::TestRaise Battle::raiseUnitForTest(Unit& u, int target, bool onBoard) {
  TestRaise r;
  r.reached = u.level;
  const int testAt = db->rules.promotion.testAt;
  if (effectiveTestLevel(u) >= target) return r;  // never lowered, nothing added
  auto climb = [&](int to) {  // levelUnitTo plus the ranks of the levels gained
    const int gained = levelUnitTo(u, to);
    if (gained > 0) r.changed = true;
    return raiseTestRanks(u, gained);
  };
  const PromotionDef* def = testPromotionOf(u);
  if (testAt <= 0 || (!def && !testPromotedType(u.type)) || (def && target < testAt)) {
    r.tail = climb(target);
    r.reached = u.level;
    return r;
  }
  if (!def) {  // already promoted: its own level is the effective one minus testAt
    r.tail = climb(target - testAt);
    r.reached = u.level;
    return r;
  }
  // A unit that can promote and is asked for testAt or more: up to testAt in its own class (so the skills that class teaches up to
  // there are learned), the promotion, then the rest of the climb in the new class.
  rankRaiseToClass(u, -1, nullptr);  // (no-op without ranks) the starting point of the rank changes told below
  const std::vector<int> xp0 = u.weaponXp;
  climb(testAt);
  r.reached = u.level;
  const bool stands = !onBoard || canStandAs(u, def->to);  // could not stand there as the new type: it stays what it is
  if (stands) {
    const std::string className = db->unitTypes[(size_t)def->to].name;
    changeType(u, def->to, def);
    rankRaiseToClass(u, -1, nullptr);
    r.changed = true;
    LevelAfter rule = def->levelAfter;
    if (rule == LevelAfter::Game) rule = db->rules.promotion.keepLevel ? LevelAfter::Keep : LevelAfter::Reset;
    climb(rule == LevelAfter::Keep ? target : 1 + (target - testAt));
    u.hp = maxHp(u);
    r.tail = ", promoted to " + className + ", Lv " + std::to_string(u.level);
  }
  for (size_t g = 0; g < xp0.size() && g < u.weaponXp.size(); ++g) {  // all rank changes of the whole climb, as one list
    const int a = db->ranks.rankAt(xp0[g]), b = db->ranks.rankAt(u.weaponXp[g]);
    if (b > a) r.tail += ", " + db->ranks.groups[g] + " " + db->ranks.ladder[(size_t)a].name + " -> " + db->ranks.ladder[(size_t)b].name;
  }
  return r;
}

int Battle::autoTestLevel() const {
  const int avg = enemyAverageLevel();
  return avg <= 0 ? 0 : avg + kAutoTestLevelBonus;
}

int Battle::testRankXpPerLevel() const {
  if (!db->ranks.on()) return 0;
  const int span = 2 * std::max(1, db->rules.maxLevel);  // the whole level range twice: before and after a promotion
  return std::max(1, (db->ranks.topXp() + span - 1) / span);
}

std::string Battle::raiseTestRanks(Unit& u, int levels) const {
  if (levels <= 0 || !db->ranks.on()) return "";
  rankRaiseToClass(u, -1, nullptr);  // a roster unit may not carry its class's ranks yet; never lowers anything
  const long long gain = (long long)testRankXpPerLevel() * levels;
  const UnitTypeDef& t = typeOf(u);
  std::string note;
  for (size_t g = 0; g < db->ranks.groups.size(); ++g) {
    // "Can wield" = the class's or character's group, or a weapon the class names whose group this is. Every such group has a rank
    // (a group nobody gave a value is at the lowest one), so a group the unit cannot wield gets nothing.
    bool can = db->canWieldGroup(u.type, db->ranks.groups[g], u.character);
    for (size_t i = 0; i < t.weapons.size() && !can; ++i)
      can = db->weapons[(size_t)t.weapons[i]].rankGroup == (int)g;
    if (!can) continue;
    int& xp = u.weaponXp[g];
    const int before = db->ranks.rankAt(xp);
    xp = (int)std::min<long long>(xp + gain, std::max(xp, db->ranks.topXp()));
    const int after = db->ranks.rankAt(xp);
    if (after > before)
      note += ", " + db->ranks.groups[g] + " " + db->ranks.ladder[(size_t)before].name + " -> " + db->ranks.ladder[(size_t)after].name;
  }
  return note;
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
  if (testLevel < 0) OTE_INFO("test level: auto aims at enemy average %d + %d = %d", enemyAverageLevel(), kAutoTestLevelBonus, target);
  int raised = 0;
  for (Unit& u : units) {
    if (u.side < 0 || u.side >= (int)sides.size() || sides[(size_t)u.side].ai) continue;  // human sides only
    const int before = u.level;
    const TestRaise t = raiseUnitForTest(u, target, true);
    if (t.changed) {
      ++raised;
      OTE_INFO("test level: %s Lv %d -> %d%s", u.name.c_str(), before, t.reached, t.tail.c_str());
    }
  }
  // The summary keeps the shape "test level: N (auto) (K units raised)": the web player's embed.html reads the number off it.
  OTE_INFO("test level: %d%s (%d unit%s raised)", target, testLevel < 0 ? " (auto)" : "", raised, raised == 1 ? "" : "s");
}

}  // namespace ote
