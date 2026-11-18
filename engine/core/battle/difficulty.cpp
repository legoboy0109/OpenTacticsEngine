// Difficulties in a battle (docs/DIFFICULTY.md): which difficulty it was built with, the numbers resolved for it, how an
// enemy placement is shifted, and the experience and resource multipliers. A game without difficulties never gets here
// with a difficulty (difficulty stays -1 and every function returns its input unchanged).
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

namespace {

// Percent of an amount, rounded to the nearest (halves up); amounts are small, but go through long long anyway.
int percentOf(int amount, int percent) { return (int)(((long long)amount * percent + 50) / 100); }

}  // namespace

void Battle::setDifficulty(int index) {
  difficulty = index;
  if (db) resolveDifficulty();  // before init() there is no database yet: init resolves it
}

void Battle::resolveDifficulty() {
  diffEnemy = DifficultyEnemy();
  diffXpPercent = diffResourcePercent = 100;
  if (!db || !db->difficulties.any()) {
    difficulty = -1;
    return;
  }
  const DifficultyRules& rules = db->difficulties;
  if (difficulty < 0 || difficulty >= (int)rules.list.size()) difficulty = rules.defaultIndex;
  const DifficultyDef& def = rules.list[(size_t)difficulty];
  diffEnemy = def.enemy;
  diffEnemy.stats.resize(db->stats.size(), 0);
  diffXpPercent = def.xpPercent;
  diffResourcePercent = def.resourcePercent;
  // The map's own numbers REPLACE the game's for what they name (they do not add to them).
  if (!diffMap) return;
  for (const MapDifficulty& md : *diffMap) {
    if (md.difficulty != difficulty) continue;
    if (md.enemy.hasLevels) diffEnemy.levels = md.enemy.levels;
    if (md.enemy.hasPercent) diffEnemy.statsPercent = md.enemy.statsPercent;
    for (size_t s = 0; s < md.enemy.hasStat.size() && s < diffEnemy.stats.size(); ++s)
      if (md.enemy.hasStat[s]) diffEnemy.stats[s] = md.enemy.stats[s];
    if (md.xpPercent >= 0) diffXpPercent = md.xpPercent;
    if (md.resourcePercent >= 0) diffResourcePercent = md.resourcePercent;
  }
}

bool Battle::difficultyEnemySide(int side) const {
  if (side < 0 || side >= (int)sides.size()) return false;
  const SideState& s = sides[(size_t)side];
  if (!s.ai || !s.difficultyShift) return false;
  for (const SideState& o : sides)
    if (!o.ai && o.team != s.team) return true;  // hostile to a human side (another team)
  return false;
}

bool Battle::appearsOnDifficulty(const MapUnitDef& def) const {
  if (def.onlyOn.empty() || difficulty < 0) return true;
  return std::find(def.onlyOn.begin(), def.onlyOn.end(), difficulty) != def.onlyOn.end();
}

// The shifted level: `levels` higher or lower, never below 1 and never above the game's top level (a unit already placed
// above it stays where it is). Without experience and levels (rules.progression) the level is only a label, but the
// shift still means the same: that many growth steps of stats.
int Battle::difficultyLevel(const MapUnitDef& def) const {
  if (difficulty < 0 || !def.difficultyShift || !difficultyEnemySide(def.side) || diffEnemy.levels == 0) return def.level;
  const int top = std::max(def.level, db->rules.maxLevel);
  return std::max(1, std::min(top, def.level + diffEnemy.levels));
}

void Battle::growStatByLevels(Unit& u, int stat, int levels) const {
  const long long total = (long long)growthOf(u, stat) * levels;
  raiseStat(u, stat, (int)((total + (total >= 0 ? 50 : -50)) / 100));
}

// Builds the stats of a shifted enemy. `levelsShifted` = how many levels it really moved (after the limits above).
// Per stat that the placement did not write itself ("stats" of the map unit are exact):
//   1. the growths of `levelsShifted` levels as their expected value, growth% x levels, rounded to the nearest (no dice:
//      the same game always builds the same unit, and the battle's random numbers are not used). The unit's growth is
//      Battle::growthOf (type + character + standing skills), and a stat cap cuts a rise as it does at a level up.
//   2. the flat addition ("stats"), 3. the percent for the stats of difficulties.percentStats. Never below 0; hit points never below 1.
void Battle::difficultyStats(Unit& u, const MapUnitDef& def, int levelsShifted) const {
  if (difficulty < 0 || !def.difficultyShift || !difficultyEnemySide(def.side)) return;
  const DifficultyRules& rules = db->difficulties;
  for (size_t s = 0; s < u.stats.size(); ++s) {
    if (s < def.statExact.size() && def.statExact[s]) continue;
    if (levelsShifted != 0) growStatByLevels(u, (int)s, levelsShifted);
    if (s < diffEnemy.stats.size() && diffEnemy.stats[s] != 0) u.stats[s] = std::max(0, u.stats[s] + diffEnemy.stats[s]);
    if (diffEnemy.statsPercent != 100 &&
        std::find(rules.percentStatIndex.begin(), rules.percentStatIndex.end(), (int)s) != rules.percentStatIndex.end())
      u.stats[s] = std::max(0, percentOf(u.stats[s], diffEnemy.statsPercent));
    if ((int)s == db->rules.statHp) u.stats[s] = std::max(1, u.stats[s]);
  }
}

int Battle::resourceGain(int side, int amount) const {
  if (difficulty < 0 || amount <= 0 || diffResourcePercent == 100 || side < 0 || side >= (int)sides.size() || sides[(size_t)side].ai)
    return amount;
  return percentOf(amount, diffResourcePercent);
}

int Battle::xpGain(int unit, int amount) const {
  if (difficulty < 0 || amount <= 0 || diffXpPercent == 100 || units[(size_t)unit].side >= (int)sides.size() ||
      sides[(size_t)units[(size_t)unit].side].ai)
    return amount;
  const int scaled = percentOf(amount, diffXpPercent);
  return diffXpPercent > 0 ? std::max(1, scaled) : 0;  // any share above 0 still gives at least 1 point
}

}  // namespace ote
