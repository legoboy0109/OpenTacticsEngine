// Game select and map select scenes.
#pragma once
#include "scene_list.h"

namespace ote {

struct GameEntry {
  std::string id;    // folder name under the games dir
  std::string name;  // display name from game.json (falls back to id)
  bool readable = true;
};

class GameSelectScene : public ListScene {
 public:
  GameSelectScene(SceneHost& host, const std::vector<GameEntry>& games, const std::string& dir);

 protected:
  void onChoose(int index) override { host_.loadGame(ids_[index]); }

 private:
  std::vector<std::string> ids_;
};

class MapSelectScene : public ListScene {
 public:
  explicit MapSelectScene(SceneHost& host);

 protected:
  void onChoose(int index) override { host_.startBattle(session_->db.maps[index].id); }
  void onCancel() override { host_.showGameSelect(); }

 private:
  GameSession* session_;
};

}  // namespace ote
