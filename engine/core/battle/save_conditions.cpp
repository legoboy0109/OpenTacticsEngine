// Conditions in saves (docs/SRPG.md section 9, docs/SAVES.md): units[i] "conditions": [ { "condition": index, "left": turns } ], oldest first.
// Written only for a unit that has some (so a game without conditions, and every unit without one, keeps its save bytes); `left` only for
// a counted condition. A key in a game without conditions is refused. Conditions are not carried between campaign maps (not built).
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitConditions(const Battle& b, const Unit& u, Json& j) {
  if (u.conditions.empty() || !b.exists(u)) return;
  Json a = Json::makeArray();
  for (const ConditionState& cs : u.conditions) {
    Json e = Json::makeObject();
    e.set("condition", Json::makeNumber(cs.condition));
    if (cs.left >= 0) e.set("left", Json::makeNumber(cs.left));
    a.push(e);
  }
  j.set("conditions", a);
}

bool readUnitConditions(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  u.conditions.clear();
  if (!j.has("conditions")) return true;
  const Database& db = *b.db;
  if (db.conditions.empty()) return rd.fail(where + ".conditions", "this game has no conditions");
  const Json* list;
  if (!rd.array(j, "conditions", where, -1, (int)db.conditions.size(), list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    const std::string at = where + ".conditions[" + std::to_string(i) + "]";
    if (!e.isObject()) return rd.fail(at, "not an object");
    ConditionState cs;
    if (!rd.integer(e, "condition", at, 0, (int)db.conditions.size() - 1, cs.condition)) return false;
    for (const ConditionState& o : u.conditions)
      if (o.condition == cs.condition) return rd.fail(at + ".condition", "condition " + std::to_string(cs.condition) + " is listed twice");
    const bool counted = db.conditions[(size_t)cs.condition].until == ConditionUntil::Turns;
    if (counted) {
      if (!rd.integer(e, "left", at, 0, kMaxConditionTurns, cs.left)) return false;
    } else if (e.has("left")) {
      return rd.fail(at + ".left", "this condition does not count turns");
    }
    u.conditions.push_back(cs);
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
