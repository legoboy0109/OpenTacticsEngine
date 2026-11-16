// Action menu: ability entries, ids kAbilityBase + n (step U3). Empty until then.
#include "battle/battle_scene.h"

namespace ote {

void BattleScene::addAbilityEntries(std::vector<MenuItem>&) {}
bool BattleScene::abilityChosen(int) { return false; }

}  // namespace ote
