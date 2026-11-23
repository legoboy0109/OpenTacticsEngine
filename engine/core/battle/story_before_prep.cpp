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

}  // namespace ote
