// A loop in a map event (docs/EVENTS.md "Loops"): the block runs again and again until a { "break": true } inside it (in an `if` or
// a choice's answer, at any depth) stops it, or after `max` rounds. A question inside works as anywhere else: with a person at the
// screen the replay (story_choice.cpp) consumes one answer per round, so "Repeat lesson?" is asked again each round; with nobody to
// ask the "cursor" answer (else the first) is taken every round, and `max` keeps that from going on for ever.
#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

void Battle::storyLoop(const Command& c, std::vector<Event>& events) {
  int round = 0;
  for (; round < c.value; ++round) {
    runCommands(c.thenDo, events);
    if (choice_.halted) return;  // a question waits inside the block: nothing after it counts (the replay comes back here)
    if (storyRun_.breaking) {
      storyRun_.breaking = false;  // only this loop ends; the commands after it run
      return;
    }
  }
  OTE_INFO("story: a loop ran its most rounds (%d) without a break", c.value);
}

}  // namespace ote
