// The launcher's first screen: the list of games, plus Options and Quit.
// The highlighted game's description, author and map count show on the right.
#pragma once
#include "scene.h"
#include "ui/confirm.h"
#include "ui/row_list.h"

namespace ote {

class HomeScene : public Scene {
 public:
  explicit HomeScene(SceneHost& host);
  void onInput(const InputEvent& e) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override { SceneAudio a; a.kind = SceneAudio::Silent; return a; }  // the launcher is silent

 private:
  void layout(const Renderer& r);
  void activate(int row);
  void drawInfo(Renderer& r, int x, int y, int w, int h) const;
  int gameCount() const { return (int)host_.games().size(); }
  int optionsRow() const { return gameCount(); }
  bool hasQuit() const;
  std::string label(int row) const;

  SceneHost& host_;
  RowList list_;
  ConfirmBox exitAsk_;  // "Exit the player?" (launcherBack() == ConfirmExit)
  int infoX_ = 0, infoW_ = 0;  // right panel, from the last layout
};

}  // namespace ote
