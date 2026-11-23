// Geometry, navigation and scrolling of a vertical list of equally tall rows: which rows are visible, which row a pointer
// hit, Up/Down stepping, and the ways a mouse scrolls a long list. Drawing the rows themselves is up to the scene.
//
// The scroll position (`first()`) is STORED: it moves only when the selection would leave the visible rows (a key or pad
// step, a jump made by the scene), or when the list is scrolled on purpose (the mouse wheel, the "^" / "v" markers, a
// finger dragging the rows). Selecting the row under a resting pointer never scrolls: the old rule ("keep the selected row
// in the middle") made a list race by itself under a mouse, because every row the pointer selected near the bottom re-centred
// the list and slid the next row under the pointer (docs/PORTING.md "Pointer scrolling").
//
// A scene that owns a RowList passes every input event to `input()` first (it takes the wheel, a click on a marker and a
// finger's drag) and goes on with its own handling when that returns false. The marker dwell runs on the list clock, which
// the App advances once a frame (`advanceListClock`), so no scene has to tick its lists.
#pragma once
#include "platform/platform.h"

namespace ote {

// The feel of list scrolling by mouse (change them here only; the owner tunes these).
struct ListFeel {
  static constexpr int kWheelRows = 1;                  // one notch of the mouse wheel scrolls this many rows
  static constexpr float kMarkerDwellSeconds = 0.3f;    // the pointer rests on a "^" / "v" marker this long before it scrolls
  static constexpr float kMarkerRowsPerSecond = 6.0f;   // then the list moves this many rows a second while it rests there
};

// The list clock: seconds of frame time since the start. The App adds every frame's time; unit tests add their own.
double listClock();
void advanceListClock(double dt);

struct RowList {
  // Area the rows may occupy (logical pixels) and the height of one row.
  int x = 0, y = 0, w = 0, h = 0, rowH = 1;
  int count = 0, sel = 0;

  int visible() const { return h / rowH < 1 ? 1 : h / rowH; }
  // Index of the first visible row (see the top of the file for when it moves).
  int first() const;
  int rowY(int index) const { return y + (index - first()) * rowH; }
  // Row under a point, or -1.
  int rowAt(int px, int py) const;
  // Up/Down/Prev/Next move the selection (wrapping) and keep it roughly centred, as keys always did; returns true if handled.
  bool step(Button b);
  // Show the selection centred the next time the list is drawn (a list opened again with new rows).
  void centre() { centre_ = true; }
  // Scrolls by `rows` (negative = up) without touching the selection; false when it could not move.
  bool scrollBy(int rows);
  // The wheel, the markers and a finger's drag. True = the event was used for scrolling (the scene ignores it).
  bool input(const InputEvent& e);
  // "^" / "v" markers just outside the area when rows are hidden.
  void drawMore(Renderer& r, Color c) const;
  // The marker a point is on: -1 = "^", +1 = "v", 0 = none (a marker counts only while it is shown).
  int markerAt(int px, int py) const;

 private:
  int topFor() const;      // the stored position after the selection rule (no marker scrolling)
  void dwell() const;      // the rows a resting pointer on a marker has earned since the last call
  void forget();

  mutable int top_ = 0;
  mutable int seenSel_ = -1;   // the selection the position was last checked against
  mutable bool centre_ = true; // centre the selection at the next check (a new list, a key step)
  mutable int logged_ = -1;    // the position last logged
  mutable int markH_ = 8;      // the marker strip's height (the line height drawMore last drew with)
  // The pointer as the list knows it (for the marker dwell and a finger's drag).
  bool here_ = false, pressed_ = false, touch_ = false;
  int px_ = 0, py_ = 0;
  mutable int holdDir_ = 0;      // the marker the pointer rests on (-1 / +1), 0 = none
  mutable double holdStart_ = 0; // list clock when it came to rest there
  mutable int holdMoved_ = 0;    // rows scrolled by this rest so far
  int dragTop_ = 0, dragY_ = 0;  // a finger's drag: the position and the finger's y when it went down
  bool dragging_ = false;
};

}  // namespace ote
