// Generic "pick a unit" and "pick a square" modes (docs/SRPG.md "Player UI > Targeting"), shared by abilities, Rescue,
// Give, Take, Pair up, Support (units) and Drop and `cell` abilities (squares). The group that opens one gives the
// legal candidates and what to do with the choice; the cycling logic is PickList (srpg_pick.h).
//
// Attack keeps its own Mode::Targeting (it needs the forecast); this one has no forecast.
#include "battle/battle_scene.h"
#include "core/base/log.h"

namespace ote {

void BattleScene::beginPickUnit(std::vector<int> units, std::function<void(int)> chosen, PickMark mark) {
  if (units.empty() || sel_ < 0) return;  // nothing legal: the caller keeps its menu
  pickCells_ = false;
  pick_.begin(std::move(units), -1);
  pickChosen_ = std::move(chosen);
  pickMark_ = std::move(mark);
  menu_.close();
  mode_ = Mode::PickUnit;
  OTE_INFO("pick unit: %d candidates", pick_.size());
  pickMoved();
}

void BattleScene::beginPickCell(std::vector<Cell> cells, std::function<void(Cell)> chosen, PickMark mark) {
  if (cells.empty() || sel_ < 0) return;
  pickCells_ = true;
  pick_.begin(std::vector<int>(cells.begin(), cells.end()), -1);
  pickChosen_ = [chosen](int c) { chosen((Cell)c); };
  pickMark_ = std::move(mark);
  menu_.close();
  mode_ = Mode::PickCell;
  OTE_INFO("pick square: %d candidates", pick_.size());
  pickMoved();
}

// Cursor on the highlighted candidate; the legal squares in the blue mask (square picks), then the group's own marks.
void BattleScene::pickMoved() {
  int cur = pick_.current();
  cursor_ = pickCells_ ? (Cell)cur : battle_.units[(size_t)cur].cell;
  int n = battle_.grid.cellCount();
  overlay_ = Overlay();
  overlay_.reach.assign((size_t)n, 0);
  overlay_.attack.assign((size_t)n, 0);
  if (pickCells_)
    for (int c : pick_.items()) overlay_.reach[(size_t)c] = 1;
  if (pickMark_) pickMark_(cur, overlay_);
}

void BattleScene::pickButton(Button b) {
  PickList::Result res = pick_.onButton(b);
  if (res == PickList::Moved) {
    uiCue(Cue::Cursor);
    pickMoved();
  } else if (res == PickList::Cancelled) {
    uiCue(Cue::Cancel);
    std::function<void()> stepBack = std::move(pickBack_);
    pickChosen_ = nullptr;
    pickMark_ = nullptr;
    pickBack_ = nullptr;
    pickWhy_ = nullptr;
    overlay_ = Overlay();
    cursor_ = battle_.units[(size_t)sel_].cell;
    if (stepBack) stepBack();  // a second stage of a staff: one step back, not all the way
    else openActionMenu();
  } else if (res == PickList::Chosen) {
    uiCue(Cue::Confirm);
    std::function<void(int)> chosen = std::move(pickChosen_);
    int value = pick_.current();
    pickChosen_ = nullptr;
    pickMark_ = nullptr;
    pickBack_ = nullptr;
    pickWhy_ = nullptr;
    overlay_ = Overlay();
    mode_ = Mode::Idle;  // the callback decides what is next (finishOrder, another menu)
    if (chosen) chosen(value);
  }
}

void BattleScene::pickPointer(int x, int y) {
  Cell c = cellAt(x, y);
  if (c == kNoCell) return;
  int value = pickCells_ ? (int)c : battle_.unitAt(c);
  PickList::Result res = pick_.tap(value);
  if (res == PickList::Moved) pickMoved();
  else if (res == PickList::Chosen) pickButton(Button::Confirm);
  else if (pickWhy_) {  // a tap on something that cannot be picked: say why
    const std::string why = pickWhy_(value);
    if (!why.empty()) {
      OTE_INFO("pick refused: %s", why.c_str());
      say(why);
    }
  }
}

std::string BattleScene::srpgHint() const {
  if (mode_ == Mode::UnitInfo) return "Left/Right:page  X:close";
  return pickCells_ ? "Arrows/Tab:next square  Z:choose  X:back" : "Arrows/Tab:next unit  Z:choose  X:back";
}

}  // namespace ote
