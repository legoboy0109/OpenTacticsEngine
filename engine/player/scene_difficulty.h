// The difficulty choice (docs/DIFFICULTY.md): shown at New Game and before a free battle when the game has more than one
// difficulty. One row per difficulty, easiest first, the game's default preselected; the selected one's description and
// what it changes are written under the list. Works with arrows + Confirm, a tap, and Cancel (back to the game page).
#pragma once
#include <functional>
#include <string>

#include "scene.h"
#include "ui/row_list.h"

namespace ote {

class DifficultyScene : public Scene {
 public:
  // `title` names what is being started ("New game", the map's name). onPick gets the chosen id; onCancel goes back.
  DifficultyScene(SceneHost& host, const std::string& title, std::function<void(const std::string&)> onPick,
                  std::function<void()> onCancel);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Title; return a; }

  // What a difficulty changes, in words ("Enemies +2 levels, stats 110%. Experience 50%."); also used by the tests' log.
  static std::string effects(const Database& db, const DifficultyDef& d);

 private:
  void layout(const Renderer& r);
  void activate(int row);

  SceneHost& host_;
  const DifficultyRules& rules_;
  std::string title_;
  std::function<void(const std::string&)> onPick_;
  std::function<void()> onCancel_;
  RowList list_;
};

}  // namespace ote
