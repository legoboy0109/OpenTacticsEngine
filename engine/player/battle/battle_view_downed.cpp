// View hooks for downed units (docs/SRPG.md "Downed units and revive", "Player"). A UnitDied entry with kDowned plays the usual fall but stops half
// faded: the unit stays on its square drawn see-through and dimmed, with the marker "Down" (condition_marker.cpp) in every view, and in the 3D
// views a unit model lies in the last frame of its fall clip (UnitView::down). UnitRevived floats "Revived +N" (an "autoRevive" condition:
// "<condition>! +N") and fades the unit back in; a downed unit's body that is removed (UnitRemoved with kDowned) fades out from where it lies.
// Each entry writes one [info] line (the windowless tests read them). Same contract as the story hooks: return true when the entry was ours.
#include <algorithm>
#include <cmath>

#include "battle/battle_view.h"
#include "battle/condition_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr float kRevivedDur = 0.5f;
const Color kRevivedColor(140, 255, 190);
}  // namespace

bool BattleView::beginDowned(const Battle& b) {
  const Event& e = cur_.ev;
  if ((e.type == EventType::UnitDied || e.type == EventType::UnitRemoved) && (e.flags & kDowned) && (size_t)e.a < b.units.size()) {
    // Only the log line: the fall (battle_view.cpp) or the fade (battle_view_story.cpp) plays as usual.
    OTE_INFO("%s: %s", e.type == EventType::UnitDied ? "downed" : "body removed", b.units[(size_t)e.a].name.c_str());
    return false;
  }
  if (e.type != EventType::UnitRevived) return false;
  cur_.dur = 0;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size()) return true;
  UnitView& v = units[(size_t)e.a];
  const Unit& u = b.units[(size_t)e.a];
  const bool autoRevive = (e.flags & kAuto) != 0;
  if (autoRevive) OTE_INFO("revived: %s by %s (%d HP)", u.name.c_str(), conditionName(*b.db, e.d).c_str(), e.b);
  else OTE_INFO("revived: %s by %s (%d HP)", u.name.c_str(), e.c >= 0 && (size_t)e.c < b.units.size() ? b.units[(size_t)e.c].name.c_str() : "?", e.b);
  cur_.hpFrom = 0;
  cur_.hpTo = e.b;
  cellPos(b, u.cell, v.px, v.py);
  v.visible = true;  // a body is always known (fog or not); the unit standing again is seen where its team sees (updateShown)
  const std::string text = (autoRevive ? conditionName(*b.db, e.d) + "! " : std::string(DownedWords::kRevived) + " ") + "+" + intStr(e.b);
  addFloat(text, v.px + tile_ / 2.0f, v.py - 2, kRevivedColor, 1.1f);
  cur_.dur = kRevivedDur;
  hasFocus = true;
  focusX = v.px + tile_ / 2.0f;
  focusY = v.py + tile_ / 2.0f;
  return true;
}

bool BattleView::progressDowned(const Battle& b) {
  (void)b;
  const Event& e = cur_.ev;
  if (e.type != EventType::UnitRevived) return false;
  const float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  UnitView& v = units[(size_t)e.a];
  if (v.downed) {  // (an "autoRevive" never lay down: nothing to raise)
    v.alpha = kDownedAlpha + (1 - kDownedAlpha) * p;
    v.down = 1 - p;
  }
  v.hp = cur_.hpFrom + (int)std::lround((cur_.hpTo - cur_.hpFrom) * p);
  hasFocus = true;
  focusX = v.px + tile_ / 2.0f;
  focusY = v.py + tile_ / 2.0f;
  return true;
}

bool BattleView::finishDowned(const Battle& b) {
  (void)b;
  const Event& e = cur_.ev;
  if (e.type != EventType::UnitRevived) return false;
  if (e.a >= 0 && (size_t)e.a < units.size()) standUp(units[(size_t)e.a], e.b);
  return true;
}

void BattleView::standUp(UnitView& v, int hp) {
  v.downed = false;
  v.down = 0;
  v.alpha = 1;
  v.hp = hp;
  v.pose = PoseNone;  // out of the KO pose it fell into (docs/GRAPHICS.md "Poses and facing")
}

void BattleView::lieDown(UnitView& v) {
  v.downed = true;
  v.down = 1;
  v.alpha = kDownedAlpha;
  v.hp = 0;
  v.pose = PoseKo;  // a sheet with a "ko" pose shows it (held on its last frame); otherwise the standing picture, lying
  v.conds.clear();  // a downed unit has no conditions (they all ended when it fell)
}

}  // namespace ote
