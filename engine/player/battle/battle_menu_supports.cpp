// Action menu: "Support: <partner>" entries, ids kSupportBase + n (step U5). Empty until then.
#include "battle/battle_scene.h"

namespace ote {

void BattleScene::addSupportEntries(std::vector<MenuItem>&) {}
bool BattleScene::supportChosen(int) { return false; }

}  // namespace ote
