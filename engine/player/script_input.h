// Test automation: feeds a comma-separated token script to the App as if a
// player were pressing buttons.
//   up down left right confirm cancel menu next prev turnleft turnright   press+release
//   wait | wait:N                                      idle for 1 / N intervals
//   background | foreground | lowmemory | terminate   a life-cycle notice as the OS would send it (docs/PORTING.md)
//   tap:X:Y                                            pointer down+up at logical pixel
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

 private:
  struct Token {
    enum Kind { Press, Wait, Tap, Life } kind = Wait;
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
};

}  // namespace ote
