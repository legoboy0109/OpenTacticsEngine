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
#include "core/data/scene_fx.h"

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

// CmpOp (= != < <= > >=) lives in core/data/srpg.h: skill conditions use it too.

struct Condition {
  enum class Kind {
    Switch, Variable, Alive, Dead, Round, Carries, Researched, Sees,
    // docs/SRPG.md: HasSkill = a living unit matching `filter` has skill `index`; Support = the level of support
    // pair `index` compared (`op`) with `value`.
    HasSkill, Support,
    // docs/SRPG.md section 9: a living unit matching `filter` has status condition `index`.
    HasCondition
  };
  Kind kind = Kind::Switch;
  int index = -1;     // switch / variable index, item index for Carries, technology index for Researched
  int side = -1;      // Researched: the side
  int team = -1;      // Sees: the team that must see a unit matching `filter`
  bool is = true;     // Switch
  CmpOp op = CmpOp::Eq;  // Variable, Round
  int value = 0;      // Variable, Round
  UnitFilter filter;  // Alive, Dead, Carries, Sees
};

// What a Place trigger waits for (and what a Place occurrence reports).
enum : int { kPlaceOpened = 0, kPlaceVisited = 1, kPlaceDestroyed = 2 };

struct Trigger {
  enum class Kind {
    BattleStart, BattleWon, BattleLost, BattleEnd, Turn, Reach, Defeated, Fight, Talk,
    // docs/SRPG.md: Promoted = a unit matching `who` promoted; Support = support pair `pair` gained `level`.
    Promoted, Support,
    // docs/SRPG.md section 10: a door, chest or visitable place (named by `place`) was opened / visited / destroyed (`placeIs`).
    Place
  };
  Kind kind = Kind::BattleStart;
  int pair = -1;       // Support: index into Database::supports.pairs
  int level = 0;       // Support: the level gained; 0 = any level
  int round = 0;       // Turn: the round; 0 = every round
  int side = 0;        // Turn: whose turn (default: the first side)
  Rect rect;           // Reach
  UnitFilter who;      // Reach: by; Defeated: the unit; Fight: the unit
  UnitFilter other;    // Defeated: by; Fight: with
  std::string talker, talkee;  // Talk: unit ids
  std::string place;   // Place: the structure's id (a map structure's "id")
  int placeIs = 0;     // Place: kPlaceOpened / kPlaceVisited / kPlaceDestroyed (below)
  int label = -1;      // Talk: index into StoryData::texts
};

// A number a setVariable / addVariable takes from a unit instead of a written one (docs/EVENTS.md "Reading a unit's
// numbers"). Shared by event commands and campaign scene commands. Kind::None = the command uses its written number.
struct UnitReading {
  enum class Kind { None, Level, Hp, MaxHp, Stat };
  Kind kind = Kind::None;
  std::string unit;  // the unit (event: a unit id of the map; scene: a roster id)
  int stat = -1;     // Stat: index into Database::stats
};

struct Command;

// One answer of a Choice command (docs/EVENTS.md "Dialogue choices"): its wording, an optional condition that hides
// it, and the commands that run when it is picked.
struct ChoiceAnswer {
  int text = -1;                 // index into StoryData::texts
  std::vector<Condition> cond;   // all must hold for the answer to be offered ("if"); empty = always
  std::vector<Command> commands;
};

struct Command {
  enum class Kind {
    Say, AddUnit, RemoveUnit, MoveUnit, ChangeSide, GiveItem, GiveResource, SetSwitch, SetVariable,
    AddVariable, Objective, Win, Lose, If, EndTurn, Focus, GrantResearch,
    PlayMusic, StopMusic, PlaySound,  // presentation only (docs/SOUND.md): the id is opaque text here
    // docs/SRPG.md. Promote: `unit` becomes unit type `index` (-1 = its type's only promotion). TeachSkill /
    // ForgetSkill: skill `index`, units matching `filter`. AddSupport: support pair `index` gains `value` points.
    Promote, TeachSkill, ForgetSkill, AddSupport,
    // docs/EVENTS.md "Dialogue choices": asks the question `text` (speaker as in Say) and runs the commands of the
    // picked answer; `index` = the variable that receives the picked answer's number (1 = the first answer), -1 = none.
    Choice,
    // docs/EVENTS.md "Taking an item away": removes one copy (`on` = every copy) of item `item` from every existing
    // unit matching `filter`; nothing for a unit that carries none.
    TakeItem,
    // docs/SRPG.md section 10: `place` names a structure of the map; `index` is what to do (a kPlaceCmd value); `unit` = the unit that
    // gets a chest's contents (OpenChest; "" = the first human side's storage / funds).
    Place,
    // docs/EVENTS.md "Status conditions": AddCondition gives condition `index` (`value` turns, 0 = its own) to every living unit
    // matching `filter`; RemoveCondition removes it (`on` = every condition).
    AddCondition, RemoveCondition,
    // docs/AI.md: SetBehaviour gives every living computer unit matching `filter` the block `behaviour` (and puts it back to sleep, for a
    // guard); WakeGroup wakes every unit whose guard group is `unit` (the group name).
    SetBehaviour, WakeGroup,
    Scene  // docs/EVENTS.md "Scene presentation": the command is `fx` (background, fade, wait, shake, leave, weather)
  };
  Kind kind = Kind::Say;
  // Say, Choice
  int text = -1, speakerName = -1, portrait = -1;  // indices into StoryData::texts (-1 = none); PlayMusic / PlaySound: `text` is the track / sound id
  std::string unit;      // Say: speaker unit id; MoveUnit / Focus: the unit ("" = a square)
  std::string place;     // Place: the structure's id
  MapUnitDef newUnit;    // AddUnit
  AiBehaviour behaviour; // SetBehaviour
  UnitFilter filter;     // RemoveUnit, ChangeSide, GiveItem, TakeItem: which units
  int x = 0, y = 0;      // MoveUnit, Focus (a square)
  int side = -1;         // ChangeSide, GiveResource, GrantResearch: side index
  int team = -1;         // Win, Lose: team index
  int item = -1;         // GiveItem, TakeItem
  std::vector<Amount> amounts;  // GiveResource
  int index = -1;        // SetSwitch / SetVariable / AddVariable: the switch / variable; GrantResearch: the technology
  bool on = false;       // SetSwitch; TakeItem: every copy
  UnitReading reading;   // SetVariable / AddVariable: where the number comes from when it is not `value`
  int value = 0;         // SetVariable / AddVariable; PlayMusic / StopMusic: the fade in milliseconds (-1 = not given)
  int objectives = -1;   // Objective: index into StoryData::objectives
  std::vector<Condition> cond;                // If
  std::vector<Command> thenDo, elseDo;        // If
  std::vector<ChoiceAnswer> answers;          // Choice: two to four
  SceneFx fx;            // Scene; also the presentation keys of Say (position, auto, narrate), Focus (ms, wait), MoveUnit (walk, wait)
};

// What a Place command does.
enum : int { kPlaceCmdOpenDoor = 0, kPlaceCmdCloseDoor = 1, kPlaceCmdLockDoor = 2, kPlaceCmdUnlockDoor = 3, kPlaceCmdOpenChest = 4 };

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
  bool hasChoice = false;  // some event has a Choice command (the Battle only needs its question machinery then)
  StoryData() : objectives(1) {}
};

// Story variables and gifts of resources stay within +-this, so saves can check them (docs/SAVES.md).
constexpr int kMaxStoryNumber = 1000000000;

// A question has two to four answers (docs/EVENTS.md "Dialogue choices"); shared by the event and scene loaders.
constexpr int kMinChoiceAnswers = 2, kMaxChoiceAnswers = 4;

// True for an id made of letters, digits, '_' and '-' (units, events, switches...).
bool isStoryId(const std::string& id);

// Reads "events" and "objectives" of a map file into map.story (left null when the file has
// neither). Needs the map's sides, units and size already loaded. Every refusal names the map,
// the event and the command. `file` is the map file path (the start of every message).
bool loadMapStory(const Database& db, const Json& root, MapDef& map, const std::string& file, std::string& err);

}  // namespace ote
