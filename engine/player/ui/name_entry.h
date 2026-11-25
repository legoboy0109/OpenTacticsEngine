// The on-screen keyboard that names a character (docs/EVENTS.md "Naming a character"; the scene command "askName"). The player has
// no text input of its own, so every device types through these keys: the arrows move the highlight (they wrap) and Confirm
// presses it; Cancel deletes the last letter; Menu jumps to OK; a pointer highlights the key under it and a tap presses it. The keys
// are the letters (the Case key switches upper and lower case), the digits, space - ' . and Del, Reset (back to the name the entry
// opened with) and OK (needs at least one letter). The look is a placeholder (owner decision).
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"

namespace ote {

class NameEntry {
 public:
  enum Result { None, Done };
  // `who` = whose name (shown in the heading), `prompt` = the scene's words ("" = a plain heading), `start` = the name it opens
  // with, `maxLetters` = the most letters (1..kMaxNameLetters). Laid out for a `screenW` x `bottom` area.
  void open(const Renderer& r, int screenW, int bottom, const std::string& who, const std::string& prompt, const std::string& start,
            int maxLetters);
  bool isOpen() const { return open_; }
  void close() { open_ = false; }
  Result onInput(const InputEvent& e);
  void draw(Renderer& r) const;

  const std::string& name() const { return name_; }
  int letters() const;
  // For tests and test scripts.
  struct Key { std::string label; int x, y, w, h; };
  const std::vector<Key>& keys() const { return keys_; }
  int highlighted() const { return sel_; }
  bool upper() const { return upper_; }
  int keyIndex(const std::string& label) const;  // -1 = none
  // One log line with where OK, Del and Case are: "name entry: keys OK@x,y Del@x,y Case@x,y A@x,y" (centres).
  std::string describeKeys() const;

 private:
  void press(int k);
  void relabel();
  void move(int dx, int dy);
  int keyAt(int x, int y) const;

  bool open_ = false;
  std::string who_, prompt_, start_, name_;
  int max_ = 12;
  bool upper_ = true;
  std::vector<Key> keys_;
  std::vector<int> row_, col_;  // each key's row and first column in the grid (wide keys take several columns)
  std::vector<int> span_;
  int sel_ = 0;
  int panelX_ = 0, panelY_ = 0, panelW_ = 0, panelH_ = 0, fieldY_ = 0, lh_ = 8;
  std::vector<std::string> promptLines_;
};

}  // namespace ote
