// Story state in saves (see save_story.h and docs/SAVES.md).
#include "core/battle/save_story.h"

namespace ote {

using savejson::Reader;

namespace {

bool hasStoryState(const Battle& b) { return b.story || !b.db->switches.empty() || !b.db->variables.empty(); }

}  // namespace

void saveStoryState(const Battle& b, Json& out) {
  if (!hasStoryState(b)) return;
  Json j = Json::makeObject();
  Json sw = Json::makeArray();
  for (uint8_t on : b.storyState.switches) sw.push(Json::makeBool(on != 0));
  j.set("switches", sw);
  Json vars = Json::makeArray();
  for (int v : b.storyState.variables) vars.push(Json::makeNumber(v));
  j.set("variables", vars);
  // Events are named by id: readable, and independent of their order in the file.
  Json fired = Json::makeArray();
  if (b.story)
    for (size_t i = 0; i < b.story->events.size(); ++i)
      if (b.storyState.fired[i]) fired.push(Json::makeString(b.story->events[i].id));
  j.set("fired", fired);
  j.set("objectives", Json::makeNumber(b.storyState.objectives));
  // Only once a playMusic / stopMusic command ran, so every other save stays byte for byte what it was.
  if (b.storyState.musicAsked) j.set("music", Json::makeString(b.storyState.music));
  // Scene presentation (docs/EVENTS.md): the picture over the map and the weather, only once a command set one.
  if (!b.storyState.background.empty() || !b.storyState.weather.empty()) {
    Json s = Json::makeObject();
    if (!b.storyState.background.empty()) s.set("background", Json::makeString(b.storyState.background));
    if (!b.storyState.weather.empty()) s.set("weather", Json::makeString(b.storyState.weather));
    j.set("scene", s);
  }
  // setPhaseMusic overrides (docs/SOUND.md), only once one is set.
  if (!b.storyState.phaseMusic.empty()) {
    Json p = Json::makeObject();
    for (const auto& e : b.storyState.phaseMusic) p.set(e.first, Json::makeString(e.second));
    j.set("phaseMusic", p);
  }
  out.set("story", j);
}

bool restoreStoryState(Reader& rd, const Json& saved, Battle& b) {
  if (!hasStoryState(b)) {
    if (saved.has("story")) return rd.fail("battle.story", "this game has no events, objectives, switches or variables");
    return true;
  }
  const Json* j;
  if (!rd.object(saved, "story", "battle", j)) return false;
  const std::string where = "battle.story";

  const Json* list;
  if (!rd.array(*j, "switches", where, (int)b.db->switches.size(), 0, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    if (!(*list)[i].isBool()) return rd.fail(where + ".switches[" + std::to_string(i) + "]", "not true/false");
    b.storyState.switches[i] = (*list)[i].asBool() ? 1 : 0;
  }
  if (!rd.array(*j, "variables", where, (int)b.db->variables.size(), 0, list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& v = (*list)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || std::fabs(v.asNumber()) > kMaxStoryNumber)
      return rd.fail(where + ".variables[" + std::to_string(i) + "]", "not a whole number");
    b.storyState.variables[i] = (int)v.asNumber();
  }

  const size_t eventCount = b.story ? b.story->events.size() : 0;
  if (!rd.array(*j, "fired", where, -1, (int)eventCount, list)) return false;
  b.storyState.fired.assign(eventCount, 0);
  for (size_t i = 0; i < list->size(); ++i) {
    std::string at = where + ".fired[" + std::to_string(i) + "]";
    if (!(*list)[i].isString()) return rd.fail(at, "not an event id");
    size_t e = 0;
    while (e < eventCount && b.story->events[e].id != (*list)[i].asString()) ++e;
    if (e == eventCount) return rd.fail(at, "the map has no event \"" + (*list)[i].asString() + "\"");
    if (b.storyState.fired[e]) return rd.fail(at, "event \"" + (*list)[i].asString() + "\" is listed twice");
    b.storyState.fired[e] = 1;
  }
  int sets = b.story ? (int)b.story->objectives.size() : 1;
  if (!rd.integer(*j, "objectives", where, 0, sets - 1, b.storyState.objectives)) return false;
  // "music" (docs/SAVES.md): the track the story asked for, "" = it stopped the music. Absent = it never asked.
  b.storyState.musicAsked = false;
  b.storyState.music.clear();
  if (j->has("music")) {
    const Json& m = (*j)["music"];
    if (!m.isString() || (!m.asString().empty() && !isStoryId(m.asString())))
      return rd.fail(where + ".music", "not a music id (letters, digits, _ and -) or \"\"");
    b.storyState.musicAsked = true;
    b.storyState.music = m.asString();
  }
  // "phaseMusic" (docs/SAVES.md): side id -> music id set by setPhaseMusic. Absent = none.
  b.storyState.phaseMusic.clear();
  if (j->has("phaseMusic")) {
    const Json& p = (*j)["phaseMusic"];
    if (!p.isObject()) return rd.fail(where + ".phaseMusic", "not an object (side id -> music id)");
    for (const Json::Member& m : p.members()) {
      const std::string at = where + ".phaseMusic." + m.first;
      bool side = false;
      for (const SideState& s : b.sides) side = side || s.id == m.first;
      if (!side) return rd.fail(at, "the map has no side \"" + m.first + "\"");
      if (!m.second.isString() || (!m.second.asString().empty() && !isStoryId(m.second.asString())))
        return rd.fail(at, "not a music id (letters, digits, _ and -) or \"\"");
      b.storyState.phaseMusic.push_back(std::make_pair(m.first, m.second.asString()));
    }
  }
  // "scene" (docs/SAVES.md): the background and weather a story command set. Absent = the map, clear sky.
  b.storyState.background.clear();
  b.storyState.weather.clear();
  b.storyState.faded = false;
  if (j->has("scene")) {
    const Json& s = (*j)["scene"];
    if (!s.isObject()) return rd.fail(where + ".scene", "not an object");
    if (s.has("background")) {
      const Json& v = s["background"];
      if (!v.isString() || !isStoryId(v.asString()))
        return rd.fail(where + ".scene.background", "not a background id (letters, digits, _ and -)");
      if (b.db->graphics.findBackground(v.asString()) < 0)
        return rd.fail(where + ".scene.background", "the game has no background \"" + v.asString() + "\"");
      b.storyState.background = v.asString();
    }
    if (s.has("weather")) {
      const Json& v = s["weather"];
      if (!v.isString() || (v.asString() != "rain" && v.asString() != "snow"))
        return rd.fail(where + ".scene.weather", "not \"rain\" or \"snow\"");
      b.storyState.weather = v.asString();
    }
  }
  return true;
}

bool readUnitStoryId(Reader& rd, const Json& unit, const std::string& where, std::string& out) {
  out.clear();
  if (!unit.has("id")) return true;
  if (!unit["id"].isString() || !isStoryId(unit["id"].asString()))
    return rd.fail(where + ".id", "not a valid unit id (letters, digits, _ and -)");
  out = unit["id"].asString();
  return true;
}

}  // namespace ote
