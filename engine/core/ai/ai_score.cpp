// Evaluation of attacks and captures. Scores are in "cost units": a unit's
// production cost where the game has one, else kDefaultValue. Everything
// here is a heuristic; keep the weights in one place so they are easy to
// tune from ote-sim runs.
#include <algorithm>
#include <cmath>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kDefaultValue = 1000;
constexpr double kCounterWeight = 0.7;  // own losses count less than enemy losses:
                                        // being aggressive wins games, trading evenly does not
constexpr double kKillBonus = 0.6;      // of the victim's value
constexpr double kLeaderDamage = 1.5;   // damage multiplier vs a leader
constexpr double kLeaderKill = 20000;   // killing a leader that ends the game
constexpr double kDefenseBonus = 8;     // per terrain defense point

// Rough probability that `f` kills a target with `hp` left. Exact when the
// forecast is deterministic (hit 100); otherwise assumes every needed
// strike must hit.
double killChance(const StrikeForecast& f, int hp) {
  if (!f.can || f.damage <= 0 || f.strikes <= 0) return 0;
  int need = (hp + f.damage - 1) / f.damage;
  if (need > f.strikes) return 0;
  return std::pow(f.hit / 100.0, need);
}

// Expected damage of one side of an exchange, capped at the victim's hp.
double expectedDamage(const Database& db, const StrikeForecast& f, int hp) {
  if (!f.can) return 0;
  double perStrike = f.damage * (f.hit / 100.0) * (1 + (db.rules.critMultiplier - 1) * f.crit / 100.0);
  return std::min<double>(hp, perStrike * f.strikes);
}

bool hasTag(const std::vector<std::string>& tags, const char* tag) {
  for (const std::string& t : tags)
    if (t == tag) return true;
  return false;
}

}  // namespace

double typeValue(const Database& db, int type) {
  double sum = 0;
  for (const Amount& a : db.unitTypes[type].cost) sum += a.amount;
  return sum > 0 ? sum : kDefaultValue;
}

double terrainDefense(const Battle& b, Cell c) {
  double d = 0;
  propFind(b.terrainAt(c).props, "defense", d);
  return d;
}

double scoreAttack(const Battle& b, const Unit& u, const Unit& target, int weapon, Cell from) {
  Forecast f;
  if (!b.forecast(u.id, target.id, weapon, from, f)) return 0;
  const Database& db = *b.db;
  double dealt = expectedDamage(db, f.attack, target.hp);
  if (dealt <= 0) return 0;  // cannot hurt it: never worth moving for

  bool leaderRule = db.rules.defeatLeaderDead;
  double vTarget = typeValue(db, target.type);
  double hpFracT = target.hp / (double)std::max(1, b.maxHp(target));
  double vTargetNow = vTarget * (0.5 + 0.5 * hpFracT);

  double gain = dealt / std::max(1, b.maxHp(target)) * vTarget;
  double pKill = killChance(f.attack, target.hp);
  if (target.leader) gain *= leaderRule ? 2.0 : kLeaderDamage;
  gain += pKill * kKillBonus * vTargetNow;
  if (target.leader && leaderRule) gain += pKill * kLeaderKill;

  // A dead target does not strike back.
  double loss = 0;
  if (f.counter.can) {
    double vSelf = typeValue(db, u.type);
    double hpFracU = u.hp / (double)std::max(1, b.maxHp(u));
    double cdealt = expectedDamage(db, f.counter, u.hp);
    loss = cdealt / std::max(1, b.maxHp(u)) * vSelf * kCounterWeight;
    double pDie = killChance(f.counter, u.hp);
    loss += pDie * kKillBonus * vSelf * (0.5 + 0.5 * hpFracU);
    if (u.leader && leaderRule) loss += pDie * kLeaderKill;
    loss *= (1 - pKill);
  }

  double score = gain - loss + kDefenseBonus * terrainDefense(b, from);
  return score;
}

double scoreCapture(const Battle& b, const Unit& u, Cell at) {
  const CellState& cs = b.cells[at];
  const TerrainDef& t = b.terrainAt(at);
  double score = 1500;
  if (!t.income.empty()) score = 3000;
  if (!t.produces.empty()) score = 5000;
  if (hasTag(t.tags, "hq")) score = 8000;
  if (cs.owner >= 0) score += 500;  // taking it also hurts the enemy
  if (cs.capturer == u.id) score += 4000;  // finish what was started (hq still ranks above the rest)
  return score;
}

}  // namespace ai
}  // namespace ote
