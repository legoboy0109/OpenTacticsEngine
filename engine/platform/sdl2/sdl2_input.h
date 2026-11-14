// Translates SDL2 events (keyboard, game controller, mouse/touch) to
// ote::InputEvent. Mirrors engine/platform/sdl3/sdl_input.cpp exactly.
#pragma once
#include <SDL.h>

#include <vector>

#include "platform/common/stick_mapper.h"
#include "platform/platform.h"

namespace ote {

class SdlInput {
 public:
  // Appends zero or more logical events for one SDL event. Mouse coordinates
  // arrive already in logical pixels: SDL2's renderer converts them in place
  // while a logical size is set.
  void translate(const SDL_Event& e, std::vector<InputEvent>& out);

 private:
  StickMapper stick_;
  // The camera stick: sideways turns, up and down tilts. The triggers zoom.
  StickMapper camStick_{Button::TurnLeft, Button::TurnRight, Button::TiltUp, Button::TiltDown};
  StickMapper triggers_{Button::None, Button::ZoomOut, Button::None, Button::ZoomIn};
  bool dragging_ = false;  // the second pointer button is held
};

}  // namespace ote
