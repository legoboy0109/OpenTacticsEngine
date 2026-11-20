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
  // Strikes that differ from one another (docs/SRPG.md "One strike of a fight"): their average stands for each of them.
  const int strikes = f.each.empty() ? f.strikes : (int)f.each.size();
  const int damage = f.each.empty() ? f.damage : std::max(1, f.totalDamage() / strikes);
  int need = (hp + damage - 1) / damage;
  if (need > strikes) return 0;
  return std::pow(f.hit / 100.0, need);
}

// Expected damage of one side of an exchange, capped at the victim's hp.
double expectedDamage(const Database& db, const StrikeForecast& f, int hp) {
  if (!f.can) return 0;
  if (!f.each.empty()) {  // each strike with its own numbers
    double sum = 0;
    for (const StrikeNumbers& n : f.each) sum += n.damage * (n.hit / 100.0) * (1 + (db.rules.critMultiplier - 1) * n.crit / 100.0);
    return std::min<double>(hp, sum);
  }
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
  Props scratch;
  propFind(b.numbersAt(c, scratch), "defense", d);  // terrain + structure, as formulas see it
  return d;
}

// What striking `target` with `atk` is worth: its damage as a share of the unit's value plus the chance to kill it (a leader that ends the game
// counts more). `pKill` gets the kill chance. Shared by a single attack and each victim of an area attack.
static double strikeGain(const Battle& b, const Unit& target, const StrikeForecast& atk, double& pKill) {
  const Database& db = *b.db;
  const bool leaderRule = db.rules.defeatLeaderDead;
  double dealt = expectedDamage(db, atk, target.hp);
  double vTarget = typeValue(db, target.type);
  double hpFracT = target.hp / (double)std::max(1, b.maxHp(target));
  double vTargetNow = vTarget * (0.5 + 0.5 * hpFracT);

  double gain = dealt / std::max(1, b.maxHp(target)) * vTarget;
  pKill = killChance(atk, target.hp);
  if (target.leader) gain *= leaderRule ? 2.0 : kLeaderDamage;
  gain += pKill * kKillBonus * vTargetNow;
  if (target.leader && leaderRule) gain += pKill * kLeaderKill;
  return gain;
}

// What the answer of the struck unit costs `u`, scaled by the chance that the strike already killed it.
static double counterLoss(const Battle& b, const Unit& u, const StrikeForecast& counter, double pKill) {
  const Database& db = *b.db;
  const bool leaderRule = db.rules.defeatLeaderDead;
  double vSelf = typeValue(db, u.type);
  double hpFracU = u.hp / (double)std::max(1, b.maxHp(u));
  double cdealt = expectedDamage(db, counter, u.hp);
  double loss = cdealt / std::max(1, b.maxHp(u)) * vSelf * kCounterWeight;
  double pDie = killChance(counter, u.hp);
  loss += pDie * kKillBonus * vSelf * (0.5 + 0.5 * hpFracU);
  if (u.leader && leaderRule) loss += pDie * kLeaderKill;
  loss *= (1 - pKill);
  return loss;
}

double scoreAttack(const Battle& b, const Unit& u, const Unit& target, int weapon, Cell from) {
  Forecast f;
  if (!b.forecast(u.id, target.id, weapon, from, f)) return 0;
  const Database& db = *b.db;
  double dealt = expectedDamage(db, f.attack, target.hp);
  if (dealt <= 0) return 0;  // cannot hurt it: never worth moving for

  double pKill = 0;
  double gain = strikeGain(b, target, f.attack, pKill);
  // A dead target does not strike back.
  double loss = 0;
  if (f.counter.can) loss = counterLoss(b, u, f.counter, pKill);

  double score = gain - loss + kDefenseBonus * terrainDefense(b, from);
  return score;
}

// An area attack (docs/AI.md "Area attacks"): the worth of every victim the computer knows of added up, enemies positive and units of
// its own side hit by friendly fire negative at full worth, less the counter-attack of the unit on the aimed square.
double scoreAreaAttack(const Battle& b, const Unit& u, int weapon, Cell aim, Cell from) {
  AreaForecast af;
  if (!b.areaForecast(u.id, weapon, from, aim, af)) return 0;
  const Database& db = *b.db;
  double score = 0, primaryKill = 0;
  for (size_t i = 0; i < af.victims.size(); ++i) {
    const AreaVictim& v = af.victims[i];
    const Unit& target = b.units[(size_t)v.unit];
    if (!v.seen || expectedDamage(db, v.hit, target.hp) <= 0) continue;
    double pKill = 0;
    const double g = strikeGain(b, target, v.hit, pKill);
    score += v.enemy ? g : -g;
    if ((int)i == af.primary) primaryKill = pKill;
  }
  if (af.primary >= 0 && af.fight.counter.can) score -= counterLoss(b, u, af.fight.counter, primaryKill);
  return score + kDefenseBonus * terrainDefense(b, from);
}

double scoreCapture(const Battle& b, const Unit& u, Cell at) {
  const int si = b.structureIndexAt(at);
  const StructureState& st = b.structures[si];
  const StructureDef& t = knownDef(b, u.side, si);  // the kind the side believes it is (the truth while it sees it)
  double score = 1500;
  if (!t.income.empty()) score = 3000;
  if (!t.produces.empty()) score = 5000;
  if (hasTag(t.tags, "hq")) score = 8000;
  if (knownOwner(b, u.side, b.structureIndexAt(at)) >= 0) score += 500;  // taking it also hurts the enemy
  if (st.capturer == u.id) score += 4000;  // finish what was started (hq still ranks above the rest)
  return score;
}

}  // namespace ai
}  // namespace ote
