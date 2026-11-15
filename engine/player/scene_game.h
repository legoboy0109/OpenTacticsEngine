// One game's page: [Continue], Play, [Load], Game options, Back; with its
// description below. Continue and Load appear according to the game's
// "saving" setting (docs/SAVES.md).
// A game with a campaign (docs/EVENTS.md "Campaign") has [Continue], New game, [Load], Game options,
// Back and then, under a heading, the maps that may be played on their own (its "freeBattles").
#pragma once
#include <vector>

#include "scene.h"
#include "ui/confirm.h"
#include "ui/row_list.h"

namespace ote {

class GameScene : public Scene {
 public:
  GameScene(SceneHost& host, const GameEntry& game);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Title; return a; }  // the game's title music

 private:
  enum Kind { kContinue, kPlay, kNewGame, kLoad, kOptions, kBack, kHeading, kFreeMap };
  struct Row {
    Kind kind;
    std::string label, detail;  // detail: second line, only on pages with saving
    bool enabled = true;        // a Continue save that cannot be loaded is greyed out
    std::string mapId;          // kFreeMap
  };
  void buildRows();
  void layout(const Renderer& r);
  void activate(int row);
  void startNewGame();
  void skipHeading(Button dir);  // the heading row cannot be selected: step past it

  SceneHost& host_;
  GameEntry game_;
  std::vector<Row> rows_;
  bool twoLines_ = false;
  std::string continueProblem_;  // why Continue cannot be used (when it is greyed out)
  RowList list_;
  ConfirmBox confirm_;  // "This replaces your Continue save." before New game
};

}  // namespace ote
