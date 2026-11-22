// Conformance tables, part 3: what a map file holds beyond its basics: a unit's behaviour, character, job and rank keys, a side's
// storage, the preparation screen, objectives, the legend and named changes. The map of a fixture the sample lands in is its
// FIRST map (alphabetical), and a unit key is tried on the map's first unit, so the mods below put the unit it needs there.
#include "conformance_contexts.h"

namespace conformance {

namespace {
#define KB "maps:units[].behaviour{behaviour block}"
// The srpg-doors fixture's first unit is a human-side lord; a computer-only key (behaviour, goesFor) needs it on the red side.
const Op kLordIsRed = {"maps/corridor.json", "units[0].side", "\"red\""};
// The jobs fixture: the first unit becomes a computer mage with a job (job keys belong to a unit that has a job), the old one a plain grunt.
const std::vector<Op> kJobUnit = {
    {"maps/duel.json", "units[0]", R"({"id":"shaman","side":"red","x":6,"y":3,"job":"mage"})"},
    {"maps/duel.json", "units[5]", R"({"type":"grunt","side":"red","x":6,"y":4,"id":"extra"})"}};
}  // namespace

void addMapContexts(std::vector<Context>& c) {
  // --- a unit's computer behaviour (ai-scripted: weights set "classic", computer sides, units b1..). A key that belongs to one
  // mode gets its own context: an override below a variant is also laid into the base value every key of the context starts from.
  // exits and the first unit's goesFor are there so that the modes loot and escape (which the loader refuses without them) are tried too.
  c.push_back({"map-behaviour", "ai-scripted",
               {{"data/units.json", "units[0].tags", R"(["flavour"])"}, {"maps/field.json", "exits", R"([{"x":0,"y":0}])"}, {"maps/field.json", "units[0].goesFor", R"(["chests"])"}},
               {"maps:units[].behaviour"},
               {{KB ".prefer{unit}.unit", "\"b2\""}, {KB ".ignores", R"(["b2"])"}, {KB ".retreat.to.terrain", R"(["fort"])"}}});
  c.push_back({"map-behaviour-within", "ai-scripted", {}, {KB ".within"}, {{KB ".within", "3"}}});
  c.push_back({"map-behaviour-measure", "ai-scripted", {}, {KB ".measure"}, {{KB, R"({"mode":"guardArea","within":3})"}}});
  c.push_back({"map-behaviour-seek", "ai-scripted", {}, {KB ".unit"}, {{KB ".unit", "\"b2\""}}});
  c.push_back({"map-behaviour-seek-xy", "ai-scripted", {}, {KB ".x", KB ".y"}, {{KB, R"({"mode":"seek","x":3,"y":3})"}}});
  c.push_back({"map-behaviour-seek-place", "srpg-doors", {kLordIsRed}, {KB ".place"}, {{KB ".place", "\"d1\""}}});
  c.push_back({"map-behaviour-patrol", "ai-scripted", {}, {KB ".route"}, {{KB ".route", R"([{"x":1,"y":1},{"x":2,"y":1}])"}}});
  // The map's exits (docs/AI.md "Looting and escaping").
  c.push_back({"map-exits", "ai-scripted", {}, {"maps:exits"}, {{"maps:exits", R"([{"x":0,"y":0},{"x":5,"y":5}])"}}});

  // --- the rest of a placed unit ---
  c.push_back({"map-unit-goes-for", "srpg-doors", {kLordIsRed}, {"maps:units[].goesFor"}, {}});
  c.push_back({"map-unit-character", "srpg-chars", {}, {"maps:units[].character"}, {}});
  c.push_back({"map-unit-xp", "unit-ranks", {}, {"maps:units[].xp"}, {}});
  c.push_back({"map-unit-merit", "merit-ranks", {}, {"maps:units[].merit"}, {}});
  c.push_back({"map-unit-ranks", "srpg-ranks", {}, {"maps:units[].ranks"}, {{"maps:units[].ranks", R"({"sword":"e"})"}}});
  c.push_back({"map-unit-job", "jobs", kJobUnit, {"maps:units[].job", "maps:units[].jobs", "maps:units[].learned"},
               {{"maps:units[].job", "\"mage\""},
                {"maps:units[].jobs", R"({"mage":{"jp":10}})"},
                {"maps:units[].jobs.*.learned", R"(["mend"])"},
                {"maps:units[].learned", R"(["mend"])"}}});
  // What a unit equips must have been learned in another job than the current one.
  std::vector<Op> equipMods = kJobUnit;
  equipMods.push_back({"maps/duel.json", "units[0].jobs", R"({"squire":{"learned":["guard"]}})"});
  c.push_back({"map-unit-equipped", "jobs", equipMods, {"maps:units[].equipped"}, {{"maps:units[].equipped", R"(["guard"])"}}});
  c.push_back({"map-unit-skills", "srpg-skills", {}, {"maps:units[].skills"}, {}});
  c.push_back({"map-unit-difficulty", "ai-scripted",
               {{"game.json", "difficulties", R"({"default":"normal","list":[{"id":"normal"},{"id":"hard"}]})"}},
               {"maps:units[].difficulty", "maps:units[].difficultyShift"}, {}});

  // --- the parts of a map that are not units ---
  c.push_back({"map-side-storage", "parties-flow", {}, {"maps:sides[].storage"}, {}});
  c.push_back({"map-objectives", "srpg-staves2", {{"data/units.json", "units[0].tags", R"(["flavour"])"}}, {"maps:objectives"}, {}});
  c.push_back({"map-changes", "mapchange", {}, {"maps:changes"},
               {{"maps:changes", R"({"bridge":{"cells":[{"x":1,"y":1,"terrain":"plain"}]}})"}}});
  c.push_back({"map-changes-structures", "srpg-doors", {}, {"maps:changes.*.structures"},
               {{"maps:changes", R"({"c":{"structures":[{"id":"d1","kind":"cell_door_open"}]}})"},
                {"maps:changes.*.structures", R"([{"id":"d1","kind":"cell_door_open"}])"},
                {"maps:changes.*.structures[].kind", "\"cell_door_open\""}}});
  // prep: hilltop's own slots stay (the loader wants no duplicate slot and a limit within them); fixed names a unit of the map.
  const char* kSlots = R"([{"x":0,"y":0},{"x":0,"y":1},{"x":0,"y":2},{"x":0,"y":3}])";
  c.push_back({"map-prep", "srpg-prep", {{"maps/hilltop.json", "units[0].id", "\"r1\""}}, {"maps:prep"},
               {{"maps:prep.slots", kSlots},
                {"maps:prep.slots[].x", "1"},
                {"maps:prep.slots[].y", "4"},
                {"maps:prep.required", R"(["hero"])"},
                {"maps:prep.required[]{character id}", "\"hero\""},
                {"maps:prep.required[]{character in a slot}", R"({"character":"hero","slot":1})"},
                {"maps:prep.required[]{character in a slot}.character", "\"hero\""},
                {"maps:prep.forbidden", R"(["hero2"])"},
                {"maps:prep.fixed", R"(["r1"])"}}});
  c.push_back({"map-legend", "ai-scripted", {}, {"maps:legend"}, {{"maps:legend", R"({".":"plain","f":"forest","F":"fort"})"}}});
}

void addMapExtraSkips(std::map<QString, QString>& s) {
  const char* kTag = "tags are free words: the loader takes any text (and a unit type's tags are not checked against the map)";
  for (const char* k : {"maps:units[].behaviour{behaviour block}.prefer{tag}.tag", "maps:objectives.win[]{defeated}.defeated{tag}.tag",
                        "maps:objectives.win[]{reach}.by{tag}.tag", "maps:objectives.lose[]{defeated}.defeated{tag}.tag",
                        "maps:objectives.lose[]{reach}.by{tag}.tag"})
    s.insert({QString(k) + " = \"zz_no_such_id\"", kTag});
  const char* kShape = "a prep character is checked only for the shape of its id (letters, digits, _ and -), the roster may not know it yet";
  for (const char* k : {"maps:prep.required[]{character id}", "maps:prep.required[]{character in a slot}.character"})
    s.insert({QString(k) + " = \"zz_no_such_id\"", kShape});
  s.insert({"maps:prep.forbidden = [\"zz_no_such_id\"]", kShape});
  const char* kMode = "each of these modes needs its own keys (a target, a route, an area): tried with them under the keys of that mode";
  for (const char* w : {"guardArea", "seek", "patrol"}) s.insert({QString(KB ".mode = \"") + w + "\"", kMode});
}


}  // namespace conformance
