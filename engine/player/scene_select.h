// Map select scene (the list of maps of the loaded game).
#pragma once
#include "scene_list.h"

namespace ote {

class MapSelectScene : public ListScene {
 public:
  explicit MapSelectScene(SceneHost& host);

 protected:
  void onChoose(int index) override;
  void onCancel() override { host_.showGame(host_.currentGameId()); }

 private:
  GameSession* session_;
};

// Settings key of the remembered map cursor of a game.
std::string lastMapKey(const std::string& gameId);

}  // namespace ote
