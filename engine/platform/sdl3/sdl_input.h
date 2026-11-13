// Translates SDL events (keyboard, gamepad, mouse/touch) to ote::InputEvent.
#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "platform/platform.h"

namespace ote {

class SdlInput {
 public:
  // Appends zero or more logical events for one SDL event. `renderer` is used
  // to convert pointer coordinates to logical pixels.
  void translate(SDL_Event& e, SDL_Renderer* renderer, std::vector<InputEvent>& out);

 private:
  void stick(int axis, int value, std::vector<InputEvent>& out);
  int stickDir_[2] = {0, 0};  // -1 / 0 / +1 for X, Y
};

}  // namespace ote
