#include "platform/sdl2/sdl2_input.h"

#include "core/base/log.h"

namespace ote {
namespace {

Button keyButton(SDL_Scancode sc) {
  switch (sc) {
    case SDL_SCANCODE_UP: case SDL_SCANCODE_W: return Button::Up;
    case SDL_SCANCODE_DOWN: case SDL_SCANCODE_S: return Button::Down;
    case SDL_SCANCODE_LEFT: case SDL_SCANCODE_A: return Button::Left;
    case SDL_SCANCODE_RIGHT: case SDL_SCANCODE_D: return Button::Right;
    case SDL_SCANCODE_RETURN: case SDL_SCANCODE_KP_ENTER: case SDL_SCANCODE_SPACE:
    case SDL_SCANCODE_Z: return Button::Confirm;
    case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_X: case SDL_SCANCODE_BACKSPACE: return Button::Cancel;
    case SDL_SCANCODE_TAB: case SDL_SCANCODE_E: return Button::Next;
    case SDL_SCANCODE_Q: return Button::Prev;
    case SDL_SCANCODE_M: return Button::Menu;
    case SDL_SCANCODE_COMMA: case SDL_SCANCODE_LEFTBRACKET: return Button::TurnLeft;
    case SDL_SCANCODE_PERIOD: case SDL_SCANCODE_RIGHTBRACKET: return Button::TurnRight;
    default: return Button::None;
  }
}

Button padButton(Uint8 b) {
  switch (b) {
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return Button::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Button::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Button::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Button::Right;
    case SDL_CONTROLLER_BUTTON_A: return Button::Confirm;  // south face button
    case SDL_CONTROLLER_BUTTON_B: return Button::Cancel;   // east face button
    case SDL_CONTROLLER_BUTTON_START: return Button::Menu;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Button::Prev;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Button::Next;
    case SDL_CONTROLLER_BUTTON_X: return Button::TurnLeft;   // west face button
    case SDL_CONTROLLER_BUTTON_Y: return Button::TurnRight;  // north face button
    default: return Button::None;
  }
}

InputEvent buttonEvent(InputEvent::Type t, Button b) {
  InputEvent ev;
  ev.type = t;
  ev.button = b;
  return ev;
}

bool isDirection(Button b) {
  return b == Button::Up || b == Button::Down || b == Button::Left || b == Button::Right;
}

}  // namespace

void SdlInput::translate(const SDL_Event& e, std::vector<InputEvent>& out) {
  switch (e.type) {
    case SDL_QUIT: {
      InputEvent q;
      q.type = InputEvent::Quit;
      out.push_back(q);
      break;
    }
    case SDL_WINDOWEVENT:
      if (e.window.event == SDL_WINDOWEVENT_CLOSE) {
        InputEvent q;
        q.type = InputEvent::Quit;
        out.push_back(q);
      }
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      Button b = keyButton(e.key.keysym.scancode);
      if (b == Button::None) break;
      bool down = e.type == SDL_KEYDOWN;
      // Auto-repeat is useful for moving a cursor, harmful for Confirm/Cancel.
      if (down && e.key.repeat && !isDirection(b)) break;
      out.push_back(buttonEvent(down ? InputEvent::ButtonDown : InputEvent::ButtonUp, b));
      break;
    }
    case SDL_CONTROLLERDEVICEADDED:  // `which` is a device index here
      if (SDL_GameController* pad = SDL_GameControllerOpen(e.cdevice.which)) {
        OTE_INFO("gamepad connected: %s", SDL_GameControllerName(pad));
      } else {
        OTE_WARN("cannot open gamepad: %s", SDL_GetError());
      }
      break;
    case SDL_CONTROLLERDEVICEREMOVED:  // `which` is an instance id here
      if (SDL_GameController* pad = SDL_GameControllerFromInstanceID(e.cdevice.which)) SDL_GameControllerClose(pad);
      stick_.reset();
      OTE_INFO("gamepad disconnected");
      break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
      Button b = padButton(e.cbutton.button);
      if (b == Button::None) break;
      out.push_back(buttonEvent(e.type == SDL_CONTROLLERBUTTONDOWN ? InputEvent::ButtonDown : InputEvent::ButtonUp, b));
      break;
    }
    case SDL_CONTROLLERAXISMOTION:
      if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) stick_.update(0, e.caxis.value, out);
      else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) stick_.update(1, e.caxis.value, out);
      break;
    // Touch is delivered as synthesized mouse events (SDL2 default
    // SDL_HINT_TOUCH_MOUSE_EVENTS=1), same as in the SDL3 backend.
    case SDL_MOUSEMOTION: {
      InputEvent ev;
      ev.type = InputEvent::PointerMove;
      ev.x = e.motion.x;
      ev.y = e.motion.y;
      out.push_back(ev);
      break;
    }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
      if (e.button.button != SDL_BUTTON_LEFT) break;
      InputEvent ev;
      ev.type = e.type == SDL_MOUSEBUTTONDOWN ? InputEvent::PointerDown : InputEvent::PointerUp;
      ev.x = e.button.x;
      ev.y = e.button.y;
      out.push_back(ev);
      break;
    }
    default: break;
  }
}

}  // namespace ote
