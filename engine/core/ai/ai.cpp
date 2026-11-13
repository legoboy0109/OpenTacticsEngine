// PLACEHOLDER: ends the turn immediately. Replaced by the real AI.
#include "core/ai/ai.h"

namespace ote {

bool aiTakeStep(Battle& battle, std::vector<Event>& events) {
  if (battle.over) return false;
  battle.apply(Action::endTurn(), events);
  return false;
}

}  // namespace ote
