// The `{ "battle": "beforePrep" }` events (docs/EVENTS.md "Before the preparation screen").
#include "core/battle/battle.h"

namespace ote {

bool Battle::hasBeforePrep() const {
  if (!story) return false;
  for (const StoryEvent& ev : story->events)
    if (ev.when.kind == Trigger::Kind::BeforePrep) return true;
  return false;
}

void Battle::runBeforePrep(std::vector<Event>& events) {
  if (beforePrepDone_) return;
  beforePrepDone_ = true;
  if (!hasBeforePrep()) return;  // a game without such an event: not one entry, not one touched field
  touch();
  EventScope scope(*this, events);
  sight_.valid = false;
  StoryOccurrence o;
  o.kind = Trigger::Kind::BeforePrep;
  fireStory(o, events);
}

void Battle::markerSpots(std::vector<MarkerSpot>& out) const {
  out.clear();
  if (!story) return;
  for (size_t i = 0; i < story->events.size(); ++i) {
    const StoryEvent& ev = story->events[i];
    if (ev.marker < 0 || ev.when.kind != Trigger::Kind::Reach) continue;
    if (storyState.fired[i] && !ev.repeat) continue;
    if (!conditionsHold(ev.cond, -1)) continue;  // a spent switch: nothing left to find
    const Rect& r = ev.when.rect;
    for (int y = r.y; y < r.y + r.h; ++y)
      for (int x = r.x; x < r.x + r.w; ++x)
        if (x >= 0 && y >= 0 && x < grid.width() && y < grid.height()) out.push_back({ev.marker, grid.at(x, y)});
  }
}

}  // namespace ote
