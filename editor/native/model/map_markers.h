// What a map places on its grid, for the painter to draw as markers: units, structures and the preparation screen's
// deployment slots (the maps' "side start positions"; a side itself has no square). Read-only: read from the Json each time.
#pragma once
#include <QColor>
#include <QString>
#include <map>
#include <string>
#include <vector>

#include "core/base/json.h"
#include "map_grid.h"

namespace ote_native {

struct MapMarker {
  enum class Kind { Unit, Structure, PrepSlot };
  Kind kind = Kind::Unit;
  size_t index = 0;  // position in the map's units / structures / prep.slots list
  int x = 0, y = 0;
  QString letter;    // one or two characters drawn on the marker
  QString tip;       // tooltip: what it is, whose side, where
  QColor color;      // the side's colour (grey for neutral)
  int rings = 0;     // hexagon structures that cover a ring of six
  int w = 1, h = 1;  // square structures' size
};

std::vector<MapMarker> readMapMarkers(const ote::Json& map, const std::map<std::string, ote_editor::Footprint>& footprints);

}  // namespace ote_native
