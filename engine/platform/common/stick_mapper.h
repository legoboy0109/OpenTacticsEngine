// Turns analog stick values (-32768..32767) into Up/Down/Left/Right button
// events with hysteresis, so every backend behaves identically. No SDL types.
#pragma once
#include <vector>

#include "platform/platform.h"

namespace ote {

class StickMapper {
 public:
  // axis 0 = horizontal, 1 = vertical. Appends ButtonUp/ButtonDown events.
  void update(int axis, int value, std::vector<InputEvent>& out) {
    int& dir = dir_[axis];
    int want = dir;
    if (value > kOn) want = 1;
    else if (value < -kOn) want = -1;
    else if (value > -kOff && value < kOff) want = 0;
    if (want == dir) return;
    Button neg = axis == 0 ? Button::Left : Button::Up;
    Button pos = axis == 0 ? Button::Right : Button::Down;
    if (dir != 0) push(out, InputEvent::ButtonUp, dir < 0 ? neg : pos);
    if (want != 0) push(out, InputEvent::ButtonDown, want < 0 ? neg : pos);
    dir = want;
  }
  void reset() { dir_[0] = dir_[1] = 0; }

 private:
  static constexpr int kOn = 16000, kOff = 9000;  // hysteresis avoids chatter
  static void push(std::vector<InputEvent>& out, InputEvent::Type t, Button b) {
    InputEvent ev;
    ev.type = t;
    ev.button = b;
    out.push_back(ev);
  }
  int dir_[2] = {0, 0};  // -1 / 0 / +1
};

}  // namespace ote
