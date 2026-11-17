// The `actor` condition of a map event (docs/EVENTS.md "Branching on who acted"):
//   { "actor": <unit filter> }     the unit that set this event off matches the filter
// A campaign scene refuses it (loader_campaign_scene.cpp): no unit acts there.
#include "core/data/loader_events.h"

namespace ote {

bool StoryLoader::actorCondition(const Json& e, const std::string& at, Condition& out) const {
  if (!onlyKeys(e, {"actor"}, at)) return false;
  out.filter = UnitFilter();
  return filter(e["actor"], at, "\"actor\"", out.filter);
}

}  // namespace ote
