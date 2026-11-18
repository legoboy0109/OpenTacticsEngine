// The weapon list of game.json "attackWeaponChoice": "pick" (docs/SRPG.md "Menu options"). Attack opens a list of the weapons the
// unit carries (and can wield) that reach at least one enemy from where it stands, the weapon in hand preselected; picking one
// shows the enemies that weapon reaches (Mode::Targeting, forecast with that weapon); Cancel there returns to the list, Cancel in the
// list returns to the action menu with nothing changed. Confirming the attack submits the same Equip order the Items menu does (when
// the weapon is not the one in hand), then the Attack, so the rules, saves, replays and the event stream see exactly what they would
// had the player equipped by hand first.
//
// The list is shown even with one weapon (the classic look; the weapon in hand is preselected, so on a touch screen one tap on the
// first row, which is already highlighted, goes on).
#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

void BattleScene::openWeaponPick(int preferTarget, int selectRow) {
  if (sel_ < 0) return;
  const Unit& u = battle_.units[(size_t)sel_];
  battle_.attackOptions(sel_, u.cell, weaponOpts_);
  weaponRows_ = weaponRows(battle_, sel_, u.cell, weaponOpts_);
  if (weaponRows_.empty()) return;  // nothing to attack with: the caller keeps its menu
  weaponPrefer_ = preferTarget;
  weaponRow_ = -1;
  if (selectRow < 0 || selectRow >= (int)weaponRows_.size()) selectRow = startRow(weaponRows_, weaponOpts_, preferTarget);
  std::vector<PanelRow> rows;
  std::string log;
  for (const WeaponRow& w : weaponRows_) {
    PanelRow row;
    row.label = w.name;
    row.extra = w.uses;
    row.detail = w.stats;
    row.mark = w.equipped;
    rows.push_back(row);
    log += (log.empty() ? "" : ", ") + w.name + (w.uses.empty() ? "" : " " + w.uses) + (w.equipped ? " [equipped]" : "") + " (" + w.stats + ")";
  }
  menu_.close();
  mode_ = Mode::WeaponPick;
  overlay_ = Overlay();
  cursor_ = u.cell;
  // On a narrow screen (a phone) the list takes the whole width, over the info panel: names and uses need the room.
  weaponPanel_.setArea(0, 0, layout_.vw < 300 ? layout_.W : layout_.vw, layout_.vh);
  weaponPanel_.open(WeaponWords::kTitle, "", rows, "", selectRow);
  OTE_INFO("weapon list: %s", log.c_str());
  int x, y, w, h;  // where the rows are, for the screen tests that tap them
  weaponPanel_.selectedBox(host_.renderer(), x, y, w, h);
  OTE_INFO("weapon list rows: row0@%d,%d step=%d sel=%d", x + w / 2, y + h / 2 - weaponPanel_.sel() * h, h, weaponPanel_.sel());
}

void BattleScene::weaponPickInput(const InputEvent& e) {
  const ItemPanel::Result res = weaponPanel_.onInput(e, host_.renderer());
  if (res == ItemPanel::Back) {  // nothing was changed: the action menu again
    weaponPanel_.close();
    openActionMenu();
  } else if (res == ItemPanel::Picked) {
    weaponPicked(weaponPanel_.sel());
  }
}

void BattleScene::weaponPicked(int row) {
  if (row < 0 || row >= (int)weaponRows_.size()) return;
  targets_ = optionsOfWeapon(weaponOpts_, weaponRows_[(size_t)row].weapon);
  if (targets_.empty()) return;
  weaponPanel_.close();
  OTE_INFO("weapon picked: %s (%d targets)", weaponRows_[(size_t)row].name.c_str(), (int)targets_.size());
  enterTargeting(weaponPrefer_);
  weaponRow_ = row;  // after enterTargeting: Cancel in the target step comes back to this row
}

bool BattleScene::equipForAttack() {
  if (weaponRow_ < 0 || weaponRow_ >= (int)weaponRows_.size()) return true;
  const WeaponRow& row = weaponRows_[(size_t)weaponRow_];
  if (!needsEquip(battle_, sel_, row)) return true;
  OTE_INFO("equipping %s before the attack", row.name.c_str());
  return submit(Action::equip(sel_, row.slot));
}

}  // namespace ote
