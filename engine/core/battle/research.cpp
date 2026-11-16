// Research (docs/ECONOMY.md): pay and wait, one technology at a time per side. A finished technology
// unlocks unit types (checked in the build rules) and adds stat bonuses (Battle::stat, unit_stats.h).
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

bool fail(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

}  // namespace

bool Battle::hasTechnology(int side, int tech) const {
  if (side < 0 || side >= (int)sides.size() || tech < 0) return false;
  const std::vector<uint8_t>& done = sides[(size_t)side].researched;
  return (size_t)tech < done.size() && done[(size_t)tech] != 0;
}

bool Battle::canResearch(int side) const {
  // By default research needs no building ("rules.research.needsStructure" off): any side of a game with technologies.
  if (!db->rules.researchNeedsStructure) return !db->research.empty();
  for (const StructureState& st : structures)
    if (st.owner == side && defOf(st).research) return true;
  return false;
}

void Battle::unlockedBy(int unitType, std::vector<int>& techs) const {
  techs.clear();
  for (size_t t = 0; t < db->research.size(); ++t) {
    const std::vector<int>& list = db->research[t].unlocksUnits;
    if (std::find(list.begin(), list.end(), unitType) != list.end()) techs.push_back((int)t);
  }
}

bool Battle::unitUnlocked(int side, int unitType) const {
  bool listed = false;
  for (size_t t = 0; t < db->research.size(); ++t) {
    const std::vector<int>& list = db->research[t].unlocksUnits;
    if (std::find(list.begin(), list.end(), unitType) == list.end()) continue;
    if (hasTechnology(side, (int)t)) return true;  // any one technology is enough
    listed = true;
  }
  return !listed;
}

// Empty = `side` could start `tech` now. Shared by the menu query and the action check.
static std::string researchBlocker(const Battle& b, int side, int tech) {
  const Database& db = *b.db;
  const TechnologyDef& t = db.research[(size_t)tech];
  if (b.over) return "the battle is over";
  if (side != b.activeSide) return "it is not that side's turn";
  const SideState& s = b.sides[(size_t)side];
  if (s.researching >= 0) return "already researching " + db.research[(size_t)s.researching].name;
  if (!b.canResearch(side)) return "needs a structure that can research";
  std::string missing;
  for (int r : t.needs)
    if (!b.hasTechnology(side, r)) missing += (missing.empty() ? "" : ", ") + db.research[(size_t)r].name;
  if (!missing.empty()) return "needs " + missing;
  for (const Amount& a : t.cost)
    if (s.resources[(size_t)a.resource] < a.amount) return "not enough " + db.resources[(size_t)a.resource].name;
  return "";
}

void Battle::researchOptions(int side, std::vector<ResearchOption>& out) const {
  out.clear();
  if (side < 0 || side >= (int)sides.size()) return;
  for (size_t t = 0; t < db->research.size(); ++t) {
    if (hasTechnology(side, (int)t) || sides[(size_t)side].researching == (int)t) continue;
    ResearchOption o;
    o.tech = (int)t;
    o.why = researchBlocker(*this, side, (int)t);
    o.ok = o.why.empty();
    out.push_back(o);
  }
}

bool Battle::validateResearch(const Action& a, std::string* why) const {
  if (a.tech < 0 || a.tech >= (int)db->research.size()) return fail(why, "no such technology");
  if (hasTechnology(activeSide, a.tech)) return fail(why, "already researched");
  std::string reason = researchBlocker(*this, activeSide, a.tech);
  return reason.empty() ? true : fail(why, reason);
}

void Battle::applyResearch(const Action& a, std::vector<Event>& events) {
  const TechnologyDef& t = db->research[(size_t)a.tech];
  pay(activeSide, t.cost, events);
  Event e;
  e.type = EventType::ResearchStarted;
  e.a = activeSide;
  e.b = a.tech;
  e.c = t.turns;
  events.push_back(e);
  if (t.turns == 0) {
    finishResearch(activeSide, a.tech, events);
    return;
  }
  sides[(size_t)activeSide].researching = a.tech;
  sides[(size_t)activeSide].researchLeft = t.turns;
}

void Battle::finishResearch(int side, int tech, std::vector<Event>& events) {
  SideState& s = sides[(size_t)side];
  if (s.researched[(size_t)tech]) return;
  s.researched[(size_t)tech] = 1;
  if (s.researching == tech) {
    s.researching = -1;
    s.researchLeft = 0;
  }
  // A bonus may be negative: keep hit points within the new maximum, as an accessory does.
  for (Unit& u : units)
    if (exists(u) && u.side == side) clampHp(u);  // R1: a carried unit keeps a valid hp too
  Event e;
  e.type = EventType::ResearchFinished;
  e.a = side;
  e.b = tech;
  events.push_back(e);
}

void Battle::researchTurn(std::vector<Event>& events) {
  SideState& s = sides[(size_t)activeSide];
  // With needsStructure and no research structure the countdown waits; the progress is kept.
  if (s.researching < 0 || !canResearch(activeSide)) return;
  if (--s.researchLeft > 0) return;
  finishResearch(activeSide, s.researching, events);
}

void Battle::setStartingResearch(int side, const std::vector<int>& techs) {
  if (side < 0 || side >= (int)sides.size()) return;
  for (int t : techs)
    if (t >= 0 && t < (int)db->research.size()) sides[(size_t)side].researched[(size_t)t] = 1;
  for (Unit& u : units)
    if (exists(u) && u.side == side) u.hp = maxHp(u);  // nothing has happened yet: everybody is at full strength
}

}  // namespace ote
