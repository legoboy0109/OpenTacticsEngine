// The battle view's side of a story scene on another map (docs/EVENTS.md "Showing another map"). The view does not play the shown map's
// entries itself: it stops at MapShown, the battle screen (battle_other_map.cpp) takes those entries out of the queue and plays them on a
// view of their own, over the shown map's Battle, and when that view reaches MapReturned the screen swaps back and this view goes on.
#include "battle/battle_view.h"
#include "core/base/log.h"

namespace ote {

bool BattleView::beginOtherMap() {
  if (cur_.ev.type != EventType::MapShown && cur_.ev.type != EventType::MapReturned) return false;
  cur_.dur = 1e9f;  // until the battle screen has swapped the maps (mapSwapped)
  return true;
}

int BattleView::mapRequest() const {
  if (!cur_.active || fight.active) return 0;
  return cur_.ev.type == EventType::MapShown ? 1 : cur_.ev.type == EventType::MapReturned ? -1 : 0;
}

std::vector<Event> BattleView::takeShownMapRun() {
  std::vector<Event> run;
  while (!queue_.empty()) {
    const Event e = queue_.front();
    queue_.pop_front();
    run.push_back(e);
    if (e.type == EventType::MapReturned) break;
  }
  if (run.empty() || run.back().type != EventType::MapReturned) {  // (the rules always end a shown map; never leave the other view waiting)
    OTE_WARN("scene: the shown map's entries have no end; going back after them");
    Event back;
    back.type = EventType::MapReturned;
    back.a = cur_.ev.a;
    run.push_back(back);
  }
  return run;
}

void BattleView::mapSwapped() {
  if (mapRequest() == 0) return;
  cur_.active = false;
}

void BattleView::startOnMap(const Battle& b, Cell cell) {
  if (b.grid.valid(cell)) {
    cellPos(b, cell, stickyX_, stickyY_);
    stickyX_ += tile_ / 2.0f;
    stickyY_ += tile_ / 2.0f;
  } else {
    stickyX_ = (float)b.grid.width() * tile_ / 2.0f;
    stickyY_ = (float)b.grid.height() * tile_ / 2.0f;
  }
  stickyFocus_ = true;
  hasFocus = true;
  focusX = stickyX_;
  focusY = stickyY_;
  focusMs = 0;  // a jump: the camera is there when the map appears
  ++focusSerial;
}

}  // namespace ote
