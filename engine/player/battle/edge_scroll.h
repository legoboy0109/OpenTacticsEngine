// Scrolling the battle map with a mouse: push the pointer against the edge of the picture and the map moves.
// (docs/PORTING.md "Pointer scrolling"). Only a mouse does this; keys, pad and touch have their own ways.
//
// The pointer must be IN THE BAND along an edge (not just over a square in the outer rows), stay there for the dwell time, and
// then the map moves at a steady speed until the pointer leaves the band or the window. Nothing here knows the scene: it gets
// the pointer position and says how far the camera moves this frame.
#pragma once
#include <algorithm>
#include <cmath>

#include "battle/layout.h"
#include "platform/platform.h"

namespace ote {

class EdgeScroll {
 public:
  // The three numbers that make the feel (change them here only). The dwell and the speed are the first values (0.25 s, 8 tiles/s):
  // a "too fast" complaint on 2026-11-21 lowered them to 0.5 s and 5 tiles/s, but it was about the menu LISTS, not the map
  // (ui/row_list.h), so they went back. The band is 1/4 tile: 1/8 was tried and could not be used in a browser, where the pointer
  // slides out of the frame instead of stopping at the edge, so it has to be RESTED inside the band; 1/8 tile is ~9 screen pixels
  // at 3x (tools/webtest/suites/edgescroll.mjs rests 12 px inside).
  static constexpr float kBandTiles = 0.25f;       // the band along an edge is this fraction of a tile wide (at least 2 pixels)
  static constexpr float kDwellSeconds = 0.25f;    // the pointer must rest in the band this long before the map starts to move
  static constexpr float kTilesPerSecond = 8.0f;   // then it moves this many tiles a second, whatever the frame rate

  // A mouse is at (x, y) in logical pixels; kPointerGone (or any negative position) = it left the window.
  void point(int x, int y) {
    // (The dwell is counted in step(), which sees whether the pointer is in a band; moving inside the band keeps it.)
    if (x < 0 || y < 0) {
      here_ = false;
      return;
    }
    here_ = true;
    x_ = x;
    y_ = y;
  }
  bool present() const { return here_; }
  int x() const { return x_; }
  int y() const { return y_; }
  // A key, a pad button or a finger: the pointer is not what drives the picture now.
  void forget() {
    here_ = false;
    held_ = 0;
  }
  float held() const { return held_; }

  // Which way the pointer pushes: -1 left / up, +1 right / down, 0 none. The picture is W x H with the map viewport vw x vh in
  // its top left corner (the panel and the hint line lie beyond it): the band of the viewport's own edge counts, and so does
  // the band at the very edge of the picture.
  static void push(const Layout& L, int x, int y, int& hx, int& hy) {
    hx = hy = 0;
    if (x < 0 || y < 0 || x >= L.W || y >= L.H) return;
    const int band = std::max(2, (int)(L.tile * kBandTiles));
    if (x < band) hx = -1;
    else if (x >= L.W - band || (x < L.vw && x >= L.vw - band)) hx = 1;
    if (y < band) hy = -1;
    else if (y >= L.H - band || (y < L.vh && y >= L.vh - band)) hy = 1;
  }

  // Once per frame. `allowed` = the scene is in a state where the map may be scrolled (no menu open, ...). Returns how far
  // the camera moves in map pixels (dx, dy; 0 when it stays).
  void step(double dt, const Layout& L, bool allowed, float& dx, float& dy) {
    dx = dy = 0;
    int hx = 0, hy = 0;
    if (here_ && allowed) push(L, x_, y_, hx, hy);
    if (!hx && !hy) {
      held_ = 0;
      return;
    }
    const float before = held_;
    held_ += (float)dt;
    if (held_ < kDwellSeconds) return;
    // The time past the dwell moves the map (so a long frame does not lose the part of it that was already due).
    const float moving = before >= kDwellSeconds ? (float)dt : held_ - kDwellSeconds;
    const float d = moving * kTilesPerSecond * (float)L.tile;
    dx = hx * d;
    dy = hy * d;
  }

 private:
  bool here_ = false;
  int x_ = 0, y_ = 0;
  float held_ = 0;  // seconds the pointer has been in the band
};

}  // namespace ote
