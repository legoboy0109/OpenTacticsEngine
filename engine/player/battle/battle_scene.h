// The battle screen: draws the Battle through a BattleView and turns player
// input into Actions via a small state machine (Idle -> Selected ->
// ActionMenu -> Targeting ...). AI turns are driven from update().
#pragma once
#include <vector>

#include "battle/battle_draw.h"
#include "battle/battle_view.h"
#include "battle/layout.h"
#include "scene.h"
#include "ui/menu.h"

namespace ote {

class BattleScene : public Scene {
 public:
  explicit BattleScene(SceneHost& host);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;

 private:
  enum class Mode { Idle, Selected, ActionMenu, Targeting, BuildMenu, EndMenu, Result };
  enum MenuId { kAttack = 1, kCapture, kWait, kEndTurn };

  // --- battle_scene.cpp: lifecycle, AI, drawing helpers
  void ensureLayout(const Renderer& r);
  bool humanTurn() const;
  void enterResult();
  void aiStep();
  void beginHumanTurn();
  bool humanHasOptions() const;
  std::string hint() const;
  void followCamera(double dt);
  Cell cellAt(int x, int y) const;

  // --- battle_input.cpp: the human state machine
  void handleButton(Button b);
  void handlePointerDown(int x, int y);
  void idleButton(Button b);
  void selectedButton(Button b);
  void targetingButton(Button b);
  void menuInput(const InputEvent& e);
  void moveCursor(int dx, int dy);
  void cycleUnit(int dir);
  void selectUnit(int unit);
  void computeOverlay();
  void openActionMenu();
  void openBuildMenu(Cell cell);
  void openEndMenu();
  void beginTargeting(int preferTarget);
  void cancelAction();
  void finishOrder();
  void anchorActionMenu();
  bool submit(const Action& a);

  SceneHost& host_;
  GameSession& session_;
  Battle& battle_;  // lives in the session; undo assigns into it
  BattleView view_;
  Layout layout_;
  Camera cam_;
  Overlay overlay_;
  Menu menu_;
  Mode mode_ = Mode::Idle;
  bool autoplay_ = false;
  int styleIdx_ = 0;  // visual style of the look in use (index into db.styles)
  float time_ = 0;

  Cell cursor_ = 0;
  int sel_ = -1;  // selected unit id
  Reach reach_;
  Battle undo_;          // copy taken before the unit's Move
  bool hasUndo_ = false;
  Cell undoCell_ = kNoCell;
  std::vector<AttackOption> targets_;
  int targetIdx_ = 0;
  Cell buildCell_ = kNoCell;
  int turnKey_ = -1;
  std::vector<Event> events_;
  std::string resultText_, resultSub_;
  Color resultColor_;
};

}  // namespace ote
