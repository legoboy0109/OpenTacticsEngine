// Loader of data/skills.json and of the "skills" / "learns" keys on unit types, weapons and items, plus the
// story commands teachSkill / forgetSkill and the condition hasSkill (docs/SRPG.md "Skills"). The blocks inside a
// skill are read by loader_skill_blocks.cpp.
//
// Skills load before weapons, items and unit types, so those can name them and get indices at once; what a skill's
// own blocks name (weapon groups, terrain, unit types, abilities) is checked last, in resolveSkillRefs.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

// A list of skill ids on a unit type, weapon or item, as indices. `key` is "skills".
bool readSkillIds(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, std::vector<int>& out) {
  std::vector<std::string> ids;
  if (!c.stringList(e, "skills", where, ids)) return false;
  for (const std::string& id : ids) {
    const int s = db.findSkill(id);
    if (s < 0) return c.fail(where, "\"skills\" names unknown skill \"" + id + "\" (data/skills.json)");
    out.push_back(s);
  }
  return true;
}

}  // namespace

bool loadSkills(const std::string& path, Database& db, std::string& err) {
  db.skills.clear();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  for (const Json::Member& m : root.members())
    if (m.first != "skills") return c.fail("", "unknown key \"" + m.first + "\" (expected \"skills\")");
  const Json* list;
  if (!c.array(root, "skills", "", list, true)) return false;
  if ((int)list->size() > kMaxSkills) return c.fail("", "too many skills (at most " + std::to_string(kMaxSkills) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "skills[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    SkillDef s;
    if (!c.str(e, "id", where, s.id, true)) return false;
    if (s.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = "skill \"" + s.id + "\"";
    for (const SkillDef& o : db.skills)
      if (o.id == s.id) return c.fail(where, "duplicate id");
    for (const Json::Member& m : e.members())
      if (m.first != "id" && m.first != "name" && m.first != "description" && m.first != "blocks")
        return c.fail(where, "unknown key \"" + m.first + "\" (expected \"id\", \"name\", \"description\", \"blocks\")");
    s.name = s.id;
    const Json* blocks;
    if (!c.str(e, "name", where, s.name) || !c.str(e, "description", where, s.description) ||
        !c.array(e, "blocks", where, blocks, true))
      return false;
    if (blocks->size() == 0) return c.fail(where, "\"blocks\" needs at least one block");
    if ((int)blocks->size() > kMaxSkillBlocks)
      return c.fail(where, "too many blocks (at most " + std::to_string(kMaxSkillBlocks) + ")");
    for (size_t b = 0; b < blocks->size(); ++b) {
      SkillBlock block;
      if (!readSkillBlock(c, db, (*blocks)[b], where + ": block " + std::to_string(b + 1), "data/skills.json: ", block))
        return false;
      s.blocks.push_back(block);
    }
    db.skills.push_back(s);
  }
  return true;
}

// "learns": [ { "level": 5, "skill": "heavy_blow" } ]
static bool readLearnList(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, std::vector<LearnDef>& out) {
  const Json* list;
  if (!c.array(e, "learns", where, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& l = (*list)[i];
    const std::string at = "\"learns\" entry " + std::to_string(i + 1);
    if (!l.isObject()) return c.fail(where, at + " must be an object");
    LearnDef d;
    if (!c.integer(l, "level", where, d.level, true) || !c.str(l, "skill", where, d.skillId, true)) return false;
    for (const Json::Member& m : l.members())
      if (m.first != "level" && m.first != "skill") return c.fail(where, at + ": unknown key \"" + m.first + "\"");
    if (d.level < 2 || d.level > db.rules.maxLevel)
      return c.fail(where, at + ": \"level\" must be from 2 to " + std::to_string(db.rules.maxLevel));
    d.skill = db.findSkill(d.skillId);
    if (d.skill < 0) return c.fail(where, at + " names unknown skill \"" + d.skillId + "\" (data/skills.json)");
    out.push_back(d);
  }
  return true;
}

bool readUnitSkills(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u) {
  return readSkillIds(c, db, e, where, u.skills) && readLearnList(c, db, e, where, u.learns);
}

bool readWeaponSkills(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w) {
  return readSkillIds(c, db, e, where, w.skills);
}

bool readItemSkills(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ItemDef& it) {
  return readSkillIds(c, db, e, where, it.skills);  // the item loader has checked that the item is an accessory
}

// What the blocks name, once weapons, terrain, structures, unit types and abilities are all loaded.
bool resolveSkillRefs(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/skills.json");
  for (SkillDef& s : db.skills) {
    for (size_t b = 0; b < s.blocks.size(); ++b) {
      SkillBlock& block = s.blocks[b];
      const std::string where = "skill \"" + s.id + "\": block " + std::to_string(b + 1);
      if (!resolveSkillConds(c, db, where, block.conds)) return false;
      for (size_t i = 0; i < block.effects.size(); ++i) {
        SkillEffect& e = block.effects[i];
        if (e.kind != SkillEffect::Kind::Ability) continue;
        e.ability = db.findAbility(e.abilityId);
        if (e.ability < 0)
          return c.fail(where + ": effect " + std::to_string(i + 1),
                        "\"ability\" names unknown ability \"" + e.abilityId + "\" (data/abilities.json)");
      }
    }
  }
  return true;
}

// { "teachSkill": "heavy_blow", "to": <filter> } and { "forgetSkill": "heavy_blow", "from": <filter> }. The filter is
// required: a command that touches every unit by accident is worse than one that must be told whom.
bool StoryLoader::skillCommand(const Json& e, const std::string& at, bool teach, Command& out) {
  const char* key = teach ? "teachSkill" : "forgetSkill";
  const char* who = teach ? "to" : "from";
  if (!onlyKeys(e, {key, who}, at)) return false;
  const Json& v = e[key];
  if (!v.isString()) return fail(at, std::string("\"") + key + "\" must be a skill id");
  out.index = db_.findSkill(v.asString());
  if (out.index < 0) return fail(at, std::string("\"") + key + "\" names unknown skill \"" + v.asString() + "\" (data/skills.json)");
  if (!e.has(who)) return fail(at, std::string("needs \"") + who + "\": which units " + (teach ? "learn" : "forget") + " it (a unit filter, {} for every unit)");
  return filter(e[who], at, teach ? "\"to\"" : "\"from\"", out.filter);
}

// { "hasSkill": "heavy_blow", "unit": <filter> }: `unit` is any unit when left out.
bool StoryLoader::hasSkillCondition(const Json& e, const std::string& at, Condition& out) const {
  if (!onlyKeys(e, {"hasSkill", "unit"}, at)) return false;
  const Json& v = e["hasSkill"];
  if (!v.isString()) return fail(at, "\"hasSkill\" must be a skill id");
  out.index = db_.findSkill(v.asString());
  if (out.index < 0) return fail(at, "\"hasSkill\" names unknown skill \"" + v.asString() + "\" (data/skills.json)");
  out.filter = UnitFilter();
  return !e.has("unit") || filter(e["unit"], at, "\"unit\"", out.filter);
}

}  // namespace ote
