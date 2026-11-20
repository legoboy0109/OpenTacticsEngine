// The direction pick after Wait in a facing game (docs/SRPG.md "Facing", "Player"): a small, skippable question, never a screen.
//   * Squares: an arrow key (or a tap on a neighbouring square) turns the unit that way and ends its turn at once; Confirm or Cancel keeps its own
//     direction and ends the turn (so one press dismisses it).
//   * Hexagons (six directions, four arrows): Left / Up and Right / Down (and Prev / Next) turn the highlight, Confirm makes the unit look that way, Cancel
//     keeps its own direction; a tap on a neighbouring hexagon picks it at once.
// The neighbouring squares are painted with the overlay every kind of view already draws (blue), the chosen one red. The rules are Action::wait(unit, dir).
#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "core/grid/facing.h"

namespace ote {

void BattleScene::beginFacing() {
  const Unit& u = battle_.units[(size_t)sel_];
  menu_.close();
  mode_ = Mode::Facing;
  facingDir_ = u.facing >= 0 ? u.facing : 0;
  OTE_INFO("facing pick: %s looks %s", u.name.c_str(), directionName(battle_.grid, u.facing).c_str());
}

void BattleScene::commitFacing(int dir) {
  const Unit& u = battle_.units[(size_t)sel_];
  const std::string was = directionName(battle_.grid, u.facing);
  Action wait = Action::wait(sel_, dir >= 0 && dir != u.facing ? dir : -1);
  if (!submit(wait)) return;
  OTE_INFO("facing chosen: %s looks %s (was %s)", battle_.units[(size_t)sel_].name.c_str(), directionName(battle_.grid, battle_.units[(size_t)sel_].facing).c_str(), was.c_str());
  finishOrder();
}

void BattleScene::facingButton(Button b) {
  const Grid& g = battle_.grid;
  if (b == Button::Cancel || (b == Button::Confirm && !g.isHex())) {  // keep: one press dismisses the question
    uiCue(b == Button::Cancel ? Cue::Cancel : Cue::Confirm);
    commitFacing(-1);
    return;
  }
  if (g.isHex()) {
    const int n = g.directionCount();
    if (b == Button::Left || b == Button::Up || b == Button::Prev) { facingDir_ = (facingDir_ + n - 1) % n; uiCue(Cue::Cursor); }
    else if (b == Button::Right || b == Button::Down || b == Button::Next) { facingDir_ = (facingDir_ + 1) % n; uiCue(Cue::Cursor); }
    else if (b == Button::Confirm) { uiCue(Cue::Confirm); commitFacing(facingDir_); }
    return;
  }
  int dx = 0, dy = 0;
  if (b == Button::Up) dy = -1;
  else if (b == Button::Down) dy = 1;
  else if (b == Button::Left) dx = -1;
  else if (b == Button::Right) dx = 1;
  else return;
  for (int i = quarterTurns(); i > 0; --i) {  // a turned view keeps the arrows' directions on screen, as for the cursor
    const int t = dx;
    dx = dy;
    dy = -t;
  }
  uiCue(Cue::Confirm);
  commitFacing(dy < 0 ? 0 : dx > 0 ? 1 : dy > 0 ? 2 : 3);
}

void BattleScene::facingPointer(Cell c) {
  const Unit& u = battle_.units[(size_t)sel_];
  if (c == u.cell) {
    uiCue(Cue::Confirm);
    commitFacing(-1);
    return;
  }
  const int dir = battle_.grid.directionTo(u.cell, c);
  if (dir < 0) return;  // somewhere else on the map: not an answer
  uiCue(Cue::Confirm);
  commitFacing(dir);
}

Overlay BattleScene::faceOverlay() const {
  Overlay ov;
  if (sel_ < 0) return ov;
  const Grid& g = battle_.grid;
  const size_t n = (size_t)g.cellCount();
  ov.reach.assign(n, 0);
  ov.attack.assign(n, 0);
  const Unit& u = battle_.units[(size_t)sel_];
  for (int d = 0; d < g.directionCount(); ++d) {
    const Cell c = g.neighbor(u.cell, d);
    if (c == kNoCell) continue;
    (d == facingDir_ && g.isHex() ? ov.attack : ov.reach)[(size_t)c] = 1;  // (on squares the arrows decide at once: no highlight to move)
  }
  return ov;
}

std::string BattleScene::facingHint() const {
  return battle_.grid.isHex() ? "{dpad}:turn  {ok}:face  {cancel}:keep" : "{dpad}:face  {ok}:keep";
}

}  // namespace ote
