// Conformance tables, part 4: story events of a map (docs/EVENTS.md): the trigger, the conditions and every command.
//
// An event is tried inside one first-map event that the mods write ("t": battle start, one say line); the sample of a command
// replaces its `do[0]`. Each family of commands names the fixture that has what it points at (a chest, resources, research,
// a tile layer ...). The command language is recursive (an `if` holds commands): the lists nested inside a command are the same
// definition as `do`, so only their own keys are tried and the commands inside them are skipped with that reason.
#include "conformance_contexts.h"

namespace conformance {

namespace {

#define DO "maps:events[].do[]"
#define WHEN "maps:events[].when"

// One event at the top of the first map: a trigger and a command list.
Op event(const char* map, const QString& when, const QString& doList) {
  return {QString("maps/%1.json").arg(map), "events", QString(R"([{"id":"t","when":%1,"do":%2}])").arg(when, doList)};
}
const char* kBattle = R"({"battle":"start"})";
const char* kSay = R"([{"say":"hi"}])";

struct Family {
  const char* name;
  const char* fixture;
  const char* map;
  std::vector<Op> extra;                 // more edits (a group name, difficulties ...)
  std::vector<const char*> claims;
  std::map<QString, QString> overrides;
  const char* trigger = nullptr;         // default: battle start
};

void add(std::vector<Context>& c, const Family& f) {
  std::vector<Op> mods = {event(f.map, f.trigger ? f.trigger : kBattle, kSay)};
  for (const Op& o : f.extra) mods.push_back(o);
  c.push_back({f.name, f.fixture, mods, f.claims, f.overrides});
}

}  // namespace

void addEventContexts(std::vector<Context>& c) {
  // --- the event itself, its triggers and conditions (srpg-rename: characters, a hamlet and a chest, a support pair, a poison) ---
  add(c, {"event-base", "srpg-rename", "field", {}, {"maps:events", "maps:events[].repeat", "maps:events[].do"}, {}});
  add(c, {"event-triggers", "srpg-rename", "field", {}, {WHEN},
          {{WHEN "{talk}", R"({"talk":"hero","to":"ally"})"},
           {WHEN "{talk}.to", "\"ally\""},
           {WHEN "{support}", R"({"support":["hero","ally"]})"},
           {WHEN "{support}.support", R"(["hero","ally"])"},
           {WHEN "{support}.level", "1"}}});
  add(c, {"event-conditions", "srpg-rename", "field",
          {{"game.json", "difficulties", R"({"default":"normal","list":[{"id":"normal"},{"id":"hard"}]})"}},
          {"maps:events[].if"},
          {{"maps:events[].if[]{support}", R"({"support":["hero","ally"],"value":1})"},
           {"maps:events[].if[]{support}.support", R"(["hero","ally"])"},
           {"maps:events[].if[]{support}.value", "1"}}});
  add(c, {"event-condition-research", "research-branches", "field", {}, {"maps:events[].if[]{researched}"}, {}});

  // --- commands about units, items and the story (srpg-rename) ---
  add(c, {"cmd-say", "srpg-rename", "field", {}, {DO "{say}", DO "{narrate}", DO "{removeUnit}", DO "{changeSide}", DO "{giveItem}", DO "{takeItem}", DO "{setSwitch}",
                                                   DO "{win}", DO "{lose}", DO "{teachSkill}", DO "{forgetSkill}",
                                                   DO "{addCondition}", DO "{removeCondition}", DO "{showUnit}"}, {}});
  add(c, {"cmd-variables", "srpg-rename", "field", {}, {DO "{setVariable", DO "{addVariable"}, {}});
  add(c, {"cmd-moveunit", "srpg-rename", "field", {}, {DO "{moveUnit}"}, {{DO "{moveUnit}", R"({"moveUnit":"hero","x":1,"y":1,"walk":true})"}}});
  add(c, {"cmd-addunit", "srpg-rename", "field", {}, {DO "{addUnit}"}, {{DO "{addUnit}", R"({"addUnit":{"type":"grunt","side":"red","x":8,"y":0,"id":"g9"}})"}}});
  add(c, {"cmd-objective", "srpg-rename", "field", {}, {DO "{objective}"}, {}});
  add(c, {"cmd-if", "srpg-rename", "field", {}, {DO "{if}"}, {}});
  add(c, {"cmd-focus", "srpg-rename", "field", {}, {DO "{focus}"}, {}});
  add(c, {"cmd-audio", "demo-economy", "misty_ford", {}, {DO "{playMusic}", DO "{stopMusic}", DO "{playSound}", DO "{setPhaseMusic}"}, {}});
  add(c, {"cmd-behaviour", "srpg-rename", "field", {}, {DO "{setBehaviour}"}, {{DO "{setBehaviour}.units{side}.side", "\"red\""}}});
  add(c, {"cmd-wake", "srpg-rename", "field", {{"maps/field.json", "units[3].behaviour", R"({"mode":"guard","group":"ambush"})"}},
          {DO "{wakeGroup}"}, {{DO "{wakeGroup}.wakeGroup", "\"ambush\""}}});
  add(c, {"cmd-promote", "srpg-rename", "field", {}, {DO "{promote}"}, {{DO "{promote}", R"({"promote":"hero","to":"veteran"})"}}});
  add(c, {"cmd-support", "srpg-rename", "field", {}, {DO "{addSupport}"},
          {{DO "{addSupport}", R"({"addSupport":["hero","ally"],"by":2})"}, {DO "{addSupport}.addSupport", R"(["hero","ally"])"}}});
  add(c, {"cmd-choice", "srpg-rename", "field", {}, {DO "{choice}"},
          {{DO "{choice}.answers", R"([{"text":"Yes"},{"text":"No"}])"}}});
  add(c, {"cmd-fight", "srpg-rename", "field", {}, {DO "{fight}"}, {{DO "{fight}", R"({"fight":{"attacker":"hero","defender":"rival"}})"}, {DO "{fight}.fight.defender", "\"rival\""}}});
  add(c, {"cmd-chest", "srpg-rename", "field", {}, {DO "{openChest}"}, {{DO "{openChest}", R"({"openChest":"c1"})"}, {DO "{openChest}.openChest", "\"c1\""}}});
  add(c, {"cmd-end-turn", "srpg-rename", "field", {}, {DO "{endTurn}"}, {}, R"({"talk":"hero","to":"ally"})"});
  add(c, {"cmd-scene", "srpg-rename", "field", {}, {DO "{clearStage}", DO "{leave}", DO "{enter}", DO "{move}", DO "{shake}", DO "{weather}", DO "{wait}"}, {}});
  add(c, {"cmd-fade", "srpg-rename", "field", {}, {DO "{fade}"}, {{DO "{fade}.color", "\"#112233\""}}});

  // --- commands that need something else of the game ---
  add(c, {"cmd-doors", "srpg-doors", "corridor", {}, {DO "{giveResource}", DO "{openDoor}", DO "{closeDoor}", DO "{lockDoor}", DO "{unlockDoor}"}, {}});
  add(c, {"cmd-research", "research-branches", "field", {}, {DO "{grantResearch}"}, {}});
  add(c, {"cmd-map-change", "mapchange", "bridge", {},
          {DO "{setTerrain}", DO "{applyChange}", DO "{revertChange}", DO "{setFog}", DO "{setVision}"},
          {{DO "{setTerrain}", R"({"setTerrain":"plain","area":{"x":4,"y":2}})"}}});
  add(c, {"cmd-storage", "parties-flow", "forest", {}, {DO "{storeItems}"}, {}});
  add(c, {"cmd-background", "scene-pres", "field", {}, {DO "{background}"}, {}});
}

void addEventExtraSkips(std::map<QString, QString>& s) {
  s.insert({
      {"*{tag}.tag = \"zz_no_such_id\"", "tags are free words: the loader takes any text"},
      {"maps:events[].when{place}.is = \"opened\"", "which words fit depends on the structure's kind (a door or chest is opened, a place visited or destroyed): tried with a hamlet"},
      {"maps:events[].when{place}.is = \"destroyed\"", "which words fit depends on the structure's kind (a door or chest is opened, a place visited or destroyed): tried with a hamlet"},
  });
}

}  // namespace conformance
