// Spending on research and structure upgrades (docs/ECONOMY.md "AI"). Simple on purpose:
//  * research: the cheapest technology it could start now, when paying for it still leaves enough to
//    build something it could build today (so research never starves production);
//  * upgrades: only with money to spare once building is done: more than twice the cost left.
// Deterministic: the lowest index wins every tie.
#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

int totalOf(const std::vector<Amount>& cost) {
  int sum = 0;
  for (const Amount& a : cost) sum += a.amount;
  return sum;
}

bool affordsAfter(const Battle& b, int side, const std::vector<Amount>& pay, const std::vector<Amount>& want) {
  for (const Amount& w : want) {
    int have = b.sides[(size_t)side].resources[(size_t)w.resource];
    for (const Amount& p : pay)
      if (p.resource == w.resource) have -= p.amount;
    if (have < w.amount) return false;
  }
  return true;
}

// Could the side, after paying `pay`, still build any unit it can build on a free production square?
// A side with nothing to build (no free producing structure) is not held back.
bool stillBuilds(const Battle& b, int side, const std::vector<Amount>& pay) {
  bool anySlot = false;
  std::vector<int> options;
  for (Cell c = 0; c < b.grid.cellCount(); ++c) {
    b.buildOptions(c, options);
    for (int t : options) {
      anySlot = true;
      if (affordsAfter(b, side, pay, b.db->unitTypes[(size_t)t].cost)) return true;
    }
  }
  return !anySlot;
}

}  // namespace

bool chooseResearch(const Battle& b, int& tech) {
  if (b.over) return false;
  const int side = b.activeSide;
  std::vector<ResearchOption> options;
  b.researchOptions(side, options);
  int best = -1;
  for (const ResearchOption& o : options) {
    if (!o.ok) continue;
    const TechnologyDef& t = b.db->research[(size_t)o.tech];
    if (!stillBuilds(b, side, t.cost)) continue;
    if (best < 0 || totalOf(t.cost) < totalOf(b.db->research[(size_t)best].cost)) best = o.tech;  // strict: ties keep the lowest index
  }
  tech = best;
  return best >= 0;
}

bool chooseUpgrade(const Battle& b, int& structure, int& to) {
  if (b.over) return false;
  const int side = b.activeSide;
  std::vector<UpgradeOption> options;
  for (size_t s = 0; s < b.structures.size(); ++s) {
    b.upgradeOptions((int)s, options);
    for (const UpgradeOption& o : options) {
      // A free upgrade could go back and forth forever within one turn; only paid ones count.
      if (!o.ok || totalOf(o.def->cost) <= 0) continue;
      bool rich = true;
      for (const Amount& a : o.def->cost)
        if (a.amount > 0 && b.sides[(size_t)side].resources[(size_t)a.resource] <= 2 * a.amount) rich = false;
      if (!rich) continue;
      structure = (int)s;
      to = o.to;
      return true;
    }
  }
  return false;
}

}  // namespace ai
}  // namespace ote
