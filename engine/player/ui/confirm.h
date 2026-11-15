// A small modal question box: a wrapped message and two buttons (or one "OK"
// button when there is nothing to decide). The safe answer comes first and is
// selected when the box opens. Everything is measured with the active font.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"
#include "sound/cue_sink.h"

namespace ote {

class ConfirmBox {
 public:
  enum Result { None, Yes, No };

  // `no` empty = a single button (`yes`) that just dismisses the message.
  void open(const std::string& message, const std::string& yes, const std::string& no);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }

  // Cancel, and a tap outside the box, answer No (or Yes for a one-button box).
  Result onInput(const InputEvent& e, const Renderer& r);
  // Dims everything behind it, then draws the box in the middle of the screen.
  void draw(Renderer& r);

 private:
  struct Choice {
    std::string label;
    bool yes = false;
    int x = 0, y = 0, w = 0, h = 0;
  };
  void layout(const Renderer& r);
  int buttonAt(int px, int py) const;
  Result answer(const Choice& b) const {
    uiCue(b.yes ? Cue::Confirm : Cue::Cancel);
    return b.yes ? Yes : No;
  }

  bool open_ = false;
  std::string message_;
  std::vector<Choice> choices_;
  std::vector<std::string> lines_;
  int sel_ = 0;
  int x_ = 0, y_ = 0, w_ = 0, h_ = 0, textY_ = 0, rowH_ = 0;
};

}  // namespace ote
