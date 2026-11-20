// The commander choice (docs/COMMANDERS.md "Player"): shown before a battle for each person-controlled side whose map says "commander": "choose", after the
// faction pick. One row per commander the side may pick (its faction's list, else every commander); the selected one's description, passive skills,
// powers with their prices and the meter's rule are written under the list. Works with arrows + Confirm, a tap, and Cancel (back to map select).
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "scene.h"
#include "ui/row_list.h"

namespace ote {

class CommanderScene : public Scene {
 public:
  // `title` names the side that picks ("Blue Army"); `choices` are indices into Database::commanders. onPick gets the commander's index in
  // Database::commanders; onCancel goes back.
  CommanderScene(SceneHost& host, const std::string& title, std::vector<int> choices, std::function<void(int)> onPick, std::function<void()> onCancel);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Title; return a; }

 private:
  void layout(const Renderer& r);
  void activate(int row);

  SceneHost& host_;
  const std::vector<CommanderDef>& list_;
  std::vector<int> choices_;
  std::string title_;
  std::function<void(int)> onPick_;
  std::function<void()> onCancel_;
  RowList rows_;
};

}  // namespace ote
