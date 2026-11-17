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
//    by db->rules flags, so one engine runs strategy-RPG-like and Advance
//    Wars-like games.
#pragma once
#include <memory>

#include "core/base/rng.h"
#include "core/base/types.h"
#include "core/battle/carried.h"
#include "core/battle/mapchange_state.h"
#include "core/battle/places_state.h"
#include "core/battle/quotes_state.h"
#include "core/battle/srpg_state.h"
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
  // Upgradable structures and research (docs/ECONOMY.md); the payments come first as ResourceChanged.
  StructureUpgraded,  // a=structure index, b=previous kind, c=new kind (indices into db->structures)
  ResearchStarted,    // a=side, b=technology (index into db->research), c=turns it takes (0 = finished at once)
  ResearchFinished,   // a=side, b=technology (also from a story event's grantResearch)
  // Repair and supply (docs/ECONOMY.md); a repair's payment comes first as ResourceChanged.
  UnitRepaired,       // a=unit, b=HP restored, c=its HP now, data=what it cost per resource index (empty = free)
  UnitResupplied,     // a=unit (fuel and/or ammunition filled up)
  // Fog of war (docs/ECONOMY.md): a move that ran into an enemy nobody on its team saw. It comes right after the
  // UnitMoved event of the squares actually walked (none when the unit never left its square).
  MoveBlocked,        // a=unit, b=the unit in the way, c=square the unit stopped on, d=square the enemy stands on
  // Sound (docs/SOUND.md), from the story commands playMusic / stopMusic / playSound. Presentation only; the ids are
  // opaque text (Battle::storyText), only the player knows what they name.
  MusicChanged,       // a=text index of the track id (-1 = the story stopped the music), b=fade in milliseconds
  SoundRequested,     // a=text index of the sound id
  // Strategy-RPG features (docs/SRPG.md "Events"). "skill" / "ability" are indices into db->skills / db->abilities.
  SkillLearned,       // a=unit, b=skill
  SkillActivated,     // a=unit, b=skill, c=the foe or -1
  UnitHurt,           // a=unit, b=HP lost, c=the unit whose skill did it
  UnitPromoted,       // a=unit, b=old type, c=new type, d=new level, data=change per stat index
  AbilityUsed,        // a=unit, b=ability, c=target unit or -1, d=target square or -1, flags=kBlocked (then data[0]=the unit in the way)
  UnitRescued,        // a=the unit that stays, b=the unit that leaves the board, c=the square it left, flags=kPaired
  UnitDropped,        // a=carrier, b=the unit, c=its square, flags=kForced|kBlocked
  UnitHanded,         // a=from, b=to, c=the unit handed over
  PairSwitched,       // a=the unit that went behind, b=the unit now in front, c=the square
  SupportPoints,      // a=unit of the pair's `a`, b=unit of `b`, c=pair, d=points now, flags=kReady
  SupportReached,     // a=unit of `a` (-1 = off the board), b=unit of `b`, c=pair, d=new level
  // Weapon ranks (docs/SRPG.md section 7): a=unit, b=weapon group (index into db->ranks.groups), c=new rank, d=old rank
  // (indices into db->ranks.ladder).
  RankUp,
  // Staves (docs/SRPG.md section 8): a=user, b=item, c=the picked unit or -1, d=units really affected / a=the unit missed, b=user, c=item.
  StaffUsed,
  StaffMissed,
  // Staves, stage 2 (docs/SRPG.md section 8; a moved unit is UnitRelocated): a=unit, b=item (index into db->items), c=uses restored,
  // d=uses it has now / a=user, b=the square lit, c=radius, d=turns.
  ItemRepaired,
  AreaLit,
  // Conditions (docs/SRPG.md section 9). "condition" is an index into db->conditions.
  // a=unit, b=condition, c=own turns it lasts (-1 = until cured / the map ends), d=the unit that did it (-1 = none), flags=kRefreshed
  ConditionApplied,
  // a=unit, b=condition, c=why: kConditionRanOut, kConditionWoreOff (early), kConditionCured, kConditionReplaced, kConditionStruck
  ConditionEnded,
  // a=unit that is immune, b=condition, c=the unit that tried
  ConditionResisted,
  // a=unit, b=condition, c=what: kTickActsAlone (berserk, its own action follows), kTickCannotAct (its turn is skipped)
  ConditionTick,
  // Doors, chests and visiting (docs/SRPG.md section 10). "structure" = an index into Battle::structures; "how" is one of kOpen*.
  DoorOpened,       // a=structure, b=the unit that did it (-1 = a story command), c=how, d=the kind it was (index into db->structures)
  DoorClosed,       // a=structure, b=the kind it was, c=the kind it is now, flags=kLocked when it was locked as well
  ChestOpened,      // a=structure, b=unit (-1 = story), c=how, d=the kind it was; the contents follow as ItemGained / ResourceChanged (flags kLooted)
  PlaceHit,         // a=unit, b=structure, c=weapon, d=damage, flags=kKill when the door broke (then DoorOpened follows)
  PlaceVisited,     // a=structure, b=unit, c=the kind it was
  PlaceDestroyed,   // a=structure, b=unit, c=the kind it was
  PlaceUnlocked,    // a=structure, b=unit (-1 = story): a story command took the lock off, the door stays shut
  // Skill and ability blocks, second set (docs/SRPG.md): a=thief, b=the unit robbed, c=item, d=uses left (-1 = unlimited) /
  // a=the unit that refreshed, b=the unit that may act again, c=ability.
  ItemStolen,
  UnitRefreshed,
  // Changing the map (docs/EVENTS.md "Changing the map"): a=number of squares, b=the terrain they all got (-1 = not all the same), data=the squares.
  TerrainChanged,
  // Scene presentation (docs/EVENTS.md "Scene presentation"), from the background / fade / wait / shake / leave / weather
  // commands: a=SceneFx::Kind, b=text index (picture id, speaker key or weather; -1 none), c=ms (-1 not given), d=Fade colour
  // 0xRRGGBB / Shake strength, flags=kSceneOut / kSceneFadeTrans / kSceneAuto (scene_fx.h). Presentation only.
  SceneEffect,
  // Dialogue choices (docs/EVENTS.md "Dialogue choices"). Kept last: the numbers of the entries above are part of the golden fight hashes.
  Choice,           // a=speaker unit (-1 none), b=question text, c=free speaker name text (-1 none), d=portrait id text (-1 none), data=the answers' texts (docs/EVENTS.md "Dialogue choices")
};

enum : int {
  kHit = 1, kCrit = 2, kKill = 4, kCounter = 8, kDropped = 16, kBought = 32,
  // docs/SRPG.md: Attack may carry kSurvived; the others belong to the new entries above.
  kSurvived = 64, kBlocked = 128, kPaired = 256, kForced = 512, kReady = 1024,
  kRefreshed = 2048,  // ConditionApplied: the unit had it already, its duration starts over
  kLooted = 4096,     // ItemGained / ResourceChanged: it came out of a chest (docs/SRPG.md section 10)
  kLocked = 8192,     // DoorClosed: the story locked it as well
  kMapWeapon = 16384, // Attack: the attacker used the weapon of the structure it stands on (docs/SRPG.md "Map weapons")
  kMapWeaponSpent = 32768  // Attack with kMapWeapon: that was the structure weapon's last use
};
// How a door or chest was opened (the `c` of DoorOpened / ChestOpened).
enum : int { kOpenKey = 0, kOpenTrait = 1, kOpenBreak = 2, kOpenStory = 3, kOpenStaff = 4, kOpenFree = 5 /* it was not locked */ };
enum : int { kConditionRanOut = 0, kConditionWoreOff = 1, kConditionCured = 2, kConditionReplaced = 3, kConditionStruck = 4 };
enum : int { kTickActsAlone = 1, kTickCannotAct = 2 };

// Bit `t` of Event::seen = team `t` saw the event (docs/ECONOMY.md "Fog of war"). Without fog every event has all bits.
const uint32_t kSeenByAll = 0xffffffffu;

struct Event {
  EventType type = EventType::TurnStarted;
  int a = -1, b = -1, c = -1, d = -1;
  int flags = 0;
  std::vector<int> data;
  // Which teams saw it (set by Battle::apply / start: what "saw" means per type is in event_visibility.cpp).
  // A consumer that shows the battle to one team skips (or hides) events whose bit for that team is clear.
  uint32_t seen = kSeenByAll;
  // UnitMoved with fog only: per team (index = team) two entries, the first and the last index into `data` of a square
  // where that team saw the mover, or -1, -1 when it never did. Empty without fog.
  std::vector<int> seenSteps;
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
  // Repair and supply (docs/ECONOMY.md). fuel: -1 = the type uses none. ammo: empty = no weapon of the type
  // has ammunition (or weapons are carried); otherwise one entry per entry of the type's `weapons`, -1 = that
  // weapon is unlimited. Filled by Battle::stockUnit.
  int fuel = -1;
  std::vector<int> ammo;
  // ---- strategy-RPG features (docs/SRPG.md); all at these defaults in a game that uses none ----
  int character = -1;            // index into db->characters, derived from storyId (dressUnit); never saved
  std::vector<int> learned;      // skills learned for good, oldest first (indices into db->skills)
  std::vector<AbilityUse> abilityUses;  // one entry per ability it has used this battle
  // A summoned unit: who summoned it (unit id), with which ability, and the turns of lifetime left (-1 = no limit).
  int summoner = -1;
  int summonAbility = -1;
  int summonLeft = -1;
  // Rescue / pair up: `carrying` on the unit that stays on the board, `carriedBy` on the one that left it. A
  // carried unit has alive == false and no cell but KEEPS its hp (a defeated unit has 0): see Battle::exists.
  int carrying = -1;
  int carriedBy = -1;
  // Turn flags of "move again" / "act again", cleared with `moved` and `done` at the start of the side's turn.
  int moveSpent = 0;       // movement points spent on moves this turn
  bool acted = false;      // has used its action; only a move (or Wait) is left
  bool refreshed = false;  // "act again" has been used this turn
  // Weapon ranks (docs/SRPG.md section 7): weapon experience per RankRules::groups index; empty in a game without ranks.
  std::vector<int> weaponXp;
  // Conditions (docs/SRPG.md section 9), in the order they were applied; empty in a game without conditions.
  std::vector<ConditionState> conditions;
  // Computer opponent (docs/SRPG.md section 10): what it goes for (kAiChests | kAiPlaces | kAiDoors | kAiSteal); 0 = ignores them. Saved when not 0.
  int aiGoals = 0;
  // Computer opponent (docs/AI.md): how it moves, and whether a guard has woken up. Saved only when not the default.
  AiBehaviour behaviour;
  bool awake = false;
  // Refresh (docs/SRPG.md "Refresh an ally"): the units that refreshed this one in the present turn of its side, cleared when that turn
  // starts. Its size is the number of refreshes it has had this turn. Saved when not empty.
  std::vector<int> refreshedBy;
  // Taken off the map by a story `removeUnit` with "hide": true (docs/EVENTS.md): alive == false and no square, like a removed unit, but it
  // keeps its hit points and everything else and a `showUnit` / `addUnit` brings it back. Saved only when true.
  bool hidden = false;
};

struct CellState {
  int terrain = 0;
  int height = 0;
};

// A structure placed on the map (docs/STRUCTURES.md). Its position and size are fixed for the
// battle; its owner, capture progress and (through upgrades) its kind change. Index = id, never reordered.
struct StructureState {
  int type = 0;       // index into db->structures; the CURRENT kind (an upgrade changes it)
  int mapType = 0;    // the kind the map placed (saves store `type` only when it differs)
  int x = 0, y = 0;   // top-left square
  int owner = -1;     // side index, -1 neutral
  int capture = 0;    // capture points remaining
  int capturer = -1;  // unit currently capturing; progress resets if it leaves the structure
  // Map weapons (docs/SRPG.md "Map weapons"): the uses left, valid while `weaponFor` is the current kind; see Battle::mapWeaponUses.
  int weaponUses = -1;
  int weaponFor = -1;
  // Doors, chests and visitable places (docs/SRPG.md section 10); at these defaults for any other structure.
  std::string id;               // the map's story id for it ("" = none); fixed, never saved
  int hp = -1;                  // a door that can be attacked: hit points left; -1 = none
  bool locked = false;          // a door / chest: shut and needs its key (or trait, or story)
  bool used = false;            // a place that was visited (and cannot be again)
  std::vector<PlaceLoot> loot;  // a chest's contents; empty once opened
  std::vector<int> visitSides;  // who may visit (see Battle::visitAllowed); empty = the rule of the owner
  std::vector<std::string> visitUnits;
};

struct SideState {
  std::string id, name;
  uint32_t color = 0xffffff;
  int team = 0;
  bool ai = false;
  bool seesAll = false;        // fog: a computer side that sees everything (map "aiSeesAll"); fixed at the start, not saved
  std::vector<int> resources;  // per resource index
  bool defeated = false;
  bool everHadUnits = false;
  bool hadHq = false;
  bool leaderLost = false;
  std::vector<ItemStack> storage;  // the side's shared storage (rules.items.storage)
  // Research (docs/ECONOMY.md): per db->research index, 1 = finished; at most one in progress.
  std::vector<uint8_t> researched;
  int researching = -1;  // technology under way (index into db->research), -1 = none
  int researchLeft = 0;  // of the side's own turns still to start before it is finished
  bool difficultyShift = true;  // docs/DIFFICULTY.md: false = the enemy numbers of the difficulty never touch this side (map side key)
};

enum class ActionType {
  Move, Attack, Capture, Wait, Build, EndTurn,
  // Items (docs/ITEMS.md). Only UseItem ends the unit's turn.
  UseItem, Equip, GiveItem, TakeItem, StoreItem, FetchItem, BuyItem, DiscardItem,
  // Story (docs/EVENTS.md): `unit` talks to `target`; free, ends the turn only if the event says so.
  Talk,
  // Upgradable structures and research (docs/ECONOMY.md). Free of any unit, like Build.
  UpgradeStructure,  // structure -> kind `to`
  StartResearch,     // technology `tech` for the active side
  // Repair and supply: a supplier unit (UnitTypeDef::supplies) refills the adjacent units of its side; ends its turn.
  Supply,
  // Strategy-RPG features (docs/SRPG.md). The target unit is in `target`, a square in `cell`.
  Promote,     // `unit` takes an offered promotion to `unitType` (-1 = the first offered); ends its turn
  Ability,     // `unit` uses menu ability `ability` on `target` / `cell`
  Rescue,      // carry: `unit` takes the neighbour `target` off the board; ends its turn
  Drop,        // carry / pair up: `unit` puts the unit it has on `cell`; ends its turn
  GiveUnit,    // carry: `unit` hands the unit it carries to `target`; free
  TakeUnit,    // carry: `unit` takes over the unit `target` carries; free
  PairUp,      // pair up: `unit` leaves the board and stands behind `target`; ends its turn
  SwitchPair,  // pair up: the unit behind `unit` comes forward; free
  Support,     // `unit` has its ready support conversation with `target`; free
  UseStaff,    // `unit` uses the staff in inventory `slot` on `target` (docs/SRPG.md section 8); ends its turn
  // Doors, chests and visiting (docs/SRPG.md section 10). `structure` is the index in Battle::structures. They end the unit's turn
  // (like Attack) unless the game's "rules.places" says the actions are automatic.
  OpenPlace,   // `unit` opens a door or chest; `slot` = the inventory slot of the key (-1 = it needs none: not locked)
  VisitPlace,  // `unit` visits the place, or destroys it when it may not visit
  BreakPlace   // `unit` attacks the door with weapon `weapon` (index into db->weapons)
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
  int structure = -1;   // UpgradeStructure: index into Battle::structures
  int to = -1;          // UpgradeStructure: the kind it becomes (index into db->structures)
  int tech = -1;        // StartResearch: index into db->research
  int ability = -1;     // Ability: index into db->abilities
  int itemSlot = -1;    // UseStaff "repair": the slot in the TARGET's inventory of the item to repair (-1 = none)

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
  static Action upgrade(int structure, int to) { Action a; a.type = ActionType::UpgradeStructure; a.structure = structure; a.to = to; return a; }
  static Action research(int tech) { Action a; a.type = ActionType::StartResearch; a.tech = tech; return a; }
  static Action supply(int unit) { Action a; a.type = ActionType::Supply; a.unit = unit; return a; }
  static Action buyItem(int unit, int item) { Action a; a.type = ActionType::BuyItem; a.unit = unit; a.item = item; return a; }
  // Strategy-RPG features (docs/SRPG.md).
  static Action unitAction(ActionType t, int unit, int target = -1, Cell cell = kNoCell) {
    Action a; a.type = t; a.unit = unit; a.target = target; a.cell = cell; return a;
  }
  static Action promote(int unit, int to = -1) { Action a = unitAction(ActionType::Promote, unit); a.unitType = to; return a; }
  // Named useAbility, not ability as docs/SRPG.md writes it: a member and a function cannot share the name `ability`.
  static Action useAbility(int unit, int ability, int target, Cell cell = kNoCell) {
    Action a = unitAction(ActionType::Ability, unit, target, cell); a.ability = ability; return a;
  }
  static Action rescue(int unit, int target) { return unitAction(ActionType::Rescue, unit, target); }
  static Action drop(int unit, Cell cell) { return unitAction(ActionType::Drop, unit, -1, cell); }
  static Action giveUnit(int unit, int target) { return unitAction(ActionType::GiveUnit, unit, target); }
  static Action takeUnit(int unit, int target) { return unitAction(ActionType::TakeUnit, unit, target); }
  static Action pairUp(int unit, int target) { return unitAction(ActionType::PairUp, unit, target); }
  static Action switchPair(int unit) { return unitAction(ActionType::SwitchPair, unit); }
  static Action support(int unit, int target) { return unitAction(ActionType::Support, unit, target); }
  // `target` is the picked unit; a staff that picks nobody (area "all", target "self") takes -1. A warp's destination goes in
  // `cell`, a light's square too (target -1), and a repair's item in `itemSlot` (docs/SRPG.md section 8, stage 2).
  static Action useStaff(int unit, int slot, int target, Cell cell = kNoCell, int itemSlot = -1) {
    Action a = unitAction(ActionType::UseStaff, unit, target, cell); a.slot = slot; a.itemSlot = itemSlot; return a;
  }
  // Doors, chests, visiting (docs/SRPG.md section 10).
  static Action openPlace(int unit, int structure, int keySlot = -1) {
    Action a = unitAction(ActionType::OpenPlace, unit); a.structure = structure; a.slot = keySlot; return a;
  }
  static Action visitPlace(int unit, int structure) { Action a = unitAction(ActionType::VisitPlace, unit); a.structure = structure; return a; }
  static Action breakPlace(int unit, int structure, int weapon) {
    Action a = unitAction(ActionType::BreakPlace, unit); a.structure = structure; a.weapon = weapon; return a;
  }
};

// Result of a movement search for one unit.
struct Reach {
  std::vector<int> cost;        // per cell; -1 = unreachable
  std::vector<Cell> parent;     // per cell; previous cell on the cheapest path
  std::vector<uint8_t> stop;    // per cell; 1 = unit may end its move here
  std::vector<Cell> stops;      // all cells with stop==1 (includes own cell)
  bool canStop(Cell c) const { return c >= 0 && c < (Cell)stop.size() && stop[c]; }
};

// One entry of an upgrade or research menu: possible now, or the plain reason it is not.
struct UpgradeOption {
  int to = -1;            // the kind it would become
  const UpgradeDef* def = nullptr;  // its cost and requirement (points into the Database)
  bool ok = false;
  std::string why;        // when !ok
};
struct ResearchOption {
  int tech = -1;
  bool ok = false;
  std::string why;        // when !ok: "needs Masonry", "not enough funds", ...
};

struct AttackOption {
  int target = -1;
  int weapon = -1;
  bool mapWeapon = false;  // the weapon is the one of the structure the unit would stand on (docs/SRPG.md "Map weapons")
};

// One strike's numbers where they differ from strike to strike (StrikeForecast::each).
struct StrikeNumbers {
  int damage = 0, hit = 100, crit = 0;
};

// What one side of a combat would do, before randomness.
struct StrikeForecast {
  bool can = false;  // false = this side does not attack at all
  int weapon = -1;
  int damage = 0;    // per strike, if it hits (not capped to target hp)
  int hit = 100;     // percent
  int crit = 0;      // percent
  int strikes = 0;
  int multi = 1;     // strikes in a row per strike (docs/SRPG.md "multiStrike"); 1 = none
  // The numbers of each strike (`strikes * multi` entries, in the order they are made) when they are not all the same: only a game
  // whose skills ask which strike it is (docs/SRPG.md "One strike of a fight"). Empty = every strike has the numbers above, which
  // are always those of the first strike.
  std::vector<StrikeNumbers> each;
  // Every strike's damage added up, the strikes in a row (`multi`) included (the player's "HP after").
  int allDamage() const {
    if (each.empty()) return damage * strikes * multi;
    int sum = 0;
    for (const StrikeNumbers& n : each) sum += n.damage;
    return sum;
  }
  // The same without the strikes in a row, as the computer opponent has always counted it (damage * strikes).
  int totalDamage() const {
    if (each.empty()) return damage * strikes;
    int sum = 0;
    for (const StrikeNumbers& n : each) sum += n.damage;
    return sum;
  }
};
struct Forecast {
  StrikeForecast attack, counter;
  bool defenderFirst = false;       // the defender strikes before the attacker in each exchange
  std::vector<ForecastNote> notes;  // activations that could fire, in roll order (not folded into the numbers)
};

// What a repair would do to one unit (repairPreview): HP restored and the price per resource index.
struct RepairPlan {
  int heal = 0;
  std::vector<int> cost;  // per resource index; all 0 = free
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
  // Difficulty (docs/DIFFICULTY.md): the difficulty this battle was built with (index into db->difficulties.list, -1 =
  // the game has none) and the numbers resolved for it (the game's, with the map's override applied). setDifficulty
  // before init() is kept by init; after init() it changes only what is built from then on.
  int difficulty = -1;
  DifficultyEnemy diffEnemy;
  int diffXpPercent = 100, diffResourcePercent = 100;
  std::shared_ptr<const std::vector<MapDifficulty>> diffMap;  // the map's own numbers (shared by copies)
  // Fog of war (docs/ECONOMY.md): true from the map's "fog". The queries below answer "everything visible" without it.
  bool fog = false;
  // The map's events and objectives (null = none) and how far they have got. Copies share the data.
  std::shared_ptr<const StoryData> story;
  StoryState storyState;
  // A campaign's roster as it was when the chapter began (null = none): named units of human sides
  // take its version of themselves (setStartingRoster). Copies share it; it is not saved.
  std::shared_ptr<const Roster> roster;
  // The rosters of the campaign's other (parked) parties, for the `party: any` of the roster conditions (docs/EVENTS.md); null = none. Not saved.
  std::shared_ptr<const std::vector<Roster>> otherRosters;
  // Closeness of the support pairs (docs/SRPG.md "Supports"); empty in a game without pairs.
  SupportState supports;

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
  // The same as `team` believes it to be: a structure it does not see counts as the kind it last saw (an upgrade done
  // out of sight must not show in what a viewer reads off a square). Without fog identical to numbersAt.
  const Props& numbersAtSeen(int team, Cell c, Props& scratch) const {
    const int si = structureIndexAt(c);
    return squareNumbers(*db, cells[c].terrain, si < 0 ? -1 : seenKind(team, si), scratch);
  }
  // Every square covered by structure `s`, row by row.
  void structureCells(int s, std::vector<Cell>& out) const;
  const UnitTypeDef& typeOf(const Unit& u) const { return db->unitTypes[u.type]; }
  int unitAt(Cell c) const { return grid.valid(c) ? occupant[c] : -1; }
  int teamOf(const Unit& u) const { return sides[u.side].team; }
  bool isEnemy(const Unit& a, const Unit& b) const { return teamOf(a) != teamOf(b); }
  // A unit's stat as the rules see it: its own value plus what its accessories and its side's research add, the
  // standing bonuses of its skills and the carry penalty / pair bonus (docs/SRPG.md). Everything that needs a stat
  // asks here (formulas too, through FormulaCtx::battle).
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
  // Types the side has not unlocked by research (docs/ECONOMY.md) are left out unless `includeLocked`
  // (a menu that shows them greyed out, with unlockedBy for the reason).
  void buildOptions(Cell cell, std::vector<int>& unitTypes, bool includeLocked = false) const;
  bool canAfford(int side, int unitType) const;
  // The whole Build check for the active side (owner, free square, produced here, can enter, unlocked,
  // affordable); false with the reason in `why` (optional).
  bool canBuild(Cell cell, int unitType, std::string* why = nullptr) const;

  // ---- upgradable structures and research (upgrades.cpp, research.cpp; docs/ECONOMY.md) ----
  // The upgrades of structure `structure` (kinds it can become) for the active side, each with whether it
  // can be done now. Empty when the structure is not the active side's or the battle is over.
  void upgradeOptions(int structure, std::vector<UpgradeOption>& out) const;
  // The technologies of `side` that are not finished (the one under way is left out: researching()),
  // each with whether it could be started now (only the active side can start any).
  void researchOptions(int side, std::vector<ResearchOption>& out) const;
  bool hasTechnology(int side, int tech) const;
  // The technology under way (index into db->research) or -1; researchTurnsLeft is the number of the
  // side's turns still to start before it is done.
  int researching(int side) const { return sides[(size_t)side].researching; }
  int researchTurnsLeft(int side) const { return sides[(size_t)side].researchLeft; }
  // Does the side own a structure of a kind with "research": true (otherwise the countdown is paused)?
  bool canResearch(int side) const;
  // Is a unit type free for the side to build: no technology lists it in "unlocks", or the side has one that does?
  bool unitUnlocked(int side, int unitType) const;
  // The technologies whose "unlocks" list the unit type (empty = needs none); any one of them is enough.
  void unlockedBy(int unitType, std::vector<int>& techs) const;
  // Hands a campaign's finished technologies to a side. Call after init(), before start(): marks them
  // finished (no event) and gives the side's units their full, bonus-included hit points.
  void setStartingResearch(int side, const std::vector<int>& techs);

  // ---- repair and supply (repair.cpp, fuel.cpp, ammo.cpp, supply.cpp; docs/ECONOMY.md) ----
  // Fuel left / full tank of a unit; -1 when its type uses no fuel.
  int fuel(int unit) const;
  int maxFuel(int unit) const;
  // Ammunition of a fixed weapon of the unit / the weapon's full load; -1 = unlimited (no "ammo", or not the
  // unit's weapon).
  int ammo(int unit, int weapon) const;
  int maxAmmo(int unit, int weapon) const;
  // Is it missing fuel or ammunition (something a supplier or a structure could fill up)?
  bool needsSupply(const Unit& u) const;
  // Neighbouring living units of the same side that `unit` (a supplier) would refill if it stood on `from`
  // (they lack something). Empty for a unit that cannot supply.
  void supplyTargetsFrom(int unit, Cell from, std::vector<int>& out) const;
  // The Supply action would be valid now: a supplier of the active side that has not acted, next to at least
  // one unit of its side that lacks something.
  bool canSupply(int unit) const;
  // Would a structure of its own side repair `unit` if it stood on `at` at the start of its turn (the rule exists,
  // the structure's "repairs" list takes the unit, the unit is hurt)? With the amount and price it would get now
  // from its side's present funds (income comes first at the turn start and is not counted). False = no repair.
  bool repairPreviewAt(int unit, Cell at, RepairPlan& out) const;
  bool repairPreview(int unit, RepairPlan& out) const;

  // ---- fog of war (sight.cpp, fog_rules.cpp, event_visibility.cpp; docs/ECONOMY.md "Fog of war") ----
  // The battle always holds the truth. What a team knows is asked here; with fog off every answer is "everything
  // is visible" / the true owner, so callers need no special case. `team` is a team index (SideState::team).
  bool fogOn() const { return fog; }
  // Does the team see the square now (terrain, heights and structure positions are known anyway)?
  bool visibleTo(int team, Cell cell) const;
  // Does the team see this unit now: alive, and on its own team, or on a square its sight reaches (a hiding square
  // shows its unit only to units next to it)?
  bool unitVisibleTo(int team, const Unit& unit) const;
  bool unitVisibleTo(int team, int unit) const;
  // Does the team see at least one square of the structure?
  bool structureVisibleTo(int team, int structure) const;
  // The owner (side index, -1 = nobody) the team believes the structure has: the true one while it sees it,
  // else the last one it saw (the map's starting owner until then).
  int seenOwner(int team, int structure) const;
  // The kind (index into db->structures) the team believes the structure is: the true one while it sees it, else
  // the last one it saw (the map's kind until then; an upgrade done out of sight stays hidden). Without fog the
  // true kind. Everything a viewer or the AI learns about a structure it does not see goes through this.
  int seenKind(int team, int structure) const;
  const StructureDef& seenDef(int team, int structure) const { return db->structures[(size_t)seenKind(team, structure)]; }
  int seenKindForSide(int side, int structure) const { return seenKind(sides[(size_t)side].team, structure); }
  // The same asked for a side (its team).
  bool visibleToSide(int side, Cell cell) const { return visibleTo(sides[(size_t)side].team, cell); }
  bool unitVisibleToSide(int side, int unit) const { return unitVisibleTo(sides[(size_t)side].team, unit); }
  int seenOwnerForSide(int side, int structure) const { return seenOwner(sides[(size_t)side].team, structure); }
  // Sight is recomputed by itself after every change made through apply() and at start / load. Code that edits the
  // public state by hand (tests, tools) calls this afterwards.
  void refreshSight() const { sight_.valid = false; }
  // For loading a save: the last seen owners, one list per team with one entry per structure (checked by the caller).
  void setSeenOwners(const std::vector<std::vector<int>>& owners) { seenOwners_ = owners; sight_.valid = false; }
  // How far a unit of this type standing on `cell` sees: its vision plus what the square gives it (terrain and
  // structure bonuses, each possibly limited to some unit types). The one place that rule lives.
  // `unit` (optional) adds what that unit's standing skills give.
  int visionAt(const UnitTypeDef& type, Cell cell, const Unit* unit = nullptr) const;
  // The same for the last seen kinds (indices into db->structures).
  void setSeenKinds(const std::vector<std::vector<int>>& kinds) { seenKinds_ = kinds; sight_.valid = false; }

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
  // Computer opponent (docs/AI.md; battle/ai_wake.cpp): wakes a guard (and its group) / every guard of a group.
  void wakeUnit(int unit);
  void wakeGroup(const std::string& group);
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
  // Dialogue choices (docs/EVENTS.md "Dialogue choices", story_choice.cpp). Off (the default): a question takes its
  // first offered answer on the spot and leaves a Dialogue entry with the question. On: apply / start stop at the
  // question, leaving a Choice entry as their last one; nothing else can be done until answerChoice(). The player
  // turns it on for a person at the screen; ote-sim and --autoplay leave it off.
  void setAskChoices(bool ask) { choice_.ask = ask; }
  // docs/DIFFICULTY.md (difficulty.cpp): -1 or an index out of range = the game's default (no effect in a game without difficulties).
  void setDifficulty(int index);
  bool choiceWaiting() const { return choice_.wait != nullptr; }
  // The pick is a position in the Choice entry's answers. Appends the entries that follow the question (up to the
  // next one, if there is another). False (nothing changed) when no question is waiting or the pick is out of range.
  bool answerChoice(int pick, std::vector<Event>& events);
  // Hands in a campaign's roster (docs/EVENTS.md "Campaign"). Every named unit on a human side that the
  // roster knows takes its type, name, level, xp, stats and items and starts at full HP; one the roster
  // marks gone is taken off the map; others stay as the map says. Later addUnit events of named units
  // on a human side get the same treatment. Call after init(), before start().
  void setStartingRoster(std::shared_ptr<const Roster> carried);
  void setStartingOtherRosters(std::shared_ptr<const std::vector<Roster>> others) { otherRosters = others; }

  // ---- actions ----
  // why (optional) receives a short human-readable reason on failure.
  bool validate(const Action& action, std::string* why = nullptr) const;
  // Validates, then applies and appends events. Returns false (state
  // untouched) if invalid.
  bool apply(const Action& action, std::vector<Event>& events, std::string* why = nullptr);

  // ---- strategy-RPG features: public queries (docs/SRPG.md). Later steps append there, not here. ----
#include "core/battle/srpg_api.h"
#include "core/battle/places_api.h"
#include "core/battle/quotes_api.h"
#include "core/battle/mapchange_api.h"
#include "core/battle/mapweapon_api.h"

 private:
  // combat.cpp
  bool versusValue(int weapon, const Unit& target, double& vs) const;
  int counterWeapon(const Unit& def, Cell defCell, const Unit& atk, Cell atkCell) const;
  bool weaponInRange(int weapon, int dist) const;
  void applyAttack(const Action& action, std::vector<Event>& events);
  // `killer` = the unit that struck it down (-1 = nobody): handed to srpgUnitGone.
  void killUnit(int unit, std::vector<Event>& events, int killer = -1);
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
  // upgrades.cpp / research.cpp
  bool validateUpgrade(const Action& action, std::string* why) const;
  void applyUpgrade(const Action& action, std::vector<Event>& events);
  bool validateResearch(const Action& action, std::string* why) const;
  void applyResearch(const Action& action, std::vector<Event>& events);
  // Start of the active side's turn: counts the research under way down (paused without a research structure).
  void researchTurn(std::vector<Event>& events);
  // Marks a technology finished for the side, re-clamps its units' hit points (a bonus may have lowered the
  // maximum) and appends ResearchFinished. Does nothing if it is finished already.
  void finishResearch(int side, int tech, std::vector<Event>& events);
  // Pays a cost (ResourceChanged events).
  void pay(int side, const std::vector<Amount>& cost, std::vector<Event>& events);
  // Resets the capture progress of `unit` unless it now stands (`newCell`) on
  // a square of the structure it is capturing.
  void releaseCapture(int unit, Cell newCell);
  bool ownsHq(int side) const;
  // repair.cpp / fuel.cpp / ammo.cpp / supply.cpp (docs/ECONOMY.md "Repair and supply")
  // Full fuel and ammunition for the unit's type (a new unit, or one dressed from a roster).
  void stockUnit(Unit& u) const;
  // Turn start, in this order: resupplyTurn (structures, then suppliers), burnFuel (fuelPerTurn; destroys what
  // runs dry and settles the battle), repairTurn.
  void resupplyTurn(std::vector<Event>& events);
  void burnFuel(std::vector<Event>& events);
  void repairTurn(std::vector<Event>& events);
  bool repairPlan(const Unit& u, Cell at, RepairPlan& out) const;
  // Fills one unit's fuel and ammunition; UnitResupplied if anything was missing.
  void refill(int unit, std::vector<Event>& events);
  // The slot of `ammo` that belongs to the fixed weapon (-1 = unlimited or no ammunition).
  int ammoSlot(const Unit& u, int weapon) const;
  // One attack's worth of ammunition (once per attack, not per strike).
  void spendAmmo(int unit, int weapon);
  void applySupply(const Action& action, std::vector<Event>& events);
  // items.cpp
  bool validateItemAction(const Action& action, std::string* why) const;
  void applyItemAction(const Action& action, std::vector<Event>& events);
  void spendUse(int unit, int slot, std::vector<Event>& events);
  // Hands an item to a unit; without room it goes to the side's storage, else it is lost.
  void giveTo(int unit, ItemStack stack, int flags, std::vector<Event>& events);
  void clampHp(Unit& u);
  // battle_init.cpp: a unit as a map (or addUnit) describes it, not yet placed.
  Unit makeUnit(const MapUnitDef& def) const;
  // carried.cpp: dresses a named unit of a human side from `roster`. 1 = done, 0 = the roster says it is
  // gone (do not place it), -1 = not concerned.
  int applyRoster(Unit& u) const;
  // story.cpp
  // `actor`: the unit that set the event off, for the `actor` condition (default -2 = the running event's own).
  bool conditionHolds(const Condition& c, int actor = -2) const;
  bool conditionsHold(const std::vector<Condition>& list, int actor = -2) const;
  bool triggerMatches(const Trigger& t, const StoryOccurrence& o) const;
  void fireStory(const StoryOccurrence& o, std::vector<Event>& events);
  void drainStory(std::vector<Event>& events);
  void applyTalk(const Action& action, std::vector<Event>& events);
  // story_commands.cpp
  void runCommands(const std::vector<Command>& list, std::vector<Event>& events);
  void runCommand(const Command& c, std::vector<Event>& events);
  void storyAddUnit(const MapUnitDef& def, std::vector<Event>& events);
  void storyRelocate(int unit, Cell to, std::vector<Event>& events, bool walk = false, bool noWait = false);
  // story_scene_fx.cpp: the scene presentation commands, a walked moveUnit, and the fade that never outlives a run of events.
  void storyScene(const Command& c, std::vector<Event>& events);
  bool storyWalkPath(int unit, Cell from, Cell to, std::vector<int>& path) const;
  void endStoryRun(std::vector<Event>& events);
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
  // `fromCombat` false: not a fight, so a summon's "givesXp" does not apply (a staff's experience).
  void awardXp(int unit, int amount, std::vector<Event>& events, bool fromCombat = true);

  // ---- strategy-RPG features: hooks and private functions (docs/SRPG.md). Later steps append there, not here. ----
#include "core/battle/srpg_internal.h"
#include "core/battle/places_internal.h"
#include "core/battle/difficulty_internal.h"

  StoryRun storyRun_;  // transient: empty between actions
  StoryChoice choice_;  // dialogue choices (story_choice.cpp)
  // story_choice.cpp: apply / start when questions wait; the command; the pieces of apply / start without that.
  void applyAction(const Action& action, std::vector<Event>& events);
  void startNow(std::vector<Event>& events);
  void runAsking(bool isStart, const Action& action, std::shared_ptr<const Battle> pre, const std::vector<int>& answers,
                 size_t skip, std::vector<Event>& out);
  void storyChoice(const Command& c, std::vector<Event>& events);
  bool askingChoices() const { return choice_.ask && story && story->hasChoice && !choice_.running; }

  // ---- fog of war, private (sight.cpp) ----
  struct SightCache {
    bool valid = false;
    std::vector<std::vector<uint8_t>> seen;      // [team][cell]: the square is in the team's sight
    std::vector<std::vector<uint8_t>> unitSeen;  // [team][cell]: a unit of ANOTHER team standing there would be seen
  };
  // Derived data kept lazily: a query recomputes it when `valid` is false. Copies carry it.
  mutable SightCache sight_;
  // Last seen owner per team and structure; the only part of fog that is state (saved). Updated by sight.
  mutable std::vector<std::vector<int>> seenOwners_;
  // Last seen kind per team and structure, refreshed under exactly the same conditions as the owner.
  mutable std::vector<std::vector<int>> seenKinds_;
  // While apply() / start() run: where events go and from which index they are not stamped with `seen` yet.
  std::vector<Event>* sightEvents_ = nullptr;
  size_t stampFrom_ = 0;

  void ensureSight() const;
  void computeTeamSight(int team) const;
  int visionOf(const Unit& u) const;
  // Call BEFORE a change that alters who sees what (a unit moves, dies, is added, changes side, a structure changes
  // owner or kind): events pushed so far are stamped with the sight as it still is, then the cache is dropped.
  void markSight();
  // Stamps the events pushed since the last stamp (event_visibility.cpp).
  void stampEvents();
  uint32_t eventSeenBy(const Event& e) const;
  uint32_t unitSeenBy(int unit) const;
  uint32_t cellSeenBy(Cell cell) const;
  uint32_t footprintSeenBy(int structure) const;
  // After a structure's owner changed: the teams in `teams` now believe the new owner.
  void learnOwner(int structure, uint32_t teams);
  // fog_rules.cpp
  // Sets sightEvents_ / stampFrom_ for the length of one apply() / start(); stamps what is left when it ends.
  struct EventScope {
    EventScope(Battle& battle, std::vector<Event>& events);
    ~EventScope();
    Battle& b;
    bool owns = false;
  };
  // fog_rules.cpp: a planned move on what the team saw can run into an enemy it did not. Index of the path square
  // the unit really reaches (the last free one before the enemy; path.size() - 1 when nothing is in the way);
  // `blocker` receives the enemy unit's id, or -1, and `blockedAt` its square.
  int pathStop(int unit, const std::vector<Cell>& path, int& blocker, Cell& blockedAt) const;
  // The unit walked `path` up to index `upTo`: its team notices the structures it passed (their last seen owner).
  void noticeAlong(int unit, const std::vector<Cell>& path, int upTo);
};

}  // namespace ote
