// Battle: the complete state of one battle plus the rules that change it.
//
// Design rules (keep these when extending):
//  * Battle is a plain copyable value. The UI copies it for undo, the AI
//    copies it for look-ahead, and a copy + the action list is a replay.
//  * State changes ONLY through apply(Action). Each apply() appends Events
//    describing what happened; the player animates those, it never diffs
//    state. Read access to the public fields is fine anywhere.
//  * All randomness comes from `rng`. Queries (forecast, reachable...) are
//    const and never consume randomness.
//  * Genre features (economy, production, capture, progression) are gated
//    by db->rules flags, so one engine runs Fire Emblem-like and Advance
//    Wars-like games.
#pragma once
#include <memory>

#include "core/base/rng.h"
#include "core/base/types.h"
#include "core/battle/story_state.h"
#include "core/data/database.h"
#include "core/data/events.h"
#include "core/grid/grid.h"

namespace ote {

class Formulas;

enum class EventType {
  TurnStarted,      // a=side, b=round
  UnitMoved,        // a=unit, data=path cells (start..dest)
  Attack,           // a=attacker, b=defender, c=weapon, d=damage, flags=kHit|kCrit|kKill|kCounter
  UnitDied,         // a=unit, b=cell it stood on
  XpGained,         // a=unit, b=amount
  LevelUp,          // a=unit, b=new level, data=gain per stat index
  CaptureProgress,  // a=unit, b=the unit's cell, c=points remaining, d=structure index
  StructureCaptured,  // a=structure index, b=new owner side, c=previous owner (-1 neutral)
  ResourceChanged,  // a=side, b=resource, c=delta, d=new total
  UnitBuilt,        // a=unit, b=cell
  SideDefeated,     // a=side
  BattleEnded,      // a=winning team (-1 = nobody)
  // Items (docs/ITEMS.md). "item" is an index into db->items.
  ItemUsed,         // a=unit, b=item, c=target unit
  UnitHealed,       // a=unit, b=amount
  StatsRaised,      // a=unit, data=gain per stat index
  ItemBroke,        // a=unit, b=item
  ItemGained,       // a=unit (-1 = the side's storage), b=item, c=side, flags=kDropped|kBought
  ItemLost,         // a=unit that could not take it, b=item
  ItemMoved,        // a=from unit (-1 = storage), b=to unit (-1 = storage, -2 = thrown away), c=item, d=side
  ItemEquipped,     // a=unit, b=item
  // Story events (docs/EVENTS.md). "text" is an index for Battle::storyText().
  Dialogue,         // a=speaker unit (-1 = none), b=text, c=free speaker name text (-1 = none), d=portrait id text (-1 = none)
  Focus,            // a=cell the view moves to, b=unit it follows (-1 = just the square)
  UnitAdded,        // a=unit, b=cell
  UnitRemoved,      // a=unit, b=cell it stood on (it is gone: not a defeat)
  UnitRelocated,    // a=unit, b=cell it left, c=cell it now stands on (moved by an event)
  SideChanged,      // a=unit, b=old side, c=new side
  ObjectiveChanged, // a=text of the new objective (-1 = none), b=objective set
};

enum : int { kHit = 1, kCrit = 2, kKill = 4, kCounter = 8, kDropped = 16, kBought = 32 };

struct Event {
  EventType type = EventType::TurnStarted;
  int a = -1, b = -1, c = -1, d = -1;
  int flags = 0;
  std::vector<int> data;
};

// One item somebody holds (docs/ITEMS.md).
struct ItemStack {
  int item = 0;       // index into db->items
  int uses = -1;      // left; -1 = unlimited
  bool drop = false;  // goes to whoever defeats the carrier
};

struct Unit {
  int id = -1;     // index into Battle::units, never reused
  int type = 0;    // index into db->unitTypes
  int side = 0;
  Cell cell = kNoCell;  // kNoCell once dead
  std::string name;
  std::string storyId;  // the map's / addUnit's "id" for events ("" = none); fixed once the unit exists
  int level = 1, xp = 0;
  std::vector<int> stats;  // current maximums, per stat index
  int hp = 0;
  bool moved = false;  // has moved this turn
  bool done = false;   // has finished acting this turn
  bool alive = true;
  bool leader = false;
  std::vector<ItemStack> items;  // the inventory, in order (at most rules.items.inventorySize)
};

struct CellState {
  int terrain = 0;
  int height = 0;
};

// A structure placed on the map (docs/STRUCTURES.md). Fixed for the battle:
// only its owner and capture progress change. Index = id, never reordered.
struct StructureState {
  int type = 0;       // index into db->structures
  int x = 0, y = 0;   // top-left square
  int owner = -1;     // side index, -1 neutral
  int capture = 0;    // capture points remaining
  int capturer = -1;  // unit currently capturing; progress resets if it leaves the structure
};

struct SideState {
  std::string id, name;
  uint32_t color = 0xffffff;
  int team = 0;
  bool ai = false;
  std::vector<int> resources;  // per resource index
  bool defeated = false;
  bool everHadUnits = false;
  bool hadHq = false;
  bool leaderLost = false;
  std::vector<ItemStack> storage;  // the side's shared storage (rules.items.storage)
};

enum class ActionType {
  Move, Attack, Capture, Wait, Build, EndTurn,
  // Items (docs/ITEMS.md). Only UseItem ends the unit's turn.
  UseItem, Equip, GiveItem, TakeItem, StoreItem, FetchItem, BuyItem, DiscardItem,
  // Story (docs/EVENTS.md): `unit` talks to `target`; free, ends the turn only if the event says so.
  Talk
};

struct Action {
  ActionType type = ActionType::EndTurn;
  int unit = -1;        // Move, Attack, Capture, Wait
  Cell cell = kNoCell;  // Move destination / Build location
  int target = -1;      // Attack: defender unit
  int weapon = -1;      // Attack: weapon index (db->weapons)
  int unitType = -1;    // Build
  int slot = -1;        // item actions: position in an inventory, or in the storage (FetchItem); Talk: the story event (-1 = the first that is offered)
  int item = -1;        // BuyItem: index into db->items

  static Action move(int unit, Cell dest) { Action a; a.type = ActionType::Move; a.unit = unit; a.cell = dest; return a; }
  static Action attack(int unit, int target, int weapon) { Action a; a.type = ActionType::Attack; a.unit = unit; a.target = target; a.weapon = weapon; return a; }
  static Action capture(int unit) { Action a; a.type = ActionType::Capture; a.unit = unit; return a; }
  static Action wait(int unit) { Action a; a.type = ActionType::Wait; a.unit = unit; return a; }
  static Action build(Cell cell, int unitType) { Action a; a.type = ActionType::Build; a.cell = cell; a.unitType = unitType; return a; }
  static Action endTurn() { return Action(); }
  static Action itemAction(ActionType t, int unit, int slot, int target = -1) {
    Action a; a.type = t; a.unit = unit; a.slot = slot; a.target = target; return a;
  }
  static Action useItem(int unit, int slot, int target) { return itemAction(ActionType::UseItem, unit, slot, target); }
  static Action equip(int unit, int slot) { return itemAction(ActionType::Equip, unit, slot); }
  static Action giveItem(int unit, int slot, int to) { return itemAction(ActionType::GiveItem, unit, slot, to); }
  // `slot` is the position in the OTHER unit's inventory.
  static Action takeItem(int unit, int from, int slot) { return itemAction(ActionType::TakeItem, unit, slot, from); }
  static Action storeItem(int unit, int slot) { return itemAction(ActionType::StoreItem, unit, slot); }
  static Action fetchItem(int unit, int index) { return itemAction(ActionType::FetchItem, unit, index); }
  static Action discardItem(int unit, int slot) { return itemAction(ActionType::DiscardItem, unit, slot); }
  static Action talk(int unit, int target, int event = -1) { Action a; a.type = ActionType::Talk; a.unit = unit; a.target = target; a.slot = event; return a; }
  static Action buyItem(int unit, int item) { Action a; a.type = ActionType::BuyItem; a.unit = unit; a.item = item; return a; }
};

// Result of a movement search for one unit.
struct Reach {
  std::vector<int> cost;        // per cell; -1 = unreachable
  std::vector<Cell> parent;     // per cell; previous cell on the cheapest path
  std::vector<uint8_t> stop;    // per cell; 1 = unit may end its move here
  std::vector<Cell> stops;      // all cells with stop==1 (includes own cell)
  bool canStop(Cell c) const { return c >= 0 && c < (Cell)stop.size() && stop[c]; }
};

struct AttackOption {
  int target = -1;
  int weapon = -1;
};

// What one side of a combat would do, before randomness.
struct StrikeForecast {
  bool can = false;  // false = this side does not attack at all
  int weapon = -1;
  int damage = 0;    // per strike, if it hits (not capped to target hp)
  int hit = 100;     // percent
  int crit = 0;      // percent
  int strikes = 0;
};
struct Forecast {
  StrikeForecast attack, counter;
};

class Battle {
 public:
  // ---- state (read freely; write only via apply) ----
  const Database* db = nullptr;
  Formulas* formulas = nullptr;  // shared Lua host; must outlive the battle
  Grid grid;
  std::vector<CellState> cells;
  std::vector<StructureState> structures;
  std::vector<int> structureAt;  // per cell: index into `structures` or -1
  std::vector<int> occupant;  // per cell: unit id or -1
  std::vector<Unit> units;
  std::vector<SideState> sides;
  int teamCount = 0;
  int round = 1;
  int activeSide = 0;
  bool over = false;
  int winnerTeam = -1;
  Rng rng;
  // The map's events and objectives (null = none) and how far they have got. Copies share the data.
  std::shared_ptr<const StoryData> story;
  StoryState storyState;

  // ---- setup ----
  bool init(const Database& database, const MapDef& map, Formulas& formulaHost,
            uint32_t seed, std::string& err);
  // Begins the first side's turn (emits TurnStarted, income). Call once.
  void start(std::vector<Event>& events);

  // ---- queries ----
  const TerrainDef& terrainAt(Cell c) const { return db->terrains[cells[c].terrain]; }
  // Structure queries. Rules code never combines terrain and structure by
  // hand: it asks these (they sit on enterCostOf / squareNumbers in database.h).
  int structureIndexAt(Cell c) const { return grid.valid(c) ? structureAt[c] : -1; }
  const StructureDef* structureDefAt(Cell c) const {
    int s = structureIndexAt(c);
    return s < 0 ? nullptr : &db->structures[structures[s].type];
  }
  // Type (index into db->structures) of the structure on the square, -1 if none.
  int structureTypeAt(Cell c) const { return structureAt[c] < 0 ? -1 : structures[structureAt[c]].type; }
  const StructureDef& defOf(const StructureState& s) const { return db->structures[s.type]; }
  // Cost for a move type to enter the square (structure's cost if it sets one,
  // else the terrain's). <= 0 = cannot enter.
  int enterCost(Cell c, int moveType) const { return enterCostOf(*db, cells[c].terrain, structureTypeAt(c), moveType); }
  // What formulas see as `ta` / `tb` for the square: terrain numbers with the
  // structure's numbers replacing/adding. May return `scratch`.
  const Props& numbersAt(Cell c, Props& scratch) const {
    return squareNumbers(*db, cells[c].terrain, structureTypeAt(c), scratch);
  }
  // Every square covered by structure `s`, row by row.
  void structureCells(int s, std::vector<Cell>& out) const;
  const UnitTypeDef& typeOf(const Unit& u) const { return db->unitTypes[u.type]; }
  int unitAt(Cell c) const { return grid.valid(c) ? occupant[c] : -1; }
  int teamOf(const Unit& u) const { return sides[u.side].team; }
  bool isEnemy(const Unit& a, const Unit& b) const { return teamOf(a) != teamOf(b); }
  // A unit's stat as the rules see it: its own value plus what its accessories add.
  int stat(const Unit& u, int index) const;
  int maxHp(const Unit& u) const { return stat(u, db->rules.statHp); }
  int movePoints(const Unit& u) const { return stat(u, db->rules.statMove); }
  // Alive, belongs to the active side, battle not over.
  bool canAct(int unit) const;

  // Where the unit could move right now (ignores its `moved` flag).
  void reachable(int unit, Reach& out) const;
  bool pathTo(const Reach& reach, Cell dest, std::vector<Cell>& path) const;

  // Attacks available if the unit stood on `from` (pass its own cell for
  // "right now"; any other cell counts as having moved).
  void attackOptions(int unit, Cell from, std::vector<AttackOption>& out) const;
  // Expected exchange. Returns false if the attack is not possible.
  bool forecast(int attacker, int defender, int weapon, Cell from, Forecast& out) const;

  // Would a Capture action be valid if the unit stood on `at`? (`at` is any
  // square of a capturable structure that the unit's team does not own.)
  bool canCaptureAt(int unit, Cell at) const;
  // Unit types the active side could build on this cell (affordable or not):
  // the cell must be a free square of a structure the side owns, and the new
  // unit must be able to enter that square.
  void buildOptions(Cell cell, std::vector<int>& unitTypes) const;
  bool canAfford(int side, int unitType) const;

  // ---- items (items.cpp; docs/ITEMS.md) ----
  const ItemDef& itemOf(const ItemStack& s) const { return db->items[(size_t)s.item]; }
  // The weapons the unit can attack with right now, in order of preference:
  // its type's (fixed weapons) or the wieldable weapon items it carries, each once.
  void weaponsOf(const Unit& u, std::vector<int>& out) const;
  // Carried weapons: the inventory slot of the first item that is this weapon (-1 = none),
  // and of the equipped weapon (the first wieldable one; -1 = unarmed).
  int weaponSlot(const Unit& u, int weapon) const;
  int equippedSlot(const Unit& u) const;
  bool hasRoom(const Unit& u) const { return (int)u.items.size() < db->rules.items.inventorySize; }
  // May this unit use its side's shared storage where it stands?
  bool storageOpen(const Unit& u) const;
  // Items the unit could buy where it stands (affordable or not); empty = no shop here.
  void shopItems(int unit, std::vector<int>& items) const;
  bool canPay(int side, const std::vector<Amount>& price) const;
  // Units a usable item in `slot` could be used on right now (the unit itself or neighbouring allies).
  void useTargets(int unit, int slot, std::vector<int>& targets) const;
  // Neighbouring living units of the same side (trade partners).
  void tradePartners(int unit, std::vector<int>& out) const;

  // ---- story (story*.cpp, objectives.cpp; docs/EVENTS.md) ----
  // Wording of dialogue, names, labels and objectives is kept here; events carry indices into it.
  const std::string& storyText(int index) const;
  // The unit with this story id (the newest if an id was reused), -1 if there is none; dead ones count.
  int unitByStoryId(const std::string& id) const;
  bool unitMatches(const UnitFilter& filter, const Unit& u) const;
  // The Talk menu for `unit` standing on `from`: events whose two named units are alive and next to
  // each other, whose conditions hold and which have not run (or repeat). Empty unless the unit can act.
  void talkOptions(int unit, Cell from, std::vector<TalkOption>& out) const;
  // The team objectives are judged for: the first human side's, else the first side's.
  int humanTeam() const;
  // What the player should show as the current objective ("" = none).
  const std::string& objectiveText() const;
  // Hands in starting switches / variables (a campaign carries them); sizes must match the game's
  // lists. Call after init(), before start(). False (nothing changed) on a size mismatch.
  bool setStartingStory(const std::vector<uint8_t>& switches, const std::vector<int>& variables);

  // ---- actions ----
  // why (optional) receives a short human-readable reason on failure.
  bool validate(const Action& action, std::string* why = nullptr) const;
  // Validates, then applies and appends events. Returns false (state
  // untouched) if invalid.
  bool apply(const Action& action, std::vector<Event>& events, std::string* why = nullptr);

 private:
  // combat.cpp
  bool versusValue(int weapon, const Unit& target, double& vs) const;
  bool strikeForecast(const Unit& atk, Cell atkCell, const Unit& def, Cell defCell,
                      int weapon, StrikeForecast& out) const;
  int counterWeapon(const Unit& def, Cell defCell, const Unit& atk, Cell atkCell) const;
  bool weaponInRange(int weapon, int dist) const;
  void doStrike(int atk, int def, int weapon, int baseFlags, std::vector<Event>& events);
  void applyAttack(const Action& action, std::vector<Event>& events);
  void killUnit(int unit, std::vector<Event>& events);
  // After a unit fell (killUnit already ran): story events waiting for it. killer -1 = nobody.
  void unitDefeated(int unit, int killer, std::vector<Event>& events);
  // turn.cpp
  void beginTurn(std::vector<Event>& events);
  void advanceTurn(std::vector<Event>& events);
  void checkDefeat(std::vector<Event>& events);
  void changeResource(int side, int resource, int delta, std::vector<Event>& events);
  // actions.cpp
  void applyMove(const Action& action, std::vector<Event>& events);
  void applyCapture(const Action& action, std::vector<Event>& events);
  void applyBuild(const Action& action, std::vector<Event>& events);
  // Resets the capture progress of `unit` unless it now stands (`newCell`) on
  // a square of the structure it is capturing.
  void releaseCapture(int unit, Cell newCell);
  bool ownsHq(int side) const;
  // items.cpp
  bool validateItemAction(const Action& action, std::string* why) const;
  void applyItemAction(const Action& action, std::vector<Event>& events);
  void spendUse(int unit, int slot, std::vector<Event>& events);
  // Hands an item to a unit; without room it goes to the side's storage, else it is lost.
  void giveTo(int unit, ItemStack stack, int flags, std::vector<Event>& events);
  void clampHp(Unit& u);
  // battle_init.cpp: a unit as a map (or addUnit) describes it, not yet placed.
  Unit makeUnit(const MapUnitDef& def) const;
  // story.cpp
  bool conditionHolds(const Condition& c) const;
  bool conditionsHold(const std::vector<Condition>& list) const;
  bool triggerMatches(const Trigger& t, const StoryOccurrence& o) const;
  void fireStory(const StoryOccurrence& o, std::vector<Event>& events);
  void drainStory(std::vector<Event>& events);
  void applyTalk(const Action& action, std::vector<Event>& events);
  // story_commands.cpp
  void runCommands(const std::vector<Command>& list, std::vector<Event>& events);
  void runCommand(const Command& c, std::vector<Event>& events);
  void storyAddUnit(const MapUnitDef& def, std::vector<Event>& events);
  void storyRelocate(int unit, Cell to, std::vector<Event>& events);
  Cell nearestFreeCell(Cell target, int moveType) const;
  // objectives.cpp
  bool objectiveHolds(const ObjectiveRule& r, bool win) const;
  // Judges the current objectives for the human team and ends the battle if one holds.
  void checkObjectives(std::vector<Event>& events);
  // The team that wins when `team` loses: the only other team still in the battle, else -1.
  int winnerAgainst(int team) const;
  // Decides the battle: BattleEnded, then the events waiting for it. No-op once over.
  void endBattle(int winnerTeam, std::vector<Event>& events);
  // Everything that can end a battle after a change: defeat rules, then objectives.
  void settle(std::vector<Event>& events);
  // progression.cpp
  void awardXp(int unit, int amount, std::vector<Event>& events);

  StoryRun storyRun_;  // transient: empty between actions
};

}  // namespace ote
