// Mode::UnitInfo: the unit details overlay (docs/SRPG.md "Player UI > Unit details"). The overlay itself and its
// pages are UnitInfoView (unit_info.*); this is the scene glue. U1 adds the entries that call openUnitInfo
// (Info in the action menu, Unit in the battle menu) in battle_menu_info.cpp.
#include "battle/battle_scene.h"

namespace ote {

// `unit` must be one the viewing team sees (the caller checks). Opened from the action menu (sel_ >= 0) Cancel goes
// back to that menu; from the battle menu it goes back to the map.
void BattleScene::openUnitInfo(int unit) {
  unitInfoFromAction_ = mode_ == Mode::ActionMenu && sel_ >= 0;
  menu_.close();
  overlay_ = Overlay();
  unitInfo_.open(battle_, unit);
  mode_ = Mode::UnitInfo;
}

void BattleScene::unitInfoInput(const InputEvent& e) {
  if (unitInfo_.onInput(e) != UnitInfoView::Closed) return;
  uiCue(Cue::Cancel);
  if (unitInfoFromAction_) openActionMenu();
  else mode_ = Mode::Idle;
}

void BattleScene::drawUnitInfoOverlay(Renderer& r) { unitInfo_.draw(r, battle_, layout_.lh); }

}  // namespace ote
