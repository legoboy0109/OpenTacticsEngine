// Loader of data/abilities.json and of the "abilities" key on unit types (docs/SRPG.md "Menu abilities and
// summoning").
// STUB (step F1): a game without them loads as before; one that has them is refused. Step A1 fills this file.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

bool loadAbilities(const std::string& path, Database& db, std::string& err) {
  db.abilities.clear();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  LoadCtx c(err, path);
  return c.fail("", "data/abilities.json is not supported by this build yet");
}

bool readUnitAbilities(const LoadCtx& c, const Database&, const Json& e, const std::string& where, UnitTypeDef&) {
  if (e.has("abilities")) return c.fail(where, "\"abilities\" is not supported by this build yet");
  return true;
}

bool resolveAbilityRefs(const std::string&, Database&, std::string&) { return true; }

}  // namespace ote
