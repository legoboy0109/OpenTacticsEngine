// A map's own starting weather (docs/ECONOMY.md "Weather and movement cost"): { "weather": "rain" } or "snow"; absent or "none" = clear sky.
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {

bool readMapWeather(const LoadCtx& c, const Json& root, MapDef& m) {
  m.weather.clear();
  if (!root.has("weather")) return true;
  std::string w;
  if (!c.str(root, "weather", "", w)) return false;
  if (w != "rain" && w != "snow" && w != "none") return c.fail("", "\"weather\" must be \"rain\", \"snow\" or \"none\", got \"" + w + "\"");
  if (w != "none") m.weather = w;
  return true;
}

}  // namespace ote
