#include "core/data/loader_research_branch.h"

#include <algorithm>

namespace ote {

namespace {

const int kMaxTier = 99;

// { "skill": id, "filter": { "type": id } | { "tag": text } | {} }  or just "skill id".
// The story's unit filter format reduced to what makes sense without a map: no unit / side / team.
bool readGrant(const LoadCtx& c, const Database& db, const Json& g, const std::string& where, SkillGrant& out) {
  const std::string at = where + ": \"grants\".\"skills\"";
  std::string skillId;
  const Json* filter = nullptr;
  if (g.isString()) {
    skillId = g.asString();
  } else if (g.isObject()) {
    for (const Json::Member& m : g.members())
      if (m.first != "skill" && m.first != "filter")
        return c.fail(at, "unknown key \"" + m.first + "\" (expected \"skill\", \"filter\")");
    if (!c.str(g, "skill", at, skillId, true)) return false;
    if (g.has("filter")) filter = &g["filter"];
  } else {
    return c.fail(at, "each entry must be a skill id or { \"skill\": id, \"filter\": ... }");
  }
  out.skill = db.findSkill(skillId);
  if (out.skill < 0) return c.fail(at, "names unknown skill \"" + skillId + "\" (data/skills.json)");
  if (!filter) return true;
  if (!filter->isObject())
    return c.fail(at, "\"filter\" must be { \"type\": id }, { \"tag\": text } or {} for every unit");
  if (filter->size() == 0) return true;
  if (filter->size() > 1) return c.fail(at, "\"filter\" names more than one thing; use one of type, tag");
  const Json::Member& m = filter->members()[0];
  if (!m.second.isString()) return c.fail(at, "\"filter\".\"" + m.first + "\" must be text");
  if (m.first == "type") {
    out.filterType = db.findUnitType(m.second.asString());
    if (out.filterType < 0) return c.fail(at, "\"filter\".\"type\" names unknown unit type \"" + m.second.asString() + "\"");
  } else if (m.first == "tag") {
    out.filterTag = m.second.asString();
    bool used = false;
    for (const UnitTypeDef& u : db.unitTypes) used = used || std::find(u.tags.begin(), u.tags.end(), out.filterTag) != u.tags.end();
    if (!used) return c.fail(at, "\"filter\".\"tag\" \"" + out.filterTag + "\" is not a tag of any unit type");
  } else {
    return c.fail(at, "\"filter\" has unknown key \"" + m.first + "\" (side-wide skills can filter by \"type\" or \"tag\")");
  }
  return true;
}

}  // namespace

bool readBonusIncome(const LoadCtx& c, const Database& db, const Json& bonus, const std::string& where, TechnologyDef& t) {
  if (!bonus.has("income")) return true;
  if (!db.rules.economy) return c.fail(where, "\"bonus\".\"income\" needs \"rules.economy\" in game.json (nothing pays income without it)");
  if (!c.amounts(db, bonus, "income", where + ": \"bonus\"", t.bonusIncome)) return false;
  bool any = false;
  for (const Amount& a : t.bonusIncome) any = any || a.amount != 0;
  if (!any) return c.fail(where, "\"bonus\".\"income\" must change at least one resource");
  return true;
}

bool readBranchKeys(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, TechnologyDef& t) {
  if (!c.str(e, "tree", where, t.tree) || !c.str(e, "exclusiveGroup", where, t.exclusiveGroup)) return false;
  if (e.has("tree") && t.tree.empty()) return c.fail(where, "\"tree\" must not be empty text (leave the key out)");
  if (e.has("exclusiveGroup") && t.exclusiveGroup.empty())
    return c.fail(where, "\"exclusiveGroup\" must not be empty text (leave the key out)");
  if (!c.integerIn(e, "tier", where, 0, kMaxTier, t.tier)) return false;
  if (!c.stringList(e, "requiresStructures", where, t.requiresStructureIds)) return false;
  for (size_t i = 0; i < t.requiresStructureIds.size(); ++i)
    for (size_t j = 0; j < i; ++j)
      if (t.requiresStructureIds[i] == t.requiresStructureIds[j])
        return c.fail(where, "\"requiresStructures\" lists \"" + t.requiresStructureIds[i] + "\" twice");
  if (e.has("grants")) {
    const Json* g;
    if (!c.object(e, "grants", where, g)) return false;
    for (const Json::Member& m : g->members())
      if (m.first != "skills") return c.fail(where + ": \"grants\"", "unknown key \"" + m.first + "\" (expected \"skills\")");
    const Json* list;
    if (!c.array(*g, "skills", where + ": \"grants\"", list, true)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      SkillGrant sg;
      if (!readGrant(c, db, (*list)[i], where, sg)) return false;
      for (const SkillGrant& other : t.grants)
        if (other.skill == sg.skill && other.filterType == sg.filterType && other.filterTag == sg.filterTag)
          return c.fail(where, "\"grants\".\"skills\" lists \"" + db.skills[(size_t)sg.skill].id + "\" twice with the same filter");
      t.grants.push_back(sg);
    }
  }
  return true;
}

bool resolveResearchStructures(const std::string& path, Database& db, std::string& err) {
  LoadCtx c(err, path);
  for (TechnologyDef& t : db.research) {
    for (const std::string& id : t.requiresStructureIds) {
      int s = db.findStructure(id);
      if (s < 0) return c.fail("technology \"" + t.id + "\"", "\"requiresStructures\" names unknown structure \"" + id + "\" (data/structures.json)");
      t.requiresStructures.push_back(s);
    }
  }
  return true;
}

}  // namespace ote
