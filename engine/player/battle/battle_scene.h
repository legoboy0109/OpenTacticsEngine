// The battle screen: draws the Battle through a BattleView and turns player
// input into Actions via a small state machine (Idle -> Selected ->
// ActionMenu -> Targeting ...). AI turns are driven from update().
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include "battle/fight_scene.h"
#include "battle/free_cam.h"
#include "core/campaign/playthrough.h"
#include "battle/battle_draw.h"
#include "battle/battle_view.h"
#include "battle/edge_scroll.h"
#include "battle/layout.h"
#include "battle/promotion_screen.h"
#include "battle/srpg_menu_ids.h"
#include "battle/srpg_pick.h"
#include "battle/unit_info.h"
#include "battle/enemy_ranges.h"
#include "battle/weapon_choice.h"
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
  // `preparing` = the preparation screen of a map that has one (docs/EVENTS.md "Preparation"): the Battle in the session has been
  // set up but not started, and the scene shows the screen first. battle_prep*.cpp.
  explicit BattleScene(SceneHost& host, bool preparing = false);
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;
  SceneAudio audioContext() const override;  // battle_sound.cpp
  void onSuspend() override;                 // battle_saves.cpp

 private:
  enum class Mode { Idle, Selected, ActionMenu, Targeting, BuildMenu, EndMenu, SlotList, Items, Objective, StructureMenu, UpgradeMenu, Research, Result,
              PickUnit, PickCell, UnitInfo, Promotion,  // the last three: docs/SRPG.md "Player UI", battle_srpg_*.cpp
              Prep,                                     // the preparation screen (battle_prep*.cpp)
              DifficultyMenu,                           // the Difficulty entry of the end menu (battle_difficulty.cpp)
              WeaponPick,                               // ...
              Facing,                                   // after Wait in a facing game: the small direction pick (battle_facing.cpp; docs/SRPG.md "Facing")
              ResearchCancel };                         // "Cancel research?" menu opened from the research panel (battle_economy.cpp)                             // Attack with "attackWeaponChoice": "pick": the weapon list (battle_weapon_pick.cpp)
  // The screens of Mode::Items (battle_items.cpp).
  enum class ItemScreen { List, Choice, Target, Discard, Partner, Trade, Storage, Shop };
  enum MenuId { kAttack = 1, kCapture, kWait, kEndTurn, kSave, kLoad, kLeave,
                kItems, kTrade, kStorage, kShop,
                kUse, kEquip, kStore, kDiscard, kObjective,
                kSupply, kBuildEntry, kUpgradeEntry, kResearchEntry, kCancelResearchEntry, kKeepResearchEntry, kDifficultyEntry, kTestLevelEntry, kRangesEntry, kMoveOnEntry, kCancelChargeEntry,
                kTalkBase = ote::kTalkBase };  // kTalkBase + n = the n-th Talk entry of the action menu (ids of the other groups: srpg_menu_ids.h)

  // --- battle_scene.cpp: lifecycle, AI, drawing helpers
  void ensureLayout(const Renderer& r);
  bool humanTurn() const;
  void enterResult();
  void aiStep();
  void beginHumanTurn();
  bool humanHasOptions() const;
  std::string hint() const;      // the line for the mode, with "(power ready)" added in the idle one (battle_commander.cpp)
  std::string hintBase() const;  // the line for the mode
  void followCamera(double dt);
  // --- battle_edge_scroll.cpp: scrolling the map with a mouse at the edge of the picture (edge_scroll.h)
  void trackPointer(const InputEvent& e);
  bool cursorUnderPointer() const;
  bool edgeScrollAllowed() const;
  void updateEdgeScroll(double dt);
  void logCamera();            // "camera: x,y" once the camera has been still for a while after moving (tests and bug reports)
  void hoverAt(int x, int y);  // the pointer rests on (x, y): the cursor goes to that square
  bool prepOnMap() const;      // battle_prep.cpp: the preparation screen shows the map (Positions / View)
  bool prepOnMapView() const;  // ... and it is the View one
  Cell cellAt(int x, int y) const;
  // Box around a square on screen (its top face in the isometric view).
  void cellScreenBox(Cell c, int& x, int& y, int& w, int& h) const;
  // Fills a cell's screen box (from cellScreenBox) in a colour: the box, or the hexagon inside it on a hexagon map.
  void fillCellBox(Renderer& r, int x, int y, int w, int h, Color col) const;

  // --- battle_other_map.cpp: a story scene on another map of the game (docs/EVENTS.md "Showing another map")
  struct OtherMap;                    // the battle's own map, put aside while a shown map is on screen
  std::shared_ptr<OtherMap> other_;   // null = the battle's own map is on screen
  bool onOtherMap() const { return other_ != nullptr; }
  const Battle& ownBattle() const;    // the battle itself, also while another map is on screen (saves, music)
  void swapOwnBattle();               // while another map is shown: swaps the battle's own Battle into battle_ and back (nothing otherwise)
  const MapDef& shownMapDef() const;  // the map on screen (the shown one, else the session's)
  void updateOtherMap();              // the view waits at MapShown / MapReturned: swap the maps
  void showOtherMap();
  void returnFromOtherMap();

  // --- battle_fight.cpp: the side-view battle scene (docs/BATTLE_SCENE.md)
  void updateFight(double dt);  // starts the scene when the view holds a fight, plays it, hands the result back
  void startFight();

  // --- battle_fog.cpp: fog of war (docs/ECONOMY.md "Fog of war > Player")
  void initFog();
  void watchFogSwitch();          // a story command switched fog on / off (setFog): the screen follows (battle_fog.cpp)
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
  bool turnInput(const InputEvent& e);
  bool listOpen() const;  // a list or menu has the input: the mouse wheel scrolls it instead of zooming (battle_input.cpp)  // moving the camera (the 3D kinds of view); true = the event was used
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
  // --- battle_ap.cpp: the action-point turn system (docs/SRPG.md "Action points"); all of it does nothing in a game without it
  void apActionEntries(std::vector<MenuItem>& items, const std::vector<AttackOption>& opts) const;  // Attack's price / greyed reason, Move
  void apKeepAffordable(std::vector<AttackOption>& opts) const;  // drops the attacks the unit may not make or the pool cannot pay
  void beginMoveOn();                                            // the Move entry: pick another square for the unit's remaining movement
  std::string apSelectedHint() const;                            // the hint line while a destination is picked: names the AP price
  // Facing (docs/SRPG.md "Facing"; battle_facing.cpp): the pick after Wait, skippable (Cancel / Confirm keep the unit's own direction).
  void beginFacing();
  void facingButton(Button b);
  void facingPointer(Cell c);
  void commitFacing(int dir);                                    // Wait with this direction (-1 = keep)
  Overlay faceOverlay() const;                                   // the neighbouring squares (blue) and the one chosen (red)
  std::string facingHint() const;
  int facingDir_ = -1;                                           // the direction highlighted in Mode::Facing
  bool movingOn_ = false;                                        // Selected was opened from the action menu (Cancel goes back to it)
  void logActionMenu(const std::vector<MenuItem>& items);
  void openBuildMenu(Cell cell);
  void openEndMenu();
  // battle_difficulty.cpp (docs/DIFFICULTY.md): the end menu's Difficulty row and its list
  int difficultyNow() const;
  void addDifficultyEntry(std::vector<MenuItem>& items);
  void addTestLevelEntry(std::vector<MenuItem>& items);  // docs/DIFFICULTY.md "Test level": a greyed row that says the run is a test (none otherwise)
  void openDifficultyMenu();
  void difficultyChosen(int id);
  void beginAttack(int preferTarget);   // Attack: straight to the targets, or the weapon list first (game.json "attackWeaponChoice")
  void beginTargeting(int preferTarget);
  void enterTargeting(int preferTarget);  // targets_ is filled: show them (Mode::Targeting)
  // --- battle_weapon_pick.cpp: the weapon list of "attackWeaponChoice": "pick" (docs/SRPG.md "Menu options")
  void openWeaponPick(int preferTarget, int selectRow = -1);
  void weaponPickInput(const InputEvent& e);
  void weaponPicked(int row);
  bool equipForAttack();   // submits the Equip order the picked weapon needs; false = refused
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
  // --- battle_commander.cpp: commander powers in the map menu (docs/COMMANDERS.md "Player")
  void addPowerEntries(std::vector<MenuItem>& items);  // after End Turn: one "Power: <name>" per power, greyed when it cannot be used
  bool powerMenuChosen(int id);                        // true when `id` was a power's (it is used)
  std::string withPowerHint(std::string line) const;
  void addSupportEntries(std::vector<MenuItem>& items);    // battle_menu_supports.cpp (U5)
  bool supportChosen(int id);
  void addAbilityEntries(std::vector<MenuItem>& items);    // battle_menu_abilities.cpp (U3)
  bool abilityChosen(int id);
  void stealMenu(int user, int ability, int target, std::function<void()> back);  // battle_menu_abilities.cpp: "Steal which?"
  void addStaffEntries(std::vector<MenuItem>& items);      // battle_menu_staves.cpp (docs/SRPG.md section 8): Staff, its list and target pick
  bool staffChosen(int id);
  void openStaffList();
  void staffListChosen(int slot);
  // Stage 2 (docs/SRPG.md section 8): the picks that come after the unit, one stage at a time. `back` is what Cancel does at
  // that stage (the stage before it: the unit pick; for a staff that picks no unit, the action menu).
  void staffAfterTarget(int user, int slot, int target, std::function<void()> back);
  void staffWarpPick(int user, int slot, int target, std::function<void()> back);
  void staffRepairMenu(int user, int slot, int target, std::function<void()> back);
  void staffLightPick(int user, int slot);
  void staffUnlockPick(int user, int slot);  // an unlock staff: the square of the door or chest (battle_menu_staves.cpp)
  // Doors, chests and visiting (battle_menu_places.cpp; docs/SRPG.md section 10).
  void addPlaceEntries(std::vector<MenuItem>& items);
  bool placeChosen(int id);
  void placeBreakPick();
  void placeSubmit(const Action& a);
  void clearStaffHint();  // takes the stage hints ("Pick a square to light") off the hint line once the picking is over
  void staffUse(int user, int slot, int target, Cell dest, int itemSlot);  // submit, then the usual end of an order
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
  // Optional, set by the caller AFTER begin*: Cancel calls `pickBack_` instead of returning to the action menu (a second stage
  // steps back to the first); `pickWhy_` gives the reason for a tapped unit / square that is not a candidate ("" = none).
  std::function<void()> pickBack_;
  std::function<std::string(int)> pickWhy_;
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
  void openTradeWith(int partner, int col = 0, int row = 0);
  void tradePicked();
  void closeItems();                // leaves every item screen without returning to the action menu
  void itemActionDone();            // after an action that does not end the turn: back to the action menu
  bool submitItem(const Action& a); // submit() for those; then itemActionDone()
  std::string itemsHint() const;
  std::string tradeHint() const;

  // --- battle_economy.cpp: structure menu, Upgrade, Research panel, Supply (docs/ECONOMY.md "Player")
  bool openStructureAt(Cell cell);         // the menu / list of an own structure; false = nothing to offer there
  void openStructureMenu(Cell cell);
  void openUpgradeMenu(int structure);
  void openResearchPanel();
  void openResearchCancel();  // asks before giving up the research under way
  void economyMenuChosen(int id);          // an entry of the structure or upgrade menu
  void researchInput(const InputEvent& e);
  void backToStructure();                  // Cancel in Build / Upgrade: the structure menu, or the map
  Overlay supplyOverlay() const;           // the units a Supply would fill, while that entry is selected
  Overlay aimOverlay() const;              // aiming a line-fire or area weapon: where it can be aimed (blue) and what the shot covers (red)
  void logInspected();                     // [info] line for the unit under the cursor when it has fuel / ammunition
  std::string economyHint() const;
  void drawEconomy(Renderer& r);           // under the panel: the flash of an upgraded structure
  void drawMapChange(Renderer& r);         // battle_mapchange.cpp: the flash of squares that changed terrain

  // --- battle_story.cpp: story events in the player (docs/EVENTS.md): dialogue box, Talk, objectives
  void updateDialogue(double dt);
  void dialogueResult(DialogueBox::Result res);
  void answerChosen(int pick);  // a question's answer: the core goes on and the view plays what follows
  PortraitArt portraitFor(const std::string& id, int face = 0) const;
  // battle_bubble.cpp (docs/EVENTS.md "Speech bubbles", "Notices"): how the current line is shown, and where its speaker is.
  DialogueFrame dialogueFrame(const DialogueState& d) const;
  bool speakerAnchor(int unit, BubbleAnchor& a) const;  // false = not on the map's part of the screen (or hidden by fog)
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

  // --- battle_prep.cpp, battle_prep_items.cpp: the preparation screen (docs/EVENTS.md "Preparation"). Mode::Prep; its state is PrepUi.
  struct PrepUi;
  std::shared_ptr<PrepUi> prep_;  // null in an ordinary battle
  bool quoteStarted_ = false;     // the scene is waiting on a quote whose lines the view is saying (battle_fight.cpp)
  void prepStart();
  void prepOpenScreen();          // opens the main menu (after the beforePrep scene, if the map has one)
  void prepRebuild();             // the not-yet-started Battle again from the choice made so far (also after a trade or a purchase)
  void prepChanged();             // after any change: rebuild, redraw the picture, write the Continue save
  void prepInput(const InputEvent& e);
  void prepDraw(Renderer& r);     // the open screen, over the map
  void prepDrawSlots(Renderer& r);  // the numbers of the deployment squares, over the map (their tint is in the overlay: addPrepTint)
  void addPrepTint(Overlay& ov) const;
  mutable std::vector<uint8_t> prepMask_;  // addPrepTint's per-cell kind (Overlay::prep points here)
  void prepDrawPanelHead(Renderer& r);
  std::string prepHint() const;
  std::string prepTradeHint() const;
  void prepMainMenu();
  void prepMenuChosen(int id);
  void prepUnitsOpen(int sel = 0);
  void prepJobsOpen();  // battle_prep_jobs.cpp (docs/JOBS.md): the same Jobs screen as the camp's
  void prepJobsInput(const InputEvent& e);
  void prepJobsDraw(Renderer& r);
  void prepUnitsPicked();
  void prepPositionsButton(Button b);
  void prepViewButton(Button b);
  void prepLogSlots();
  void prepViewLook();            // the reach of the enemy under the cursor
  void prepBeginPressed();
  void prepAskBegin(int deployed, int wanted);
  void prepItemsWhoOpen(int sel = 0);
  void prepItemsWithOpen();
  void prepTradeOpen(int col = 0, int row = 0);
  void prepTradePicked(bool direct = false);  // direct: skip the Hand over / Equip menu
  void prepShopOpen(int sel = 0);
  void prepShopForOpen();
  // battle_unit_icon.cpp: the unit's map picture and "Lv2 Soldier" on a row of the prep lists
  void drawUnitIcon(Renderer& r, int type, int x, int y, int size);
  void decorateUnitRow(PanelRow& row, int type, int level);
  // The class and level a preparation list shows for the unit `id`: the unit in the battle when it is there (the test level and
  // the difficulty change it after the roster was read), else what the list has.
  void prepLive(const std::string& id, int& type, int& level) const;
  void updateShopMusic();  // battle_shop_music.cpp
  // battle_prep_sell.cpp: selling at the shop, and Equip on the trade screen
  void prepShopTab(int tab);                 // 0 = Buy, 1 = Sell (the shop's tabs)
  void prepSellWhoOpen(int sel = 0);
  void prepSellItemsOpen(int sel = 0);
  void prepSellPicked();
  void prepTradeChoiceOpen(int row);
  void prepTradeChoice(int id);
  bool prepTradeNeedsChoice(int row) const;  // the picked item is a weapon: the small menu (Hand over / Equip) opens first
  void prepSellInput(const InputEvent& e);
  void prepListInput(const InputEvent& e);
  void prepItemsInput(const InputEvent& e);
  void prepCellInput(const InputEvent& e);

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

  // --- battle_enemy_ranges.cpp: the player's enemy-range display (battle/enemy_ranges.h)
  int viewTeam() const;       // the team whose eyes the screen shows (the active human side's)
  bool rangesOn() const;      // the feature is available: not switched off by the game or the player, not in the preparation screen
  void refreshRanges();
  bool markEnemy(int unit);   // Confirm on a visible enemy: mark / unmark; false = not markable
  void toggleAllRanges();
  bool clearRanges();
  void addRangeTint(Overlay& ov) const;
  // --- turn_strip.cpp: the speed turn order's part of the screen (docs/TURN_ORDER.md "Player"); none of it does anything in another game
  void drawTurnStrip(Renderer& r) const;      // the next entries of the order along the top edge of the map
  void refreshChargeMarks();                  // the squares of charging units and of the squares they aim at (every frame in a speed game)
  void addChargeTint(Overlay& ov) const;
  int turnKey() const { return battle_.speedGame() ? battle_.turnNo : battle_.round * 1000 + battle_.activeSide; }  // changes when a (unit's) turn begins
  std::vector<uint8_t> chargeFromMask_, chargeAimMask_;
  int aimedChargeSpeed_ = 0;                  // a charged ability is being aimed: how fast it charges (the strip then shows where it would land)

  SceneHost& host_;
  GameSession& session_;
  Battle& battle_;  // lives in the session; undo assigns into it
  BattleView view_;
  FightScene fightScene_;
  // The forecast of each fight ordered, taken before the rules applied it (the Battle is already post-fight when the scene opens):
  // key = (attacker, defender); the scene shows hit and crit percentages from it.
  std::map<std::pair<int, int>, Forecast> forecasts_;
  void noteForecast(const Battle& before, int attacker, int defender, int weapon);
  int fightSerial_ = -1;  // the view's fight (BattleView::FightRun::serial) the scene was started for
  Layout layout_;
  Camera cam_;
  Cell hovered_ = kNoCell;  // the square the mouse last rested on (cursorUnderPointer)
  int camLogX_ = 0, camLogY_ = 0, camStill_ = -1;  // logCamera: where it was, frames still (-1 = already logged)
  EdgeScroll edge_;  // the mouse, for scrolling at the picture's edge
  MapProj proj_;  // where squares are on screen in the style in use
  std::unique_ptr<Map3D> map3d_;  // the drawer of the 3D kinds of view (only in those)
  FreeCam freeCam_;     // full 3D: the player's camera
  // A story scroll (docs/EVENTS.md "Scene presentation", focus "ms"): from where the camera was to where the focus wants it, over
  // the Focus entry's milliseconds (battle_scene.cpp followCamera). `scrollSerial_` follows BattleView::focusSerial.
  struct Scroll { bool active = false; float t = 0, dur = 0, fromX = 0, fromY = 0; };
  Scroll scroll_;
  int scrollSerial_ = 0;
  float sceneHold_ = -1;  // Cancel / Menu held while a scene plays (seconds), -1 = not held: half a second skips the scene
  int turn_ = 0;        // 3D isometric: turns of the camera asked for (quarters; sixths on hexagon maps)
  float turnPos_ = 0;   // where the camera is now (eases towards turn_)
  Overlay overlay_;
  EnemyRanges ranges_;  // marked enemies / "show all": a display aid, never saved
  Menu menu_;
  Mode mode_ = Mode::Idle;
  bool autoplay_ = false;
  int styleIdx_ = 0;  // visual style of the look in use (index into db.styles)
  float time_ = 0;
  bool fogSeenOn_ = false;  // what battle_.fogOn() was when watchFogSwitch last looked
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
  ItemPanel weaponPanel_;                 // Mode::WeaponPick
  std::vector<WeaponRow> weaponRows_;     // its rows
  std::vector<AttackOption> weaponOpts_;  // every attack option of the unit (the targets_ of a row are the ones of its weapon)
  int weaponPrefer_ = -1;                 // an enemy the player tapped on the map to start the attack (-1 = none)
  int weaponRow_ = -1;                    // the row picked; >= 0 while Targeting came from the list (Back returns there)
  Cell buildCell_ = kNoCell;
  int turnKey_ = -1;
  bool jumpCamera_ = false;  // a new unit's turn in a speed game: the camera arrives at once
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
  std::function<void()> staffBack_;       // the open menu is a staff's second stage ("Repair which?"): Cancel does this
  int staffItemUser_ = -1, staffItemSlot_ = -1, staffItemTarget_ = -1;  // what that menu is about
  int stealAbility_ = -1;                 // the open menu is "Steal which?" (staffItem* then hold the user and the foe)
  StaffForecast staffFc_;
  std::vector<PlaceOption> placeOpts_;    // the place entries of the action menu now open (ids kPlaceBase + n)
  std::vector<PlaceAttack> placeAtk_;
  int placeKeyStructure_ = -1;            // the open menu is "Which key?" for this structure
  bool placeFcOn_ = false;                // a door is being picked: placeFc_ is drawn as the forecast box
  PlaceForecast placeFc_;
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
