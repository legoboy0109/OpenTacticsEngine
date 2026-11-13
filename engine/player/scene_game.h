// One game's page: Play, Game options, Back; with its description below.
#pragma once
#include "scene.h"
#include "ui/row_list.h"

namespace ote {

class GameScene : public Scene {
 public:
  GameScene(SceneHost& host, const GameEntry& game);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;

 private:
  enum Row { kPlay, kOptions, kBack, kRows };
  void layout(const Renderer& r);
  void activate(int row);

  SceneHost& host_;
  GameEntry game_;
  RowList list_;
};

}  // namespace ote
