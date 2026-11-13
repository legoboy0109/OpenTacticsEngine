// Translates SDL events (keyboard, gamepad, mouse/touch) to ote::InputEvent.
#pragma once
#include <SDL3/SDL.h>

#include <vector>

#include "platform/common/stick_mapper.h"
#include "platform/platform.h"

namespace ote {

class SdlInput {
 public:
  // Appends zero or more logical events for one SDL event. `renderer` is used
  // to convert pointer coordinates to logical pixels.
  void translate(SDL_Event& e, SDL_Renderer* renderer, std::vector<InputEvent>& out);

 private:
  StickMapper stick_;
};

}  // namespace ote
