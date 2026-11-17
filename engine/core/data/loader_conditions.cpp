// Conditions (docs/SRPG.md section 9): data/conditions.json and the keys elsewhere that name a condition.
#include "core/data/loader_conditions.h"

#include <algorithm>

#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// How many characters (UTF-8 code points) a text has.
size_t codePoints(const std::string& s) {
  size_t n = 0;
  for (unsigned char ch : s) n += (ch & 0xC0) != 0x80;
  return n;
}

// The first `n` characters of a text.
std::string firstChars(const std::string& s, size_t n) {
  size_t seen = 0, i = 0;
  for (; i < s.size(); ++i) {
    if (((unsigned char)s[i] & 0xC0) != 0x80 && seen++ == n) break;
  }
  return s.substr(0, i);
}

bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(where, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

// The hidden skill that holds blocks: never in a unit's skill list, never found by id.
int addHiddenSkill(Database& db, const std::string& id, const std::string& name, std::vector<SkillBlock> blocks) {
  SkillDef s;
  s.id = id;
  s.name = name;
  s.hidden = true;
  s.blocks = std::move(blocks);
  db.skills.push_back(s);
  return (int)db.skills.size() - 1;
}

bool effectOf(const ConditionDef&, const Database& db, const SkillDef& s, SkillEffect::Kind k) {
  (void)db;
  for (const SkillBlock& b : s.blocks)
    for (const SkillEffect& e : b.effects)
      if (e.kind == k) return true;
  return false;
}

}  // namespace

// ---- data/conditions.json ----

bool loadConditions(const std::string& path, Database& db, std::string& err) {
  db.conditions.clear();
  if (!fileExists(path)) return true;  // a game without the file: no conditions
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  for (const Json::Member& m : root.members())
    if (m.first != "conditions") return c.fail("", "unknown key " + q(m.first) + " (expected \"conditions\")");
  const Json* list;
  if (!c.array(root, "conditions", "", list, true)) return false;
  if ((int)list->size() > kMaxConditions) return c.fail("", "too many conditions (at most " + std::to_string(kMaxConditions) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "conditions[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    ConditionDef d;
    if (!c.str(e, "id", where, d.id, true)) return false;
    if (d.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = "condition " + q(d.id);
    for (const ConditionDef& o : db.conditions)
      if (o.id == d.id) return c.fail(where, "duplicate id");
    if (!onlyKeys(c, e, {"id", "name", "description", "tag", "turns", "until", "wearOff", "endsWhenStruck", "replaces", "blocks"}, where))
      return false;
    d.name = d.id;
    if (!c.str(e, "name", where, d.name) || !c.str(e, "description", where, d.description) || !c.str(e, "tag", where, d.tag)) return false;
    if (d.name.empty()) return c.fail(where, "\"name\" must not be empty");
    if (e.has("tag") && (d.tag.empty() || codePoints(d.tag) > 4))
      return c.fail(where, "\"tag\" is the short text shown on the map and must be 1 to 4 characters");
    if (d.tag.empty()) d.tag = firstChars(d.name, 2);

    // The duration: a number of the unit's own turns, or "until": "cured" / "mapEnd".
    if (e.has("turns") && e.has("until")) return c.fail(where, "has both \"turns\" and \"until\"; use one");
    if (!e.has("turns") && !e.has("until"))
      return c.fail(where, "needs a duration: \"turns\": 3 (the unit's own turns) or \"until\": \"cured\" or \"mapEnd\"");
    if (e.has("turns")) {
      if (!LoadCtx::isWhole(e["turns"]) || e["turns"].asInt() < 1 || e["turns"].asInt() > kMaxConditionTurns)
        return c.fail(where, "\"turns\" must be a whole number from 1 to " + std::to_string(kMaxConditionTurns));
      d.turns = e["turns"].asInt();
      d.until = ConditionUntil::Turns;
    } else {
      std::string until;
      if (!c.str(e, "until", where, until, true)) return false;
      if (until == "cured") d.until = ConditionUntil::Cured;
      else if (until == "mapEnd") d.until = ConditionUntil::MapEnd;
      else return c.fail(where, "\"until\" must be \"cured\" or \"mapEnd\", not " + q(until) + " (for a number of turns write \"turns\")");
    }
    if (e.has("wearOff")) {
      if (!LoadCtx::isWhole(e["wearOff"]) || e["wearOff"].asInt() < 1 || e["wearOff"].asInt() > 99)
        return c.fail(where, "\"wearOff\" is a percent chance each turn that it ends early and must be from 1 to 99 (leave it out for never)");
      d.wearOff = e["wearOff"].asInt();
    }
    if (!c.boolean(e, "endsWhenStruck", where, d.endsWhenStruck) || !c.stringList(e, "replaces", where, d.replaceIds)) return false;
    for (const std::string& r : d.replaceIds)
      if (r == d.id) return c.fail(where, "\"replaces\" lists the condition itself");

    const Json* blocks;
    if (!c.array(e, "blocks", where, blocks, true)) return false;
    if (blocks->size() == 0) return c.fail(where, "\"blocks\" needs at least one block");
    if ((int)blocks->size() > kMaxSkillBlocks) return c.fail(where, "too many blocks (at most " + std::to_string(kMaxSkillBlocks) + ")");
    std::vector<SkillBlock> read;
    for (size_t b = 0; b < blocks->size(); ++b) {
      SkillBlock block;
      if (!readSkillBlock(c, db, (*blocks)[b], where + ": block " + std::to_string(b + 1), "data/conditions.json: ", block, true))
        return false;
      read.push_back(block);
    }
    d.skill = addHiddenSkill(db, "condition:" + d.id, d.name, std::move(read));
    // Blocks that contradict each other (one condition cannot both stop a unit acting and make it act by itself).
    const SkillDef& s = db.skills[(size_t)d.skill];
    if (effectOf(d, db, s, SkillEffect::Kind::CannotAct) && effectOf(d, db, s, SkillEffect::Kind::Berserk))
      return c.fail(where, "\"cannotAct\" and \"berserk\" contradict each other (a unit that cannot act cannot act on its own either); make them two conditions");
    db.conditions.push_back(d);
  }
  return true;
}

// ---- the keys that name a condition ----

bool readInflictSpec(const LoadCtx& c, const Database& db, const Json& v, const std::string& where, const std::string& at,
                     bool allowChance, InflictSpec& out) {
  const std::string head = at.empty() ? "\"inflict\"" : at + ": \"inflict\"";
  if (!v.isObject() || !v.has("condition") || !v["condition"].isString())
    return c.fail(where, head + " must be { \"condition\": \"poison\" }" + (allowChance ? " with an optional \"chance\" and \"turns\"" : " with an optional \"turns\""));
  if (!onlyKeys(c, v, allowChance ? std::initializer_list<const char*>{"condition", "chance", "turns"}
                                  : std::initializer_list<const char*>{"condition", "turns"}, where + ": " + head))
    return false;
  out.condition = db.findCondition(v["condition"].asString());
  if (out.condition < 0) {
    if (db.conditions.empty()) return c.fail(where, head + " names condition " + q(v["condition"].asString()) + " but the game has no conditions (data/conditions.json)");
    return c.fail(where, head + " names unknown condition " + q(v["condition"].asString()) + " (data/conditions.json)");
  }
  if (v.has("turns")) {
    if (!LoadCtx::isWhole(v["turns"]) || v["turns"].asInt() < 1 || v["turns"].asInt() > kMaxConditionTurns)
      return c.fail(where, head + ": \"turns\" must be a whole number from 1 to " + std::to_string(kMaxConditionTurns));
    if (db.conditions[(size_t)out.condition].until != ConditionUntil::Turns)
      return c.fail(where, head + ": " + q(db.conditions[(size_t)out.condition].id) + " does not count turns (it lasts until it is cured or the map ends), so \"turns\" means nothing");
    out.turns = v["turns"].asInt();
  }
  if (v.has("chance")) out.chance = &v["chance"];
  return true;
}

bool readCureList(const LoadCtx& c, const Database& db, const Json& v, const std::string& where, const std::string& at,
                  std::vector<int>& ids, bool& all) {
  const std::string head = at.empty() ? "\"cure\"" : at + ": \"cure\"";
  if (v.isString()) {
    if (v.asString() != "all") return c.fail(where, head + " must be a list of condition ids or \"all\"");
    all = true;
    return true;
  }
  if (!v.isArray() || v.size() == 0) return c.fail(where, head + " must be a list of condition ids such as [\"poison\"], or \"all\"");
  for (size_t i = 0; i < v.size(); ++i) {
    if (!v[i].isString()) return c.fail(where, head + " entry " + std::to_string(i + 1) + " must be a condition id");
    const int id = db.findCondition(v[i].asString());
    if (id < 0) {
      if (db.conditions.empty()) return c.fail(where, head + " names condition " + q(v[i].asString()) + " but the game has no conditions (data/conditions.json)");
      return c.fail(where, head + " names unknown condition " + q(v[i].asString()) + " (data/conditions.json)");
    }
    if (std::find(ids.begin(), ids.end(), id) != ids.end()) return c.fail(where, head + " lists " + q(v[i].asString()) + " twice");
    ids.push_back(id);
  }
  return true;
}

bool readWeaponInflict(const LoadCtx& c, Database& db, const Json& e, const std::string& where, WeaponDef& w) {
  if (!e.has("inflict")) return true;
  InflictSpec spec;
  if (!readInflictSpec(c, db, e["inflict"], where, "", true, spec)) return false;
  SkillBlock block;
  block.when = SkillWhen::Strike;
  if (spec.chance) {
    const Json& ch = *spec.chance;
    if (ch.isString() && !ch.asString().empty()) {
      block.hasChance = true;
      block.chanceExpr = db.addExpr(ch.asString(), "data/weapons.json: " + where + ": \"inflict\": \"chance\"");
    } else if (LoadCtx::isWhole(ch) && ch.asInt() >= 1 && ch.asInt() <= 100) {
      block.hasChance = ch.asInt() < 100;  // 100 = every hit, no roll and nothing to show as a chance
      block.chance = ch.asInt();
    } else {
      return c.fail(where, "\"inflict\": \"chance\" must be a percent from 1 to 100 or a formula such as \"60 - b.res * 5\" (a = the wielder, b = the unit struck)");
    }
  }
  SkillEffect fx;
  fx.kind = SkillEffect::Kind::Inflict;
  fx.names.push_back(db.conditions[(size_t)spec.condition].id);
  fx.conds.push_back(spec.condition);
  fx.turns = spec.turns;
  block.effects.push_back(fx);
  w.skills.push_back(addHiddenSkill(db, "weapon:" + w.id, db.conditions[(size_t)spec.condition].name, {block}));
  return true;
}

bool readItemConditions(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ItemDef& it) {
  if (e.has("cure") && !readCureList(c, db, e["cure"], where, "", it.cures, it.cureAll)) return false;
  if (e.has("inflict")) {
    InflictSpec spec;
    if (!readInflictSpec(c, db, e["inflict"], where, "", false, spec)) return false;
    it.inflict = spec.condition;
    it.inflictTurns = spec.turns;
  }
  return true;
}

// ---- after everything is loaded ----

bool resolveBlockConditions(const LoadCtx& c, const Database& db, const std::string& where, SkillBlock& block) {
  if (block.aura && !block.auraMatch.empty()) {  // an aura's "match": a unit type id or tag, or a character tag
    bool found = false;
    for (const UnitTypeDef& t : db.unitTypes) found = found || unitTypeMatchesKey(t, block.auraMatch);
    for (const CharacterDef& ch : db.characters)
      found = found || std::find(ch.tags.begin(), ch.tags.end(), block.auraMatch) != ch.tags.end();
    if (!found) return c.fail(where, "\"aura\".\"match\" names no unit type id or tag \"" + block.auraMatch + "\"");
  }
  for (size_t i = 0; i < block.effects.size(); ++i) {
    SkillEffect& e = block.effects[i];
    if (e.kind != SkillEffect::Kind::Immune && e.kind != SkillEffect::Kind::Inflict && e.kind != SkillEffect::Kind::Cure) continue;
    e.conds.clear();
    const char* key = e.kind == SkillEffect::Kind::Immune ? "immune" : e.kind == SkillEffect::Kind::Cure ? "cure" : "inflict";
    for (const std::string& id : e.names) {
      const int k = db.findCondition(id);
      if (k < 0)
        return c.fail(where + ": effect " + std::to_string(i + 1),
                      q(key) + " names unknown condition " + q(id) + (db.conditions.empty() ? " (the game has no data/conditions.json)" : " (data/conditions.json)"));
      if (e.kind == SkillEffect::Kind::Inflict && e.turns > 0 && db.conditions[(size_t)k].until != ConditionUntil::Turns)
        return c.fail(where + ": effect " + std::to_string(i + 1),
                      q(id) + " does not count turns, so \"turns\" means nothing here");
      e.conds.push_back(k);
    }
  }
  return true;
}

bool resolveConditionRefs(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/conditions.json");
  // The groups a "forbid" may name: the weapon groups of weapons.json and of staves.
  std::vector<std::string> groups;
  for (const WeaponDef& w : db.weapons)
    if (!w.group.empty()) groups.push_back(w.group);
  for (const StaffDef& s : db.staves) groups.push_back(s.gear.group);
  for (ConditionDef& d : db.conditions) {
    const std::string where = "condition " + q(d.id);
    d.replaces.clear();
    for (const std::string& r : d.replaceIds) {
      const int k = db.findCondition(r);
      if (k < 0) return c.fail(where, "\"replaces\" names unknown condition " + q(r));
      d.replaces.push_back(k);
    }
    SkillDef& s = db.skills[(size_t)d.skill];
    for (size_t b = 0; b < s.blocks.size(); ++b) {
      SkillBlock& block = s.blocks[b];
      const std::string at = where + ": block " + std::to_string(b + 1);
      if (!resolveSkillConds(c, db, at, block.conds) || !resolveBlockConditions(c, db, at, block)) return false;
      for (size_t i = 0; i < block.effects.size(); ++i) {
        SkillEffect& e = block.effects[i];
        if (e.kind == SkillEffect::Kind::Ability) {
          e.ability = db.findAbility(e.abilityId);
          if (e.ability < 0) return c.fail(at + ": effect " + std::to_string(i + 1), "\"ability\" names unknown ability " + q(e.abilityId) + " (data/abilities.json)");
        }
        if (e.kind != SkillEffect::Kind::Forbid) continue;
        for (const std::string& g : e.names)
          if (std::find(groups.begin(), groups.end(), g) == groups.end())
            return c.fail(at + ": effect " + std::to_string(i + 1), "\"forbid\" names " + q(g) + ", which is no weapon or staff group of this game");
      }
    }
  }
  return true;
}

}  // namespace ote
