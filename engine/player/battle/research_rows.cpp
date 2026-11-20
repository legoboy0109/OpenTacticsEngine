// The research panel in its grouped form (docs/ECONOMY.md "Branching research"): a game whose technologies name a "tree" or a
// "tier" gets one list sorted by tree and tier, with a heading row where either changes. Every technology shows in its
// place with its state (done, under way, available, or greyed with the reason in its detail line).
#include <algorithm>

#include "battle/economy_text.h"
#include "ui/text.h"

namespace ote {

bool researchIsGrouped(const Database& db) {
  for (const TechnologyDef& t : db.research)
    if (!t.tree.empty() || t.tier > 0) return true;
  return false;
}

std::vector<PanelRow> groupedResearchRows(const Battle& b, int side, std::vector<int>& techOfRow) {
  const Database& db = *b.db;
  techOfRow.clear();
  // Trees in the order the file first names them; entries without a tree come last, under "Other".
  std::vector<std::string> trees;
  for (const TechnologyDef& t : db.research)
    if (!t.tree.empty() && std::find(trees.begin(), trees.end(), t.tree) == trees.end()) trees.push_back(t.tree);
  auto treeRank = [&](const TechnologyDef& t) {
    if (t.tree.empty()) return (int)trees.size();
    return (int)(std::find(trees.begin(), trees.end(), t.tree) - trees.begin());
  };
  std::vector<int> order;
  for (size_t t = 0; t < db.research.size(); ++t) order.push_back((int)t);
  std::stable_sort(order.begin(), order.end(), [&](int a, int c) {
    const TechnologyDef &x = db.research[(size_t)a], &y = db.research[(size_t)c];
    if (treeRank(x) != treeRank(y)) return treeRank(x) < treeRank(y);
    return x.tier < y.tier;
  });

  std::vector<ResearchOption> opts;
  b.researchOptions(side, opts);
  const int cur = b.researching(side);
  std::vector<PanelRow> rows;
  int lastTree = -2, lastTier = -2;
  for (int t : order) {
    const TechnologyDef& def = db.research[(size_t)t];
    const int rank = treeRank(def);
    if (rank != lastTree || def.tier != lastTier) {  // heading: "Military  tier 2"
      PanelRow h;
      h.header = true;
      h.label = (def.tree.empty() ? std::string(EconomyWords::kOtherTree) : def.tree) + (def.tier > 0 ? std::string("  ") + EconomyWords::kTier + intStr(def.tier) : std::string());
      rows.push_back(h);
      techOfRow.push_back(-1);
      lastTree = rank;
      lastTier = def.tier;
    }
    PanelRow r;
    r.label = def.name;
    if (t == cur) {
      r.extra = researchLeft(b, side);
      r.mark = true;
      r.detail = techDetail(db, t, b.canResearch(side) ? std::string(EconomyWords::kInProgress) : std::string(EconomyWords::kPausedWhy) + ". ");
    } else if (b.hasTechnology(side, t)) {
      r.extra = EconomyWords::kDone;
      r.dim = true;
      r.detail = techDetail(db, t, "");
    } else {
      r.extra = techPrice(db, t);
      for (const ResearchOption& o : opts)
        if (o.tech == t) {
          r.dim = !o.ok;
          r.detail = techDetail(db, t, o.ok ? "" : o.why + ". ");
        }
    }
    rows.push_back(r);
    techOfRow.push_back(t);
  }
  return rows;
}

}  // namespace ote
