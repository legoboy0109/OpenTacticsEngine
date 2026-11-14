// How the battle view plays the story entries of the event stream (docs/EVENTS.md "What the player
// gets"): Dialogue (waits for the player), Focus, UnitRemoved, UnitRelocated, SideChanged and
// ObjectiveChanged. UnitAdded shares the pop-in of UnitBuilt in battle_view.cpp.
// Each hook returns true when the entry was one of these (and so fully handled here).
#include <algorithm>

#include "battle/battle_view.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr float kDialogueAutoSeconds = 1.0f;  // autoplay: a line dismisses itself after this long
constexpr float kRemoveFade = 0.4f;
constexpr float kSlideSecPerCell = 0.05f;
constexpr int kSlideMaxCells = 8;             // a unit moved further than this is snapped there
constexpr float kFlashDur = 0.5f;
constexpr float kFocusDur = 0.5f;             // the camera has this long to arrive
constexpr float kNoticeDur = 2.4f;

}  // namespace

bool BattleView::beginStory(const Battle& b) {
  const Event& e = cur_.ev;
  switch (e.type) {
    case EventType::Dialogue: {
      dialogue.line = dialogueLine(b, e);
      ++dialogue.serial;
      // An empty line has nothing to show; a skipped run passes every line without showing it.
      if (skipping_ || (dialogue.line.text.empty() && dialogue.line.name.empty())) {
        cur_.dur = 0;
        return true;
      }
      dialogue.active = true;
      // Everything after this entry waits: the view's queue does not advance until the player dismisses
      // the line (the scene calls skip()), or, in autoplay, until the timer runs out.
      cur_.dur = autoplay_ ? kDialogueAutoSeconds : 1e9f;
      return true;
    }
    case EventType::Focus: {
      if (e.b >= 0 && (size_t)e.b < units.size() && units[(size_t)e.b].visible) {
        stickyX_ = units[(size_t)e.b].px + tile_ / 2.0f;
        stickyY_ = units[(size_t)e.b].py + tile_ / 2.0f;
      } else if (b.grid.valid(e.a)) {
        cellPos(b, e.a, stickyX_, stickyY_);
        stickyX_ += tile_ / 2.0f;
        stickyY_ += tile_ / 2.0f;
      } else {
        cur_.dur = 0;
        return true;
      }
      stickyFocus_ = true;
      cur_.dur = kFocusDur;
      return true;
    }
    case EventType::UnitRemoved: cur_.dur = kRemoveFade; return true;
    case EventType::UnitRelocated: {
      UnitView& v = units[(size_t)e.a];
      cellPos(b, e.b, v.px, v.py);  // starts from the square it left
      int dist = b.grid.distance(e.b, e.c);
      cur_.path.assign({e.b, e.c});
      cur_.dur = dist > kSlideMaxCells ? 0.0f : std::max(0.12f, kSlideSecPerCell * (float)dist);
      return true;
    }
    case EventType::SideChanged: {
      UnitView& v = units[(size_t)e.a];
      v.side = e.c;
      v.flash = 1;
      if (e.c >= 0 && (size_t)e.c < b.sides.size())
        addFloat("Joins " + b.sides[(size_t)e.c].name, v.px + tile_ / 2.0f, v.py - 2, Color::rgb(b.sides[(size_t)e.c].color),
                 1.2f);
      cur_.dur = kFlashDur;
      return true;
    }
    case EventType::ObjectiveChanged:
      if (e.a < 0) {  // the objective was cleared: nothing to announce
        cur_.dur = 0;
        return true;
      }
      banner = "New objective";
      bannerSub = b.storyText(e.a);
      bannerColor = Color(70, 110, 200);
      cur_.dur = kNoticeDur;
      return true;
    default: return false;
  }
}

bool BattleView::progressStory(const Battle& b) {
  const Event& e = cur_.ev;
  float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  switch (e.type) {
    case EventType::Dialogue:
    case EventType::ObjectiveChanged: return true;
    case EventType::Focus:
      hasFocus = true;
      focusX = stickyX_;
      focusY = stickyY_;
      return true;
    case EventType::UnitRemoved: units[(size_t)e.a].alpha = 1 - p; return true;
    case EventType::UnitRelocated: {
      UnitView& v = units[(size_t)e.a];
      float x0, y0, x1, y1;
      cellPos(b, e.b, x0, y0);
      cellPos(b, e.c, x1, y1);
      v.px = x0 + (x1 - x0) * p;
      v.py = y0 + (y1 - y0) * p;
      hasFocus = true;
      focusX = v.px + tile_ / 2.0f;
      focusY = v.py + tile_ / 2.0f;
      return true;
    }
    case EventType::SideChanged: {
      UnitView& v = units[(size_t)e.a];
      v.flash = 1 - p;
      hasFocus = true;
      focusX = v.px + tile_ / 2.0f;
      focusY = v.py + tile_ / 2.0f;
      return true;
    }
    default: return false;
  }
}

bool BattleView::finishStory(const Battle& b) {
  const Event& e = cur_.ev;
  switch (e.type) {
    case EventType::Dialogue: dialogue.active = false; return true;
    case EventType::Focus: return true;  // the camera stays (stickyFocus_) until the run is over
    case EventType::ObjectiveChanged:
      banner.clear();
      bannerSub.clear();
      return true;
    case EventType::UnitRemoved: {
      UnitView& v = units[(size_t)e.a];
      v.visible = false;
      v.hp = 0;
      v.alpha = 1;
      return true;
    }
    case EventType::UnitRelocated: {
      UnitView& v = units[(size_t)e.a];
      cellPos(b, e.c, v.px, v.py);
      return true;
    }
    case EventType::SideChanged: {
      UnitView& v = units[(size_t)e.a];
      v.side = e.c;
      v.flash = 0;
      return true;
    }
    default: return false;
  }
}

}  // namespace ote
