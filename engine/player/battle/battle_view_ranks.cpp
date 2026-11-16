// View hooks for RankUp (docs/SRPG.md section 7 "Player"): "Sword rank C" floats over the unit that ranked up, the way a
// skill learned outside a level-up box does (battle_view_skills.cpp); the camera follows the unit for the moment.
#include "battle/battle_view.h"
#include "battle/rank_text.h"
#include "core/base/log.h"

namespace ote {
namespace {
constexpr float kRankDur = 0.7f;
const Color kRankColor(255, 225, 110);
}  // namespace

bool BattleView::beginRanks(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::RankUp) return false;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size() || !units[(size_t)e.a].visible) {
    cur_.dur = 0;  // not on the map (or out of sight): nothing to show
    return true;
  }
  const UnitView& v = units[(size_t)e.a];
  addFloat(rankUpText(b, e), v.px + tile_ / 2.0f, v.py - 2, kRankColor, 1.5f);
  OTE_INFO("rank up: %s - %s", b.units[(size_t)e.a].name.c_str(), rankUpText(b, e).c_str());
  cur_.dur = kRankDur;
  return true;
}

bool BattleView::progressRanks(const Battle&) {
  const Event& e = cur_.ev;
  if (e.type != EventType::RankUp) return false;
  if (e.a < 0 || (size_t)e.a >= units.size() || !units[(size_t)e.a].visible) return true;
  const UnitView& v = units[(size_t)e.a];
  hasFocus = true;
  focusX = v.px + tile_ / 2.0f;
  focusY = v.py + tile_ / 2.0f;
  return true;
}

bool BattleView::finishRanks(const Battle&) { return cur_.ev.type == EventType::RankUp; }

}  // namespace ote
