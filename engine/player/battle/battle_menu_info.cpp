// "Info" in the action menu (kInfo) and "Unit" in the battle menu (kUnitEntry), both opening the unit details through
// openUnitInfo (battle_srpg_unitinfo.cpp). Step U1.
//
// Interpretation: the entries only exist in a game that uses some strategy-RPG data (skills, characters, abilities,
// supports, promotion or rescue). Every other game keeps its menus exactly as they were.
#include "battle/battle_scene.h"
#include "core/base/log.h"

namespace ote {
namespace {

bool usesSrpg(const Database& db) {
  if (!db.skills.empty() || !db.characters.empty() || !db.abilities.empty() || !db.supports.pairs.empty()) return true;
  if (db.rules.rescue.mode != RescueMode::Off) return true;
  for (const UnitTypeDef& t : db.unitTypes)
    if (!t.promotions.empty()) return true;
  return false;
}

}  // namespace

void BattleScene::addInfoEntry(std::vector<MenuItem>& items) {
  if (!usesSrpg(session_.db)) return;
  items.push_back({"Info", "", true, kInfo});
  // The line the player tests read (this is the last group before Wait, which is always the last entry).
  std::string log;
  for (const MenuItem& it : items) log += it.label + ", ";
  OTE_INFO("action menu: %sWait", log.c_str());
}

bool BattleScene::infoChosen(int id) {
  if (id != kInfo || sel_ < 0) return false;
  openUnitInfo(sel_);
  return true;
}

// The unit under the cursor, if the player may see it (alive, on the map, not hidden by fog).
void BattleScene::addUnitEntry(std::vector<MenuItem>& items) {
  if (!usesSrpg(session_.db)) return;
  const int uid = battle_.unitAt(cursor_);
  if (uid >= 0 && battle_.units[(size_t)uid].alive && (size_t)uid < view_.units.size() && view_.units[(size_t)uid].visible)
    items.push_back({"Unit", "", true, kUnitEntry});
}

bool BattleScene::unitEntryChosen(int id) {
  if (id != kUnitEntry) return false;
  const int uid = battle_.unitAt(cursor_);
  if (uid < 0) return false;
  openUnitInfo(uid);
  return true;
}

}  // namespace ote
