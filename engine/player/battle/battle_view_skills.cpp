// View hooks for SkillLearned, SkillActivated and UnitHurt (step U1). Contract of the three: see the story hooks
// (battle_view_story.cpp). docs/SRPG.md "Player UI > During a fight".
//
//  - SkillActivated: the skill's name floats over its owner. The core puts the entry before the Attack it belongs to
//    (a strike's activations) or after the combat (the end-of-combat ones), so the label is read in event order.
//  - UnitHurt: "-N" over the receiver and its hit points run down (the skill that hurt it was announced just before).
//  - SkillLearned: a level-up box lists the skills learned right after it ("New skill: Heavy Blow", see
//    addLearnedToBox); one learned any other way (a story's teachSkill) floats "Learned Heavy Blow" over the unit.
#include <algorithm>

#include "battle/battle_view.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr float kLabelDur = 0.45f, kHurtDur = 0.35f, kLearnedDur = 0.6f;
const Color kSkillColor(255, 225, 110), kHurtColor(255, 140, 120), kLearnedColor(150, 255, 170);

std::string skillName(const Battle& b, int skill) {
  return skill >= 0 && (size_t)skill < b.db->skills.size() ? b.db->skills[(size_t)skill].name : "skill";
}

}  // namespace

void BattleView::addLearnedToBox(const Battle& b, int unit) {
  while (!queue_.empty() && queue_.front().type == EventType::SkillLearned && queue_.front().a == unit) {
    levelUp.lines.push_back("New skill: " + skillName(b, queue_.front().b));
    OTE_INFO("level-up box: %s", levelUp.lines.back().c_str());
    queue_.pop_front();
  }
}

bool BattleView::beginSkills(const Battle& b) {
  const Event& e = cur_.ev;
  const bool mine = e.type == EventType::SkillActivated || e.type == EventType::UnitHurt || e.type == EventType::SkillLearned;
  if (!mine) return false;
  if (e.a < 0 || (size_t)e.a >= units.size() || !units[(size_t)e.a].visible) {  // not on the map (or out of sight): nothing to show
    cur_.dur = 0;
    return true;
  }
  UnitView& v = units[(size_t)e.a];
  const float x = v.px + tile_ / 2.0f, y = v.py - 2;
  switch (e.type) {
    case EventType::SkillActivated:
      addFloat(skillName(b, e.b), x, y, kSkillColor, 1.1f);
      OTE_INFO("skill activated: %s - %s", b.units[(size_t)e.a].name.c_str(), skillName(b, e.b).c_str());
      cur_.dur = kLabelDur;
      break;
    case EventType::UnitHurt:
      addFloat("-" + intStr(e.b), x, y, kHurtColor, 0.9f);
      OTE_INFO("unit hurt: %s -%d", b.units[(size_t)e.a].name.c_str(), e.b);
      cur_.hpFrom = v.hp;
      cur_.hpTo = std::max(0, v.hp - e.b);
      cur_.dur = kHurtDur;
      break;
    default:  // SkillLearned outside a level-up box
      addFloat("Learned " + skillName(b, e.b), x, y, kLearnedColor, 1.4f);
      OTE_INFO("skill learned: %s - %s", b.units[(size_t)e.a].name.c_str(), skillName(b, e.b).c_str());
      cur_.dur = kLearnedDur;
      break;
  }
  return true;
}

bool BattleView::progressSkills(const Battle&) {
  const Event& e = cur_.ev;
  if (e.type != EventType::SkillActivated && e.type != EventType::UnitHurt && e.type != EventType::SkillLearned) return false;
  if (e.a < 0 || (size_t)e.a >= units.size() || !units[(size_t)e.a].visible) return true;
  UnitView& v = units[(size_t)e.a];
  if (e.type == EventType::UnitHurt) {
    const float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
    v.hp = cur_.hpFrom + (int)((cur_.hpTo - cur_.hpFrom) * p);
  }
  hasFocus = true;  // the camera follows the unit the label is over
  focusX = v.px + tile_ / 2.0f;
  focusY = v.py + tile_ / 2.0f;
  return true;
}

bool BattleView::finishSkills(const Battle&) {
  const Event& e = cur_.ev;
  if (e.type != EventType::SkillActivated && e.type != EventType::UnitHurt && e.type != EventType::SkillLearned) return false;
  if (e.type == EventType::UnitHurt && e.a >= 0 && (size_t)e.a < units.size() && units[(size_t)e.a].visible)
    units[(size_t)e.a].hp = cur_.hpTo;
  return true;
}

}  // namespace ote
