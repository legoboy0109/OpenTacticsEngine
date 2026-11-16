// Loader of data/supports.json, plus the story command addSupport, the condition support and the trigger
// support (docs/SRPG.md "Supports").
// STUB (step F1): a game without them loads as before; one that has them is refused. Step S1 fills this file.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

bool loadSupports(const std::string& path, Database& db, std::string& err) {
  db.supports = SupportData();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  LoadCtx c(err, path);
  return c.fail("", "data/supports.json is not supported by this build yet");
}

bool resolveSupportRefs(const std::string&, Database&, std::string&) { return true; }

bool StoryLoader::supportCommand(const Json&, const std::string& at, Command&) {
  return fail(at, "\"addSupport\" is not supported by this build yet");
}

bool StoryLoader::supportCondition(const Json&, const std::string& at, Condition&) const {
  return fail(at, "\"support\" is not supported by this build yet");
}

bool StoryLoader::supportTrigger(const Json&, const std::string& at, Trigger&) {
  return fail(at, "\"support\" is not supported by this build yet");
}

}  // namespace ote
