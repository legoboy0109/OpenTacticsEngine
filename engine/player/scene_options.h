// Options screen, shared by the global options and one game's overrides.
// One row per entry of optionDefs() (options/option_defs.h), then "Reset to
// defaults" and "Back". Changes apply and are stored immediately; the App
// writes the settings file when the screen is left.
#pragma once
#include <vector>

#include "core/data/database.h"
#include "options/option_defs.h"
#include "scene.h"
#include "ui/row_list.h"

namespace ote {

class OptionsScene : public Scene {
 public:
  // gameId empty = global options; otherwise that game's overrides.
  OptionsScene(SceneHost& host, const std::string& gameId);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;

 private:
  bool perGame() const { return !gameId_.empty(); }
  int defCount() const { return (int)optionDefs().size(); }
  int resetRow() const { return defCount(); }
  int backRow() const { return defCount() + 1; }

  // --- logic (scene_options.cpp)
  Json storedValue(int def) const;  // this level's value (Null = unset)
  int currentChoice(int def);       // index into choices_[def]
  std::string choiceLabel(int def, int index) const;
  bool overridden(int def) const;
  void change(int def, int delta);
  void resetAll();
  void leave();
  void activate(int row);

  // --- drawing (scene_options_draw.cpp)
  void layout(const Renderer& r);  // fills list_, rowX_/rowW_/valueX_ and the preview box
  void drawRow(Renderer& r, int row);
  void drawPreview(Renderer& r);
  std::string title() const;

  SceneHost& host_;
  std::string gameId_;
  const GameEntry* entry_ = nullptr;
  std::vector<std::vector<OptionChoice>> choices_;  // per option def; [0] = unset choice
  std::vector<std::string> globalLabel_;            // per-game: what "Use global" currently means
  Database db_;                                     // per-game: for the style swatches
  bool dbLoaded_ = false;
  RowList list_;
  int rowX_ = 0, rowW_ = 0, valueX_ = 0, arrowW_ = 0;  // from the last layout, for pointer hits
  int previewY_ = 0, previewH_ = 0;                     // preview box (height 0 = no room)
};

}  // namespace ote
