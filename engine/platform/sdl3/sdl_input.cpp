#include "platform/sdl3/sdl_input.h"

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
    case SDL_SCANCODE_ESCAPE: case SDL_SCANCODE_X: case SDL_SCANCODE_BACKSPACE:
    case SDL_SCANCODE_AC_BACK: return Button::Cancel;  // AC_BACK: Android's back button / UWP's B (docs/PORTING.md)
    case SDL_SCANCODE_TAB: case SDL_SCANCODE_E: return Button::Next;
    case SDL_SCANCODE_Q: return Button::Prev;
    case SDL_SCANCODE_M: return Button::Menu;
    case SDL_SCANCODE_COMMA: case SDL_SCANCODE_LEFTBRACKET: return Button::TurnLeft;
    case SDL_SCANCODE_PERIOD: case SDL_SCANCODE_RIGHTBRACKET: return Button::TurnRight;
    case SDL_SCANCODE_PAGEUP: case SDL_SCANCODE_R: return Button::TiltUp;
    case SDL_SCANCODE_PAGEDOWN: case SDL_SCANCODE_F: return Button::TiltDown;
    case SDL_SCANCODE_EQUALS: case SDL_SCANCODE_KP_PLUS: return Button::ZoomIn;
    case SDL_SCANCODE_MINUS: case SDL_SCANCODE_KP_MINUS: return Button::ZoomOut;
    default: return Button::None;
  }
}

Button padButton(Uint8 b) {
  switch (b) {
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return Button::Up;
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return Button::Down;
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return Button::Left;
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return Button::Right;
    case SDL_GAMEPAD_BUTTON_SOUTH: return Button::Confirm;
    case SDL_GAMEPAD_BUTTON_EAST: return Button::Cancel;
    case SDL_GAMEPAD_BUTTON_START: return Button::Menu;
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER: return Button::Prev;
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return Button::Next;
    case SDL_GAMEPAD_BUTTON_WEST: return Button::TurnLeft;
    case SDL_GAMEPAD_BUTTON_NORTH: return Button::TurnRight;
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

void SdlInput::translate(SDL_Event& e, SDL_Renderer* renderer, std::vector<InputEvent>& out) {
  switch (e.type) {
    case SDL_EVENT_QUIT:
    case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
      InputEvent q;
      q.type = InputEvent::Quit;
      out.push_back(q);
      break;
    }
    case SDL_EVENT_KEY_DOWN:
    case SDL_EVENT_KEY_UP: {
      Button b = keyButton(e.key.scancode);
      if (b == Button::None) break;
      bool down = e.type == SDL_EVENT_KEY_DOWN;
      // Auto-repeat is useful for moving a cursor, harmful for Confirm/Cancel.
      if (down && e.key.repeat && !isDirection(b)) break;
      const PressSource src{e.key.scancode == SDL_SCANCODE_AC_BACK ? PressSource::BackRequest : PressSource::Keyboard, 0};
      if (!dedupe_.pass(b, src, down, static_cast<int64_t>(SDL_GetTicks()))) break;
      out.push_back(buttonEvent(down ? InputEvent::ButtonDown : InputEvent::ButtonUp, b));
      break;
    }
    case SDL_EVENT_GAMEPAD_ADDED:
      if (SDL_Gamepad* pad = SDL_OpenGamepad(e.gdevice.which)) {
        OTE_INFO("gamepad connected: %s", SDL_GetGamepadName(pad));
      } else {
        OTE_WARN("cannot open gamepad: %s", SDL_GetError());
      }
      break;
    case SDL_EVENT_GAMEPAD_REMOVED:
      if (SDL_Gamepad* pad = SDL_GetGamepadFromID(e.gdevice.which)) SDL_CloseGamepad(pad);
      stick_.reset();
      camStick_.reset();
      triggers_.reset();
      dedupe_.reset();  // the removed pad's releases may never come
      OTE_INFO("gamepad disconnected");
      break;
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP: {
      Button b = padButton(e.gbutton.button);
      if (b == Button::None) break;
      const bool down = e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN;
      if (!dedupe_.pass(b, PressSource{PressSource::Pad, static_cast<int>(e.gbutton.which)}, down,
                        static_cast<int64_t>(SDL_GetTicks()))) break;
      out.push_back(buttonEvent(down ? InputEvent::ButtonDown : InputEvent::ButtonUp, b));
      break;
    }
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
      if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX) stick_.update(0, e.gaxis.value, out);
      else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY) stick_.update(1, e.gaxis.value, out);
      else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTX) camStick_.update(0, e.gaxis.value, out);
      else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHTY) camStick_.update(1, e.gaxis.value, out);
      else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) triggers_.update(0, e.gaxis.value, out);
      else if (e.gaxis.axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) triggers_.update(1, e.gaxis.value, out);
      break;
    case SDL_EVENT_MOUSE_WHEEL:  // one press per notch: away from the player zooms in
      if (e.wheel.y != 0) {
        Button b = e.wheel.y > 0 ? Button::ZoomIn : Button::ZoomOut;
        out.push_back(buttonEvent(InputEvent::ButtonDown, b));
        out.push_back(buttonEvent(InputEvent::ButtonUp, b));
      }
      break;
    case SDL_EVENT_MOUSE_MOTION:
    case SDL_EVENT_MOUSE_BUTTON_DOWN:
    case SDL_EVENT_MOUSE_BUTTON_UP: {
      if (e.type != SDL_EVENT_MOUSE_MOTION && e.button.button != SDL_BUTTON_LEFT) {
        if (e.button.button == SDL_BUTTON_RIGHT) dragging_ = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        break;
      }
      SDL_ConvertEventToRenderCoordinates(renderer, &e);
      InputEvent ev;
      if (e.type == SDL_EVENT_MOUSE_MOTION && dragging_ && (e.motion.state & SDL_BUTTON_RMASK)) {
        ev.type = InputEvent::CameraDrag;  // how far, in logical pixels (rounded: slow drags still add up over events)
        dragX_ += e.motion.xrel;
        dragY_ += e.motion.yrel;
        ev.x = (int)dragX_;
        ev.y = (int)dragY_;
        dragX_ -= (float)ev.x;
        dragY_ -= (float)ev.y;
        if (ev.x == 0 && ev.y == 0) break;
      } else if (e.type == SDL_EVENT_MOUSE_MOTION) {
        ev.type = InputEvent::PointerMove;
        ev.x = (int)SDL_floorf(e.motion.x);
        ev.y = (int)SDL_floorf(e.motion.y);
      } else {
        ev.type = e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ? InputEvent::PointerDown : InputEvent::PointerUp;
        ev.x = (int)SDL_floorf(e.button.x);
        ev.y = (int)SDL_floorf(e.button.y);
      }
      out.push_back(ev);
      break;
    }
    default: break;
  }
}

}  // namespace ote
