// Names the controls of the input device the person is using now, for every hint / reminder line of the player. No SDL.
//
// Hint lines are written once, as TEMPLATES with {tokens} ("{dpad}:cursor  {ok}:select  {menu}:menu"), and expanded for the
// current device when they are drawn (drawFooter and the battle's hint strip expand for you). One table per device in
// input_hints.cpp holds every label; nothing else in the player names a key or a pad button.
//
// Expansion rules (hintText):
//  - {token} becomes the device's label; an unknown token is left as written.
//  - A label that is empty means "this device has no such control" (touch has no D-pad, no Tab): the chunk it sits in is
//    dropped. A chunk is the text between runs of two or more spaces, so "{dpad}:cursor  {ok}:select" on touch is "tap:select".
//  - Touch's Cancel is "Back"; "Back:back" would read badly, so a ":back" right after a "Back" is dropped.
//
// The current device is the device of the most recent real press (key, pad button, stick past its dead zone, tap / mouse
// press) that a backend reported in InputEvent::device; releases, pointer moves and events without a device never switch it.
#pragma once
#include <string>

#include "platform/platform.h"

namespace ote {

// What a person can press, in the engine's terms: the label of one engine Button on a device ("Z", "A", "tap"). Empty = the
// device has no such control. Skip has no key anywhere.
const char* buttonLabel(InputDevice device, Button button);

// Expands a template for `device` (see above).
std::string hintText(InputDevice device, const std::string& tmpl);

// The device in use now. Starts as Keyboard until the player sets the platform's natural one.
InputDevice currentInputDevice();
// Sets it outright (start-up, tests, the scripted `device:` word). Device None is ignored.
void setInputDevice(InputDevice device);
// Feeds one event: a press from another device switches to it at once. Returns true when the device changed.
bool noteInputEvent(const InputEvent& event);

// hintText for the current device and the current-device overload of buttonLabel.
std::string hintText(const std::string& tmpl);
inline const char* buttonLabel(Button button) { return buttonLabel(currentInputDevice(), button); }

// hintText for the current device, for the bottom hint line of a screen: also writes "hint: <text>" to the log whenever
// the line changes (what the screen says is then visible to tests and to bug reports).
std::string hintLine(const std::string& tmpl);

// "keyboard" / "controller" / "touch", for logs and the script word.
const char* inputDeviceName(InputDevice device);
bool parseInputDevice(const std::string& name, InputDevice& out);

}  // namespace ote
