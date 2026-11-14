// One game's page: [Continue], Play, [Load], Game options, Back; with its
// description below. Continue and Load appear according to the game's
// "saving" setting (docs/SAVES.md).
#pragma once
#include <vector>

#include "scene.h"
#include "ui/row_list.h"

namespace ote {

class GameScene : public Scene {
 public:
  GameScene(SceneHost& host, const GameEntry& game);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;

 private:
  enum Kind { kContinue, kPlay, kLoad, kOptions, kBack };
  struct Row {
    Kind kind;
    std::string label, detail;  // detail: second line, only on pages with saving
    bool enabled = true;        // a Continue save that cannot be loaded is greyed out
  };
  void buildRows();
  void layout(const Renderer& r);
  void activate(int row);

  SceneHost& host_;
  GameEntry game_;
  std::vector<Row> rows_;
  bool twoLines_ = false;
  std::string continueProblem_;  // why Continue cannot be used (when it is greyed out)
  RowList list_;
};

}  // namespace ote
