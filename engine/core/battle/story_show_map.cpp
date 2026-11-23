// A story scene on another map of the game (docs/EVENTS.md "Showing another map"): showMap builds a second, throw-away Battle from that map
// (its own units placed, nothing started), the commands of the section that act on units, squares and the camera run on it, and
// returnMap (or the end of the event) drops it. The battle's own units, squares, fired list and objectives are never touched; switches,
// variables, music and the scene's presentation (fades, backgrounds, the stage) stay with the real battle, which runs those commands.
//
// The event stream gets MapShown / MapReturned around the shown map's entries. The player needs the shown map as it was set up and as the
// scene left it to play them; both are kept in `shownMaps` (presentation only, never saved: a save is never made while a scene runs).
#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

bool Battle::onShownMap(const Command& c) const {
  using K = Command::Kind;
  switch (c.kind) {
    case K::Say:  // the speaker is a unit of the shown map
    case K::AddUnit:
    case K::RemoveUnit:
    case K::MoveUnit:
    case K::ChangeSide:
    case K::Focus:
    case K::Fight:
    case K::AddCondition:
    case K::RemoveCondition:
    case K::Place:
    case K::SetTerrain:
    case K::ApplyChange:
    case K::RevertChange:
      return true;
    case K::Scene:  // weather lies over the shown map; fades, backgrounds and the stage are the scene's, played on the real battle
      return c.fx.kind == SceneFx::Kind::Weather;
    default:
      return false;
  }
}

void Battle::storyShowMap(const Command& c, std::vector<Event>& events) {
  if (shown_) storyReturnMap(events);  // (the loader refuses a second showMap before a return)
  if (!c.shownMap) return;
  auto m = std::make_shared<Battle>();
  m->difficulty = difficulty;  // addUnit "difficulty" and the enemy numbers follow the battle's
  // Its own generator, seeded from this battle's state without drawing from it: real-roll fights on the shown map leave the battle's
  // random numbers where they were, and the same scene plays the same way again (a beforePrep event runs more than once).
  const uint32_t seed = (uint32_t)(rng.state ^ (rng.state >> 32)) + 7919u * (uint32_t)(shownMaps.size() + 1);
  std::string err;
  if (!m->init(*db, *c.shownMap, *formulas, seed, err)) {
    OTE_ERROR("story: showMap '%s': %s", c.shownMap->id.c_str(), err.c_str());
    return;
  }
  m->sceneOnly_ = true;
  m->fog = false;  // a scene shows everything
  auto texts = std::make_shared<StoryData>();  // the commands' wording is this map's (they carry indices into its table); no events
  if (story) texts->texts = story->texts;
  m->story = texts;
  m->storyState.switches = storyState.switches;
  m->storyState.variables = storyState.variables;
  m->storyState.weather = c.shownMap->weather;

  ShownMap sm;
  sm.map = c.index;
  sm.def = c.shownMap;
  if (c.on) {
    if (!c.unit.empty()) {
      const int u = m->unitByStoryId(c.unit);
      if (u >= 0 && m->units[(size_t)u].alive) sm.focus = m->units[(size_t)u].cell;
    } else {
      sm.focus = m->grid.at(c.x, c.y);
    }
  }
  sm.start = std::make_shared<const Battle>(*m);
  shownMaps.push_back(sm);
  shown_ = m;
  Event e;
  e.type = EventType::MapShown;
  e.a = (int)shownMaps.size() - 1;
  e.b = c.index;
  e.c = sm.focus;
  events.push_back(e);
  OTE_INFO("story: showing map '%s' (%d units)", c.shownMap->id.c_str(), (int)m->units.size());
}

void Battle::storyReturnMap(std::vector<Event>& events) {
  if (!shown_) return;
  const int index = (int)shownMaps.size() - 1;
  shownMaps[(size_t)index].end = shown_;
  shown_.reset();
  Event e;
  e.type = EventType::MapReturned;
  e.a = index;
  events.push_back(e);
  OTE_INFO("story: back to the battle's map");
}

}  // namespace ote
