#include "platform/sdl2/sdl2_input.h"

#include "core/base/log.h"

namespace ote {
namespace {

// SDL_GetTicks64 is SDL 2.0.18+; the original Xbox's SDL2 (nxdk) is 2.0.9, whose 32-bit millisecond clock wraps after 49 days:
// plenty for press de-duplication, which only compares times a few hundred milliseconds apart.
inline int64_t nowMs() {
#if SDL_VERSION_ATLEAST(2, 0, 18)
  return static_cast<int64_t>(SDL_GetTicks64());
#else
  return static_cast<int64_t>(SDL_GetTicks());
#endif
}

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
    case SDL_SCANCODE_T: return Button::Ranges;  // "threat": the enemies' reach on / off (in a battle)
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
    case SDL_CONTROLLER_BUTTON_DPAD_UP: return Button::Up;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return Button::Down;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return Button::Left;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return Button::Right;
    case SDL_CONTROLLER_BUTTON_A: return Button::Confirm;  // south face button
    case SDL_CONTROLLER_BUTTON_B: return Button::Cancel;   // east face button
    case SDL_CONTROLLER_BUTTON_START: return Button::Menu;
    case SDL_CONTROLLER_BUTTON_BACK: return Button::Ranges;  // Back / View (the original Xbox's Back)
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return Button::Prev;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return Button::Next;
    case SDL_CONTROLLER_BUTTON_X: return Button::TurnLeft;   // west face button
    case SDL_CONTROLLER_BUTTON_Y: return Button::TurnRight;  // north face button
    default: return Button::None;
  }
}

InputEvent buttonEvent(InputEvent::Type t, Button b, InputDevice device = InputDevice::None) {
  InputEvent ev;
  ev.type = t;
  ev.button = b;
  ev.device = device;
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
      } else if (e.window.event == SDL_WINDOWEVENT_LEAVE) {  // the pointer left the window: a move far outside the picture (docs/PORTING.md "Pointer scrolling")
        InputEvent ev;
        ev.type = InputEvent::PointerMove;
        ev.device = InputDevice::None;
        ev.x = ev.y = kPointerGone;
        out.push_back(ev);
      }
      break;
    case SDL_KEYDOWN:
    case SDL_KEYUP: {
      Button b = keyButton(e.key.keysym.scancode);
      if (b == Button::None) break;
      bool down = e.type == SDL_KEYDOWN;
      // Auto-repeat is useful for moving a cursor, harmful for Confirm/Cancel.
      if (down && e.key.repeat && !isDirection(b)) break;
      const PressSource src{e.key.keysym.scancode == SDL_SCANCODE_AC_BACK ? PressSource::BackRequest : PressSource::Keyboard, 0};
      if (!dedupe_.pass(b, src, down, nowMs())) break;
      // The back request is no keyboard press: on UWP / Android it arrives with (or instead of) a pad's B, and must not switch the hints.
      const InputDevice dev = src.kind == PressSource::BackRequest ? InputDevice::None : InputDevice::Keyboard;
      out.push_back(buttonEvent(down ? InputEvent::ButtonDown : InputEvent::ButtonUp, b, dev));
      break;
    }
    case SDL_CONTROLLERDEVICEADDED:  // `which` is a device index here
      if (SDL_GameController* pad = SDL_GameControllerOpen(e.cdevice.which)) {
        // Instance id and GUID are logged because on Xbox one pad was seen listed three times (docs/XBOX.md): the next
        // log.txt then shows whether those copies differ in anything.
        char guid[40] = "";
        SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(e.cdevice.which), guid, sizeof guid);
        OTE_INFO("gamepad connected: %s (device %d, instance %d, guid %s)", SDL_GameControllerName(pad), e.cdevice.which,
                 static_cast<int>(SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))), guid);
      } else {
        OTE_WARN("cannot open gamepad: %s", SDL_GetError());
      }
      break;
    case SDL_CONTROLLERDEVICEREMOVED:  // `which` is an instance id here
      if (SDL_GameController* pad = SDL_GameControllerFromInstanceID(e.cdevice.which)) SDL_GameControllerClose(pad);
      stick_.reset();
      camStick_.reset();
      triggers_.reset();
      dedupe_.reset();  // the removed pad's releases may never come
      OTE_INFO("gamepad disconnected");
      break;
    case SDL_CONTROLLERBUTTONDOWN:
    case SDL_CONTROLLERBUTTONUP: {
      Button b = padButton(e.cbutton.button);
      if (b == Button::None) break;
      const bool down = e.type == SDL_CONTROLLERBUTTONDOWN;
      if (!dedupe_.pass(b, PressSource{PressSource::Pad, static_cast<int>(e.cbutton.which)}, down,
                        nowMs())) break;
      out.push_back(buttonEvent(down ? InputEvent::ButtonDown : InputEvent::ButtonUp, b, InputDevice::Controller));
      break;
    }
    case SDL_CONTROLLERAXISMOTION:
      if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTX) stick_.update(0, e.caxis.value, out);
      else if (e.caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) stick_.update(1, e.caxis.value, out);
      else if (e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX) camStick_.update(0, e.caxis.value, out);
      else if (e.caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) camStick_.update(1, e.caxis.value, out);
      else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT) triggers_.update(0, e.caxis.value, out);
      else if (e.caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT) triggers_.update(1, e.caxis.value, out);
      break;
    case SDL_MOUSEWHEEL:  // one press per notch: away from the player zooms in
      if (e.wheel.y != 0) {
        Button b = e.wheel.y > 0 ? Button::ZoomIn : Button::ZoomOut;
        out.push_back(buttonEvent(InputEvent::ButtonDown, b));
        out.push_back(buttonEvent(InputEvent::ButtonUp, b));
      }
      break;
    // Touch is delivered as synthesized mouse events (SDL2 default
    // SDL_HINT_TOUCH_MOUSE_EVENTS=1), same as in the SDL3 backend.
    case SDL_MOUSEMOTION: {
      InputEvent ev;
      // a finger's synthesized event says so in `which`; a real mouse has its own hint wording ("click")
      ev.device = e.motion.which == SDL_TOUCH_MOUSEID ? InputDevice::Touch : InputDevice::Mouse;
      if (dragging_ && (e.motion.state & SDL_BUTTON_RMASK)) {
        if (e.motion.xrel == 0 && e.motion.yrel == 0) break;
        ev.type = InputEvent::CameraDrag;  // how far, in logical pixels
        ev.x = e.motion.xrel;
        ev.y = e.motion.yrel;
        out.push_back(ev);
        break;
      }
      ev.type = InputEvent::PointerMove;
      ev.x = e.motion.x;
      ev.y = e.motion.y;
      out.push_back(ev);
      break;
    }
    case SDL_MOUSEBUTTONDOWN:
    case SDL_MOUSEBUTTONUP: {
      if (e.button.button == SDL_BUTTON_RIGHT) dragging_ = e.type == SDL_MOUSEBUTTONDOWN;
      if (e.button.button != SDL_BUTTON_LEFT) break;
      InputEvent ev;
      ev.device = e.button.which == SDL_TOUCH_MOUSEID ? InputDevice::Touch : InputDevice::Mouse;
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
