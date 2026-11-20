// Branching research (docs/ECONOMY.md "Branching research") and the side-wide modifiers it feeds (docs/SRPG.md
// "Side-wide modifiers"): exclusive branches, structure requirements, cancelling, income bonuses and granted skills.
// Everything here is a no-op for a technology that uses none of the new keys.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

std::string Battle::researchBranchBlocker(int side, int tech) const {
  const TechnologyDef& t = db->research[(size_t)tech];
  // A branch already taken (started or finished) closes the rest of its group, for this side only.
  if (!t.exclusiveGroup.empty()) {
    const SideState& s = sides[(size_t)side];
    for (size_t o = 0; o < db->research.size(); ++o) {
      if ((int)o == tech || db->research[o].exclusiveGroup != t.exclusiveGroup) continue;
      if (hasTechnology(side, (int)o) || s.researching == (int)o) return "closed: you chose " + db->research[o].name;
    }
  }
  for (int kind : t.requiresStructures) {
    bool owned = false;
    for (const StructureState& st : structures) owned = owned || (st.owner == side && st.type == kind);
    if (!owned) return "needs a " + db->structures[(size_t)kind].name;
  }
  return "";
}

bool Battle::canCancelResearch(std::string* why) const {
  auto fail = [&](const char* reason) {
    if (why) *why = reason;
    return false;
  };
  if (over) return fail("the battle is over");
  if (activeSide < 0 || activeSide >= (int)sides.size()) return fail("no side is playing");
  if (sides[(size_t)activeSide].researching < 0) return fail("no research is under way");
  return true;
}

void Battle::cancelRefund(int side, std::vector<Amount>& out) const {
  out.clear();
  if (side < 0 || side >= (int)sides.size() || sides[(size_t)side].researching < 0) return;
  const int percent = db->rules.researchCancelRefundPercent;
  for (const Amount& a : db->research[(size_t)sides[(size_t)side].researching].cost) {
    Amount back = a;
    back.amount = a.amount > 0 ? (int)((long long)a.amount * percent / 100) : 0;  // rounded down; a negative cost is never "refunded"
    if (back.amount > 0) out.push_back(back);
  }
}

void Battle::applyCancelResearch(std::vector<Event>& events) {
  SideState& s = sides[(size_t)activeSide];
  std::vector<Amount> back;
  cancelRefund(activeSide, back);
  Event e;
  e.type = EventType::ResearchCancelled;
  e.a = activeSide;
  e.b = s.researching;
  e.c = s.researchLeft;
  events.push_back(e);
  s.researching = -1;
  s.researchLeft = 0;
  // The refund goes straight back (not through the difficulty's income share: it is the side's own money).
  for (const Amount& a : back) changeResource(activeSide, a.resource, a.amount, events);
}

void Battle::researchIncome(int side, std::vector<int>& perResource) const {
  if (side < 0 || side >= (int)sides.size()) return;
  const std::vector<uint8_t>& done = sides[(size_t)side].researched;
  for (size_t t = 0; t < done.size(); ++t)
    if (done[t])
      for (const Amount& a : db->research[t].bonusIncome) perResource[(size_t)a.resource] += a.amount;
}

bool Battle::modifierApplies(const SideModifier& m, const Unit& u) const {
  if (m.filterType >= 0 && u.type != m.filterType) return false;
  if (!m.filterTag.empty()) {
    const std::vector<std::string>& tags = typeOf(u).tags;
    if (std::find(tags.begin(), tags.end(), m.filterTag) == tags.end()) return false;
  }
  return true;
}

void Battle::addSideModifier(int side, const SideModifier& m) {
  if (side < 0 || side >= (int)sides.size()) return;
  for (const SideModifier& o : sides[(size_t)side].modifiers)
    if (o.skill == m.skill && o.filterType == m.filterType && o.filterTag == m.filterTag && o.tech == m.tech && o.grant == m.grant) return;
  sides[(size_t)side].modifiers.push_back(m);
}

void Battle::applyTechnologyEffects(int side, int tech) {
  const std::vector<SkillGrant>& grants = db->research[(size_t)tech].grants;
  for (size_t g = 0; g < grants.size(); ++g) {
    SideModifier m;
    m.skill = grants[g].skill;
    m.filterType = grants[g].filterType;
    m.filterTag = grants[g].filterTag;
    m.tech = tech;
    m.grant = (int)g;
    addSideModifier(side, m);
  }
}

}  // namespace ote
