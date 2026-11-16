// Abilities and summons in saves (docs/SRPG.md "Menu abilities and summoning": Saves, events): the abilities a
// unit has used this battle and what makes a unit a summon.
// STUB (step F1): no game has abilities yet, so nothing is written and every key is refused. Step A1 fills this file.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitAbilities(const Battle&, const Unit&, Json&) {}

bool readUnitAbilities(Reader& rd, const Battle&, const Json& j, const std::string& where, Unit&) {
  for (const char* key : {"abilities", "summon"})
    if (j.has(key)) return rd.fail(where + "." + key, "this game has no abilities");
  return true;
}

bool checkUnitsAbilities(Reader&, const Battle&) { return true; }

}  // namespace savejson
}  // namespace ote
