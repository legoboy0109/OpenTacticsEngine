// Upgradable structures (docs/ECONOMY.md): a structure becomes another kind of the same size for a
// cost. Its kind is StructureState::type, so every rule that asks "what is this structure" (income,
// production, capture points, movement cost, numbers, tags, shops) follows the change by itself.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

bool fail(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

}  // namespace

void Battle::pay(int side, const std::vector<Amount>& cost, std::vector<Event>& events) {
  for (const Amount& c : cost) changeResource(side, c.resource, -c.amount, events);
}

// Empty = the upgrade can be done now. Shared by the menu query and the action check so they never disagree.
static std::string upgradeBlocker(const Battle& b, int structure, const UpgradeDef& up) {
  const Database& db = *b.db;
  const int side = b.activeSide;
  if (up.needs >= 0 && !b.hasTechnology(side, up.needs)) return "needs " + db.research[(size_t)up.needs].name;
  std::vector<Cell> cells;
  b.structureCells(structure, cells);
  for (Cell c : cells) {
    int occ = b.occupant[(size_t)c];
    if (occ >= 0 && b.teamOf(b.units[(size_t)occ]) != b.sides[(size_t)side].team) return "an enemy unit stands on it";
  }
  for (const Amount& a : up.cost)
    if (b.sides[(size_t)side].resources[(size_t)a.resource] < a.amount) return "not enough " + db.resources[(size_t)a.resource].name;
  return "";
}

void Battle::upgradeOptions(int structure, std::vector<UpgradeOption>& out) const {
  out.clear();
  if (over || structure < 0 || structure >= (int)structures.size() || structures[(size_t)structure].owner != activeSide) return;
  for (const UpgradeDef& up : defOf(structures[(size_t)structure]).upgrades) {
    UpgradeOption o;
    o.to = up.to;
    o.def = &up;
    o.why = upgradeBlocker(*this, structure, up);
    o.ok = o.why.empty();
    out.push_back(o);
  }
}

bool Battle::validateUpgrade(const Action& a, std::string* why) const {
  if (a.structure < 0 || a.structure >= (int)structures.size()) return fail(why, "no such structure");
  const StructureState& st = structures[(size_t)a.structure];
  if (st.owner != activeSide) return fail(why, "you do not own that structure");
  for (const UpgradeDef& up : defOf(st).upgrades) {
    if (up.to != a.to) continue;
    std::string reason = upgradeBlocker(*this, a.structure, up);
    return reason.empty() ? true : fail(why, reason);
  }
  return fail(why, "that structure cannot be upgraded to that");
}

void Battle::applyUpgrade(const Action& a, std::vector<Event>& events) {
  StructureState& st = structures[(size_t)a.structure];
  for (const UpgradeDef& up : defOf(st).upgrades) {
    if (up.to != a.to) continue;
    pay(activeSide, up.cost, events);
    break;
  }
  const int from = st.type;
  st.type = a.to;
  // Same place, same owner; the capture starts over for the new kind (nobody is capturing: only the owner's
  // own units may stand here, which was checked).
  st.capture = defOf(st).capturePoints;
  st.capturer = -1;
  Event e;
  e.type = EventType::StructureUpgraded;
  e.a = a.structure;
  e.b = from;
  e.c = a.to;
  events.push_back(e);
}

}  // namespace ote
