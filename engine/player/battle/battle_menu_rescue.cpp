// Action menu: Rescue / Drop / Give / Take / Pair up / Switch, fixed ids kRescue.. kSwitch (step U4). Empty until then.
#include "battle/battle_scene.h"

namespace ote {

void BattleScene::addRescueEntries(std::vector<MenuItem>&) {}
bool BattleScene::rescueChosen(int) { return false; }

}  // namespace ote
