// Aiming a line-fire or area weapon (docs/SRPG.md "Area attacks and line fire"): the squares the weapon can be aimed at and the squares
// the shot at the selected target covers, drawn with the overlay every kind of view already draws (blue / red squares, 3D views included).
#include "battle/battle_scene.h"
#include "core/grid/area.h"

namespace ote {

Overlay BattleScene::aimOverlay() const {
  Overlay ov;
  if (mode_ != Mode::Targeting || targetIdx_ < 0 || targetIdx_ >= (int)targets_.size()) return ov;
  const AttackOption& t = targets_[(size_t)targetIdx_];
  const WeaponDef& w = battle_.db->weapons[(size_t)t.weapon];
  if (!w.aimsStraight() && !w.area.on()) return ov;  // an ordinary weapon: the targets are the units, nothing to paint
  const size_t n = (size_t)battle_.grid.cellCount();
  ov.reach.assign(n, 0);
  ov.attack.assign(n, 0);
  std::vector<Cell> cells;
  battle_.aimCells(t.weapon, battle_.units[(size_t)sel_].cell, sel_, cells);
  for (Cell c : cells) ov.reach[(size_t)c] = 1;  // blue: where it can be aimed
  battle_.areaCover(t.weapon, battle_.units[(size_t)sel_].cell, t.cell, cells);
  for (Cell c : cells) {  // red: what the shot hits (the aimed square alone for line fire without an area)
    ov.reach[(size_t)c] = 0;
    ov.attack[(size_t)c] = 1;
  }
  return ov;
}

}  // namespace ote
