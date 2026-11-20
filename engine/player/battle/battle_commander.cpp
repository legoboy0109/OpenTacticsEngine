// The commander's part of the battle scene's menus (docs/COMMANDERS.md "Player"): "Power: <name>" entries in the map menu (greyed with what is missing, or
// "active"), the choice of one, and the hint line's "power ready" tail. The meter itself is drawn by commander_panel.cpp, the banner of a power used by
// battle_view_commander.cpp.
#include "battle/battle_scene.h"
#include "battle/commander_text.h"
#include "core/base/log.h"

namespace ote {

void BattleScene::addPowerEntries(std::vector<MenuItem>& items) {
  const int side = battle_.activeSide;
  const CommanderDef* d = battle_.commanderOf(side);
  if (!d || !humanTurn()) return;
  std::string log;
  for (size_t i = 0; i < d->powers.size(); ++i) {
    MenuItem it;
    it.label = powerEntryLabel(battle_, side, (int)i);
    it.extra = powerEntryExtra(battle_, side, (int)i, it.enabled);
    it.id = kPowerBase + (int)i;
    items.push_back(it);
    log += (log.empty() ? "" : "; ") + it.label + " (" + it.extra + ")" + (it.enabled ? "" : " [grey]");
  }
  OTE_INFO("power entries: %s (meter %s)", log.c_str(), meterText(battle_, side).c_str());
}

bool BattleScene::powerMenuChosen(int id) {
  if (id < kPowerBase || id >= kPowerBase + kMaxCommanderPowers) return false;
  const int power = id - kPowerBase;
  if (!submit(Action::usePower(power))) return false;
  return true;
}

// The idle hint line says when a power can be used: the tail of the "menu" entry is what a narrow line cuts first.
std::string BattleScene::withPowerHint(std::string line) const {
  if (!humanTurn() || !anyPowerReady(battle_, battle_.activeSide)) return line;
  size_t at = line.find("{menu}:menu");
  if (at != std::string::npos) return line.insert(at + 11, CommanderWords::kHintReady);
  at = line.find("{menu}:end turn");
  if (at != std::string::npos) return line.insert(at + 15, CommanderWords::kHintReadySpeed);
  return line;
}

}  // namespace ote
