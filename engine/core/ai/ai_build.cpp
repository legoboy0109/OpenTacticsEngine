// Production: what to build, and where.
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr int kWantedCapturers = 3;  // modest: more infantry-like units than this are just weak fighters

int cost(const Database& db, int type) {
  int sum = 0;
  for (const Amount& a : db.unitTypes[type].cost) sum += a.amount;
  return sum;
}

// Mirrors the engine's targeting rule (Battle::versusValue) using only
// database data: can this weapon hurt a unit of that type at all?
bool weaponCanTarget(const WeaponDef& w, const UnitTypeDef& target) {
  if (w.versus.empty()) return true;
  double v;
  if (propFind(w.versus, target.id, v)) return v > 0;
  for (const std::string& tag : target.tags)
    if (propFind(w.versus, tag, v)) return v > 0;
  return w.hasVersusDefault && w.versusDefault > 0;
}

// True when the side could pay for two of this unit.
bool canAffordTwice(const Battle& b, int side, int type) {
  for (const Amount& c : b.db->unitTypes[type].cost)
    if (b.sides[side].resources[c.resource] < 2 * c.amount) return false;
  return true;
}

}  // namespace

bool chooseBuild(const Battle& b, Cell& outCell, int& outType) {
  const Database& db = *b.db;
  if (b.over || !db.rules.production) return false;
  int side = b.activeSide;

  // Situation: how many capturers do we have vs. how many things to capture,
  // and which enemy unit types are on the field (a weapon that can hit none
  // of them is a wasted purchase).
  int capturers = 0, targets = 0;
  std::vector<char> enemyType(db.unitTypes.size(), 0);
  for (const Unit& u : b.units) {
    if (!u.alive) continue;
    if (u.side == side) {
      if (db.unitTypes[u.type].canCapture) ++capturers;
    } else if (knownEnemy(b, side, u)) {
      enemyType[u.type] = 1;
    }
  }
  // Fog: with no enemy in sight it cannot tell what to expect, so every type counts (the same data a human has).
  if (b.fogOn()) {
    bool any = false;
    for (char t : enemyType) any = any || t != 0;
    if (!any) std::fill(enemyType.begin(), enemyType.end(), 1);
  }
  if (db.rules.capture) {
    for (size_t si = 0; si < b.structures.size(); ++si) {
      const StructureState& st = b.structures[si];
      int owner = knownOwner(b, side, (int)si);  // the owner it believes, not the true one
      if (b.defOf(st).capturePoints > 0 && (owner < 0 || b.sides[owner].team != b.sides[side].team)) ++targets;
    }
  }
  int wanted = std::min(kWantedCapturers, targets);

  std::vector<int> options;
  for (Cell c = 0; c < b.grid.cellCount(); ++c) {
    b.buildOptions(c, options);
    int bestCapturer = -1, bestFighter = -1;
    for (int t : options) {
      if (!b.canAfford(side, t)) continue;
      const UnitTypeDef& type = db.unitTypes[t];
      if (type.canCapture && (bestCapturer < 0 || cost(db, t) < cost(db, bestCapturer))) bestCapturer = t;
      bool useful = false;
      for (int w : type.weapons)
        for (size_t e = 0; e < enemyType.size() && !useful; ++e)
          if (enemyType[e] && weaponCanTarget(db.weapons[w], db.unitTypes[e])) useful = true;
      if (useful && (bestFighter < 0 || cost(db, t) > cost(db, bestFighter))) bestFighter = t;
    }
    int pick = -1;
    // Rich with few factories: a factory builds one unit per turn, so a cheap
    // capturer would waste the slot. Buy the strongest unit instead.
    bool rich = bestFighter >= 0 && canAffordTwice(b, side, bestFighter);
    if (capturers < wanted && bestCapturer >= 0 && !rich) pick = bestCapturer;
    else if (bestFighter >= 0) pick = bestFighter;
    if (pick >= 0) {
      outCell = c;
      outType = pick;
      return true;
    }
  }
  return false;
}

}  // namespace ai
}  // namespace ote
