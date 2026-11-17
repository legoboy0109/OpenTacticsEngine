// Loader of data/characters.json (docs/SRPG.md "Characters"): who a character is, and the checks of the lists
// that name skills and abilities. Loaded after unit types and items (it names them); the skill and ability ids are
// resolved last, in resolveCharacterRefs, once those lists exist.
#include "core/data/loader_content.h"
#include "core/data/loader_events.h"
#include "core/data/loader_ranks.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool knownKey(const std::string& k) {
  static const char* const keys[] = {"id",      "name",   "description", "type",      "level", "stats",
                                     "growths", "skills", "learns",      "abilities", "items",       "ranks", "capAdjust", "wields", "quotes"};
  for (const char* n : keys)
    if (k == n) return true;
  return false;
}

// "learns": [ { "level": 8, "skill": "heavy_blow" } ]; the skill id is resolved later.
bool readLearns(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, CharacterDef& ch) {
  const Json* list;
  if (!c.array(e, "learns", where, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& l = (*list)[i];
    const std::string at = "\"learns\" entry " + std::to_string(i + 1);
    if (!l.isObject()) return c.fail(where, at + " must be an object");
    LearnDef d;
    if (!c.integer(l, "level", where, d.level, true) || !c.str(l, "skill", where, d.skillId, true)) return false;
    if (d.level < 2 || d.level > db.rules.maxLevel)
      return c.fail(where, at + ": \"level\" must be from 2 to " + std::to_string(db.rules.maxLevel));
    for (const Json::Member& m : l.members())
      if (m.first != "level" && m.first != "skill") return c.fail(where, at + ": unknown key \"" + m.first + "\"");
    ch.learns.push_back(d);
  }
  return true;
}

bool readCharacter(const LoadCtx& c, const Database& db, const Json& e, size_t index, CharacterDef& ch) {
  std::string where = "characters[" + std::to_string(index) + "]";
  if (!e.isObject()) return c.fail(where, "must be an object");
  if (!c.str(e, "id", where, ch.id, true)) return false;
  if (!isStoryId(ch.id)) return c.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got \"" + ch.id + "\")");
  where = "character \"" + ch.id + "\"";
  for (const CharacterDef& o : db.characters)
    if (o.id == ch.id) return c.fail(where, "duplicate id");
  for (const Json::Member& m : e.members())
    if (!knownKey(m.first)) return c.fail(where, "unknown key \"" + m.first + "\"");

  ch.name = ch.id;
  std::string type;
  if (!c.str(e, "name", where, ch.name) || !c.str(e, "description", where, ch.description) ||
      !c.str(e, "type", where, type, true) || !c.integer(e, "level", where, ch.level))
    return false;
  ch.type = db.findUnitType(type);
  if (ch.type < 0) return c.fail(where, "unknown unit type \"" + type + "\"");
  if (ch.level < 1 || ch.level > db.rules.maxLevel)
    return c.fail(where, "\"level\" must be from 1 to " + std::to_string(db.rules.maxLevel));

  ch.stats.assign(db.stats.size(), 0);
  ch.growths.assign(db.stats.size(), 0);
  if (!c.statMap(db, e, "stats", where, ch.stats) || !c.statMap(db, e, "growths", where, ch.growths)) return false;
  if (!c.stringList(e, "skills", where, ch.skillIds) || !readLearns(c, db, e, where, ch) ||
      !c.stringList(e, "abilities", where, ch.abilityIds))
    return false;
  if (!readCharacterRanks(c, db, e, where, ch)) return false;  // "ranks", "capAdjust" (docs/SRPG.md section 7)
  if (!c.stringList(e, "wields", where, ch.wields)) return false;  // extra weapon groups, same shape as a unit type's "wields"
  ch.hasItems = e.has("items");
  return readItemGrants(c, db, e, "items", where, true, db.rules.items.inventorySize, ch.items);
}

}  // namespace

bool loadCharacters(const std::string& path, Database& db, std::string& err) {
  db.characters.clear();
  db.quotes = QuoteData();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "characters", "", list, true)) return false;
  if ((int)list->size() > kMaxCharacters)
    return c.fail("", "too many characters (at most " + std::to_string(kMaxCharacters) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    CharacterDef ch;
    if (!readCharacter(c, db, (*list)[i], i, ch)) return false;
    db.characters.push_back(ch);
  }
  return readCharacterQuotes(c, db, *list);  // docs/EVENTS.md "Battle and defeat quotes": needs every character
}

// Skill and ability ids become indices. The lists exist by now; in a game without the file every id is unknown
// and the message says where it should have been defined.
bool resolveCharacterRefs(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/characters.json");
  for (CharacterDef& ch : db.characters) {
    const std::string where = "character \"" + ch.id + "\"";
    ch.skills.clear();
    ch.abilities.clear();
    for (const std::string& id : ch.skillIds) {
      int s = db.findSkill(id);
      if (s < 0) return c.fail(where, "\"skills\" names unknown skill \"" + id + "\" (data/skills.json)");
      ch.skills.push_back(s);
    }
    for (size_t i = 0; i < ch.learns.size(); ++i) {
      LearnDef& l = ch.learns[i];
      l.skill = db.findSkill(l.skillId);
      if (l.skill < 0)
        return c.fail(where, "\"learns\" entry " + std::to_string(i + 1) + " names unknown skill \"" + l.skillId +
                                 "\" (data/skills.json)");
    }
    for (const std::string& id : ch.abilityIds) {
      int a = db.findAbility(id);
      if (a < 0) return c.fail(where, "\"abilities\" names unknown ability \"" + id + "\" (data/abilities.json)");
      ch.abilities.push_back(a);
    }
  }
  return true;
}

}  // namespace ote
