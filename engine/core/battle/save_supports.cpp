// Supports in saves (docs/SRPG.md "Supports": Saves and hash): the closeness of the pairs, in a battle save and in
// a campaign's playthrough.
// STUB (step F1): no game has support pairs yet, so nothing is written and the key is refused. Step S1 fills this file.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeBattleSupports(const Battle&, Json&) {}

bool readBattleSupports(Reader& rd, const Json& saved, Battle&) {
  if (saved.has("supports")) return rd.fail("battle.supports", "this game has no supports");
  return true;
}

void writePlaythroughSupports(const Database&, const Playthrough&, Json&) {}

bool readPlaythroughSupports(Reader& rd, const Database&, const Json& saved, Playthrough&) {
  if (saved.has("supports")) return rd.fail("playthrough.supports", "this game has no supports");
  return true;
}

}  // namespace savejson
}  // namespace ote
