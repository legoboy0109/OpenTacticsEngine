#include "player/ui/input_hints.h"

#include <cstring>
#include <vector>

#include "core/base/log.h"

namespace ote {
namespace {

// One column per device: Keyboard, Controller, Touch, Mouse. "" = no such control on that device.
// Mouse = a desktop pointer with the keyboard beside it: a click selects, the keys do the rest.
struct Row {
  const char* name;
  const char* label[4];
};

int column(InputDevice d) { return d == InputDevice::Controller ? 1 : d == InputDevice::Touch ? 2 : d == InputDevice::Mouse ? 3 : 0; }

// The shoulder buttons: the original Xbox pad has White and Black there (platform/nxdk maps White = previous, Black = next).
#if defined(OTE_NXDK)
#define OTE_PAD_NEXT "Black"
#define OTE_PAD_PREV "White"
#else
#define OTE_PAD_NEXT "RB"
#define OTE_PAD_PREV "LB"
#endif

// The engine's Buttons, in enum order (Button::None .. Button::Skip). Keyboard: the keys platform/sdl*/..._input.cpp binds
// first. Controller: the Xbox layout names (A confirm, B cancel, Start menu, LB/RB, X/Y turn the camera). The right
// stick tilts, the triggers zoom. Touch has no buttons except what is on the screen: a tap confirms, Back is the system back.
const Row kButtons[] = {
    {"none", {"", "", "", ""}},
    {"up", {"Up", "D-pad up", "", ""}},
    {"down", {"Down", "D-pad down", "", ""}},
    {"left", {"Left", "D-pad left", "", ""}},
    {"right", {"Right", "D-pad right", "", ""}},
    {"confirm", {"Z", "A", "tap", "click"}},
    {"cancel", {"X", "B", "Back", "X"}},
    {"menu", {"M", "Start", "", "M"}},
    {"next", {"Tab", OTE_PAD_NEXT, "", "Tab"}},
    {"prev", {"Q", OTE_PAD_PREV, "", "Q"}},
    {"turnleft", {",", "X", "", ","}},
    {"turnright", {".", "Y", "", "."}},
    {"tiltup", {"R", "R-stick up", "", "R"}},
    {"tiltdown", {"F", "R-stick down", "", "F"}},
    {"zoomin", {"=", "RT", "", "="}},
    {"zoomout", {"-", "LT", "", "-"}},
    {"skip", {"", "", "", ""}},  // only the --input script (and tests) makes it
};

// The {tokens} of hint templates: groups and phrases the lines need. Keyboard column = the text the lines had before
// devices were told apart, so desktop users see no change.
const Row kTokens[] = {
    {"ok", {"Z", "A", "tap", "click"}},
    {"okTap", {"Z / tap", "A", "tap", "click"}},
    {"confirm", {"Confirm", "A", "tap", "click"}},  // launcher-style lines: a word, since Enter, Space and Z all confirm
    {"orTap", {"or tap", "", "", ""}},  // the pointer as an extra way: only the keyboard line mentions it
    {"confirmTap", {"Confirm / tap", "A", "tap", "click"}},
    {"confirmCancelTap", {"Confirm / Cancel / tap", "A / B", "tap", "click / X"}},
    {"okCancelTap", {"Z / X / tap", "A / B", "tap", "click / X"}},
    {"cancel", {"X", "B", "Back", "X"}},
    {"cancelWord", {"Cancel", "B", "Back", "Cancel"}},
    {"cancelTap", {"Cancel / tap", "B", "tap", "click"}},
    {"menu", {"M", "Start", "", "M"}},
    {"menuName", {"Menu (M)", "Menu (Start)", "Tap an empty square", "Menu (M)"}},
    {"dpad", {"Arrows", "D-pad", "", ""}},
    {"updown", {"Up/Down", "D-pad", "", ""}},
    {"leftright", {"Left/Right", "D-pad", "", ""}},
    {"left", {"Left", "D-pad left", "tap", "click"}},    // a tab or a button on the screen: a tap
    {"right", {"Right", "D-pad right", "tap", "click"}},
    {"udok", {"Up/Down + Z", "D-pad + A", "tap", "click"}},
    {"lrok", {"Left/Right + Z", "D-pad + A", "tap", "click"}},
    {"nextprev", {"Tab/Q", OTE_PAD_PREV "/" OTE_PAD_NEXT, "", "Tab/Q"}},
    {"dpadnext", {"Arrows/Tab", "D-pad/" OTE_PAD_NEXT, "", "Tab"}},
    {"turn", {",/.", "X/Y", "", ",/."}},
    {"tilt", {"R/F", "RS", "", "R/F"}},
    {"zoom", {"-/=", "LT/RT", "", "-/="}},
    {"skipAll", {"hold X or Menu, or tap Skip", "hold B or Start", "tap Skip", "hold X or Menu, or click Skip"}},
    {"choiceHint", {"Up/Down + Z: answer   tap: pick, tap again: confirm", "D-pad + A: answer", "tap: pick, tap again: confirm", "click: pick, click again: confirm"}},
};

InputDevice g_device = InputDevice::Keyboard;

// Expands the tokens of one chunk. Sets `drop` when a token's label is empty here.
std::string expandChunk(int col, const std::string& in, bool& drop) {
  std::string out;
  size_t i = 0;
  while (i < in.size()) {
    size_t open = in.find('{', i);
    if (open == std::string::npos) { out.append(in, i, std::string::npos); break; }
    size_t close = in.find('}', open);
    if (close == std::string::npos) { out.append(in, i, std::string::npos); break; }
    out.append(in, i, open - i);
    const std::string name = in.substr(open + 1, close - open - 1);
    const Row* hit = nullptr;
    for (const Row& r : kTokens)
      if (name == r.name) { hit = &r; break; }
    if (!hit) {
      out.append(in, open, close - open + 1);  // not ours: left as written
      i = close + 1;
      continue;
    }
    const char* label = hit->label[col];
    if (!*label) drop = true;
    out += label;
    i = close + 1;
    // "Back:back" reads badly: the label already says it.
    if (!std::strcmp(label, "Back") && in.compare(i, 5, ":back") == 0) i += 5;
  }
  return out;
}

}  // namespace

const char* buttonLabel(InputDevice device, Button button) {
  const size_t i = static_cast<size_t>(button);
  if (i >= sizeof kButtons / sizeof kButtons[0]) return "";
  return kButtons[i].label[column(device)];
}

std::string hintText(InputDevice device, const std::string& tmpl) {
  if (tmpl.find('{') == std::string::npos) return tmpl;
  const int col = column(device);
  std::string out;
  bool any = false;  // a chunk has been written
  size_t pos = 0;
  while (pos < tmpl.size()) {
    // The separator (a run of 2+ spaces) before the chunk, then the chunk up to the next such run.
    size_t sepEnd = pos;
    while (sepEnd < tmpl.size() && tmpl[sepEnd] == ' ') ++sepEnd;
    if (sepEnd - pos < 2) sepEnd = pos;  // a single space belongs to the chunk
    size_t end = sepEnd;
    while (end < tmpl.size() && !(tmpl[end] == ' ' && end + 1 < tmpl.size() && tmpl[end + 1] == ' ')) ++end;
    bool drop = false;
    const std::string chunk = expandChunk(col, tmpl.substr(sepEnd, end - sepEnd), drop);
    if (!drop) {
      if (any) out.append(tmpl, pos, sepEnd - pos);
      out += chunk;
      any = true;
    }
    pos = end;  // always moves on: a chunk ends only at a run of spaces, which the next round reads as its separator
  }
  return out;
}

InputDevice currentInputDevice() { return g_device; }

void setInputDevice(InputDevice device) {
  if (device != InputDevice::None) g_device = device;
}

bool noteInputEvent(const InputEvent& e) {
  if (e.device == InputDevice::None || e.device == g_device) return false;
  if (e.type != InputEvent::ButtonDown && e.type != InputEvent::PointerDown) return false;  // only a press switches
  g_device = e.device;
  return true;
}

std::string hintText(const std::string& tmpl) { return hintText(g_device, tmpl); }

std::string hintLine(const std::string& tmpl) {
  std::string text = hintText(g_device, tmpl);
  static std::string last;  // a plain static: no destructor-order trouble (the original Xbox build has no thread_local destructors)
  if (text != last) {
    last = text;
    OTE_INFO("hint: %s", text.c_str());
  }
  return text;
}

const char* inputDeviceName(InputDevice device) {
  switch (device) {
    case InputDevice::Controller: return "controller";
    case InputDevice::Touch: return "touch";
    case InputDevice::Mouse: return "mouse";
    case InputDevice::Keyboard: return "keyboard";
    default: return "none";
  }
}

bool parseInputDevice(const std::string& name, InputDevice& out) {
  if (name == "keyboard") out = InputDevice::Keyboard;
  else if (name == "controller") out = InputDevice::Controller;
  else if (name == "touch") out = InputDevice::Touch;
  else if (name == "mouse") out = InputDevice::Mouse;
  else return false;
  return true;
}

}  // namespace ote
