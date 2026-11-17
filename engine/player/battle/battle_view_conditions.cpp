// View hooks for the condition events (docs/SRPG.md section 9 "Player"): ConditionApplied floats "Poison!" over the unit, ConditionEnded
// "Cured Poison" / "Poison ended", ConditionResisted "Immune", ConditionTick the condition's name when a unit's turn is skipped and
// "Berserk!" before a berserk unit takes its own action. The marker on the map follows the events (UnitView::conds), so it appears
// when "Poison!" does and goes when "Poison ended" does. What a condition did to the unit's hit points arrives as the usual UnitHurt
// (the "-3" and the bar) and UnitHealed, after the SkillActivated event that floats the condition's name. Each entry writes one [info]
// line (the windowless tests read them). Same contract as the story hooks: return true when the entry was ours.
#include <algorithm>

#include "battle/battle_view.h"
#include "battle/condition_text.h"
#include "core/base/log.h"

namespace ote {
namespace {
constexpr float kAppliedDur = 0.45f, kEndedDur = 0.4f, kTickDur = 0.45f;
const Color kBadColor(215, 160, 255), kGoodColor(150, 240, 170), kPlainColor(190, 195, 215), kActColor(255, 170, 120);

bool ours(EventType t) {
  return t == EventType::ConditionApplied || t == EventType::ConditionEnded || t == EventType::ConditionResisted || t == EventType::ConditionTick;
}
}  // namespace

bool BattleView::beginConditions(const Battle& b) {
  const Event& e = cur_.ev;
  if (!ours(e.type)) return false;
  const Database& db = *b.db;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size()) {
    cur_.dur = 0;
    return true;
  }
  UnitView& v = units[(size_t)e.a];
  const std::string who = b.units[(size_t)e.a].name;
  // The marker follows the event even when the unit is out of sight (the viewer's picture catches up with the rules when it is seen).
  if (e.type == EventType::ConditionApplied) {
    if (std::find(v.conds.begin(), v.conds.end(), e.b) == v.conds.end()) v.conds.push_back(e.b);
  } else if (e.type == EventType::ConditionEnded) {
    v.conds.erase(std::remove(v.conds.begin(), v.conds.end(), e.b), v.conds.end());
  }
  const std::string name = conditionName(db, e.b);
  switch (e.type) {
    case EventType::ConditionApplied:
      OTE_INFO("condition applied: %s - %s%s", who.c_str(), name.c_str(), (e.flags & kRefreshed) ? " (again)" : "");
      break;
    case EventType::ConditionEnded: {
      static const char* const kWhy[] = {"ran out", "wore off", "cured", "replaced", "struck"};
      OTE_INFO("condition ended: %s - %s (%s)", who.c_str(), name.c_str(), e.c >= 0 && e.c < 5 ? kWhy[e.c] : "?");
      break;
    }
    case EventType::ConditionResisted: OTE_INFO("condition resisted: %s - %s", who.c_str(), name.c_str()); break;
    default: OTE_INFO("condition tick: %s - %s (%s)", who.c_str(), name.c_str(), e.c == kTickActsAlone ? "acts on its own" : "cannot act"); break;
  }
  if (!v.visible) {  // not on the map (or out of sight): nothing to show
    cur_.dur = 0;
    return true;
  }
  const float x = v.px + tile_ / 2.0f, y = v.py - 2;
  switch (e.type) {
    case EventType::ConditionApplied:
      addFloat(conditionAppliedText(db, e.b), x, y, kBadColor, 1.1f);
      cur_.dur = kAppliedDur;
      break;
    case EventType::ConditionEnded: {
      const std::string text = conditionEndedText(db, e.b, e.c);
      if (!text.empty()) addFloat(text, x, y, e.c == kConditionCured ? kGoodColor : kPlainColor, 1.1f);
      cur_.dur = text.empty() ? 0 : kEndedDur;
      break;
    }
    case EventType::ConditionResisted:
      addFloat(ConditionWords::kImmune, x, y, kPlainColor, 1.0f);
      cur_.dur = kEndedDur;
      break;
    default:
      addFloat(conditionTickText(db, e.b, e.c), x, y, e.c == kTickActsAlone ? kActColor : kBadColor, 1.1f);
      cur_.dur = kTickDur;
      break;
  }
  return true;
}

bool BattleView::progressConditions(const Battle&) {
  const Event& e = cur_.ev;
  if (!ours(e.type)) return false;
  if (e.a >= 0 && (size_t)e.a < units.size() && units[(size_t)e.a].visible) {
    hasFocus = true;  // the camera follows the unit the label is over
    focusX = units[(size_t)e.a].px + tile_ / 2.0f;
    focusY = units[(size_t)e.a].py + tile_ / 2.0f;
  }
  return true;
}

bool BattleView::finishConditions(const Battle&) { return ours(cur_.ev.type); }

}  // namespace ote
