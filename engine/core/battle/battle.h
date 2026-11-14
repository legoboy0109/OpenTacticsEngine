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
#include "core/base/rng.h"
#include "core/base/types.h"
#include "core/data/database.h"
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
};

enum : int { kHit = 1, kCrit = 2, kKill = 4, kCounter = 8 };

struct Event {
  EventType type = EventType::TurnStarted;
  int a = -1, b = -1, c = -1, d = -1;
  int flags = 0;
  std::vector<int> data;
};

struct Unit {
  int id = -1;     // index into Battle::units, never reused
  int type = 0;    // index into db->unitTypes
  int side = 0;
  Cell cell = kNoCell;  // kNoCell once dead
  std::string name;
  int level = 1, xp = 0;
  std::vector<int> stats;  // current maximums, per stat index
  int hp = 0;
  bool moved = false;  // has moved this turn
  bool done = false;   // has finished acting this turn
  bool alive = true;
  bool leader = false;
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
};

enum class ActionType { Move, Attack, Capture, Wait, Build, EndTurn };

struct Action {
  ActionType type = ActionType::EndTurn;
  int unit = -1;        // Move, Attack, Capture, Wait
  Cell cell = kNoCell;  // Move destination / Build location
  int target = -1;      // Attack: defender unit
  int weapon = -1;      // Attack: weapon index (db->weapons)
  int unitType = -1;    // Build

  static Action move(int unit, Cell dest) { Action a; a.type = ActionType::Move; a.unit = unit; a.cell = dest; return a; }
  static Action attack(int unit, int target, int weapon) { Action a; a.type = ActionType::Attack; a.unit = unit; a.target = target; a.weapon = weapon; return a; }
  static Action capture(int unit) { Action a; a.type = ActionType::Capture; a.unit = unit; return a; }
  static Action wait(int unit) { Action a; a.type = ActionType::Wait; a.unit = unit; return a; }
  static Action build(Cell cell, int unitType) { Action a; a.type = ActionType::Build; a.cell = cell; a.unitType = unitType; return a; }
  static Action endTurn() { return Action(); }
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
  int maxHp(const Unit& u) const { return u.stats[db->rules.statHp]; }
  int movePoints(const Unit& u) const { return u.stats[db->rules.statMove]; }
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
  // progression.cpp
  void awardXp(int unit, int amount, std::vector<Event>& events);
};

}  // namespace ote
