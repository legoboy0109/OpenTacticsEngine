// "Info" in the action menu (kInfo) and "Unit" in the battle menu (kUnitEntry), both opening the unit details through
// openUnitInfo (battle_srpg_unitinfo.cpp). Step U1; empty until then (no entry, so nothing changes for a game).
#include "battle/battle_scene.h"

namespace ote {

void BattleScene::addInfoEntry(std::vector<MenuItem>&) {}
bool BattleScene::infoChosen(int) { return false; }
void BattleScene::addUnitEntry(std::vector<MenuItem>&) {}
bool BattleScene::unitEntryChosen(int) { return false; }

}  // namespace ote
