// Loader of the "promotions" key on unit types and "promotes" on items, plus the story command promote and the
// trigger promoted (docs/SRPG.md "Promotion"). rules.promotion is read in loader_srpg_rules.cpp.
// STUB (step F1): a game without them loads as before; one that has them is refused. Step P1 fills this file.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

bool readUnitPromotions(const LoadCtx& c, const Database&, const Json& e, const std::string& where, UnitTypeDef&) {
  if (e.has("promotions")) return c.fail(where, "\"promotions\" is not supported by this build yet");
  return true;
}

bool readItemPromotes(const LoadCtx& c, const Json& e, const std::string& where, ItemDef&) {
  if (e.has("promotes")) return c.fail(where, "\"promotes\" is not supported by this build yet");
  return true;
}

bool resolvePromotionRefs(const std::string&, Database&, std::string&) { return true; }

bool StoryLoader::promoteCommand(const Json&, const std::string& at, Command&) {
  return fail(at, "\"promote\" is not supported by this build yet");
}

bool StoryLoader::promotedTrigger(const Json&, const std::string& at, Trigger&) {
  return fail(at, "\"promoted\" is not supported by this build yet");
}

}  // namespace ote
