// The hint lines name the controls of the device in use (player/ui/input_hints.h).
#include <string>

#include "battle/economy_text.h"
#include "battle/prep_text.h"
#include "platform/common/stick_mapper.h"
#include "test.h"
#include "ui/dialogue_box.h"
#include "ui/input_hints.h"

using namespace ote;

// Two strings are equal (CHECK_EQ is for numbers).
#define SEQ(a, b) CHECK(std::string(a) == std::string(b))

namespace {
const InputDevice K = InputDevice::Keyboard, C = InputDevice::Controller, T = InputDevice::Touch;

InputEvent press(InputDevice d, bool pointer = false) {
  InputEvent e;
  e.type = pointer ? InputEvent::PointerDown : InputEvent::ButtonDown;
  e.button = pointer ? Button::None : Button::Confirm;
  e.device = d;
  return e;
}

// Restores the device a test case found (the state is global).
struct Keep {
  InputDevice was = currentInputDevice();
  ~Keep() { setInputDevice(was); }
};
}  // namespace

TEST(hints_button_labels_per_device) {
  SEQ(std::string(buttonLabel(K, Button::Confirm)), "Z");
  SEQ(std::string(buttonLabel(K, Button::Cancel)), "X");
  SEQ(std::string(buttonLabel(K, Button::Menu)), "M");
  SEQ(std::string(buttonLabel(K, Button::Next)), "Tab");
  SEQ(std::string(buttonLabel(C, Button::Confirm)), "A");
  SEQ(std::string(buttonLabel(C, Button::Cancel)), "B");
  SEQ(std::string(buttonLabel(C, Button::Menu)), "Start");
  SEQ(std::string(buttonLabel(C, Button::Prev)), "LB");
  SEQ(std::string(buttonLabel(C, Button::Next)), "RB");
  SEQ(std::string(buttonLabel(C, Button::TurnLeft)), "X");
  SEQ(std::string(buttonLabel(C, Button::TurnRight)), "Y");
  SEQ(std::string(buttonLabel(T, Button::Confirm)), "tap");
  SEQ(std::string(buttonLabel(T, Button::Cancel)), "Back");
  SEQ(std::string(buttonLabel(T, Button::Up)), "");  // no such control
  SEQ(std::string(buttonLabel(K, Button::Skip)), "");
  SEQ(std::string(buttonLabel(K, Button::None)), "");
}

TEST(hints_expand_keyboard_keeps_the_old_text) {  // desktop players see the lines they always saw
  SEQ(hintText(K, "{dpad}:cursor  {ok}:move here (own cell = stay)  {cancel}:cancel"),
           "Arrows:cursor  Z:move here (own cell = stay)  X:cancel");
  SEQ(hintText(K, "{udok}:choose  {cancel}:back"), "Up/Down + Z:choose  X:back");
  SEQ(hintText(K, "{updown}: choose   {confirm}: open   {orTap}"), "Up/Down: choose   Confirm: open   or tap");
  SEQ(hintText(K, kDialogueHint), "Z / tap: next   hold X or Menu, or tap Skip: skip all");
  SEQ(hintText(K, kChoiceHint), "Up/Down + Z: answer   tap: pick, tap again: confirm");
  SEQ(hintText(K, "All units done.  {menuName}: end turn"), "All units done.  Menu (M): end turn");
}

TEST(hints_expand_controller) {
  SEQ(hintText(C, "{dpad}:cursor  {ok}:select  {nextprev}:next unit  {menu}:menu"), "D-pad:cursor  A:select  LB/RB:next unit  Start:menu");
  SEQ(hintText(C, "{udok}:choose  {cancel}:back"), "D-pad + A:choose  B:back");
  SEQ(hintText(C, kDialogueHint), "A: next   hold B or Start: skip all");
  SEQ(hintText(C, kChoiceHint), "D-pad + A: answer");
  SEQ(hintText(C, "{confirmTap}: continue"), "A: continue");
}

TEST(hints_expand_touch_drops_what_touch_lacks) {
  SEQ(hintText(T, "{dpad}:cursor  {ok}:select  {nextprev}:next unit  {menu}:menu"), "tap:select");
  SEQ(hintText(T, "{udok}:choose  {cancel}:back"), "tap:choose  Back");  // not "Back:back"
  SEQ(hintText(T, "{udok}:choose  {cancel}:undo move"), "tap:choose  Back:undo move");
  SEQ(hintText(T, "{updown}: choose   {confirm}: open   {orTap}"), "tap: open");
  SEQ(hintText(T, "{dpad}:cursor"), "");
  SEQ(hintText(T, kDialogueHint), "tap: next   tap Skip: skip all");
}

TEST(hints_unknown_tokens_and_plain_text_stay) {
  SEQ(hintText(C, "Nothing to expand here"), "Nothing to expand here");
  SEQ(hintText(C, "{nope}: stays  {ok}:yes"), "{nope}: stays  A:yes");
  SEQ(hintText(K, "a {unclosed"), "a {unclosed");
  SEQ(hintText(K, ""), "");
}

TEST(hints_every_player_template_expands_clean) {  // no token left behind, and no empty line, on any device
  const char* all[] = {PrepWords::kHintMain, PrepWords::kHintUnits, PrepWords::kHintPick, PrepWords::kHintPlace, PrepWords::kHintView,
                       PrepWords::kHintList, PrepWords::kHintTrade, PrepWords::kHintBuy, PrepWords::kHintBuyTabs, PrepWords::kHintSellWho,
                       PrepWords::kHintSell, PrepWords::kHintTradeEquip, PrepWords::kHintAsk, EconomyWords::kHintStructure,
                       EconomyWords::kHintUpgrade, EconomyWords::kHintResearch, kDialogueHint, kChoiceHint};
  for (const char* t : all)
    for (InputDevice d : {K, C, T}) {
      const std::string s = hintText(d, t);
      CHECK(!s.empty());
      CHECK(s.find('{') == std::string::npos);
      CHECK(s.size() <= 56);  // 58 characters is all the 8x8 font gets in a 480 pixel footer
    }
}

TEST(hints_device_switches_on_the_latest_press) {
  Keep keep;
  setInputDevice(K);
  CHECK(!noteInputEvent(press(K)));      // the same device: nothing changes
  CHECK(noteInputEvent(press(C)));
  CHECK(currentInputDevice() == C);
  CHECK(noteInputEvent(press(T, true)));  // a tap
  CHECK(currentInputDevice() == T);
  CHECK(noteInputEvent(press(K)));
  CHECK(currentInputDevice() == K);
}

TEST(hints_only_presses_switch) {  // no flicker: releases, pointer moves, wheel / back (no device) and quit never switch
  Keep keep;
  setInputDevice(K);
  InputEvent e = press(C);
  e.type = InputEvent::ButtonUp;
  CHECK(!noteInputEvent(e));
  e.type = InputEvent::PointerMove;
  e.device = T;
  CHECK(!noteInputEvent(e));
  e.type = InputEvent::PointerUp;
  CHECK(!noteInputEvent(e));
  e.type = InputEvent::CameraDrag;
  CHECK(!noteInputEvent(e));
  CHECK(!noteInputEvent(press(InputDevice::None)));
  setInputDevice(InputDevice::None);  // ignored
  CHECK(currentInputDevice() == K);
}

TEST(hints_stick_noise_does_not_switch_but_a_real_push_does) {
  Keep keep;
  setInputDevice(K);
  StickMapper stick;
  std::vector<InputEvent> out;
  for (int v : {3000, -5000, 8000, -12000, 15000, -9500, 2000}) stick.update(0, v, out);  // wobble below the dead zone
  CHECK(out.empty());
  for (const InputEvent& e : out) noteInputEvent(e);
  CHECK(currentInputDevice() == K);
  stick.update(0, 24000, out);  // pushed
  CHECK(!out.empty() && out[0].device == C);
  for (const InputEvent& e : out) noteInputEvent(e);
  CHECK(currentInputDevice() == C);
  // Released: the stick's ButtonUp never takes the hints back from a key pressed meanwhile.
  noteInputEvent(press(K));
  out.clear();
  stick.update(0, 0, out);
  for (const InputEvent& e : out) noteInputEvent(e);
  CHECK(currentInputDevice() == K);
}

TEST(hints_device_names_round_trip) {
  InputDevice d = InputDevice::None;
  CHECK(parseInputDevice("controller", d) && d == C);
  CHECK(parseInputDevice("touch", d) && d == T);
  CHECK(parseInputDevice("keyboard", d) && d == K);
  CHECK(!parseInputDevice("mouse", d));
  SEQ(std::string(inputDeviceName(C)), "controller");
}
