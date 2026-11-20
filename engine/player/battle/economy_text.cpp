#include "battle/economy_text.h"

#include <algorithm>

#include "battle/item_text.h"
#include "ui/text.h"

namespace ote {

std::string turnsText(int n) { return intStr(n) + (n == 1 ? " turn" : " turns"); }

std::string techPrice(const Database& db, int tech) {
  const TechnologyDef& t = db.research[(size_t)tech];
  std::string cost = priceText(db, t.cost);
  return (cost.empty() ? std::string(EconomyWords::kFree) : cost) + ", " +
         (t.turns == 0 ? std::string(EconomyWords::kInstant) : turnsText(t.turns));
}

namespace {

std::string joinNames(const Database& db, const std::vector<int>& types) {
  std::string out;
  for (int t : types) out += (out.empty() ? "" : ", ") + db.unitTypes[(size_t)t].name;
  return out;
}

}  // namespace

std::string techGives(const Database& db, int tech) {
  const TechnologyDef& t = db.research[(size_t)tech];
  std::string out;
  auto add = [&](const std::string& s) { out += (out.empty() ? "" : "; ") + s; };
  if (!t.unlocksUnits.empty()) add(EconomyWords::kUnlocks + joinNames(db, t.unlocksUnits));
  // Upgrades name their technology themselves ("requires"): list the kinds they lead to.
  std::string kinds;
  std::vector<int> seen;
  for (const StructureDef& s : db.structures)
    for (const UpgradeDef& u : s.upgrades)
      if (u.needs == tech && std::find(seen.begin(), seen.end(), u.to) == seen.end()) {
        seen.push_back(u.to);
        kinds += (kinds.empty() ? "" : ", ") + db.structures[(size_t)u.to].name;
      }
  if (!kinds.empty()) add(EconomyWords::kAllows + kinds);
  std::string gains = gainsText(db, t.bonus);
  if (!gains.empty())
    add(gains + (t.bonusTypes.empty() ? std::string(EconomyWords::kForAll) : EconomyWords::kFor + joinNames(db, t.bonusTypes)));
  if (!t.bonusIncome.empty()) add("+" + priceText(db, t.bonusIncome) + EconomyWords::kPerTurn);
  for (const SkillGrant& g : t.grants) {
    std::string who = g.filterType >= 0 ? EconomyWords::kFor + db.unitTypes[(size_t)g.filterType].name
                                        : !g.filterTag.empty() ? EconomyWords::kFor + g.filterTag : std::string(EconomyWords::kForAll);
    add(EconomyWords::kSkill + db.skills[(size_t)g.skill].name + who);
  }
  return out;
}

// The panel's detail lines: what is special about the row (a reason, "in progress"), the description, then what
// the technology gives, then the other branches it closes.
std::string techDetail(const Database& db, int tech, const std::string& lead) {
  const TechnologyDef& t = db.research[(size_t)tech];
  std::string out = lead;
  auto add = [&](const std::string& s) {
    if (s.empty()) return;
    out += (out.empty() || out.back() == ' ' ? "" : " ") + s;
  };
  add(t.description);
  add(techGives(db, tech));
  if (!t.exclusiveGroup.empty()) {
    std::string others;
    for (size_t o = 0; o < db.research.size(); ++o)
      if ((int)o != tech && db.research[o].exclusiveGroup == t.exclusiveGroup) others += (others.empty() ? "" : ", ") + db.research[o].name;
    if (!others.empty()) add(EconomyWords::kCloses + others + ".");
  }
  return out;
}

std::string researchName(const Battle& b, int side) {
  int t = b.researching(side);
  return t < 0 ? "" : b.db->research[(size_t)t].name;
}

std::string researchLeft(const Battle& b, int side) {
  if (b.researching(side) < 0) return "";
  return b.canResearch(side) ? turnsText(b.researchTurnsLeft(side)) + " left" : EconomyWords::kPaused;
}

std::vector<PanelRow> researchRows(const Battle& b, int side, std::vector<int>& techOfRow) {
  if (researchIsGrouped(*b.db)) return groupedResearchRows(b, side, techOfRow);  // research_rows.cpp
  const Database& db = *b.db;
  std::vector<PanelRow> rows;
  techOfRow.clear();
  int cur = b.researching(side);
  if (cur >= 0) {
    PanelRow r;
    r.label = db.research[(size_t)cur].name;
    r.extra = researchLeft(b, side);
    r.mark = true;  // the small square: this one is under way
    r.detail = techDetail(db, cur, b.canResearch(side) ? std::string(EconomyWords::kInProgress) : std::string(EconomyWords::kPausedWhy) + ". ");
    rows.push_back(r);
    techOfRow.push_back(cur);
  }
  std::vector<ResearchOption> opts;
  b.researchOptions(side, opts);
  for (const ResearchOption& o : opts) {
    PanelRow r;
    r.label = db.research[(size_t)o.tech].name;
    r.extra = techPrice(db, o.tech);
    r.dim = !o.ok;
    r.detail = techDetail(db, o.tech, o.ok ? "" : o.why + ". ");
    rows.push_back(r);
    techOfRow.push_back(o.tech);
  }
  for (size_t t = 0; t < db.research.size(); ++t) {
    if (!b.hasTechnology(side, (int)t)) continue;
    PanelRow r;
    r.label = db.research[t].name;
    r.extra = EconomyWords::kDone;
    r.dim = true;
    r.detail = techDetail(db, (int)t, "");
    rows.push_back(r);
    techOfRow.push_back((int)t);
  }
  return rows;
}

std::string fuelValue(const Battle& b, const Unit& u) {
  int mx = b.maxFuel(u.id);
  return mx < 0 ? "" : intStr(b.fuel(u.id)) + "/" + intStr(mx);
}

bool fuelLow(const Battle& b, const Unit& u) {
  int mx = b.maxFuel(u.id);
  return mx > 0 && b.fuel(u.id) * 4 <= mx;
}

std::vector<AmmoLine> ammoLines(const Battle& b, const Unit& u) {
  std::vector<AmmoLine> out;
  for (int w : b.typeOf(u).weapons) {
    int mx = b.maxAmmo(u.id, w);
    if (mx < 0) continue;  // unlimited
    AmmoLine l;
    l.name = b.db->weapons[(size_t)w].name;
    int left = b.ammo(u.id, w);
    l.empty = left <= 0;
    l.value = intStr(left) + "/" + intStr(mx);
    out.push_back(l);
  }
  return out;
}

std::string supplyLogLine(const Battle& b, const Unit& u) {
  std::string out;
  std::string fuel = fuelValue(b, u);
  if (!fuel.empty()) out += std::string(EconomyWords::kFuel) + " " + fuel;
  for (const AmmoLine& l : ammoLines(b, u))
    out += (out.empty() ? "" : ", ") + l.name + " " + l.value + (l.empty ? std::string(" ") + EconomyWords::kEmpty : "");
  return out.empty() ? "" : u.name + ": " + out;
}

std::string rowLogText(const PanelRow& r) {
  return r.label + (r.extra.empty() ? "" : " (" + r.extra + ")") + (r.dim ? " [greyed]" : "");
}

}  // namespace ote
