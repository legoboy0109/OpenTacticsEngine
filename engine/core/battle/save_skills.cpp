// Skills in saves (docs/SRPG.md "Skills": Saves and hash): a unit's learned skills and its "move again" /
// "act again" turn flags, and a roster entry's learned skills.
// STUB (step F1): no game has skills yet, so nothing is written and every key is refused. Step K1 fills this file.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitSkills(const Battle&, const Unit&, Json&) {}

bool readUnitSkills(Reader& rd, const Battle&, const Json& j, const std::string& where, Unit&) {
  for (const char* key : {"skills", "moveSpent", "acted", "refreshed"})
    if (j.has(key)) return rd.fail(where + "." + key, "this game has no skills");
  return true;
}

void writeRosterSkills(const Database&, const RosterUnit&, Json&) {}

bool readRosterSkills(Reader& rd, const Database&, const Json& j, const std::string& where, RosterUnit&) {
  if (j.has("skills")) return rd.fail(where + ".skills", "this game has no skills");
  return true;
}

}  // namespace savejson
}  // namespace ote
