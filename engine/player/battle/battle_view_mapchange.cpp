// How the battle view plays a change of the map (docs/EVENTS.md "Changing the map"): TerrainChanged. The squares already show their new
// terrain (every kind of view draws terrain from the battle, in 3D the meshes are rebuilt every frame, so the whole map is "rebuilt" each
// frame and a changed square needs no special update); the event adds a brief white flash on the changed squares, drawn in every kind of
// view like the flash of an upgraded structure, and one [info] line the windowless tests read.
#include <algorithm>

#include "battle/battle_view.h"
#include "core/base/log.h"

namespace ote {
namespace {

constexpr float kFlashTime = 0.5f;

}  // namespace

bool BattleView::beginMapChange(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::TerrainChanged) return false;
  const size_t n = (size_t)b.grid.cellCount();
  if (cellFlash.size() < n) cellFlash.resize(n, 0.0f);
  int shown = 0;
  for (int c : e.data)
    if (b.grid.valid(c) && !dimmed(c)) {  // a square the viewer cannot see changes without a flash
      cellFlash[(size_t)c] = 1;
      ++shown;
    }
  const std::string what = e.b >= 0 && (size_t)e.b < b.db->terrains.size() ? b.db->terrains[(size_t)e.b].name : std::string("mixed terrain");
  OTE_INFO("map changed: %d square(s) -> %s (%d shown)", e.a, what.c_str(), shown);
  cur_.dur = shown > 0 ? kFlashTime : 0;
  return true;
}

bool BattleView::progressMapChange(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::TerrainChanged) return false;
  const float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  float sx = 0, sy = 0;
  int count = 0;
  for (int c : e.data)
    if ((size_t)c < cellFlash.size() && cellFlash[(size_t)c] > 0) {
      cellFlash[(size_t)c] = 1 - p;
      float x, y;
      cellPos(b, c, x, y);
      sx += x;
      sy += y;
      ++count;
    }
  if (count > 0) {  // the camera goes to the middle of what changed
    hasFocus = true;
    focusX = sx / (float)count + tile_ / 2.0f;
    focusY = sy / (float)count + tile_ / 2.0f;
  }
  return true;
}

bool BattleView::finishMapChange(const Battle&) {
  const Event& e = cur_.ev;
  if (e.type != EventType::TerrainChanged) return false;
  for (int c : e.data)
    if ((size_t)c < cellFlash.size()) cellFlash[(size_t)c] = 0;
  return true;
}

}  // namespace ote
