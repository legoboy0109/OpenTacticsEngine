// View hooks for StaffUsed and StaffMissed (docs/SRPG.md section 8 "Playing it"): the staff's name floats over its user and the
// camera looks at the user, like an ability; "Missed" floats over a unit the staff missed. What the staff did arrives as the
// usual entries (UnitHealed: the green "+8" and the HP bar, UnitRelocated: the unit slides to its new square (a long way: it is
// just there), ItemBroke: "Mend Staff broke", XpGained, LevelUp, RankUp), played by the code that plays them after fights and item
// uses. Stage 2 adds two of its own: ItemRepaired floats "Iron Blade +5" over the unit, AreaLit floats "Lit" over the lit square
// and the camera looks at it (the new squares come into sight by themselves: sight is derived from the battle). Each entry writes one [info] line (the windowless tests read them).
// Same contract as the story hooks: return true when the entry was ours.
#include "battle/battle_view.h"
#include "battle/staff_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr float kStaffDur = 0.45f, kMissDur = 0.4f, kRepairDur = 0.4f, kLitDur = 0.5f;
const Color kStaffColor(150, 240, 170), kMissColor(190, 195, 215);
}  // namespace

bool BattleView::beginStaves(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type == EventType::ItemRepaired) {  // a = the unit, b = the item, c = uses restored
    if (e.a < 0 || (size_t)e.a >= units.size() || !units[(size_t)e.a].visible) {
      cur_.dur = 0;
      return true;
    }
    const UnitView& v = units[(size_t)e.a];
    addFloat(itemName(b, e.b) + " +" + intStr(e.c), v.px + tile_ / 2.0f, v.py - 2, kStaffColor, 1.2f);
    OTE_INFO("staff repaired: %s's %s +%d (now %d uses)", b.units[(size_t)e.a].name.c_str(), itemName(b, e.b).c_str(), e.c, e.d);
    cur_.dur = kRepairDur;
    return true;
  }
  if (e.type == EventType::AreaLit) {  // a = the user, b = the square, c = radius, d = turns
    OTE_INFO("staff lit: square %d radius %d for %d turns", e.b, e.c, e.d);
    if (!b.grid.valid(e.b) || dimmed(e.b)) {  // (the viewer's team always sees its own light, so this is another team's view)
      cur_.dur = 0;
      return true;
    }
    float x, y;
    cellPos(b, e.b, x, y);
    addFloat("Lit", x + tile_ / 2.0f, y, kStaffColor, 1.2f);
    cur_.dur = kLitDur;
    return true;
  }
  if (e.type != EventType::StaffUsed && e.type != EventType::StaffMissed) return false;
  const int who = e.a;  // StaffUsed: the user floats the name; StaffMissed: the missed unit floats "Missed"
  if (who < 0 || (size_t)who >= units.size() || (size_t)who >= b.units.size() || !units[(size_t)who].visible) {
    cur_.dur = 0;  // not on the map (or out of sight): nothing to show
    return true;
  }
  const UnitView& v = units[(size_t)who];
  if (e.type == EventType::StaffUsed) {
    const std::string& name = e.b >= 0 && (size_t)e.b < b.db->items.size() ? b.db->items[(size_t)e.b].name : std::string("Staff");
    addFloat(name, v.px + tile_ / 2.0f, v.py - 2, kStaffColor, 1.0f);
    OTE_INFO("staff: %s uses %s on unit %d (%d affected)", b.units[(size_t)e.a].name.c_str(), name.c_str(), e.c, e.d);
    cur_.dur = kStaffDur;
  } else {
    addFloat(StaffWords::kMissed, v.px + tile_ / 2.0f, v.py - 2, kMissColor, 0.9f);
    OTE_INFO("staff missed: %s", b.units[(size_t)e.a].name.c_str());
    cur_.dur = kMissDur;
  }
  return true;
}

bool BattleView::progressStaves(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type == EventType::AreaLit) {
    if (b.grid.valid(e.b)) {
      float x, y;
      cellPos(b, e.b, x, y);
      hasFocus = true;
      focusX = x + tile_ / 2.0f;
      focusY = y + tile_ / 2.0f;
    }
    return true;
  }
  if (e.type == EventType::ItemRepaired) return true;
  if (e.type != EventType::StaffUsed && e.type != EventType::StaffMissed) return false;
  if (e.a >= 0 && (size_t)e.a < units.size() && units[(size_t)e.a].visible) {
    hasFocus = true;
    focusX = units[(size_t)e.a].px + tile_ / 2.0f;
    focusY = units[(size_t)e.a].py + tile_ / 2.0f;
  }
  return true;
}

bool BattleView::finishStaves(const Battle&) {
  const EventType t = cur_.ev.type;
  return t == EventType::StaffUsed || t == EventType::StaffMissed || t == EventType::ItemRepaired || t == EventType::AreaLit;
}

}  // namespace ote
