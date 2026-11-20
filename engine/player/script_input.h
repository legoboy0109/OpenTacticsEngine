// Test automation: feeds a comma-separated token script to the App as if a
// player were pressing buttons.
//   up down left right confirm cancel menu next prev turnleft turnright   press+release
//   wait | wait:N                                      idle for 1 / N intervals
//   background | foreground | lowmemory | terminate   a life-cycle notice as the OS would send it (docs/PORTING.md)
//   tap:X:Y                                            pointer down+up at logical pixel
//   device:keyboard|controller|touch                   the input device the following tokens come from (docs/PORTING.md "Hints
//                                                      name the device"); takes effect at once and costs no time. Scripted input
//                                                      is "keyboard" unless OTE_INPUT_DEVICE or this word says otherwise.
// One token is consumed every `interval` frames, starting after frame 1.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"

namespace ote {

class ScriptInput {
 public:
  bool parse(const std::string& text, int interval, std::string& err);
  bool active() const { return !tokens_.empty(); }
  // Call once per frame; appends the events to inject this frame.
  void tick(std::vector<InputEvent>& out, std::vector<Lifecycle>& notices);
  // True once the script finished and the settle time (~30 frames) elapsed.
  bool finished() const { return finished_; }
  // The device events are tagged with at the start (the player's start-up device: keyboard in headless runs unless
  // OTE_INPUT_DEVICE says otherwise).
  void setDevice(InputDevice d) { device_ = d; }
  // True once after a `device:` word ran; `d` is the device it named (the player switches its hints right away, since
  // the word may come before any press).
  bool takeDevice(InputDevice& d) {
    if (!deviceChanged_) return false;
    deviceChanged_ = false;
    d = device_;
    return true;
  }

 private:
  struct Token {
    enum Kind { Press, Wait, Tap, Life, Device } kind = Wait;
    InputDevice device = InputDevice::Keyboard;
    Lifecycle life = Lifecycle::LowMemory;
    Button button = Button::None;
    int x = 0, y = 0, count = 1;
  };
  std::vector<Token> tokens_;
  size_t next_ = 0;
  int interval_ = 6;
  int wait_ = 2;      // frames until the next token fires
  int settle_ = -1;   // frames left after the last token
  bool finished_ = false;
  InputDevice device_ = InputDevice::Keyboard;
  bool deviceChanged_ = false;
};

}  // namespace ote
