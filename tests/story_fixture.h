// Helpers for the story tests (docs/EVENTS.md): the miniature game of battle_fixture.h with named
// units, two switches, two variables, and events/objectives given as JSON text.
//   switches:  saved, alarm        variables: gold, kills
//   sides: blue (human, team blue), red (ai, team red)
#pragma once
#include "battle_fixture.h"
#include "core/data/events.h"

namespace story {

using namespace ote;
using fixture::UnitSpec;

struct StoryWorld : fixture::World {
  StoryWorld(const std::vector<std::string>& rows, const std::vector<UnitSpec>& units,
             const std::vector<const char*>& ids = {})
      : World(rows, units) {
    db.switches = {{"saved", "Saved"}, {"alarm", "Alarm"}};
    db.variables = {{"gold", "Gold"}, {"kills", "Kills"}};
    for (size_t i = 0; i < ids.size(); ++i) map.units[i].id = ids[i];
  }

  // Parses `{ "events": [...], "objectives": {...} }` fragments into the map. Returns the loader's
  // error ("" = fine); the text may leave either part out (pass "" ).
  std::string load(const std::string& events, const std::string& objectives = "") {
    std::string text = "{";
    if (!events.empty()) text += "\"events\": " + events;
    if (!objectives.empty()) text += std::string(events.empty() ? "" : ",") + "\"objectives\": " + objectives;
    text += "}";
    Json root;
    std::string err;
    if (!Json::parse(text, root, err)) return "bad test JSON: " + err;
    map.story.reset();
    if (!loadMapStory(db, root, map, "test.json", err)) return err;
    return "";
  }

  // load() that must succeed, then begin().
  bool play(const std::string& events, const std::string& objectives = "", uint32_t seed = 1) {
    std::string err = load(events, objectives);
    if (!err.empty()) {
      std::printf("  story: %s\n", err.c_str());
      return false;
    }
    return begin(seed);
  }

  // Dialogue lines so far, as "Speaker: text" with the unit's name or the free name.
  std::vector<std::string> said() const {
    std::vector<std::string> out;
    for (const Event& e : events) {
      if (e.type != EventType::Dialogue) continue;
      std::string who = e.a >= 0 ? battle.units[(size_t)e.a].name : e.c >= 0 ? battle.storyText(e.c) : "";
      out.push_back(who + ": " + battle.storyText(e.b));
    }
    return out;
  }
  bool saidLine(const char* line) const {
    for (const std::string& s : said())
      if (s.find(line) != std::string::npos) return true;
    return false;
  }
  bool fired(const char* id) const {
    for (size_t i = 0; i < battle.story->events.size(); ++i)
      if (battle.story->events[i].id == id) return battle.storyState.fired[i] != 0;
    return false;
  }
  bool sw(const char* id) const { return battle.storyState.switches[(size_t)db.findSwitch(id)] != 0; }
  int var(const char* id) const { return battle.storyState.variables[(size_t)db.findVariable(id)]; }
  int unit(const char* id) const { return battle.unitByStoryId(id); }
};

// Index of the first event of this type at or after `from`, -1 if none.
inline int find(const std::vector<Event>& events, EventType t, size_t from = 0) {
  for (size_t i = from; i < events.size(); ++i)
    if (events[i].type == t) return (int)i;
  return -1;
}

}  // namespace story
