// View hooks for the speed turn order's events (docs/TURN_ORDER.md "Player"): a unit's turn begins (the camera goes to it), a round starts, an
// action starts charging ("Charging Fire" over the caster), goes off, or does not. Same contract as the other hooks: return true when the entry
// was ours. Each entry writes one [info] line (the windowless tests read them).
#include "battle/battle_view.h"
#include "battle/speed_text.h"
#include "core/base/log.h"

namespace ote {
namespace {
constexpr float kTurnDur = 0.18f, kChargingDur = 0.5f, kGoesOffDur = 0.3f, kCancelledDur = 0.45f;
const Color kChargeColor(120, 235, 225), kGoesOffColor(255, 150, 225), kLostColor(200, 200, 215);

bool ours(EventType t) {
  return t == EventType::UnitTurnBegan || t == EventType::RoundStarted || t == EventType::ActionCharging || t == EventType::ChargeResolved ||
         t == EventType::ChargeCancelled;
}
}  // namespace

bool BattleView::beginSpeed(const Battle& b) {
  const Event& e = cur_.ev;
  if (!ours(e.type)) return false;
  if (e.type == EventType::RoundStarted) {
    OTE_INFO("round %d begins", e.a);
    cur_.dur = 0;
    return true;
  }
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size()) {
    cur_.dur = 0;
    return true;
  }
  const UnitView& v = units[(size_t)e.a];
  const std::string who = b.units[(size_t)e.a].name;
  switch (e.type) {
    case EventType::UnitTurnBegan:
      OTE_INFO("unit turn: %s, round %d, charge %d", who.c_str(), e.b, e.c);
      if (!v.visible) {  // not on the map for this viewer (fog): the camera stays
        cur_.dur = 0;
        return true;
      }
      focusMs = 0;  // the camera arrives at once (a jump from the last unit to this one)
      ++focusSerial;
      cur_.dur = skipping_ ? 0.0f : kTurnDur;
      return true;
    case EventType::ActionCharging: {
      const std::string name = chargeActionName(b, e.b == 1, e.c);
      OTE_INFO("charging: %s starts %s at %d,%d (speed %d)", who.c_str(), name.c_str(), b.grid.valid(e.d) ? b.grid.x(e.d) : -1,
               b.grid.valid(e.d) ? b.grid.y(e.d) : -1, e.data.size() > 1 ? e.data[1] : 0);
      if (!v.visible) {
        cur_.dur = 0;
        return true;
      }
      addFloat(std::string(SpeedWords::kCharging) + name, v.px + tile_ / 2.0f, v.py - 2, kChargeColor, 1.2f);
      cur_.dur = skipping_ ? 0.0f : kChargingDur;
      return true;
    }
    case EventType::ChargeResolved: {
      const std::string name = chargeActionName(b, e.b == 1, e.c);
      OTE_INFO("charge goes off: %s's %s at %d,%d", who.c_str(), name.c_str(), b.grid.valid(e.d) ? b.grid.x(e.d) : -1, b.grid.valid(e.d) ? b.grid.y(e.d) : -1);
      float x, y;
      if (b.grid.valid(e.d) && !dimmed(e.d)) {
        cellPos(b, e.d, x, y);
        addFloat(name + SpeedWords::kGoesOff + "!", x + tile_ / 2.0f, y - 2, kGoesOffColor, 1.2f);
      }
      cur_.dur = skipping_ ? 0.0f : kGoesOffDur;
      return true;
    }
    default: {  // ChargeCancelled
      const std::string name = chargeActionName(b, e.b == 1, e.c);
      const int why = e.d >= 0 && e.d < 5 ? e.d : 0;
      OTE_INFO("charge cancelled: %s's %s (%s)", who.c_str(), name.c_str(), SpeedWords::kCancelled[why]);
      if (!v.visible) {
        cur_.dur = 0;
        return true;
      }
      addFloat(SpeedWords::kCancelled[why], v.px + tile_ / 2.0f, v.py - 2, kLostColor, 1.1f);
      cur_.dur = skipping_ ? 0.0f : kCancelledDur;
      return true;
    }
  }
}

bool BattleView::progressSpeed(const Battle& b) {
  const Event& e = cur_.ev;
  if (!ours(e.type) || e.type == EventType::RoundStarted) return false;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size()) return true;
  const UnitView& v = units[(size_t)e.a];
  if (!v.visible) return true;
  hasFocus = true;  // the camera stays on the unit (its turn, its charging, the square the action hits)
  if (e.type == EventType::ChargeResolved && b.grid.valid(e.d)) {
    float x, y;
    cellPos(b, e.d, x, y);
    focusX = x + tile_ / 2.0f;
    focusY = y + tile_ / 2.0f;
  } else {
    focusX = v.px + tile_ / 2.0f;
    focusY = v.py + tile_ / 2.0f;
  }
  return true;
}

}  // namespace ote
