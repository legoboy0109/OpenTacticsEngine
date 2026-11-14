// The launcher's "Load" screen: the five save slots of one game.
#pragma once
#include "scene.h"
#include "ui/slot_panel.h"

namespace ote {

class SlotScene : public Scene {
 public:
  SlotScene(SceneHost& host, const GameEntry& game);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;

 private:
  SceneHost& host_;
  GameEntry game_;
  SlotPanel panel_;
};

// Summary of the five manual slots of a game, judged against the game as it is
// now (loads the game's data once for the version check; without it the files
// are only checked for being readable).
std::vector<SaveInfo> inspectSlots(const SaveStore& store, const std::string& gamesDir, const std::string& gameId);

}  // namespace ote
