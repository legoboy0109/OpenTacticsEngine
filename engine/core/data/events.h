// The parsed form of a map's story events and objectives (docs/EVENTS.md).
// Plain structs, filled by the loader (loader_events.cpp and friends) and
// only read by Battle. Everything that names a game thing is already an
// index here (side, team, unit type, item, resource, switch, variable);
// units are named by their story id (a string) because they come and go.
#pragma once
#include <string>
#include <vector>

#include "core/base/json.h"
#include "core/data/database.h"

namespace ote {

// "Which unit": one of a unit id, a side, a team, a unit type, a tag, or any.
// A plain string where a filter is expected means { "unit": that }.
struct UnitFilter {
  enum class Kind { Any, Unit, Side, Team, Type, Tag };
  Kind kind = Kind::Any;
  std::string unit;  // Kind::Unit; also the tag text for Kind::Tag
  int index = -1;    // Side / Team / Type
};

struct Rect {
  int x = 0, y = 0, w = 1, h = 1;
};

enum class CmpOp { Eq, Ne, Lt, Le, Gt, Ge };

struct Condition {
  enum class Kind { Switch, Variable, Alive, Dead, Round, Carries, Researched, Sees };
  Kind kind = Kind::Switch;
  int index = -1;     // switch / variable index, item index for Carries, technology index for Researched
  int side = -1;      // Researched: the side
  int team = -1;      // Sees: the team that must see a unit matching `filter`
  bool is = true;     // Switch
  CmpOp op = CmpOp::Eq;  // Variable, Round
  int value = 0;      // Variable, Round
  UnitFilter filter;  // Alive, Dead, Carries, Sees
};

struct Trigger {
  enum class Kind { BattleStart, BattleWon, BattleLost, BattleEnd, Turn, Reach, Defeated, Fight, Talk };
  Kind kind = Kind::BattleStart;
  int round = 0;       // Turn: the round; 0 = every round
  int side = 0;        // Turn: whose turn (default: the first side)
  Rect rect;           // Reach
  UnitFilter who;      // Reach: by; Defeated: the unit; Fight: the unit
  UnitFilter other;    // Defeated: by; Fight: with
  std::string talker, talkee;  // Talk: unit ids
  int label = -1;      // Talk: index into StoryData::texts
};

struct Command {
  enum class Kind {
    Say, AddUnit, RemoveUnit, MoveUnit, ChangeSide, GiveItem, GiveResource, SetSwitch, SetVariable,
    AddVariable, Objective, Win, Lose, If, EndTurn, Focus, GrantResearch
  };
  Kind kind = Kind::Say;
  // Say
  int text = -1, speakerName = -1, portrait = -1;  // indices into StoryData::texts (-1 = none)
  std::string unit;      // Say: speaker unit id; MoveUnit / Focus: the unit ("" = a square)
  MapUnitDef newUnit;    // AddUnit
  UnitFilter filter;     // RemoveUnit, ChangeSide, GiveItem: which units
  int x = 0, y = 0;      // MoveUnit, Focus (a square)
  int side = -1;         // ChangeSide, GiveResource, GrantResearch: side index
  int team = -1;         // Win, Lose: team index
  int item = -1;         // GiveItem
  std::vector<Amount> amounts;  // GiveResource
  int index = -1;        // SetSwitch / SetVariable / AddVariable: the switch / variable; GrantResearch: the technology
  bool on = false;       // SetSwitch
  int value = 0;         // SetVariable / AddVariable
  int objectives = -1;   // Objective: index into StoryData::objectives
  std::vector<Condition> cond;                // If
  std::vector<Command> thenDo, elseDo;        // If
};

struct StoryEvent {
  std::string id;
  Trigger when;
  std::vector<Condition> cond;
  bool repeat = false;
  std::vector<Command> commands;
  bool endsTurnOnTalk = false;  // contains an endTurn command somewhere (only allowed for Talk)
};

struct ObjectiveRule {
  enum class Kind { Defeated, Reach, Round, Survive };
  Kind kind = Kind::Defeated;
  UnitFilter filter;  // Defeated: who; Reach: by
  bool hasBy = false; // Reach: `by` was written (else: the human team for win, everybody else for lose)
  Rect rect;          // Reach
  int rounds = 0;     // Round, Survive
};

struct ObjectiveSet {
  int text = -1;  // index into StoryData::texts, -1 = none
  std::vector<ObjectiveRule> win, lose;
  bool judges() const { return !win.empty() || !lose.empty(); }
};

struct StoryData {
  // Every piece of wording (dialogue, names, labels, objective text) lives in this table,
  // so events carry small indices and the wording stays out of the rules (docs/SAVES.md).
  std::vector<std::string> texts;
  std::vector<StoryEvent> events;
  // [0] is the map's own objectives (possibly empty); one more for each `objective` command.
  std::vector<ObjectiveSet> objectives;
  StoryData() : objectives(1) {}
};

// Story variables and gifts of resources stay within +-this, so saves can check them (docs/SAVES.md).
constexpr int kMaxStoryNumber = 1000000000;

// True for an id made of letters, digits, '_' and '-' (units, events, switches...).
bool isStoryId(const std::string& id);

// Reads "events" and "objectives" of a map file into map.story (left null when the file has
// neither). Needs the map's sides, units and size already loaded. Every refusal names the map,
// the event and the command. `file` is the map file path (the start of every message).
bool loadMapStory(const Database& db, const Json& root, MapDef& map, const std::string& file, std::string& err);

}  // namespace ote
