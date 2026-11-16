// Loader of data/characters.json (docs/SRPG.md "Characters").
// STUB (step F1): a game without the file loads as before; one that has it is refused. Step C1 fills this file.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

bool loadCharacters(const std::string& path, Database& db, std::string& err) {
  db.characters.clear();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  LoadCtx c(err, path);
  return c.fail("", "data/characters.json is not supported by this build yet");
}

bool resolveCharacterRefs(const std::string&, Database&, std::string&) { return true; }

}  // namespace ote
