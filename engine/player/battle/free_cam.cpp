#include "battle/free_cam.h"

#include <algorithm>
#include <cmath>

namespace ote {

namespace {
const float kTurnStep = 0.2617994f;  // a tap: 15 degrees
const float kTiltStep = 0.1308997f;  // 7.5 degrees
const float kZoomStep = 1.15f;
const float kTurnSpeed = 2.2f, kTiltSpeed = 0.9f, kZoomSpeed = 1.2f;  // while held, per second
const float kDragTurn = 0.008f, kDragTilt = 0.006f;                   // per pixel dragged
const float kTwoPi = 6.2831853f;
}  // namespace

int FreeCam::slot(Button b) {
  switch (b) {
    case Button::TurnLeft: return 0;
    case Button::TurnRight: return 1;
    case Button::TiltUp: return 2;
    case Button::TiltDown: return 3;
    case Button::ZoomIn: return 4;
    case Button::ZoomOut: return 5;
    default: return -1;
  }
}

void FreeCam::press(Button b) {
  int s = slot(b);
  if (s < 0) return;
  held_[s] = true;
  switch (s) {
    case 0: yaw_ -= kTurnStep; break;
    case 1: yaw_ += kTurnStep; break;
    case 2: pitch_ += kTiltStep; break;  // up: more from above
    case 3: pitch_ -= kTiltStep; break;
    case 4: distance_ /= kZoomStep; break;
    default: distance_ *= kZoomStep; break;
  }
}

void FreeCam::releaseAll() {
  for (bool& h : held_) h = false;
}

bool FreeCam::input(const InputEvent& e) {
  if (e.type == InputEvent::CameraDrag) {
    // The picture follows the hand: dragging right brings the camera round to the left.
    yaw_ -= e.x * kDragTurn;
    pitch_ += e.y * kDragTilt;
    return true;
  }
  if (e.type != InputEvent::ButtonDown && e.type != InputEvent::ButtonUp) return false;
  int s = slot(e.button);
  if (s < 0) return false;
  if (e.type == InputEvent::ButtonDown) press(e.button);
  else held_[s] = false;
  return true;
}

void FreeCam::update(double dt, Free3DGeometry& g, float X, float Y, float Z) {
  float t = (float)dt;
  yaw_ += ((held_[1] ? 1.0f : 0.0f) - (held_[0] ? 1.0f : 0.0f)) * kTurnSpeed * t;
  pitch_ += ((held_[2] ? 1.0f : 0.0f) - (held_[3] ? 1.0f : 0.0f)) * kTiltSpeed * t;
  distance_ *= std::exp(((held_[5] ? 1.0f : 0.0f) - (held_[4] ? 1.0f : 0.0f)) * kZoomSpeed * t);
  pitch_ = std::max(Free3DGeometry::kMinPitch, std::min(pitch_, Free3DGeometry::kMaxPitch));
  distance_ = std::max(Free3DGeometry::kMinDistance, std::min(distance_, Free3DGeometry::kMaxDistance));
  // Keep the numbers small: whole turns change nothing.
  while (yaw_ > kTwoPi) { yaw_ -= kTwoPi; g.yaw -= kTwoPi; }
  while (yaw_ < -kTwoPi) { yaw_ += kTwoPi; g.yaw += kTwoPi; }

  float k = placed_ ? std::min(1.0f, t * 14.0f) : 1.0f;
  auto ease = [k](float& v, float to, float close) {
    v += (to - v) * k;
    if (std::fabs(to - v) < close) v = to;
  };
  ease(g.yaw, yaw_, 0.0005f);
  ease(g.pitch, pitch_, 0.0005f);
  ease(g.distance, distance_, 0.002f);
  if (!placed_) {  // start with the spot in the middle
    g.focusX = X;
    g.focusY = Y;
    g.focusZ = Z;
  } else {
    float dX, dZ;
    g.shiftIntoView(X, Y, Z, g.viewW * 0.22f, g.viewH * 0.24f, dX, dZ);
    float f = std::min(1.0f, t * 16.0f);
    g.focusX += dX * f;
    g.focusZ += dZ * f;
    g.focusY += (Y - g.focusY) * std::min(1.0f, t * 6.0f);  // rise and sink with the ground, gently
  }
  g.clamp();
  placed_ = true;
}

}  // namespace ote
