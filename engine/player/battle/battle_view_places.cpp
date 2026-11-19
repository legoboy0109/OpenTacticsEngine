// View hooks for doors, chests and visiting (docs/SRPG.md section 10 "Player"): DoorOpened, ChestOpened, DoorClosed, PlaceHit,
// PlaceVisited, PlaceDestroyed, PlaceUnlocked, and the "Found .." lines of what a chest gave (ItemGained / ResourceChanged with
// kLooted). The structure's new kind is drawn by itself (every view kind draws structures from the battle's state, through
// the kind the viewer last saw), so these only float words over the structure and look at it. Fog: sight is derived from the
// battle, so a door that opens shows what is behind it by itself. Each entry writes one [info] line (the windowless tests read them).
// Same contract as the story hooks: return true when the entry was ours.
#include "battle/battle_view.h"
#include "battle/place_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr float kPlaceDur = 0.5f;
const Color kPlaceColor(255, 225, 150), kHitColor(255, 245, 210);
// Where the last door / chest event was (the gold a chest gives floats there).
float gPlaceX = 0, gPlaceY = 0;
bool gPlaceKnown = false;

bool isPlaceEvent(EventType t) {
  switch (t) {
    case EventType::DoorOpened: case EventType::ChestOpened: case EventType::DoorClosed: case EventType::PlaceHit:
    case EventType::PlaceVisited: case EventType::PlaceDestroyed: case EventType::PlaceUnlocked: return true;
    default: return false;
  }
}
}  // namespace

// The middle of the structure's top edge, in map pixels.
bool BattleView::placeTop(const Battle& b, int s, float& x, float& y) const {
  const float tile = (float)tile_;
  const BattleView& v = *this;
  if (s < 0 || (size_t)s >= b.structures.size()) return false;
  std::vector<Cell> cells;
  b.structureCells(s, cells);
  if (cells.empty()) return false;
  float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f;
  for (Cell c : cells) {
    float cx, cy;
    v.cellPos(b, c, cx, cy);
    x0 = std::min(x0, cx);
    x1 = std::max(x1, cx);
    y0 = std::min(y0, cy);
  }
  x = (x0 + x1) / 2 + tile / 2.0f;
  y = y0 - 2;
  return true;
}

bool BattleView::beginPlaces(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type == EventType::ItemGained && (e.flags & kLooted)) {  // a = unit (-1 = the storage), b = item
    const std::string what = std::string(PlaceWords::kFound) + itemName(b, e.b);
    if (e.a >= 0 && (size_t)e.a < units.size()) addFloat(what, units[(size_t)e.a].px + tile_ / 2.0f, units[(size_t)e.a].py - 2, Color(170, 235, 255), 1.1f);
    else if (gPlaceKnown) addFloat(what, gPlaceX, gPlaceY, Color(170, 235, 255), 1.1f);
    OTE_INFO("chest gave: %s%s", what.c_str(), e.a < 0 ? " (to the storage)" : "");
    cur_.dur = 0.3f;
    return true;
  }
  if (e.type == EventType::ResourceChanged && (e.flags & kLooted)) {  // a = side, b = resource, c = amount
    const std::string what = std::string(PlaceWords::kFound) + intStr(e.c) + " " + b.db->resources[(size_t)e.b].name;
    if (gPlaceKnown) addFloat(what, gPlaceX, gPlaceY, Color(255, 225, 120), 1.1f);
    OTE_INFO("chest gave: %s", what.c_str());
    cur_.dur = 0.3f;
    return true;
  }
  if (!isPlaceEvent(e.type)) return false;
  const int s = e.type == EventType::PlaceHit ? e.b : e.a;
  float x = 0, y = 0;
  const bool where = placeTop(b, s, x, y);
  std::vector<Cell> cells;
  b.structureCells(s, cells);
  if (!where || cells.empty() || dimmed(cells[0])) {  // not on the map, or out of the viewer's sight
    cur_.dur = 0;
    return true;
  }
  gPlaceX = x;
  gPlaceY = y;
  gPlaceKnown = true;
  const std::string name = [&] {
    const int kind = e.type == EventType::PlaceHit ? b.structures[(size_t)s].type : e.type == EventType::DoorClosed ? e.b : e.type == EventType::PlaceVisited || e.type == EventType::PlaceDestroyed ? e.c : e.d;
    return kind >= 0 && (size_t)kind < b.db->structures.size() ? b.db->structures[(size_t)kind].name : std::string("Door");
  }();
  const std::string id = b.structures[(size_t)s].id.empty() ? name : b.structures[(size_t)s].id;
  switch (e.type) {
    case EventType::DoorOpened:
    case EventType::ChestOpened:
      addFloat(name + PlaceWords::kFloatOpens, x, y, kPlaceColor, 1.2f);
      OTE_INFO("opened: %s (%s) by %s", name.c_str(), id.c_str(), e.b >= 0 ? b.units[(size_t)e.b].name.c_str() : "a story event");
      break;
    case EventType::DoorClosed:
      addFloat(name + PlaceWords::kFloatShuts, x, y, kPlaceColor, 1.2f);
      OTE_INFO("closed: %s (%s)%s", name.c_str(), id.c_str(), (e.flags & kLocked) ? " and locked" : "");
      break;
    case EventType::PlaceUnlocked:
      addFloat(PlaceWords::kFloatUnlocked, x, y, kPlaceColor, 1.2f);
      OTE_INFO("unlocked: %s (%s)", name.c_str(), id.c_str());
      break;
    case EventType::PlaceVisited:
      addFloat(PlaceWords::kFloatVisited, x, y, kPlaceColor, 1.2f);
      OTE_INFO("visited: %s (%s)", name.c_str(), id.c_str());
      break;
    case EventType::PlaceDestroyed:
      addFloat(PlaceWords::kFloatDestroyed, x, y, Color(255, 140, 110), 1.3f);
      OTE_INFO("destroyed: %s (%s)", name.c_str(), id.c_str());
      break;
    case EventType::PlaceHit:  // a = unit, b = structure, d = damage
      addFloat(intStr(e.d), x, y, kHitColor, 0.9f);
      OTE_INFO("door hit: %s takes %d from %s (%d left)", name.c_str(), e.d, b.units[(size_t)e.a].name.c_str(), std::max(0, b.structures[(size_t)s].hp));
      break;
    default: break;
  }
  cur_.dur = e.type == EventType::PlaceHit ? 0.35f : kPlaceDur;
  return true;
}

bool BattleView::progressPlaces(const Battle& b) {
  const Event& e = cur_.ev;
  if (!isPlaceEvent(e.type)) return false;
  const int s = e.type == EventType::PlaceHit ? e.b : e.a;
  std::vector<Cell> cells;
  b.structureCells(s, cells);
  if (!cells.empty() && !dimmed(cells[0])) {
    float x, y;
    cellPos(b, cells[0], x, y);
    hasFocus = true;
    focusX = x + tile_ / 2.0f;
    focusY = y + tile_ / 2.0f;
  }
  return true;
}

bool BattleView::finishPlaces(const Battle&) { return isPlaceEvent(cur_.ev.type); }

}  // namespace ote
