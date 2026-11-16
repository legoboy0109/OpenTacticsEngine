// Loader of data/skills.json and of the "skills" / "learns" keys on unit types, weapons and items, plus the
// story commands teachSkill / forgetSkill and the condition hasSkill (docs/SRPG.md "Skills").
// STUB (step F1): a game without them loads as before; one that has them is refused. Step K1 fills this file
// (and loader_skill_blocks.cpp).
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

bool loadSkills(const std::string& path, Database& db, std::string& err) {
  db.skills.clear();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  LoadCtx c(err, path);
  return c.fail("", "data/skills.json is not supported by this build yet");
}

bool readUnitSkills(const LoadCtx& c, const Database&, const Json& e, const std::string& where, UnitTypeDef&) {
  if (e.has("skills")) return c.fail(where, "\"skills\" is not supported by this build yet");
  if (e.has("learns")) return c.fail(where, "\"learns\" is not supported by this build yet");
  return true;
}

bool readWeaponSkills(const LoadCtx& c, const Database&, const Json& e, const std::string& where, WeaponDef&) {
  if (e.has("skills")) return c.fail(where, "\"skills\" is not supported by this build yet");
  return true;
}

bool readItemSkills(const LoadCtx& c, const Database&, const Json& e, const std::string& where, ItemDef&) {
  if (e.has("skills")) return c.fail(where, "\"skills\" is not supported by this build yet");
  return true;
}

bool resolveSkillRefs(const std::string&, Database&, std::string&) { return true; }

bool StoryLoader::skillCommand(const Json&, const std::string& at, bool teach, Command&) {
  return fail(at, "\"" + std::string(teach ? "teachSkill" : "forgetSkill") + "\" is not supported by this build yet");
}

bool StoryLoader::hasSkillCondition(const Json&, const std::string& at, Condition&) const {
  return fail(at, "\"hasSkill\" is not supported by this build yet");
}

}  // namespace ote
