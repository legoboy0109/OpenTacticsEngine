// Internal to the event loader (loader_events.cpp, loader_story_*.cpp): the state shared while
// one map's "events" and "objectives" are read. Every reader returns false after writing a
// message of the form
//   <map file>: map "<id>": event "<id>": command 3 (addUnit): <what is wrong>
#pragma once
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <vector>

#include "core/data/events.h"
#include "core/data/loader_util.h"

namespace ote {

class StoryLoader {
 public:
  StoryLoader(const Database& db, const MapDef& map, const std::string& file, std::string& err, StoryData& data)
      : db_(db), map_(&map), realMap_(&map), c_(err, file), data_(data) {}

  const Database& db() const { return db_; }
  const MapDef& map() const { return *map_; }
  const LoadCtx& ctx() const { return c_; }
  StoryData& data() { return data_; }

  // Story ids the map knows: its units' and those of addUnit commands (collected before parsing).
  std::vector<std::string> unitIds;
  // Ids already taken by units (map units first, then each addUnit as it is read), each with the difficulties it exists on
  // (empty = all): the same id may be reused by placements limited to disjoint difficulties (docs/DIFFICULTY.md).
  std::vector<std::pair<std::string, std::vector<int>>> usedIds;
  // Guard group names the map's units and its commands give (docs/AI.md), for wakeGroup.
  std::vector<std::string> groups;
  // Variables that a choice command of this map already writes ("store"): a second one may not (loader_story_choice.cpp).
  std::vector<int> choiceStores;

  bool fail(const std::string& where, const std::string& msg) const { return c_.fail(where, msg); }
  // Adds a piece of wording to the text table; returns its index.
  int text(const std::string& s) {
    data_.texts.push_back(s);
    return (int)data_.texts.size() - 1;
  }

  // An object may only have these keys (a typo must not pass silently).
  bool onlyKeys(const Json& o, std::initializer_list<const char*> keys, const std::string& where) const;

  // -- ids (loader_story_refs.cpp) --
  bool sideIndex(const std::string& id, const std::string& where, const char* what, int& out) const;
  // A team name; a side id also works (its team).
  bool teamIndex(const std::string& id, const std::string& where, const char* what, int& out) const;
  bool unitId(const Json& j, const std::string& where, const char* what, std::string& out) const;
  // { "unit": id } | { "side": id } | { "team": name } | { "type": id } | { "tag": text } | {} | "unit id"
  // `{ "actor": true }` is a filter only inside the commands of an event that has an acting unit (loader_events.cpp sets this).
  void allowActorFilter(bool on) { actorOk_ = on; }
  // Reading the commands of a `{ "battle": "beforePrep" }` event: the ones that make no sense before deployment are refused.
  void readingBeforePrep(bool on) { beforePrep_ = on; }
  bool filter(const Json& j, const std::string& where, const char* what, UnitFilter& out) const;
  bool rect(const Json& j, const std::string& where, Rect& out) const;
  bool conditions(const Json& list, const std::string& where, std::vector<Condition>& out) const;

  // -- commands (loader_story_commands.cpp) --
  bool commands(const Json& list, const std::string& where, bool talk, std::vector<Command>& out,
                bool& usesEndTurn);

  // -- objectives (loader_story_objectives.cpp) --
  bool objectiveSet(const Json& o, const std::string& where, ObjectiveSet& out);

  // -- strategy-RPG features (docs/SRPG.md): each area reads its own commands, conditions and triggers. `e` / `w`
  //    is the whole command / condition / "when" object, `at` names it in messages. --
  bool promoteCommand(const Json& e, const std::string& at, Command& out);             // loader_promotion.cpp
  bool promotedTrigger(const Json& w, const std::string& at, Trigger& out);            // loader_promotion.cpp
  bool skillCommand(const Json& e, const std::string& at, bool teach, Command& out);   // loader_skills.cpp: teachSkill / forgetSkill
  bool hasSkillCondition(const Json& e, const std::string& at, Condition& out) const;  // loader_skills.cpp
  bool supportCommand(const Json& e, const std::string& at, Command& out);             // loader_supports.cpp: addSupport
  bool supportCondition(const Json& e, const std::string& at, Condition& out) const;   // loader_supports.cpp
  bool supportTrigger(const Json& w, const std::string& at, Trigger& out);             // loader_supports.cpp
  // Doors, chests and places (docs/SRPG.md section 10), loader_places_story.cpp: the "place" trigger and the commands openDoor,
  // closeDoor, lockDoor, unlockDoor, openChest (`kind` = the command's key).
  bool placeTrigger(const Json& w, const std::string& at, Trigger& out);
  bool placeCommand(const Json& e, const std::string& kind, const std::string& at, Command& out);
  bool conditionCommand(const Json& e, const std::string& at, bool add, Command& out);           // loader_story_conditions.cpp: addCondition / removeCondition
  bool hasConditionCondition(const Json& e, const std::string& at, Condition& out) const;      // loader_story_conditions.cpp
  bool actorCondition(const Json& e, const std::string& at, Condition& out) const;                // loader_story_actor.cpp: { "actor": filter }
  bool behaviourCommand(const Json& e, const std::string& at, Command& out);                    // loader_behaviour.cpp: setBehaviour / wakeGroup
  bool mapChangeCommand(const Json& e, const std::string& kind, const std::string& at, Command& out);  // loader_story_mapchange.cpp: setTerrain, applyChange, revertChange, setFog, setVision

  bool phaseMusicCommand(const Json& e, const std::string& at, Command& out);                    // loader_story_music.cpp: setPhaseMusic
  bool storeItemsCommand(const Json& e, const std::string& at, Command& out);                    // loader_story_store.cpp
  bool rosterCondition(const Json& e, const std::string& at, const char* kind, Condition& out) const;  // loader_story_roster.cpp: rosterAlive / rosterDead / inRoster
  bool fightCommand(const Json& e, const std::string& at, Command& out);                         // loader_story_fight.cpp: fight
  bool showUnitCommand(const Json& e, const std::string& at, Command& out);                     // loader_story_roster.cpp

  // -- another map shown by a scene (loader_story_show_map.cpp; docs/EVENTS.md "Showing another map") --
  // Between a showMap and its returnMap (or the end of the event) the commands act on the shown map: unit ids, sides and squares are
  // checked against that map and the units of the section's own addUnit commands; the commands that act on the battle itself are refused.
  bool showMapCommand(const Json& e, const std::string& at, Command& out);
  bool returnMapCommand(const Json& e, const std::string& at, Command& out);
  bool showingMap() const { return shown_ != nullptr; }
  // Reads with the battle's own map in force: an `if`'s conditions, a variable read from a unit and setPhaseMusic act on the real battle.
  bool onRealMap(const std::function<bool()>& read);

 private:
  bool command(const Json& e, const std::string& where, bool talk, Command& out, bool& usesEndTurn);
  bool choiceCommand(const Json& e, const std::string& at, bool talk, Command& out, bool& usesEndTurn);  // loader_story_choice.cpp
  bool addUnitCommand(const Json& e, const std::string& where, Command& out);
  bool condition(const Json& e, const std::string& where, Condition& out) const;
  bool rules(const Json& list, const char* key, const std::string& where, bool win, std::vector<ObjectiveRule>& out) const;

  // What a showMap section swaps out (the battle's own map and the ids it knows) while it reads the shown map's commands.
  struct Context {
    const MapDef* map = nullptr;
    std::vector<std::string> unitIds, groups;
    std::vector<std::pair<std::string, std::vector<int>>> usedIds;
    bool actorOk = false;
  };
  void swapContext();
  void endShownMap();  // the event's own list ended with a map still shown: the return is implied
  std::shared_ptr<const MapDef> shown_;  // the map a showMap section shows (null = none)
  Context saved_;                        // the battle's own context while shown_ is set
  int depth_ = 0;                        // nesting of command lists: 1 = the event's own "do"
  const Json* list_ = nullptr;           // the event's own list and the entry being read (for the ids a section adds)
  size_t at_ = 0;

  const Database& db_;
  const MapDef* map_;      // the map commands act on: the battle's own, or the one a showMap section shows
  const MapDef* realMap_;  // the battle's own map (the file being read)
  LoadCtx c_;
  StoryData& data_;
  bool actorOk_ = false, beforePrep_ = false;
  int loopDepth_ = 0;  // inside how many "loop" blocks the command being read is ("break" needs one)
};

}  // namespace ote
