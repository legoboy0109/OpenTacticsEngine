// Mouse scrolling of the battle map (edge_scroll.h: the numbers; docs/PORTING.md "Pointer scrolling").
// Hovering a square only moves the cursor; the camera does not chase a cursor the pointer itself rests on (followCamera).
// The map scrolls from the pointer only when it is pushed against the edge of the picture for a moment.
#include "battle/battle_prep_ui.h"
#include "battle/battle_scene.h"
#include "core/base/log.h"

namespace ote {

// What the scene sees of the pointer, from the top of onInput: a mouse is tracked, a key, a pad button or a finger makes it
// let go (a finger held near an edge must not scroll, and a keyboard player is not scrolled by a mouse left at the border).
void BattleScene::trackPointer(const InputEvent& e) {
  const bool pointerEvent = e.type == InputEvent::PointerMove || e.type == InputEvent::PointerDown || e.type == InputEvent::PointerUp;
  if (pointerEvent) {
    if (e.device == InputDevice::Touch) {
      edge_.forget();
      hovered_ = kNoCell;
    } else {
      edge_.point(e.x, e.y);
    }
  } else if (e.type == InputEvent::ButtonDown) {
    edge_.forget();
    hovered_ = kNoCell;
  }
}

// True while the cursor is the square the mouse last rested on and it is on screen: the camera has no reason to chase it (the
// "keep two tiles of margin" rule is for a cursor moved by keys or the game, not for one under the pointer).
bool BattleScene::cursorUnderPointer() const {
  if (hovered_ == kNoCell || hovered_ != cursor_ || !battle_.grid.valid(cursor_)) return false;
  int x, y, w, h;
  cellScreenBox(cursor_, x, y, w, h);
  return x + w > 0 && y + h > 0 && x < layout_.vw && y < layout_.vh;
}

bool BattleScene::edgeScrollAllowed() const {
  if (covered_ || view_.dialogue.active || fightScene_.active() || view_.busy() || !humanTurn() || proj_.free3d()) return false;
  // The modes where the pointer itself carries the cursor (a targeting or picking mode has its own cursor the camera follows).
  if (mode_ == Mode::Prep) {
    if (!prepOnMap()) return false;
  } else if (mode_ != Mode::Idle && mode_ != Mode::Selected && mode_ != Mode::Facing) {
    return false;
  }
  if (map3d_) {  // the turn buttons sit in the bottom left corner: the pointer over one belongs to the button
    for (int i = 0; i < 2; ++i) {
      int x, y, w, h;
      turnButtonBox(i, x, y, w, h);
      if (edge_.x() >= x && edge_.y() >= y && edge_.x() < x + w && edge_.y() < y + h) return false;
    }
  }
  return true;
}

void BattleScene::updateEdgeScroll(double dt) {
  float dx, dy;
  edge_.step(dt, layout_, edgeScrollAllowed(), dx, dy);
  if (dx == 0 && dy == 0) return;
  const float maxX = (float)(proj_.worldW() - layout_.vw), maxY = (float)(proj_.worldH() - layout_.vh);
  if (maxX > 0) cam_.x = std::max(0.0f, std::min(cam_.x + dx, maxX));  // (a map smaller than the view stays centred)
  if (maxY > 0) cam_.y = std::max(0.0f, std::min(cam_.y + dy, maxY));
  // The square under the pointer changes as the map slides under it. A pointer in the panel or on the hint line (the picture's
  // own edge) counts as resting on the nearest square of the view: the cursor must stay where the camera is looking.
  hoverAt(std::min(edge_.x(), layout_.vw - 1), std::min(edge_.y(), layout_.vh - 1));
}

void BattleScene::logCamera() {
  const int x = cam_.ix(), y = cam_.iy();
  if (x != camLogX_ || y != camLogY_) {
    camLogX_ = x;
    camLogY_ = y;
    camStill_ = 0;
  } else if (camStill_ >= 0 && ++camStill_ == 20) {
    camStill_ = -1;
    OTE_INFO("camera: %d,%d", x, y);
  }
}

void BattleScene::hoverAt(int x, int y) {
  const Cell c = cellAt(x, y);
  if (c == kNoCell) return;
  hovered_ = c;
  if (c == cursor_) return;
  cursor_ = c;
  if (mode_ == Mode::Prep && prepOnMapView()) prepViewLook();
}

}  // namespace ote
