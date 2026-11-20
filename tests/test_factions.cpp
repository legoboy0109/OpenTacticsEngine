// Factions (docs/ECONOMY.md "Factions"). Fixture tests/fixtures/factions: three factions (order: soldier archer knight, barracks fort,
// drill forging, +50 gold, bulwark for infantry; horde: soldier golem, barracks keep, arcana, fleet for everybody, its own attack
// weights; free: no restriction at all), a human "blue" playing order and a computer "red" playing horde on map "field"; "pick" leaves both
// to "choose"; "duel" is two computer sides.
#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "core/battle/save_faction.h"
#include "json_edit.h"
using namespace jsonedit;
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::saysIn;
using skilltest::World;

namespace {

const char* kFix = "factions";

std::string fixtureDir() { return ote_test::sourceDir() + "/tests/fixtures/factions"; }

int unitType(World& w, const char* id) { return w.db().findUnitType(id); }
int tech(World& w, const char* id) { return w.db().findTechnology(id); }
int kind(World& w, const char* id) { return w.db().findStructure(id); }

// Why `side` (the active one) cannot build `type` on the barracks at x,y ("" = it can).
std::string buildRefusal(World& w, int x, int y, const char* type) {
  std::string why;
  return w.b().canBuild(w.at(x, y), unitType(w, type), &why) ? "" : why;
}

std::string researchWhy(World& w, const char* id) {
  std::vector<ResearchOption> opts;
  w.b().researchOptions(w.b().activeSide, opts);
  for (const ResearchOption& o : opts)
    if (o.tech == tech(w, id)) return o.ok ? "" : o.why;
  return "(not listed)";
}

std::string upgradeWhy(World& w, int structure, const char* to) {
  std::vector<UpgradeOption> opts;
  w.b().upgradeOptions(structure, opts);
  for (const UpgradeOption& o : opts)
    if (o.to == kind(w, to)) return o.ok ? "" : o.why;
  return "(not listed)";
}

std::string idsOf(const Battle& b, const std::vector<int>& types) {
  std::string out;
  for (int t : types) out += (out.empty() ? "" : " ") + b.db->unitTypes[(size_t)t].id;
  return out;
}

std::string optionsAt(World& w, int x, int y, bool all) {
  std::vector<int> opts;
  w.b().buildOptions(w.at(x, y), opts, all);
  return idsOf(w.b(), opts);
}

// A battle built from a map as the player would after the faction screen (picks written into the map).
struct Built {
  Database db;
  MapDef map;
  Formulas formulas;
  Battle battle;
  bool build(const std::string& dir, const char* mapId, unsigned seed, int pick0 = -2) {
    std::string err;
    if (!loadGame(dir, db, err) || !loadMap(db, mapId, map, err) || !formulas.init(db, err)) {
      std::printf("  %s\n", err.c_str());
      return false;
    }
    if (pick0 >= -1) {
      map.sides[0].faction = pick0;
      map.sides[0].factionChoose = false;
    }
    return battle.init(db, map, formulas, seed, err);
  }
  std::string factionIds() const {
    std::string out;
    for (const SideState& s : battle.sides) out += (out.empty() ? "" : " ") + (s.faction < 0 ? std::string("-") : db.factions[(size_t)s.faction].id);
    return out;
  }
};

}  // namespace

// ---- loader ----

TEST(factions_fixture_loads_with_every_key) {
  Copy c("fac_load", kFix);
  CHECK_STR(c.gameError(), "");
  World w("field", fixtureDir());
  const Database& db = w.db();
  CHECK_EQ((int)db.factions.size(), 3);
  const FactionDef& order = db.factions[(size_t)db.findFaction("order")];
  CHECK_STR(order.name, "Order");
  CHECK(order.limitUnits && order.limitStructures && order.limitTechs);
  CHECK_EQ((int)order.units.size(), 3);
  CHECK_EQ((int)order.structures.size(), 2);
  CHECK_EQ((int)order.techs.size(), 2);
  CHECK_EQ((int)order.start.size(), 1);
  CHECK_EQ(order.start[0].amount, 50);
  CHECK_EQ((int)order.modifiers.size(), 1);
  CHECK_STR(order.modifiers[0].filterTag, "infantry");
  CHECK_EQ((int)order.tags.size(), 2);
  const FactionDef& horde = db.factions[(size_t)db.findFaction("horde")];
  CHECK(horde.hasWeights);
  CHECK_EQ(horde.weights.dmg, 2);
  CHECK_EQ(horde.weights.counter, 1);
  CHECK_EQ(horde.weights.killValue, 50);  // a key left out keeps the default
  const FactionDef& free = db.factions[(size_t)db.findFaction("free")];
  CHECK(!free.limitUnits && !free.limitStructures && !free.limitTechs && !free.hasWeights);
  CHECK_EQ(w.b().sides[0].faction, db.findFaction("order"));
  CHECK_EQ(w.b().sides[1].faction, db.findFaction("horde"));
}

TEST(a_game_without_factions_loads_and_its_sides_have_none) {
  Copy c("fac_none", kFix);
  c.drop("data/factions.json");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.factions.empty());
  World w("field", ote_test::sourceDir() + "/tests/fixtures/research-branches");  // a game that never heard of factions
  CHECK_EQ(w.b().sides[0].faction, -1);
  CHECK(w.b().factionOf(0) == nullptr);
  CHECK_STR(w.b().factionBlocksUnit(0, 0), "");
}

TEST(loader_refusals_name_the_file_the_entry_and_the_key) {
  struct Case {
    const char* what;
    const char* from;
    const char* to;
    const char* needle;
  };
  const Case cases[] = {
      {"an unknown unit", "\"units\": [\"soldier\", \"archer\", \"knight\"]", "\"units\": [\"soldier\", \"dragon\"]",
       "factions.json: faction \"order\": \"units\" names unknown unit type \"dragon\""},
      {"an unknown structure", "\"structures\": [\"barracks\", \"fort\"]", "\"structures\": [\"barracks\", \"castle\"]",
       "faction \"order\": \"structures\" names unknown structure \"castle\""},
      {"an unknown technology", "\"techs\": [\"drill\", \"forging\"]", "\"techs\": [\"drill\", \"magic\"]",
       "faction \"order\": \"techs\" names unknown technology \"magic\""},
      {"an unknown skill", "\"modifiers\": [ { \"skill\": \"bulwark\"", "\"modifiers\": [ { \"skill\": \"nope\"",
       "faction \"order\": \"modifiers\": names unknown skill \"nope\""},
      {"an unknown resource", "\"start\": { \"resources\": { \"gold\": 50 } }", "\"start\": { \"resources\": { \"mana\": 50 } }",
       "faction \"order\": \"start\": \"resources\" names unknown resource \"mana\""},
      {"a negative start", "\"gold\": 50", "\"gold\": -5", "\"start\": \"resources\".\"gold\" must be 0 or more"},
      {"a duplicate id", "\"id\": \"horde\"", "\"id\": \"order\"", "faction \"order\": duplicate id"},
      {"an unknown key", "\"tags\": [\"monster\"]", "\"tag\": [\"monster\"]", "faction \"horde\": unknown key \"tag\""},
      {"an unknown key of start", "\"start\": {", "\"start\": { \"gold\": 1,", "\"start\": unknown key \"gold\""},
      {"an unknown weight key", "\"counter\": 1", "\"cunning\": 1", "\"ai\": \"weights\": unknown key \"cunning\""},
      {"an unknown ai key", "\"ai\": { \"weights\"", "\"ai\": { \"mood\": 1, \"weights\"", "faction \"horde\": \"ai\": unknown key \"mood\""},
      {"a unit listed twice", "\"units\": [\"soldier\", \"golem\"]", "\"units\": [\"golem\", \"golem\"]", "\"units\" lists \"golem\" twice"},
      {"a skill twice", "\"modifiers\": [\"fleet\"]", "\"modifiers\": [\"fleet\", \"fleet\"]", "\"modifiers\" lists \"fleet\" twice"},
      {"a filter that is not a tag", "\"filter\": { \"tag\": \"infantry\" }", "\"filter\": { \"tag\": \"flying\" }", "is not a tag of any unit type"},
  };
  for (const Case& k : cases) {
    Copy c("fac_bad", kFix);
    c.swap("data/factions.json", k.from, k.to);
    std::printf("  case: %s\n", k.what);
    CHECK(saysIn(c.gameError(), k.needle));
  }
  Copy c("fac_empty", kFix);
  c.put("data/factions.json", "{ \"factions\": [] }");
  CHECK(saysIn(c.gameError(), "\"factions\" is empty"));
}

TEST(a_side_naming_a_faction_the_game_lacks_is_refused_and_choose_needs_factions) {
  Copy c("fac_side", kFix);
  c.swap("maps/field.json", "\"faction\": \"order\"", "\"faction\": \"elves\"");
  CHECK(saysIn(c.mapError("field"), "maps/field.json: side \"blue\": \"faction\" names unknown faction \"elves\""));
  Copy none("fac_side_none", kFix);
  none.drop("data/factions.json");
  CHECK(saysIn(none.mapError("field"), "side \"blue\": \"faction\" needs data/factions.json"));
  none.swap("maps/field.json", "\"faction\": \"order\"", "\"faction\": \"choose\"");
  CHECK(saysIn(none.mapError("field"), "side \"blue\": \"faction\" needs data/factions.json"));
  Copy bad("fac_side_type", kFix);
  bad.swap("maps/field.json", "\"faction\": \"order\"", "\"faction\": 3");
  CHECK(saysIn(bad.mapError("field"), "\"faction\" must be a string"));
}

// ---- what a faction filters ----

TEST(production_is_filtered_by_the_side_faction_and_says_why) {
  World w("field", fixtureDir());
  // Blue plays order: soldier, archer and knight; not the golem.
  CHECK_STR(optionsAt(w, 0, 0, false), "soldier archer knight");
  CHECK_STR(optionsAt(w, 0, 0, true), "soldier archer knight golem");  // a menu asks for all and greys the refused
  CHECK_STR(buildRefusal(w, 0, 0, "knight"), "");
  CHECK_STR(buildRefusal(w, 0, 0, "golem"), "not available to Order");
  Action golem = Action::build(w.at(0, 0), unitType(w, "golem"));
  CHECK(saysIn(w.refusal(golem), "not available to Order"));
  CHECK(!w.apply(golem));
  CHECK(w.apply(Action::build(w.at(0, 0), unitType(w, "knight"))));
  CHECK(w.apply(Action::endTurn()));
  // Red plays horde: soldier and golem; no knight, no archer.
  CHECK_STR(optionsAt(w, 7, 2, false), "soldier golem");
  CHECK_STR(buildRefusal(w, 7, 2, "knight"), "not available to Horde");
}

TEST(units_already_on_the_map_stay_whatever_the_faction) {
  World w("field", fixtureDir());
  CHECK(w.b().units[(size_t)w.id("gol")].alive);  // a golem placed for blue, which may not build one
  for (int i = 0; i < 3; ++i) CHECK(w.apply(Action::endTurn()));
  CHECK(w.b().units[(size_t)w.id("gol")].alive);
  CHECK_EQ(w.b().units[(size_t)w.id("gol")].side, 0);
}

TEST(research_is_filtered_by_the_side_faction) {
  World w("field", fixtureDir());
  CHECK_STR(researchWhy(w, "drill"), "");
  CHECK_STR(researchWhy(w, "arcana"), "not available to Order");
  CHECK(saysIn(w.refusal(Action::research(tech(w, "arcana"))), "not available to Order"));
  CHECK(!w.apply(Action::research(tech(w, "arcana"))));
  CHECK(w.apply(Action::research(tech(w, "drill"))));
  CHECK(w.apply(Action::endTurn()));
  CHECK_STR(researchWhy(w, "drill"), "not available to Horde");
  CHECK_STR(researchWhy(w, "forging"), "not available to Horde");
  CHECK_STR(researchWhy(w, "arcana"), "");
}

TEST(structure_upgrades_are_filtered_by_the_side_faction) {
  World w("field", fixtureDir());
  const int barracks = w.b().structureAt[w.at(0, 0)];
  CHECK_STR(upgradeWhy(w, barracks, "fort"), "");
  CHECK_STR(upgradeWhy(w, barracks, "keep"), "not available to Order");
  CHECK(!w.apply(Action::upgrade(barracks, kind(w, "keep"))));
  CHECK(w.apply(Action::upgrade(barracks, kind(w, "fort"))));
  CHECK(w.apply(Action::endTurn()));
  const int red = w.b().structureAt[w.at(7, 2)];
  CHECK_STR(upgradeWhy(w, red, "fort"), "not available to Horde");
  CHECK_STR(upgradeWhy(w, red, "keep"), "");
}

TEST(a_faction_without_a_list_restricts_nothing_and_an_empty_list_allows_nothing) {
  Copy c("fac_free", kFix);
  c.swap("maps/field.json", "\"faction\": \"order\"", "\"faction\": \"free\"");
  World w("field", c.dir);
  CHECK_STR(optionsAt(w, 0, 0, false), "soldier archer knight golem");
  CHECK_STR(researchWhy(w, "arcana"), "");
  CHECK_STR(upgradeWhy(w, w.b().structureAt[w.at(0, 0)], "keep"), "");
  Copy none("fac_nothing", kFix);
  none.swap("data/factions.json", "\"units\": [\"soldier\", \"golem\"]", "\"units\": []");
  World v("field", none.dir);
  CHECK(w.apply(Action::endTurn()));
  CHECK(v.apply(Action::endTurn()));
  CHECK_STR(optionsAt(v, 7, 2, false), "");
  CHECK_STR(optionsAt(v, 7, 2, true), "soldier archer knight golem");
}

TEST(start_resources_are_added_and_the_side_wide_skills_apply_to_the_matching_units) {
  World w("field", fixtureDir());
  CHECK_EQ(w.b().sides[0].resources[0], 156);  // the map's 100, order's 50 and the barracks' 6 of income at the start of the turn
  CHECK_EQ(w.b().sides[1].resources[0], 100);  // horde adds nothing
  // Order: bulwark for infantry (the soldier) only. Horde: fleet for everybody.
  CHECK_STR(w.skillIds(w.unit("sol")), "bulwark");
  CHECK_STR(w.skillIds(w.unit("kni")), "");
  CHECK_STR(w.skillIds(w.unit("foe")), "fleet");
  CHECK_EQ(w.stat(w.unit("foe"), "mov"), 4);
  CHECK_EQ(w.stat(w.unit("sol"), "def"), 4);
  // The faction's skills come first; a technology's grants would follow.
  CHECK_EQ(w.b().sides[0].modifiers[0].faction, w.db().findFaction("order"));
}

// ---- "choose" ----

TEST(choose_is_settled_by_the_picks_for_a_person_and_the_seed_for_a_computer_side) {
  Built a;
  CHECK(a.build(fixtureDir(), "pick", 1, 1));  // the person picked horde
  CHECK_STR(a.db.factions[(size_t)a.battle.sides[0].faction].id, "horde");
  // Nobody picked for the person's side: the first faction, in --autoplay and wherever there is no screen.
  Built b1;
  CHECK(b1.build(fixtureDir(), "pick", 1));
  CHECK_EQ(b1.battle.sides[0].faction, 0);
  // The computer side is the seed's, the same every time and not always the same faction.
  std::set<int> seen;
  for (unsigned seed = 1; seed <= 24; ++seed) {
    Built x, y;
    CHECK(x.build(fixtureDir(), "pick", seed));
    CHECK(y.build(fixtureDir(), "pick", seed));
    CHECK_STR(x.factionIds(), y.factionIds());
    CHECK(x.battle.sides[1].faction >= 0);
    seen.insert(x.battle.sides[1].faction);
  }
  CHECK_EQ((int)seen.size(), 3);  // all three factions turn up over 24 seeds
}

TEST(the_seed_pick_does_not_touch_the_battle_random_stream) {
  Built a, b;
  CHECK(a.build(fixtureDir(), "pick", 5));
  CHECK(b.build(fixtureDir(), "pick", 5));
  CHECK_EQ(a.battle.rng.next(), b.battle.rng.next());
  Built plain;  // the same seed on a map that names its factions: the stream is the same one
  CHECK(plain.build(fixtureDir(), "field", 5));
  Built again;
  CHECK(again.build(fixtureDir(), "pick", 5));
  CHECK_EQ(plain.battle.rng.next(), again.battle.rng.next());
}

// ---- saves ----

TEST(the_faction_is_saved_only_when_set_and_round_trips) {
  World w("field", fixtureDir());
  Json j;
  saveBattleState(w.b(), j);
  CHECK_STR(j["sides"][size_t(0)]["faction"].asString(), "order");
  CHECK_STR(j["sides"][size_t(1)]["faction"].asString(), "horde");
  CHECK(!j["sides"][size_t(0)].has("modifiers"));  // a faction's skills are rebuilt, not saved
  Json text;
  std::string err;
  CHECK(Json::parse(j.dump(0), text, err));
  Battle r;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err));
  r.sides[0].faction = -1;  // restoring brings it back from the save
  r.sides[0].modifiers.clear();
  CHECK(restoreBattleState(r, text, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK_EQ(r.sides[0].faction, w.b().sides[0].faction);
  CHECK(r.hasSkill(r.units[(size_t)w.id("sol")], w.skill("bulwark")));
  CHECK(!r.hasSkill(r.units[(size_t)w.id("kni")], w.skill("bulwark")));
  // A game without factions writes no key at all.
  World p("field", ote_test::sourceDir() + "/tests/fixtures/research-branches");
  Json k;
  saveBattleState(p.b(), k);
  CHECK(!k["sides"][size_t(0)].has("faction"));
}

TEST(the_faction_skills_stay_in_front_of_a_technologys_after_a_save) {
  Copy c("fac_order", kFix);
  c.swap("data/research.json", "\"turns\": 1, \"bonus\"", "\"turns\": 1, \"grants\": { \"skills\": [\"fleet\"] }, \"bonus\"");  // drill grants fleet too
  World w("field", c.dir);
  CHECK_STR(w.skillIds(w.unit("sol")), "bulwark");
  CHECK(w.apply(Action::research(tech(w, "drill"))));
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));  // blue's turn again: drill is finished
  CHECK(w.b().hasTechnology(0, tech(w, "drill")));
  CHECK_STR(w.skillIds(w.unit("sol")), "bulwark fleet");  // the faction's skill first, then the technology's
  Json j;
  saveBattleState(w.b(), j);
  CHECK_EQ(j["sides"][size_t(0)]["modifiers"].size(), size_t(1));  // only the technology's is written
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err));
  CHECK(restoreBattleState(r, j, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  std::vector<int> list;
  r.skillsOf(r.units[(size_t)w.id("sol")], -1, list);
  CHECK_EQ((int)list.size(), 2);
  CHECK_EQ(r.sides[0].modifiers[0].faction, w.db().findFaction("order"));  // in front, as in the live battle
  CHECK_EQ(r.sides[0].modifiers[1].tech, tech(w, "drill"));
}

TEST(the_second_side_keeps_its_faction_through_a_save) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::endTurn()));
  Json j;
  saveBattleState(w.b(), j);
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err));
  CHECK(restoreBattleState(r, j, err));
  CHECK_EQ(r.activeSide, 1);
  CHECK_EQ(r.sides[1].faction, w.db().findFaction("horde"));
  CHECK_STR(w.skillIds(w.unit("foe")), "fleet");
}

TEST(damaged_faction_saves_are_refused) {
  World w("field", fixtureDir());
  Json good;
  saveBattleState(w.b(), good);
  auto refusal = [&](const Json& bad) {
    Battle r;
    std::string err;
    CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err));
    return restoreBattleState(r, bad, err) ? std::string() : err;
  };
  CHECK_STR(refusal(good), "");
  const std::vector<std::string> at = {"sides", "#0", "faction"};
  CHECK(saysIn(refusal(withValue(good, at, str("elves"))), "names unknown faction \"elves\""));
  CHECK(saysIn(refusal(withValue(good, at, num(1))), "not a faction id"));
  // A save of a game that has no factions cannot carry one.
  World p("field", ote_test::sourceDir() + "/tests/fixtures/research-branches");
  Json other;
  saveBattleState(p.b(), other);
  Battle r;
  std::string err;
  CHECK(r.init(p.rig.db, p.rig.map, p.rig.formulas, 7, err));
  CHECK(!restoreBattleState(r, withValue(other, at, str("order")), err));
  CHECK(saysIn(err, "the game has no factions"));
}

// ---- the computer opponent ----

TEST(the_computer_never_picks_what_its_faction_may_not_have) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::endTurn()));  // red (horde)
  int pick = -1;
  CHECK(ai::chooseResearch(w.b(), pick));
  CHECK_EQ(pick, tech(w, "arcana"));  // drill (cheaper) is not horde's
  w.b().sides[1].researched[(size_t)tech(w, "arcana")] = 1;
  CHECK(!ai::chooseResearch(w.b(), pick));  // nothing left it may research: no stall, no pick
  int structure = -1, to = -1;
  w.b().sides[1].resources[0] = 1000;
  CHECK(ai::chooseUpgrade(w.b(), structure, to));
  CHECK_EQ(to, kind(w, "keep"));  // never the fort
  int cell = -1, type = -1;
  CHECK(ai::chooseBuild(w.b(), cell, type));
  CHECK(type == unitType(w, "soldier") || type == unitType(w, "golem"));
}

TEST(a_faction_that_can_build_nothing_does_not_stall_the_computer) {
  Copy c("fac_stall", kFix);
  c.swap("data/factions.json", "\"units\": [\"soldier\", \"golem\"]", "\"units\": []");
  World w("field", c.dir);
  CHECK(w.apply(Action::endTurn()));
  std::vector<Event> ev;
  const size_t before = w.b().units.size();
  int steps = 0;
  while (w.b().activeSide == 1 && !w.b().over && steps++ < 50) aiTakeStep(w.b(), ev);
  CHECK(steps < 50);
  CHECK_EQ(w.b().activeSide, 0);  // its turn ended
  CHECK_EQ(w.b().units.size(), before);  // and it built nothing
}

TEST(the_faction_weights_reach_the_computer_units_that_have_none_of_their_own) {
  World w("field", fixtureDir());
  CHECK(w.b().factionWeights(0) == nullptr);  // order brings none
  const AiWeightSet* set = w.b().factionWeights(1);
  CHECK(set != nullptr);
  if (set) {
    CHECK_EQ(set->dmg, 2);
    CHECK_EQ(set->counter, 1);
  }
  // A unit whose behaviour names a set of rules.ai.weights keeps its own.
  CHECK_EQ(w.unit("foe").behaviour.weightSet, -1);
}

TEST(two_computer_factions_play_a_whole_game_and_it_finishes) {
  for (unsigned seed : {1u, 2u, 3u}) {
    Built g;
    CHECK(g.build(fixtureDir(), "duel", seed));
    std::vector<Event> ev;
    g.battle.start(ev);
    for (int i = 0; i < 400 && !g.battle.over; ++i) aiTakeStep(g.battle, ev);
    std::printf("  seed %u: over=%d round %d\n", seed, g.battle.over ? 1 : 0, g.battle.round);
    CHECK(g.battle.over);
    // Whatever each side built, it only built what its faction allows.
    for (const Unit& u : g.battle.units) {
      if (!u.storyId.empty()) continue;  // the two leaders the map placed
      const FactionDef* f = g.battle.factionOf(u.side);
      CHECK(f != nullptr);
      if (f) CHECK(FactionDef::allows(f->limitUnits, f->units, u.type));
    }
  }
}
