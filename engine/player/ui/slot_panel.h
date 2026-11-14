// The list of the five manual save slots, used both by the launcher's Load
// screen and by the Save / Load pages inside a battle. It shows each slot
// (map, turn, date and time, or "Empty"), greys out saves that cannot be
// loaded and says why, and asks before anything is replaced or left behind.
// The owner only hears about a finished choice.
#pragma once
#include <string>
#include <vector>

#include "saves.h"
#include "ui/confirm.h"
#include "ui/row_list.h"

namespace ote {

class SlotPanel {
 public:
  enum Mode { Save, Load };
  enum Result { None, Back, Picked };

  // `slots` holds SaveStore::kSlots entries (slot 1 first). `loadWarning`, when
  // not empty, is appended to a "Load slot N?" question ("You will leave the
  // battle in progress."); empty = loading needs no question.
  // `warnOnlyForCampaign`: the warning belongs to campaign saves only (a free battle's plain save does not
  // touch the Continue file of a story).
  void open(Mode mode, const std::vector<SaveInfo>& slots, const std::string& loadWarning,
            bool warnOnlyForCampaign = false);
  void close() { open_ = false; confirm_.close(); }
  bool isOpen() const { return open_; }
  // The rectangle the panel fills (logical pixels).
  void setArea(int x, int y, int w, int h) { ax_ = x; ay_ = y; aw_ = w; ah_ = h; }
  // Centred, nearly the whole screen high and at most 380 wide.
  void fitScreen(const Renderer& r);
  // Shows a message the player must dismiss (a failed save, a failed load).
  void showMessage(const std::string& text);

  Result onInput(const InputEvent& e, const Renderer& r);
  void draw(Renderer& r);

  Mode mode() const { return mode_; }
  int slot() const { return picked_; }  // 1-based, valid after Picked

 private:
  void layout(const Renderer& r);
  Result activate(int row);

  bool open_ = false;
  Mode mode_ = Load;
  std::vector<SaveInfo> slots_;
  std::string loadWarning_;
  bool warnOnlyForCampaign_ = false;
  RowList list_;
  ConfirmBox confirm_;
  int pending_ = -1;  // row waiting for an answer in confirm_; -1 = a plain message
  int picked_ = 0;
  int ax_ = 0, ay_ = 0, aw_ = 0, ah_ = 0;
  int titleY_ = 0, footY_ = 0, innerX_ = 0, innerW_ = 0;
};

}  // namespace ote
