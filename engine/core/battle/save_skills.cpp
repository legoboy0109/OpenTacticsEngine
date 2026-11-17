// Skills in saves (docs/SRPG.md "Skills": Saves and hash): a unit's learned skills and its "move again" /
// "act again" turn flags, and a roster entry's learned skills. A key is written only when the game uses the feature
// and the value is not the default; a key in a game that does not use it is refused.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

namespace {

Json indexList(const std::vector<int>& v) {
  Json a = Json::makeArray();
  for (int i : v) a.push(Json::makeNumber(i));
  return a;
}

// "skills": a list of skill indices, each valid, none twice.
bool readSkillList(Reader& rd, const Database& db, const Json& j, const std::string& where, std::vector<int>& out) {
  out.clear();
  if (!j.has("skills")) return true;
  if (db.skills.empty()) return rd.fail(where + ".skills", "this game has no skills");
  const Json* list;
  if (!rd.array(j, "skills", where, -1, (int)db.skills.size(), list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& v = (*list)[i];
    const std::string at = where + ".skills[" + std::to_string(i) + "]";
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || v.asNumber() < 0 || v.asNumber() >= (double)db.skills.size())
      return rd.fail(at, "not a skill of this game (0.." + std::to_string(db.skills.size() - 1) + ")");
    const int s = (int)v.asNumber();
    if (db.skills[(size_t)s].hidden) return rd.fail(at, "not a skill of this game (it belongs to a condition)");
    for (int o : out)
      if (o == s) return rd.fail(at, "skill " + std::to_string(s) + " is listed twice");
    out.push_back(s);
  }
  return true;
}

}  // namespace

void writeUnitSkills(const Battle& b, const Unit& u, Json& j) {
  if (!u.learned.empty()) j.set("skills", indexList(u.learned));
  if (!b.db->turnFlagSkills) return;
  if (u.moveSpent > 0) j.set("moveSpent", Json::makeNumber(u.moveSpent));
  if (u.acted) j.set("acted", Json::makeBool(true));
  if (u.refreshed) j.set("refreshed", Json::makeBool(true));
}

bool readUnitSkills(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  const Database& db = *b.db;
  if (!readSkillList(rd, db, j, where, u.learned)) return false;
  u.moveSpent = 0;
  u.acted = u.refreshed = false;
  for (const char* key : {"moveSpent", "acted", "refreshed"})
    if (j.has(key)) {
      if (db.skills.empty()) return rd.fail(where + "." + key, "this game has no skills");
      if (!db.turnFlagSkills) return rd.fail(where + "." + key, "this game has no \"moveAgain\" or \"actAgain\" skills");
    }
  if (j.has("moveSpent") && !rd.integer(j, "moveSpent", where, 1, kMaxNumber, u.moveSpent)) return false;
  if (j.has("acted") && !rd.boolean(j, "acted", where, u.acted)) return false;
  if (j.has("refreshed") && !rd.boolean(j, "refreshed", where, u.refreshed)) return false;
  return true;
}

void writeRosterSkills(const Database&, const RosterUnit& r, Json& j) {
  if (!r.skills.empty()) j.set("skills", indexList(r.skills));
}

bool readRosterSkills(Reader& rd, const Database& db, const Json& j, const std::string& where, RosterUnit& r) {
  return readSkillList(rd, db, j, where, r.skills);
}

}  // namespace savejson
}  // namespace ote
