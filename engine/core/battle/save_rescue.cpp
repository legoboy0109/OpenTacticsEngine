// Rescue and pair up in saves (docs/SRPG.md "Rescue and pair up": Saves, events): who carries whom.
// STUB (step F1): no game has rules.rescue yet, so nothing is written and the key is refused. Step R1 fills this
// file (the carried unit itself is saved as it is: not alive, no cell, its HP above 0).
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitRescue(const Battle&, const Unit&, Json&) {}

bool readUnitRescue(Reader& rd, const Battle&, const Json& j, const std::string& where, Unit&) {
  if (j.has("carrying")) return rd.fail(where + ".carrying", "this game has no rescue or pair up");
  return true;
}

bool checkUnitsRescue(Reader&, const Battle&) { return true; }

}  // namespace savejson
}  // namespace ote
