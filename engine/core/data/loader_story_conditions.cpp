// Story commands and the story condition for status conditions (docs/SRPG.md section 9, docs/EVENTS.md "Status conditions").
// Kept apart from loader_story_commands.cpp so that file only gains a dispatch line each:
//   { "addCondition": "poison", "to": <unit filter>, "turns": 2 }      (turns optional; counted conditions only)
//   { "removeCondition": "poison" | "all", "from": <unit filter> }
//   { "hasCondition": "poison", "unit": <unit filter> }                (condition test; `unit` = any unit when left out)
// Map events only: conditions end with the map, so the campaign scene loader refuses all three (loader_campaign_scene.cpp).
#include "core/data/loader_events.h"

namespace ote {

namespace {

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

// Resolves a condition id of the game; says so in plain words when the game has no conditions at all.
bool findConditionId(const StoryLoader& l, const Database& db, const std::string& id, const char* key, const std::string& at, int& out) {
  out = db.findCondition(id);
  if (out >= 0) return true;
  return l.fail(at, quoted(key) + " names " +
                        (db.conditions.empty() ? "condition " + quoted(id) + " but the game has no conditions (data/conditions.json)"
                                               : "unknown condition " + quoted(id) + " (data/conditions.json)"));
}

}  // namespace

bool StoryLoader::conditionCommand(const Json& e, const std::string& at, bool add, Command& out) {
  const char* key = add ? "addCondition" : "removeCondition";
  const char* who = add ? "to" : "from";
  if (!onlyKeys(e, add ? std::initializer_list<const char*>{key, who, "turns"} : std::initializer_list<const char*>{key, who}, at))
    return false;
  const Json& v = e[key];
  if (!v.isString()) return fail(at, quoted(key) + (add ? " must be a condition id" : " must be a condition id or \"all\""));
  if (!add && v.asString() == "all") {
    out.on = true;  // every condition the unit has
  } else if (!findConditionId(*this, db_, v.asString(), key, at, out.index)) {
    return false;
  }
  if (add && e.has("turns")) {
    if (!c_.integerIn(e, "turns", at, 1, kMaxConditionTurns, out.value)) return false;
    if (db_.conditions[(size_t)out.index].until != ConditionUntil::Turns)
      return fail(at, quoted(v.asString()) + " does not count turns (it lasts until it is cured or the map ends), so \"turns\" means nothing");
  }
  if (!e.has(who))
    return fail(at, std::string("needs \"") + who + "\": which units " + (add ? "get" : "lose") + " it (a unit filter, {} for every unit)");
  return filter(e[who], at, add ? "\"to\"" : "\"from\"", out.filter);
}

bool StoryLoader::hasConditionCondition(const Json& e, const std::string& at, Condition& out) const {
  if (!onlyKeys(e, {"hasCondition", "unit"}, at)) return false;
  const Json& v = e["hasCondition"];
  if (!v.isString()) return fail(at, "\"hasCondition\" must be a condition id");
  if (!findConditionId(*this, db_, v.asString(), "hasCondition", at, out.index)) return false;
  out.filter = UnitFilter();
  return !e.has("unit") || filter(e["unit"], at, "\"unit\"", out.filter);
}

}  // namespace ote
