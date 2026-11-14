// Event loader, part 3: objectives (docs/EVENTS.md "Objectives"). Used for the map's "objectives"
// and for the `objective` command, which replaces them.
#include "core/data/loader_events.h"

namespace ote {

bool StoryLoader::rules(const Json& list, const char* key, const std::string& where, bool win,
                        std::vector<ObjectiveRule>& out) const {
  if (!list.isArray()) return fail(where, std::string("\"") + key + "\" must be a list of conditions");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    std::string at = where + ": \"" + key + "\" entry " + std::to_string(i + 1);
    if (!e.isObject()) return fail(at, "must be an object such as { \"defeated\": { \"unit\": \"gorm\" } }");
    static const char* const kKinds[] = {"defeated", "reach", "round", "survive"};
    const char* kind = nullptr;
    for (const char* k : kKinds)
      if (e.has(k)) {
        if (kind) return fail(at, std::string("has two kinds (") + kind + " and " + k + "); use one entry for each");
        kind = k;
      }
    if (!kind) return fail(at, "is not an objective; it needs one of defeated, reach, round, survive");
    std::string k = kind;
    at += " (" + k + ")";
    ObjectiveRule r;
    if (k == "defeated") {
      r.kind = ObjectiveRule::Kind::Defeated;
      if (!onlyKeys(e, {"defeated"}, at) || !filter(e["defeated"], at, "\"defeated\"", r.filter)) return false;
    } else if (k == "reach") {
      r.kind = ObjectiveRule::Kind::Reach;
      if (!onlyKeys(e, {"reach", "by"}, at) || !rect(e["reach"], at, r.rect)) return false;
      r.hasBy = e.has("by");
      if (r.hasBy && !filter(e["by"], at, "\"by\"", r.filter)) return false;
    } else {
      r.kind = k == "round" ? ObjectiveRule::Kind::Round : ObjectiveRule::Kind::Survive;
      if (!onlyKeys(e, {kind}, at)) return false;
      if (!LoadCtx::isWhole(e[kind]) || e[kind].asInt() < 1 || e[kind].asInt() > 100000)
        return fail(at, "must be a round number from 1 to 100000");
      r.rounds = e[kind].asInt();
      if (r.kind == ObjectiveRule::Kind::Survive && !win)
        return fail(at, "\"survive\" is a way to win; to lose after some rounds use { \"round\": n }");
    }
    out.push_back(r);
  }
  return true;
}

bool StoryLoader::objectiveSet(const Json& o, const std::string& where, ObjectiveSet& out) {
  if (!o.isObject()) return fail(where, "\"objectives\" must be an object: { \"text\": ..., \"win\": [...], \"lose\": [...] }");
  if (!onlyKeys(o, {"text", "win", "lose"}, where)) return false;
  std::string t;
  if (!c_.str(o, "text", where, t)) return false;
  if (o.has("text")) out.text = text(t);
  if (o.has("win") && !rules(o["win"], "win", where, true, out.win)) return false;
  if (o.has("lose") && !rules(o["lose"], "lose", where, false, out.lose)) return false;
  return true;
}

}  // namespace ote
