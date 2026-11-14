// Event loader, part 4: the map's "events" list and the entry point loadMapStory
// (docs/EVENTS.md "Events (map file)" and "Triggers").
#include <algorithm>
#include <memory>

#include "core/data/loader_events.h"

namespace ote {

namespace {

// Story ids of units that addUnit commands create (looked at before parsing, so that
// an event may refer to a unit that a later event adds). Tolerates malformed input.
void collectAddedIds(const Json& commands, std::vector<std::string>& out) {
  for (const Json& c : commands.items()) {
    if (!c.isObject()) continue;
    const Json& u = c["addUnit"];
    if (u.isObject() && u["id"].isString()) out.push_back(u["id"].asString());
    collectAddedIds(c["then"], out);
    collectAddedIds(c["else"], out);
  }
}

}  // namespace

// The trigger: exactly one of battle / turn / reach / defeated / fight / talk, plus its extras.
static bool readTrigger(StoryLoader& L, const Json& w, const std::string& where, Trigger& t) {
  if (!w.isObject()) return L.fail(where, "\"when\" must be an object such as { \"battle\": \"start\" }");
  static const char* const kKinds[] = {"battle", "turn", "reach", "defeated", "fight", "talk"};
  const char* kind = nullptr;
  for (const char* k : kKinds)
    if (w.has(k)) {
      if (kind) return L.fail(where, std::string("\"when\" has two triggers (") + kind + " and " + k + "); an event has one");
      kind = k;
    }
  if (!kind) return L.fail(where, "\"when\" needs a trigger: one of battle, turn, reach, defeated, fight, talk");
  std::string k = kind;
  const Json& v = w[kind];
  const LoadCtx& c = L.ctx();
  using K = Trigger::Kind;
  std::string at = where + ": \"when\"";
  if (k == "battle") {
    if (!L.onlyKeys(w, {"battle"}, at)) return false;
    std::string s = v.asString();
    if (s == "start") t.kind = K::BattleStart;
    else if (s == "won") t.kind = K::BattleWon;
    else if (s == "lost") t.kind = K::BattleLost;
    else if (s == "end") t.kind = K::BattleEnd;
    else return L.fail(at, "\"battle\" must be \"start\", \"won\", \"lost\" or \"end\"");
    return true;
  }
  if (k == "turn") {
    t.kind = K::Turn;
    if (!L.onlyKeys(w, {"turn", "side"}, at)) return false;
    if (v.isString() && v.asString() == "every") t.round = 0;
    else if (LoadCtx::isWhole(v) && v.asInt() >= 1 && v.asInt() <= 100000) t.round = v.asInt();
    else return L.fail(at, "\"turn\" must be a round number (1 or more) or \"every\"");
    std::string side;
    if (!c.str(w, "side", at, side)) return false;
    return !w.has("side") || L.sideIndex(side, at, "\"side\"", t.side);
  }
  if (k == "reach") {
    t.kind = K::Reach;
    if (!L.onlyKeys(w, {"reach", "by"}, at) || !L.rect(v, at, t.rect)) return false;
    return !w.has("by") || L.filter(w["by"], at, "\"by\"", t.who);
  }
  if (k == "defeated") {
    t.kind = K::Defeated;
    if (!L.onlyKeys(w, {"defeated", "by"}, at) || !L.filter(v, at, "\"defeated\"", t.who)) return false;
    return !w.has("by") || L.filter(w["by"], at, "\"by\"", t.other);
  }
  if (k == "fight") {
    t.kind = K::Fight;
    if (!L.onlyKeys(w, {"fight", "with"}, at) || !L.filter(v, at, "\"fight\"", t.who)) return false;
    return !w.has("with") || L.filter(w["with"], at, "\"with\"", t.other);
  }
  t.kind = K::Talk;
  if (!L.onlyKeys(w, {"talk", "to", "label"}, at) || !L.unitId(v, at, "\"talk\"", t.talker)) return false;
  if (!w.has("to")) return L.fail(at, "a talk trigger needs \"to\" (the unit that is talked to)");
  if (!L.unitId(w["to"], at, "\"to\"", t.talkee)) return false;
  if (t.talker == t.talkee) return L.fail(at, "a unit cannot talk to itself");
  std::string label = "Talk";
  if (!c.str(w, "label", at, label)) return false;
  t.label = L.text(label);
  return true;
}

static bool readEvents(StoryLoader& L, const Json& list, const std::string& mapName, StoryData& data) {
  if (!list.isArray()) return L.fail(mapName, "\"events\" must be a list");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    std::string where = mapName + ": event " + std::to_string(i + 1);
    if (!e.isObject()) return L.fail(where, "must be an object");
    StoryEvent ev;
    if (!L.ctx().str(e, "id", where, ev.id, true)) return false;
    if (!isStoryId(ev.id))
      return L.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got \"" + ev.id + "\")");
    for (const StoryEvent& o : data.events)
      if (o.id == ev.id) return L.fail(mapName + ": event \"" + ev.id + "\"", "duplicate id");
    where = mapName + ": event \"" + ev.id + "\"";
    if (!L.onlyKeys(e, {"id", "when", "if", "repeat", "do"}, where)) return false;
    if (!e.has("when")) return L.fail(where, "missing required \"when\" (the trigger)");
    if (!readTrigger(L, e["when"], where, ev.when)) return false;
    if (e.has("if") && !L.conditions(e["if"], where, ev.cond)) return false;
    if (!L.ctx().boolean(e, "repeat", where, ev.repeat)) return false;
    if (ev.when.kind == Trigger::Kind::Turn && ev.when.round == 0) ev.repeat = true;  // "every" round means every round
    if (!e.has("do")) return L.fail(where, "missing required \"do\" (the commands to run)");
    bool talk = ev.when.kind == Trigger::Kind::Talk;
    if (!L.commands(e["do"], where + ": \"do\"", talk, ev.commands, ev.endsTurnOnTalk)) return false;
    data.events.push_back(ev);
  }
  return true;
}

bool loadMapStory(const Database& db, const Json& root, MapDef& map, const std::string& file, std::string& err) {
  if (!root.has("events") && !root.has("objectives")) return true;
  auto data = std::make_shared<StoryData>();
  StoryLoader L(db, map, file, err, *data);
  std::string mapName = "map \"" + map.id + "\"";

  for (const MapUnitDef& u : map.units)
    if (!u.id.empty()) L.usedIds.push_back(u.id);
  L.unitIds = L.usedIds;
  if (root["events"].isArray())
    for (const Json& e : root["events"].items()) collectAddedIds(e["do"], L.unitIds);

  if (root.has("objectives") && !L.objectiveSet(root["objectives"], mapName + ": \"objectives\"", data->objectives[0]))
    return false;
  if (root.has("events") && !readEvents(L, root["events"], mapName, *data)) return false;
  map.story = data;
  return true;
}

}  // namespace ote
