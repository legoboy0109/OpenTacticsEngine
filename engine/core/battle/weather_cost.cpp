// Weather and movement cost (docs/ECONOMY.md "Weather and movement cost"). While it rains or snows (the map's "weather", or the last story
// `weather` command), a terrain with a "weatherCost" for that weather costs that much for the move type instead of its own number. A structure
// that sets its own cost for the move type (a road, a bridge) keeps it: weather only changes what the ground itself costs. A cost of 0 closes the
// terrain to that move type while the weather lasts (enterCost <= 0, like any impassable square): a unit already standing there stays and may leave
// by any open neighbour (movement never asks about the square it starts on); nobody is moved by the sky, unlike the map-change stranded-unit rule.
// Only reached in a game where some terrain has a "weatherCost" (Database::anyWeatherCost), so every other game takes the old path.
#include "core/battle/battle.h"

namespace ote {

int Battle::weatherEnterCost(Cell c, int moveType, int base) const {
  const std::string& w = storyState.weather;
  const int kind = w == "rain" ? 0 : w == "snow" ? 1 : -1;
  if (kind < 0) return base;
  const int structure = structureTypeAt(c);
  if (structure >= 0) {
    const std::vector<int>& mc = db->structures[(size_t)structure].moveCost;
    if ((size_t)moveType < mc.size() && mc[(size_t)moveType] >= 0) return base;
  }
  const std::vector<int>& wc = db->terrains[(size_t)cells[c].terrain].weatherCost[kind];
  return (size_t)moveType < wc.size() && wc[(size_t)moveType] >= 0 ? wc[(size_t)moveType] : base;  // 0 = closed by the weather
}

}  // namespace ote
