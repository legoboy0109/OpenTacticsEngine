// Map select scene (the list of maps of the loaded game).
#pragma once
#include "scene_list.h"
#include "ui/confirm.h"

namespace ote {

class MapSelectScene : public ListScene {
 public:
  explicit MapSelectScene(SceneHost& host);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;

 protected:
  void onChoose(int index) override;
  void onCancel() override { host_.showGame(host_.currentGameId()); }

 private:
  void startMap(int index);

  GameSession* session_;
  ConfirmBox confirm_;  // "This replaces your Continue save."
  int pendingMap_ = -1;
  std::vector<int> maps_;  // the list's rows as indices into db.maps (cut-scene maps are not listed)
};

// Settings key of the remembered map cursor of a game.
std::string lastMapKey(const std::string& gameId);

}  // namespace ote
