// View hooks for SkillLearned, SkillActivated and UnitHurt (step U1). Empty until then: the dispatcher in
// battle_view_srpg.cpp logs them. Contract of the three: see the story hooks (battle_view_story.cpp).
#include "battle/battle_view.h"

namespace ote {

bool BattleView::beginSkills(const Battle&) { return false; }
bool BattleView::progressSkills(const Battle&) { return false; }
bool BattleView::finishSkills(const Battle&) { return false; }

}  // namespace ote
