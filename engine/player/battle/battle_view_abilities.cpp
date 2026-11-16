// View hooks for AbilityUsed (docs/SRPG.md "Events", step U3): the ability's name floats over its user, the camera
// looks at the user; a blocked one (kBlocked, data[0] = the unit in the way) also says "Blocked!" over the square of
// the blocker, which is how a player in fog learns that something stands there. What the ability then does arrives as
// its own entries (UnitRelocated, UnitHealed, UnitAdded, UnitRemoved) played by the story / build / heal code, so
// nothing is moved here. Each use writes one [info] line (the windowless tests read it). Same contract as the story
// hooks: return true when the entry was ours.
#include "battle/ability_text.h"
#include "battle/battle_view.h"
#include "core/base/log.h"

namespace ote {
namespace {
constexpr float kAbilityDur = 0.45f;
}

bool BattleView::beginAbilities(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::AbilityUsed) return false;
  const bool ok = e.a >= 0 && (size_t)e.a < units.size() && (size_t)e.a < b.units.size() && e.b >= 0 &&
                  (size_t)e.b < b.db->abilities.size();
  if (!ok) {
    cur_.dur = 0;
    return true;
  }
  const std::string& name = b.db->abilities[(size_t)e.b].name;
  const UnitView& v = units[(size_t)e.a];
  const bool blocked = (e.flags & kBlocked) != 0;
  addFloat(name, v.px + tile_ / 2.0f, v.py - 2, Color(150, 225, 255), 1.0f);
  if (blocked && !e.data.empty() && e.data[0] >= 0 && (size_t)e.data[0] < b.units.size() && b.units[(size_t)e.data[0]].alive) {
    float x, y;
    cellPos(b, b.units[(size_t)e.data[0]].cell, x, y);
    // Under the square, not above it: the ability's name floats above its user, and the two are often two squares apart.
    addFloat(AbilityWords::kBlocked, x + tile_ / 2.0f, y + tile_, Color(255, 170, 90), 1.1f);
  }
  OTE_INFO("ability: %s uses %s on unit %d square %d%s", b.units[(size_t)e.a].name.c_str(), name.c_str(), e.c, e.d,
           blocked ? " (blocked)" : "");
  cur_.dur = blocked ? kAbilityDur + 0.2f : kAbilityDur;
  return true;
}

bool BattleView::progressAbilities(const Battle&) {
  const Event& e = cur_.ev;
  if (e.type != EventType::AbilityUsed) return false;
  if (e.a >= 0 && (size_t)e.a < units.size()) {
    hasFocus = true;
    focusX = units[(size_t)e.a].px + tile_ / 2.0f;
    focusY = units[(size_t)e.a].py + tile_ / 2.0f;
  }
  return true;
}

bool BattleView::finishAbilities(const Battle&) { return cur_.ev.type == EventType::AbilityUsed; }

}  // namespace ote
