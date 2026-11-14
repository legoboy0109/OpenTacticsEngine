// The battle screen: draws the Battle through a BattleView and turns player
// input into Actions via a small state machine (Idle -> Selected ->
// ActionMenu -> Targeting ...). AI turns are driven from update().
#pragma once
#include <memory>
#include <vector>

#include "battle/free_cam.h"
#include "battle/battle_draw.h"
#include "battle/battle_view.h"
#include "battle/layout.h"
#include "scene.h"
#include "saves.h"
#include "ui/confirm.h"
#include "ui/item_panel.h"
#include "ui/menu.h"
#include "ui/slot_panel.h"
#include "ui/trade_panel.h"

namespace ote {

class BattleScene : public Scene {
 public:
  explicit BattleScene(SceneHost& host);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;

 private:
  enum class Mode { Idle, Selected, ActionMenu, Targeting, BuildMenu, EndMenu, SlotList, Items, Result };
  // The screens of Mode::Items (battle_items.cpp).
  enum class ItemScreen { List, Choice, Target, Discard, Partner, Trade, Storage, Shop };
  enum MenuId { kAttack = 1, kCapture, kWait, kEndTurn, kSave, kLoad, kLeave,
                kItems, kTrade, kStorage, kShop,
                kUse, kEquip, kStore, kDiscard };

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
  // Box around a square on screen (its top face in the isometric view).
  void cellScreenBox(Cell c, int& x, int& y, int& w, int& h) const;

  // --- battle_input.cpp: the human state machine
  void handleButton(Button b);
  void handlePointerDown(int x, int y);
  void idleButton(Button b);
  void selectedButton(Button b);
  void targetingButton(Button b);
  void menuInput(const InputEvent& e);
  void moveCursor(int dx, int dy);
  bool turnInput(const InputEvent& e);  // moving the camera (the 3D kinds of view); true = the event was used
  void turnButtonBox(int which, int& x, int& y, int& w, int& h) const;  // the on-screen camera buttons
  int cameraButtons() const { return proj_.free3d() ? 6 : 2; }  // turn left/right; full 3D adds tilt and zoom
  int quarterTurns() const { return proj_.free3d() ? proj_.cam3().quarter() : ((turn_ % 4) + 4) % 4; }
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

  // --- battle_items.cpp: the item screens (Items, Trade, Storage, Shop; docs/ITEMS.md "Player")
  void openItems(int selectSlot = 0);
  void openTrade();
  void openStorage();
  void openShop();
  void openChoiceMenu();            // the small menu on the item picked in the list
  void itemsInput(const InputEvent& e);
  void drawItems(Renderer& r);
  void itemChoice(int id);          // an entry of the small menu on an item (Use, Equip, Store, Discard)
  void chooseUseTarget(int slot);   // asks for the target when there are several, else uses at once
  void openTradeWith(int partner);
  void tradePicked();
  void closeItems();                // leaves every item screen without returning to the action menu
  void itemActionDone();            // after an action that does not end the turn: back to the action menu
  bool submitItem(const Action& a); // submit() for those; then itemActionDone()
  std::string itemsHint() const;

  // --- battle_saves.cpp: Continue save, manual Save / Load (docs/SAVES.md)
  bool savingOn() const;       // saves usable here: folder, game setting, not autoplay
  bool manualSaves() const;    // savingOn and the game allows slots
  void updateSaves();          // writes the Continue save when one is due (view idle)
  void writeContinue();
  void clearContinue();
  void saveFailed(const std::string& err);
  void openSlotList(SlotPanel::Mode mode);
  void slotInput(const InputEvent& e);
  void say(const std::string& text);  // short message in the hint line

  SceneHost& host_;
  GameSession& session_;
  Battle& battle_;  // lives in the session; undo assigns into it
  BattleView view_;
  Layout layout_;
  Camera cam_;
  MapProj proj_;  // where squares are on screen in the style in use
  std::unique_ptr<Map3D> map3d_;  // the drawer of the 3D kinds of view (only in those)
  FreeCam freeCam_;     // full 3D: the player's camera
  int turn_ = 0;        // 3D isometric: quarter turns of the camera asked for
  float turnPos_ = 0;   // where the camera is now (eases towards turn_)
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
  bool itemsUsed_ = false;  // an item action was done in this order: the move can no longer be undone
  Cell undoCell_ = kNoCell;
  std::vector<AttackOption> targets_;
  int targetIdx_ = 0;
  Cell buildCell_ = kNoCell;
  int turnKey_ = -1;
  ItemScreen itemScreen_ = ItemScreen::List;
  ItemPanel itemPanel_;
  TradePanel tradePanel_;
  ConfirmBox confirm_;
  int itemSlot_ = -1;        // the item being worked on (slot in the acting unit's inventory)
  int partner_ = -1;         // trade partner unit
  std::vector<int> shopList_;   // item indexes in the shop panel's rows
  std::vector<Event> events_;
  std::string resultText_, resultSub_;
  Color resultColor_;

  SaveStore store_;
  SlotPanel slots_;
  bool dirty_ = true;        // the Continue save is out of date
  long long savedTurn_ = -1; // round/side of the last turn start that was saved
  bool continueCleared_ = false;
  bool saveFailWarned_ = false;
  std::string notice_;
  float noticeTime_ = 0;
};

}  // namespace ote
