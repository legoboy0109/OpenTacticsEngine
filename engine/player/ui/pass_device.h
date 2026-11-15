// The cover of a hot-seat game with fog of war ("Pass the device to <side>"): it hides the whole screen, not
// even the map is drawn under it, until the next player confirms.
#pragma once
#include <string>

#include "platform/platform.h"

namespace ote {

class PassDevice {
 public:
  void open(const std::string& side, Color sideColor);
  void close() { open_ = false; }
  bool isOpen() const { return open_; }
  // True when the player confirmed (Confirm, or a tap anywhere).
  bool confirmed(const InputEvent& e) const;
  void draw(Renderer& r) const;

 private:
  bool open_ = false;
  std::string side_;
  Color color_;
};

}  // namespace ote
