// Translates SDL events (keyboard, gamepad, mouse/touch) to ote::InputEvent.
#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "platform/common/press_dedupe.h"
#include "platform/common/stick_mapper.h"
#include "platform/platform.h"

namespace ote {

class SdlInput {
 public:
  // Appends zero or more logical events for one SDL event. `renderer` is used
  // to convert pointer coordinates to logical pixels.
  void translate(SDL_Event& e, SDL_Renderer* renderer, std::vector<InputEvent>& out);

 private:
  // One press, one ButtonDown / ButtonUp even if the platform reports it twice (Android: a pad's B and the back key).
  PressDedupe dedupe_;
  StickMapper stick_;
  // The camera stick: sideways turns, up and down tilts. The triggers zoom.
  StickMapper camStick_{Button::TurnLeft, Button::TurnRight, Button::TiltUp, Button::TiltDown};
  StickMapper triggers_{Button::None, Button::ZoomOut, Button::None, Button::ZoomIn};
  bool dragging_ = false;  // the second pointer button is held
  float dragX_ = 0, dragY_ = 0;  // drag distance not handed on yet (fractions of a pixel)
};

}  // namespace ote
