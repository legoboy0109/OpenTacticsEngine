// The battle screen: draws the Battle through a BattleView and turns player
// input into Actions via a small state machine (Idle -> Selected ->
// ActionMenu -> Targeting ...). AI turns are driven from update().
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "battle/free_cam.h"
#include "core/campaign/playthrough.h"
#include "battle/battle_draw.h"
#include "battle/battle_view.h"
#include "battle/layout.h"
#include "battle/promotion_screen.h"
#include "battle/srpg_menu_ids.h"
#include "battle/srpg_pick.h"
#include "battle/unit_info.h"
#include "scene.h"
#include "saves.h"
#include "ui/confirm.h"
#include "ui/dialogue_box.h"
#include "ui/item_panel.h"
#include "ui/menu.h"
#include "ui/pass_device.h"
#include "ui/slot_panel.h"
#include "ui/trade_panel.h"

namespace ote {

class BattleScene : public Scene {
 public:
  explicit BattleScene(SceneHost& host);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override;  // battle_sound.cpp

 private:
  enum class Mode { Idle, Selected, ActionMenu, Targeting, BuildMenu, EndMenu, SlotList, Items, Objective, StructureMenu, UpgradeMenu, Research, Result,
              PickUnit, PickCell, UnitInfo, Promotion };  // the last three: docs/SRPG.md "Player UI", battle_srpg_*.cpp
  // The screens of Mode::Items (battle_items.cpp).
  enum class ItemScreen { List, Choice, Target, Discard, Partner, Trade, Storage, Shop };
  enum MenuId { kAttack = 1, kCapture, kWait, kEndTurn, kSave, kLoad, kLeave,
                kItems, kTrade, kStorage, kShop,
                kUse, kEquip, kStore, kDiscard, kObjective,
                kSupply, kBuildEntry, kUpgradeEntry, kResearchEntry,
                kTalkBase = ote::kTalkBase };  // kTalkBase + n = the n-th Talk entry of the action menu (ids of the other groups: srpg_menu_ids.h)

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
  // Fills a cell's screen box (from cellScreenBox) in a colour: the box, or the hexagon inside it on a hexagon map.
  void fillCellBox(Renderer& r, int x, int y, int w, int h, Color col) const;

  // --- battle_fog.cpp: fog of war (docs/ECONOMY.md "Fog of war > Player")
  void initFog();
  void setViewerSide(int side);   // who the screen is drawn for
  void raiseCover(int side);      // hot-seat: "Pass the device to <side>"
  bool fogTurnGate();             // a human turn is about to begin: true = the cover went up and the turn waits
  void coverConfirmed();

  // --- battle_sound.cpp: sound (docs/SOUND.md)
  void soundCue(Cue cue, int unit = -1);  // a menu / map cue with the unit's type for the lookup
  void soundResult(bool anyHuman, bool humanWon);  // the victory / defeat jingle

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
  // Whole quarter turns of the camera, which way the arrow keys go; a hexagon map's sixths count as the nearest quarter.
  int quarterTurns() const {
    if (proj_.free3d()) return proj_.cam3().quarter();
    const int n = proj_.turnCount(), t = ((turn_ % n) + n) % n;
    return n == 4 ? t : (t * 4 + n / 2) / n % 4;
  }
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

  // --- battle_srpg_menu.cpp: the strategy-RPG entry groups of the menus (docs/SRPG.md "Player UI"; step U0).
  // The dispatcher calls one function pair per group, each group in its own file so later steps do not collide.
  // A builder appends its entries (none when it does not apply); a handler returns true when it took the id.
  void addSrpgEntries(std::vector<MenuItem>& items);     // action menu, after Talk: Support, abilities, Rescue.., Promote
  void addSrpgInfoEntry(std::vector<MenuItem>& items);   // action menu, before Wait: Info
  void addSrpgEndEntries(std::vector<MenuItem>& items);  // battle menu, after Objective: Unit
  bool srpgMenuChosen(int id);                           // an id of the action menu outside the old ones
  bool srpgEndMenuChosen(int id);
  void addSupportEntries(std::vector<MenuItem>& items);    // battle_menu_supports.cpp (U5)
  bool supportChosen(int id);
  void addAbilityEntries(std::vector<MenuItem>& items);    // battle_menu_abilities.cpp (U3)
  bool abilityChosen(int id);
  void addStaffEntries(std::vector<MenuItem>& items);      // battle_menu_staves.cpp (docs/SRPG.md section 8): Staff, its list and target pick
  bool staffChosen(int id);
  void openStaffList();
  void staffListChosen(int slot);
  void addRescueEntries(std::vector<MenuItem>& items);     // battle_menu_rescue.cpp (U4): Rescue / Drop / Give / Take / Pair up / Switch
  bool rescueChosen(int id);
  void addPromotionEntries(std::vector<MenuItem>& items);  // battle_menu_promotion.cpp (U2)
  bool promotionChosen(int id);
  bool promotionItemUse(int slot);                         // a promotion item picked in the item list (battle_items.cpp calls it)
  void openPromotion(int slot, const std::vector<int>& types);  // slot -1 = the Promote entry; Mode::Promotion
  void promotionInput(const InputEvent& e);
  void promotionBack();
  void drawPromotion(Renderer& r);
  void drawPromotionPicture(Renderer& r, int type, int x, int y, int w, int h);
  std::string promotionHint() const;
  void addInfoEntry(std::vector<MenuItem>& items);         // battle_menu_info.cpp (U1): Info (action menu) and Unit (battle menu)
  bool infoChosen(int id);
  void addUnitEntry(std::vector<MenuItem>& items);
  bool unitEntryChosen(int id);

  // --- battle_srpg_target.cpp: pick a unit / pick a square, for every group. The caller opens it from a menu handler
  // (the action menu is closed by then); Cancel goes back to the action menu; Confirm clears the mode and calls
  // `chosen`, which normally submits an Action and calls finishOrder() (or opens another menu).
  // `mark` is called for the highlighted candidate and may fill the overlay: `reach` = legal squares are drawn by the
  // pick itself, `attack` = what the choice would do (e.g. the squares two units would end on).
  using PickMark = std::function<void(int candidate, Overlay& overlay)>;
  void beginPickUnit(std::vector<int> units, std::function<void(int)> chosen, PickMark mark = PickMark());
  void beginPickCell(std::vector<Cell> cells, std::function<void(Cell)> chosen, PickMark mark = PickMark());
  void pickButton(Button b);
  void pickPointer(int x, int y);
  void pickMoved();   // cursor, overlay and the sound after the highlight changed
  std::string srpgHint() const;  // the hint line of PickUnit / PickCell / UnitInfo

  // --- battle_srpg_unitinfo.cpp: Mode::UnitInfo (the pages are in unit_info*.cpp)
  void openUnitInfo(int unit);
  void unitInfoInput(const InputEvent& e);
  void drawUnitInfoOverlay(Renderer& r);

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

  // --- battle_economy.cpp: structure menu, Upgrade, Research panel, Supply (docs/ECONOMY.md "Player")
  bool openStructureAt(Cell cell);         // the menu / list of an own structure; false = nothing to offer there
  void openStructureMenu(Cell cell);
  void openUpgradeMenu(int structure);
  void openResearchPanel();
  void economyMenuChosen(int id);          // an entry of the structure or upgrade menu
  void researchInput(const InputEvent& e);
  void backToStructure();                  // Cancel in Build / Upgrade: the structure menu, or the map
  Overlay supplyOverlay() const;           // the units a Supply would fill, while that entry is selected
  void logInspected();                     // [info] line for the unit under the cursor when it has fuel / ammunition
  std::string economyHint() const;
  void drawEconomy(Renderer& r);           // under the panel: the flash of an upgraded structure

  // --- battle_story.cpp: story events in the player (docs/EVENTS.md): dialogue box, Talk, objectives
  void updateDialogue(double dt);
  void dialogueResult(DialogueBox::Result res);
  void answerChosen(int pick);  // a question's answer: the core goes on and the view plays what follows
  PortraitArt portraitFor(const std::string& id) const;
  void addTalkEntries(std::vector<MenuItem>& items);
  void talkChosen(int index);
  void openObjective();
  bool objectiveInput(const InputEvent& e);  // true = the event was used
  void drawStory(Renderer& r);               // under the panel: the flash of a unit that changed sides
  void drawObjectiveBox(Renderer& r);

  // --- battle_campaign.cpp: a chapter battle of the campaign (docs/EVENTS.md "Campaign")
  bool chapterBattle() const { return session_.campaign != nullptr; }
  void chapterDecided();       // the battle is over: take the result into the playthrough and save
  void leaveResult();          // Confirm on the result: on to the next step of the story, or the map list

  // --- battle_saves.cpp: Continue save, manual Save / Load (docs/SAVES.md)
  bool savingOn() const;       // saves usable here: folder, game setting, not autoplay
  bool continueOn() const;     // the Continue file belongs to this battle (never a free battle of a campaign game)
  bool buildSave(Json& file, std::string& err) const;  // a campaign save for a chapter battle, else a plain one
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
  int turn_ = 0;        // 3D isometric: turns of the camera asked for (quarters; sixths on hexagon maps)
  float turnPos_ = 0;   // where the camera is now (eases towards turn_)
  Overlay overlay_;
  Menu menu_;
  Mode mode_ = Mode::Idle;
  bool autoplay_ = false;
  int styleIdx_ = 0;  // visual style of the look in use (index into db.styles)
  float time_ = 0;
  bool fogActive_ = false;  // the screen is drawn for one team (fog on, not --show-all, a human side exists)
  bool hotSeat_ = false;    // fogActive_ and humans of two or more teams share the device: the cover between turns
  bool covered_ = false;    // the cover is up
  int viewerSide_ = -1;     // the human side the screen is drawn for (its team is the viewer)
  int coverTeam_ = -1;      // the team the cover was last confirmed for (-1 = none yet: a new or resumed battle starts with it)
  PassDevice cover_;

  Cell cursor_ = 0;
  int sel_ = -1;  // selected unit id
  Reach reach_;
  Battle undo_;          // copy taken before the unit's Move
  bool hasUndo_ = false;
  bool itemsUsed_ = false;  // an item action was done in this order: the move can no longer be undone
  bool ambushed_ = false;   // fog, "canAct": the order was stopped by an ambush, the unit may still act, the move stays
  Cell undoCell_ = kNoCell;
  std::vector<AttackOption> targets_;
  int targetIdx_ = 0;
  Cell buildCell_ = kNoCell;
  int turnKey_ = -1;
  int aiTurnKey_ = -1;  // the AI turn the log last said it started
  ItemScreen itemScreen_ = ItemScreen::List;
  ItemPanel itemPanel_;
  ItemPanel researchPanel_;       // Mode::Research
  std::vector<int> techRows_;     // technology of each row of researchPanel_
  bool structureMenuBack_ = false;  // Build / Upgrade was opened from the structure menu: Cancel goes back
  int upgradeStructure_ = -1;     // the structure of the open Upgrade menu
  std::string inspectedLine_;     // the last logInspected() line
  TradePanel tradePanel_;
  ConfirmBox confirm_;
  PickList pick_;                         // Mode::PickUnit / PickCell (battle_srpg_target.cpp)
  bool pickCells_ = false;                // the candidates are squares (else unit ids)
  std::function<void(int)> pickChosen_;
  PickMark pickMark_;
  UnitInfoView unitInfo_;                 // Mode::UnitInfo
  PromotionScreen promoScreen_;           // Mode::Promotion with several types (battle_menu_promotion.cpp)
  bool promoAsk_ = false;                 // one type: confirm_ asks "Promote ...?" instead of the screen
  int promoSlot_ = -1;                    // the promotion item's slot, -1 = the Promote entry
  int promoType_ = -1;                    // the type the question is about
  bool staffList_ = false;                // the open menu is the Staff list (Cancel goes back to the action menu)
  bool staffFcOn_ = false;                // a staff target is being picked: staffFc_ is drawn as the forecast box
  StaffForecast staffFc_;
  bool unitInfoFromAction_ = false;       // opened from the action menu: Cancel returns there (else to the map)
  int itemSlot_ = -1;        // the item being worked on (slot in the acting unit's inventory)
  int partner_ = -1;         // trade partner unit
  std::vector<int> shopList_;   // item indexes in the shop panel's rows
  std::vector<Event> events_;
  DialogueBox dialogue_;
  int dialogueSerial_ = -1;       // the line (BattleView::dialogue.serial) the box is showing
  std::vector<TalkOption> talk_;  // the Talk entries of the action menu now open
  std::string resultText_, resultSub_;
  Color resultColor_;
  ChapterResult chapterResult_ = ChapterResult::NotOver;  // chapter battles: what finishChapterBattle said
  bool leaving_ = false;  // leaveResult() was called
  float resultTime_ = 0;  // autoplay of a chapter battle: how long the result has been up

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
