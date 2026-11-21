// Conformance tables, part 1: game.json, the core data files (weapons, units, items, terrain) and the mechanics of
// milestone 2 (action points, height, facing, speed order, research, commanders, factions, ranks, jobs).
#include "conformance_contexts.h"

namespace conformance {

namespace {
// Jobs: characters and maps of the fixture name the learned skills of the existing jobs, so those jobs stay as they are.
// A new job "tester" (the last row, where the table tests place their samples) and a new skill and ability for it to
// teach are added instead (one skill or ability is taught by one job only).
const std::vector<Op> kJobsMods = {
    {"data/skills.json", "skills[+]", R"({"id":"extra_skill","name":"Extra","blocks":[{"when":"always","do":[{"stats":{"def":2}}]}]})"},
    {"data/abilities.json", "abilities[+]",
     R"({"id":"extra_ability","name":"Extra","target":"ally","range":[1,2],"effect":{"heal":10},"cost":"action"})"},
    {"data/jobs.json", "jobs[+]", R"({"id":"tester","name":"Tester","unitType":"squire"})"}};
}  // namespace

void addCoreContexts(std::vector<Context>& c) {
  c.push_back({"damage-types", "damage-types", {},
               {"game:damageTypes", "game:resistRange", "weapons:damageType", "units:resist"},
               {{"game:damageTypes", R"([{"id":"kinetic"},{"id":"emf"},{"id":"fire"}])"}}});
  c.push_back({"action-points", "action-points", {},
               {"game:rules.actionPoints", "game:resources", "weapons:apCost", "weapons:area.apExtra", "units:ap"},
               {{"weapons:area", R"({"shape":"radius","radius":1})"}, {"game:resources", R"([{"id":"ap"},{"id":"gold"}])"}}});
  c.push_back({"height", "height-rules", {},
               {"game:rules.height", "weapons:heightRange", "weapons:area.vertical", "units:jump"},
               {{"weapons:area", R"({"shape":"radius","radius":1})"}}});
  c.push_back({"facing", "facing", {}, {"game:rules.facing", "maps:units[].facing"}, {{"maps:units[].facing", "\"north\""}}});
  c.push_back({"speed", "speed-order", {},
               {"game:rules.turnOrder", "weapons:chargeSpeed", "weapons:chargeTargets", "weapons:interruptedBy"},
               {{"game:rules.turnOrder", R"({"mode":"speed","stat":"spd"})"}, {"game:rules.turnOrder{speed, custom}.mode", "\"speed\""}}});
  // The plain word "speed" needs a stat called speed; the fixture's clock stat is spd.
  c.push_back({"speed-default-stat", "speed-order", {{"game.json", "stats[+]", R"({"id":"speed","name":"Speed"})"}},
               {"game:rules.turnOrder{speed, defaults}"}, {}});
  c.push_back({"research", "research-branches", {}, {"research:", "game:rules.research"},
               {{"research:requires", R"(["tactics"])"},
                {"research:grants", R"({"skills":["bulwark"]})"},
                {"research:grants.skills", R"(["bulwark"])"},
                {"research:bonus", R"({"stats":{"str":1}})"}}});
  // Commanders before factions: a faction's "commanders" list needs the commanders file.
  c.push_back({"commanders", "commanders", {}, {"commanders:", "factions:commanders", "maps:sides[].commander"},
               {{"commanders:passive", R"(["bulwark"])"},
                {"commanders:meter.max", "200"},
                {"commanders:powers", R"([{"id":"p","cost":1,"modifiers":["frenzy"],"effects":[{"heal":5}]}])"},
                {"commanders:powers[].modifiers", R"(["frenzy"])"},
                {"commanders:powers[].effects", R"([{"heal":5}])"}}});
  c.push_back({"factions", "factions", {}, {"factions:", "maps:sides[].faction"},
               {{"factions:modifiers", R"(["bulwark"])"},
                {"factions:ai", R"({"weights":{"dmg":3}})"},
                {"factions:ai.weights", R"({"dmg":3})"}}});
  c.push_back({"merit-ranks", "merit-ranks", {}, {"game:rules.ranks"},
               {{"game:rules.ranks.levels[].badge", "\"I\""},
                {"game:rules.ranks.levels", R"([{"at":1000,"name":"Top"}])"},
                {"game:rules.ranks", R"({"levels":[{"at":1000,"name":"Top"}]})"}}});
  c.push_back({"rank-table", "unit-ranks", {}, {"game:rules.progression.levels"},
               {{"game:rules.progression.levels[].badge", "\"I\""}, {"game:rules.progression.levels", R"([{"at":1000,"name":"Top"}])"}}});
  c.push_back({"progression", "area-fire", {}, {"game:rules.progression", "units:maxLevel"},
               {{"game:rules.progression.xpHit", "\"10\""}, {"game:rules.progression.xpKill", "\"20\""}}});
  c.push_back({"jobs-learn-skill", "jobs", kJobsMods, {"jobs:learn[].skill"},
               {{"jobs:learn", R"([{"skill":"extra_skill","cost":1,"slot":"support"}])"}, {"jobs:learn[].skill", "\"extra_skill\""}}});
  c.push_back({"jobs-learn-ability", "jobs", kJobsMods, {"jobs:learn[].ability"},
               {{"jobs:learn", R"([{"ability":"extra_ability","cost":1,"slot":"action"}])"}, {"jobs:learn[].ability", "\"extra_ability\""}}});
  c.push_back({"jobs", "jobs", kJobsMods, {"jobs:", "game:rules.jobs"},
               {{"jobs:learn", R"([{"skill":"extra_skill","cost":1,"slot":"support"}])"},
                {"game:rules.jobs", R"({"jp":{"perAction":1},"jpLevels":[20,60,120]})"}}});
  c.push_back({"characters", "damage-types", {}, {"weapons:wieldedBy"}, {}});
  c.push_back({"area-line", "ai-scripted", {}, {"weapons:area.length"}, {{"weapons:area", R"({"shape":"line","length":2})"}}});
  c.push_back({"core", "ai-scripted", {},
               {"terrain:", "items:", "units:", "weapons:", "maps:", "game:rules.counterAttacks", "game:rules.defeat"},
               {{"weapons:area", R"({"shape":"radius","radius":2})"},
                {"weapons:area.shape", "\"radius\""},
                {"maps:sides", R"([{"id":"blue","controller":"ai"},{"id":"red","controller":"ai"}])"},
                {"maps:sides[].controller", "\"ai\""},
                {"maps:units", R"([{"type":"soldier","side":"blue","x":0,"y":0}])"},
                {"maps:units[].side", "\"blue\""},
                {"maps:units[].x", "0"},
                {"maps:units[].y", "0"}}});
}

void addCoreExtraSkips(std::map<QString, QString>& s) {
  s.insert({
      {"commanders:meter.perDamageTaken = 0", "the meter must fill by at least one of its three rates (cross-key rule)"},
      {"commanders:powers[].cost = 100000", "a power costs at most the meter's own max (cross-key rule)"},
      {"game:rules.progression.levels[].at = 1000000", "ranks must rise; the second rank cannot start at the same value (cross-key rule)"},
      {"game:rules.ranks.levels[].at = 1000000", "ranks must rise; the second rank cannot start at the same value (cross-key rule)"},
      {"game:rules.jobs.slots.support = 0", "the fixture's characters equip support abilities, so zero slots is refused (cross-file rule)"},
      {"items:kind = \"weapon\"", "heal / raise / bonus belong to one kind (cross-key rule)"},
      {"items:kind = \"accessory\"", "heal / raise / bonus belong to one kind (cross-key rule)"},
      {"items:kind = \"staff\"", "heal / raise / bonus belong to one kind (cross-key rule)"},
      {"jobs:requires[].level = 20", "a required level cannot exceed the required job's level table (cross-key rule)"},
      {"maps:sides[].controller = \"human\"", "a unit with a computer behaviour cannot stand on a person's side (cross-key rule)"},
  });
}

}  // namespace conformance
