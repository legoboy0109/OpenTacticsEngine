// The painter's tools. The names are what the app settings store (so they must not change); the words the person sees are in
// map_page.cpp.
#pragma once
#include <QString>

namespace ote_native {

enum class MapTool { Paint, Rectangle, Fill, Pick, Raise, Lower, SetHeight, Inspect, Slots, MoveUnit };

inline const char* mapToolKey(MapTool t) {
  switch (t) {
    case MapTool::Paint: return "paint";
    case MapTool::Rectangle: return "rectangle";
    case MapTool::Fill: return "fill";
    case MapTool::Pick: return "pick";
    case MapTool::Raise: return "raise";
    case MapTool::Lower: return "lower";
    case MapTool::SetHeight: return "setHeight";
    case MapTool::Inspect: return "inspect";
    case MapTool::Slots: return "slots";
    case MapTool::MoveUnit: return "moveUnit";
  }
  return "paint";
}

inline MapTool mapToolFromKey(const QString& key) {
  for (MapTool t : {MapTool::Paint, MapTool::Rectangle, MapTool::Fill, MapTool::Pick, MapTool::Raise, MapTool::Lower, MapTool::SetHeight, MapTool::Inspect, MapTool::Slots, MapTool::MoveUnit})
    if (key == QLatin1String(mapToolKey(t))) return t;
  return MapTool::Paint;
}

}  // namespace ote_native
