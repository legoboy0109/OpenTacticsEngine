#include "ui/row_list.h"

#include <algorithm>
#include <cstdlib>

#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {

namespace {
double& clockSlot() {
  static double t = 0;
  return t;
}
}  // namespace

double listClock() { return clockSlot(); }
void advanceListClock(double dt) {
  if (dt > 0) clockSlot() += dt;
}

int RowList::topFor() const {
  const int vis = visible();
  const int maxTop = std::max(0, count - vis);
  // Not laid out yet (no height): answer as the old rule would and decide nothing, so the first real layout still centres.
  if (h <= 0) return std::max(0, std::min(sel - vis / 2, maxTop));
  if (centre_) {
    top_ = sel - vis / 2;
    centre_ = false;
    seenSel_ = sel;
  } else if (sel != seenSel_) {  // the scene moved the selection: bring it into view only if it left it
    seenSel_ = sel;
    if (sel >= 0 && sel < count && (sel < top_ || sel >= top_ + vis)) top_ = sel - vis / 2;
  }
  top_ = std::max(0, std::min(top_, maxTop));
  if (top_ != logged_) {  // the player tests read this line (tests/list_scroll_player_test.py); a list that never moves says nothing
    if (logged_ >= 0 || top_ > 0) OTE_INFO("list scrolled: first row %d (%d of %d shown, rows at y %d)", top_, vis, count, y);
    logged_ = top_;
  }
  return top_;
}

void RowList::dwell() const {
  if (!holdDir_) return;
  if (!here_ || markerAt(px_, py_) != holdDir_) {  // the pointer left, or the marker went away (the end was reached)
    holdDir_ = 0;
    return;
  }
  const double held = listClock() - holdStart_;
  if (held < ListFeel::kMarkerDwellSeconds) return;
  // One row as the dwell ends, then one every 1/kMarkerRowsPerSecond s (frame-rate independent: a long frame moves several).
  const int due = 1 + (int)((held - ListFeel::kMarkerDwellSeconds) * ListFeel::kMarkerRowsPerSecond + 1e-6);
  while (holdMoved_ < due) {
    const int t = topFor();
    const int nt = std::max(0, std::min(t + holdDir_, std::max(0, count - visible())));
    if (nt == t) {
      holdMoved_ = due;
      break;
    }
    top_ = nt;
    ++holdMoved_;
  }
}

int RowList::first() const {
  dwell();
  return topFor();
}

int RowList::rowAt(int px, int py) const {
  if (px < x || px >= x + w || py < y || py >= y + h) return -1;
  int row = (py - y) / rowH;
  if (row >= visible()) return -1;
  row += first();
  return row < count ? row : -1;
}

bool RowList::step(Button b) {
  if (count <= 0) return false;
  if (b == Button::Up || b == Button::Prev) sel = (sel + count - 1) % count;
  else if (b == Button::Down || b == Button::Next) sel = (sel + 1) % count;
  else return false;
  centre_ = true;
  uiCue(Cue::Cursor);
  return true;
}

bool RowList::scrollBy(int rows) {
  const int t = first();
  const int nt = std::max(0, std::min(t + rows, std::max(0, count - visible())));
  top_ = nt;
  seenSel_ = sel;  // the selection may now be out of view: that is wanted, do not bring it back
  centre_ = false;
  return nt != t;
}

int RowList::markerAt(int px, int py) const {
  if (px < x || px >= x + w) return 0;
  const int t = topFor();
  if (t > 0 && py >= y - markH_ - 1 && py < y) return -1;
  if (t + visible() < count && py >= y + h && py < y + h + markH_ + 1) return 1;
  return 0;
}

void RowList::forget() {
  here_ = false;
  holdDir_ = 0;
  dragging_ = false;
}

bool RowList::input(const InputEvent& e) {
  switch (e.type) {
    case InputEvent::ButtonDown:
      if (e.button == Button::ZoomIn || e.button == Button::ZoomOut) {  // the wheel (also + / - and the triggers)
        if (count <= visible()) return false;
        scrollBy(e.button == Button::ZoomIn ? -ListFeel::kWheelRows : ListFeel::kWheelRows);
        return true;
      }
      forget();  // a key or a pad button: the pointer lets go until it moves again
      return false;
    case InputEvent::ButtonUp: return (e.button == Button::ZoomIn || e.button == Button::ZoomOut) && count > visible();
    case InputEvent::PointerMove: {
      if (e.x < 0 || e.y < 0) {  // kPointerGone: it left the window
        forget();
        return false;
      }
      // A finger only "is" somewhere while it touches (the moves a touch screen makes before a tap are not a resting pointer).
      if (e.device == InputDevice::Touch && !pressed_) return false;
      here_ = true;
      px_ = e.x;
      py_ = e.y;
      if (pressed_ && touch_) {  // a finger dragging the rows: they follow it, whole rows at a time
        if (!dragging_ && std::abs(e.y - dragY_) >= std::max(4, rowH / 2)) dragging_ = true;
        if (dragging_) {
          top_ = std::max(0, std::min(dragTop_ + (dragY_ - e.y) / rowH, std::max(0, count - visible())));
          seenSel_ = sel;
          centre_ = false;
          return true;
        }
      }
      const int d = markerAt(px_, py_);
      if (d != holdDir_) {
        holdDir_ = d;
        holdStart_ = listClock();
        holdMoved_ = 0;
      }
      return false;
    }
    case InputEvent::PointerDown: {
      pressed_ = true;
      touch_ = e.device == InputDevice::Touch;
      here_ = true;
      px_ = e.x;
      py_ = e.y;
      dragTop_ = first();
      dragY_ = e.y;
      dragging_ = false;
      const int d = markerAt(px_, py_);
      if (!d) return false;
      // A click on a marker scrolls one row; holding it there goes on after the dwell, as resting does.
      scrollBy(d);
      holdDir_ = d;
      holdStart_ = listClock();
      holdMoved_ = 0;
      return true;
    }
    case InputEvent::PointerUp: {
      pressed_ = false;
      const bool dragged = dragging_;
      dragging_ = false;
      if (touch_) forget();  // a finger lifted is nowhere
      return dragged;
    }
    default: return false;
  }
}

void RowList::drawMore(Renderer& r, Color c) const {
  const int lh = r.lineHeight();
  markH_ = lh;
  const int t = first();
  if (t > 0) drawTextCentered(r, x + w / 2, y - lh - 1, "^", c);
  if (t + visible() < count) drawTextCentered(r, x + w / 2, y + h, "v", c);
}

}  // namespace ote
