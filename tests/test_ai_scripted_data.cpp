// Computer opponent, scripted-style options (docs/AI.md "Scripted-style computer players"): the loader and its refusals, the story commands,
// reinforcements, and saves.
#include "ai_scripted_fixture.h"

using namespace ote;
using namespace scripted;

namespace {

std::string game(const char* aiRules) {
  return std::string(aiRules);
}

// The text the loader says about a game with these rules.ai and one red unit with this behaviour block ("" = loads).
std::string badAi(const char* name, const char* aiRules) {
  Options o;
  o.aiRules = game(aiRules);
  return SField::error(name, blue("lord", 0, 2) + ", " + red("r", 8, 2), o);
}
std::string badBeh(const char* name, const std::string& behaviour, const char* aiRules = R"({ "weights": { "w": { "dmg": 2 } } })") {
  Options o;
  o.aiRules = aiRules;
  return SField::error(name, blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(behaviour)), o);
}

}  // namespace

TEST(ai_scripted_data_every_key_loads) {
  CHECK_STR(badAi("aisd_ok1", R"({ "turnOrder": "order" })"), "");
  CHECK_STR(badAi("aisd_ok2", R"({ "turnOrder": "best", "weights": { "w": { "dmg": 3, "left": 1, "friends": 2, "class": 4, "turn": 5, "counter": 6, "danger": 7, "wounds": 8,
      "killValue": 60, "dmgCap": 30, "leftBase": 25, "friendsCap": 9, "counterCap": 35, "noCounterBonus": 12, "dangerDiv": 4, "dangerCap": 15, "attackAtZero": true,
      "priority": { "archer": 5, "soldier": -3 } } } })"), "");
  CHECK_STR(badBeh("aisd_ok3", R"({ "order": -7, "weights": "w", "caution": "none", "idleTurns": 3, "ignores": ["lord"] })"), "");
  CHECK_STR(badBeh("aisd_ok4", R"({ "retreat": { "below": 30, "until": 80, "to": { "terrain": ["fort"] }, "useItems": false } })"), "");
  CHECK_STR(badBeh("aisd_ok5", R"({ "retreat": { "below": 30 } })"), "");
  CHECK_STR(badBeh("aisd_ok6", R"({ "mode": "patrol", "route": [ { "x": 3, "y": 1 }, { "x": 9, "y": 1 } ] })"), "");
  CHECK_STR(badBeh("aisd_ok7", R"({ "mode": "guardArea", "within": 3, "measure": "moveCost" })"), "");
  CHECK_STR(badBeh("aisd_ok8", R"({ "mode": "guardArea", "within": 3, "measure": "steps" })"), "");
}

TEST(ai_scripted_data_rules_ai_refusals_say_what_is_wrong) {
  CHECK(says(badAi("aisd_a1", R"({ "turnOrder": "fast" })"), "\"turnOrder\" must be \"best\""));
  CHECK(says(badAi("aisd_a2", R"({ "speed": 1 })"), "unknown key \"speed\""));
  CHECK(says(badAi("aisd_a3", R"({ "weights": [] })"), "\"weights\" must be an object of named sets"));
  CHECK(says(badAi("aisd_a4", R"({ "weights": { "w": 5 } })"), "must be an object"));
  CHECK(says(badAi("aisd_a5", R"({ "weights": { "w": { "power": 5 } } })"), "unknown key \"power\""));
  CHECK(says(badAi("aisd_a6", R"({ "weights": { "w": { "dmg": 101 } } })"), "\"dmg\" must be a whole number from 0 to 100"));
  CHECK(says(badAi("aisd_a7", R"({ "weights": { "w": { "dmg": 1.5 } } })"), "\"dmg\" must be a whole number"));
  CHECK(says(badAi("aisd_a8", R"({ "weights": { "w": { "dangerDiv": 0 } } })"), "\"dangerDiv\" must be a whole number from 1 to 1000"));
  CHECK(says(badAi("aisd_a9", R"({ "weights": { "w": { "priority": { "dragon": 3 } } } })"), "unknown unit type \"dragon\""));
  CHECK(says(badAi("aisd_a10", R"({ "weights": { "w": { "priority": { "archer": 5000 } } } })"), "from -1000 to 1000"));
  CHECK(says(badAi("aisd_a11", R"({ "weights": { "w": { "attackAtZero": 1 } } })"), "attackAtZero"));
}

TEST(ai_scripted_data_behaviour_refusals_say_what_is_wrong) {
  CHECK(says(badBeh("aisd_b1", R"({ "order": 100001 })"), "\"order\" must be a whole number from -100000 to 100000"));
  CHECK(says(badBeh("aisd_b2", R"({ "order": 1.5 })"), "\"order\" must be a whole number"));
  CHECK(says(badBeh("aisd_b3", R"({ "weights": "ghost" })"), "\"weights\" names \"ghost\""));
  CHECK(says(badBeh("aisd_b4", R"({ "weights": 4 })"), "\"weights\" must be the name of a set"));
  CHECK(says(badBeh("aisd_b5", R"({ "caution": "reckless" })"), "\"caution\" must be \"normal\""));
  CHECK(says(badBeh("aisd_b6", R"({ "idleTurns": 0 })"), "\"idleTurns\" must be a whole number from 1 to 99"));
  CHECK(says(badBeh("aisd_b7", R"({ "idleTurns": 100 })"), "from 1 to 99"));
  CHECK(says(badBeh("aisd_b8", R"({ "retreat": { "until": 80 } })"), "needs \"below\""));
  CHECK(says(badBeh("aisd_b9", R"({ "retreat": { "below": 0 } })"), "\"below\" must be a whole number from 1 to 99"));
  CHECK(says(badBeh("aisd_b10", R"({ "retreat": { "below": 50, "until": 50 } })"), "\"until\" (50) must be more than \"below\" (50)"));
  CHECK(says(badBeh("aisd_b11", R"({ "retreat": { "below": 30, "to": { "terrain": ["lava"] } } })"), "unknown terrain \"lava\""));
  CHECK(says(badBeh("aisd_b12", R"({ "retreat": { "below": 30, "speed": 1 } })"), "unknown key \"speed\""));
  CHECK(says(badBeh("aisd_b13", R"({ "retreat": { "below": 30, "to": { "terrain": [] } } })"), "needs \"terrain\""));
  CHECK(says(badBeh("aisd_b14", R"({ "retreat": { "below": 30, "useItems": "yes" } })"), "\"useItems\" must be true or false"));
  CHECK(says(badBeh("aisd_b15", R"({ "retreat": 30 })"), "\"retreat\" must be an object"));
  CHECK(says(badBeh("aisd_b16", R"({ "mode": "patrol" })"), "needs a \"route\""));
  CHECK(says(badBeh("aisd_b17", R"({ "mode": "patrol", "route": [ { "x": 3, "y": 1 } ] })"), "2 to 32 squares"));
  CHECK(says(badBeh("aisd_b18", R"({ "mode": "hold", "route": [ { "x": 3, "y": 1 }, { "x": 4, "y": 1 } ] })"), "goes with mode \"patrol\""));
  CHECK(says(badBeh("aisd_b19", R"({ "mode": "patrol", "route": [ { "x": 3, "y": 1 }, { "x": 3, "y": 1 } ] })"), "the same square twice in a row"));
  CHECK(says(badBeh("aisd_b20", R"({ "mode": "patrol", "route": [ { "x": 3, "y": 1 }, { "x": 40, "y": 1 } ] })"), "square 2 (40,1) is outside the map"));
  CHECK(says(badBeh("aisd_b21", R"({ "mode": "patrol", "route": [ { "x": 3 }, { "x": 4, "y": 1 } ] })"), "needs a whole number"));
  CHECK(says(badBeh("aisd_b22", R"({ "ignores": ["ghost"] })"), "unknown unit \"ghost\""));
  CHECK(says(badBeh("aisd_b23", R"({ "ignores": "lord" })"), "\"ignores\" must be a list"));
  CHECK(says(badBeh("aisd_b24", R"({ "measure": "moveCost" })"), "goes with mode \"guardArea\" and \"within\""));
  CHECK(says(badBeh("aisd_b25", R"({ "mode": "guardArea", "area": { "x": 1, "y": 1 }, "measure": "moveCost" })"), "goes with mode \"guardArea\" and \"within\""));
  CHECK(says(badBeh("aisd_b26", R"({ "mode": "guardArea", "within": 3, "measure": "miles" })"), "\"measure\" must be \"steps\""));
  CHECK(says(badBeh("aisd_b27", R"({ "mode": "dance" })"), "\"patrol\" too"));
  // A key that is not one of the behaviour keys still says what the choices are.
  CHECK(says(badBeh("aisd_b28", R"({ "speed": 2 })"), "order, weights, caution, retreat, idleTurns, route, ignores"));
}

TEST(ai_scripted_data_the_keys_work_in_reinforcements_and_set_behaviour_and_placement_order_is_the_unit_number) {
  // p is on the map; "late" arrives in round 2 with order -1 and acts before it; q gets order -3 by setBehaviour in round 2 and goes first.
  Options o;
  o.aiRules = R"({ "turnOrder": "order" })";
  o.events = R"({ "id": "more", "when": { "turn": 2 }, "do": [ { "addUnit": { "type": "soldier", "id": "late", "side": "red", "x": 12, "y": 1, "behaviour": { "order": -1 } } } ] },
              { "id": "re", "when": { "turn": 2 }, "do": [ { "setBehaviour": { "order": -3 }, "units": { "unit": "q" } } ] })";
  SField f("aisd_reinforce", blue("lord", 0, 2) + ", " + red("p", 9, 1) + ", " + red("q", 9, 3), o);
  CHECK(f.loaded);
  std::vector<std::string> seq = f.redSequence();  // round 1: p, q (both 0: placement order)
  CHECK_EQ((int)seq.size(), 2);
  CHECK_STR(seq[0], "p");
  CHECK_STR(seq[1], "q");
  seq = f.redSequence();  // round 2: q (-3), late (-1), p (0)
  CHECK_EQ((int)seq.size(), 3);
  CHECK_STR(seq[0], "q");
  CHECK_STR(seq[1], "late");
  CHECK_STR(seq[2], "p");
  CHECK_EQ(f.unit("late").behaviour.order, -1);
}

TEST(ai_scripted_data_a_map_without_the_keys_saves_exactly_as_before) {
  SField f("aisd_save_plain", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh("\"guard\"")));
  CHECK(f.redTurn());
  const std::string text = savetest::stateText(f.b());
  for (const char* key : {"retreating", "idleSeen", "patrolAt", "\"order\"", "weights", "caution", "idleTurns", "route", "ignores"})
    CHECK(text.find(key) == std::string::npos);
}

TEST(ai_scripted_data_retreat_idle_and_patrol_state_survive_a_save_and_play_on_the_same) {
  Options o;
  o.aiRules = R"({ "turnOrder": "order", "weights": { "w": { "dmg": 2 } } })";
  o.rows = "............../............../...........F../............../..............";
  const std::string units = blue("lord", 0, 2) + ", " +
      red("hurt", 8, 4, beh(R"({ "order": 4, "weights": "w", "caution": "none", "ignores": ["lord"], "retreat": { "below": 30, "until": 80, "to": { "terrain": ["fort"] }, "useItems": false } })")) + ", " +
      red("idle", 9, 0, beh(R"({ "idleTurns": 3 })")) + ", " +
      red("walk", 10, 1, beh(R"({ "mode": "patrol", "route": [ { "x": 12, "y": 1 }, { "x": 8, "y": 1 }, { "x": 8, "y": 0 } ] })")) + ", " +
      red("area", 13, 4, beh(R"({ "mode": "guardArea", "within": 2, "measure": "moveCost" })"));
  SField f("aisd_save", units, o);
  CHECK(f.loaded);
  f.unit("hurt").hp = 5;
  CHECK(f.redTurn());
  CHECK(f.redTurn());
  CHECK(f.unit("hurt").retreating);
  CHECK_EQ(f.unit("idle").idleSeen, 2);
  CHECK(f.unit("walk").patrolAt > 0);
  const std::string text = savetest::stateText(f.b());
  CHECK(says(text, "\"retreating\":true") || says(text, "\"retreating\": true"));
  CHECK(says(text, "idleSeen"));
  CHECK(says(text, "patrolAt"));
  Json saved;
  saveBattleState(f.b(), saved);
  Battle r;
  std::string err;
  CHECK(r.init(f.rig.db, f.rig.map, f.rig.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  const Unit& hurt = r.units[(size_t)f.id("hurt")];
  CHECK(hurt.retreating);
  CHECK_EQ(hurt.behaviour.order, 4);
  CHECK_STR(hurt.behaviour.weightsName, "w");
  CHECK(hurt.behaviour.weightSet >= 0 && hurt.behaviour.cautionNone);
  CHECK_EQ(hurt.behaviour.retreat.below, 30);
  CHECK_EQ(hurt.behaviour.retreat.until, 80);
  CHECK(!hurt.behaviour.retreat.useItems);
  CHECK_EQ((int)hurt.behaviour.retreat.terrains.size(), 1);
  CHECK_EQ((int)hurt.behaviour.ignores.size(), 1);
  CHECK_EQ(r.units[(size_t)f.id("idle")].idleSeen, 2);
  const Unit& walk = r.units[(size_t)f.id("walk")];
  CHECK_EQ(walk.patrolAt, f.unit("walk").patrolAt);
  CHECK_EQ((int)walk.behaviour.route.size(), 3);
  CHECK(r.units[(size_t)f.id("area")].behaviour.areaMoveCost);
  CHECK_STR(savetest::stateText(r), text);
  // The restored battle plays on exactly as the original does.
  std::vector<Event> e1, e2;
  f.b().apply(Action::endTurn(), e1);
  r.apply(Action::endTurn(), e2);
  for (int i = 0; i < 80 && f.b().activeSide == 1; ++i) aiTakeStep(f.b(), e1);
  for (int i = 0; i < 80 && r.activeSide == 1; ++i) aiTakeStep(r, e2);
  CHECK_STR(savetest::stateText(r), savetest::stateText(f.b()));
}

TEST(ai_scripted_data_a_damaged_save_is_refused_in_plain_words) {
  Options o;
  o.aiRules = R"({ "weights": { "w": { "dmg": 2 } } })";
  SField f("aisd_save_bad", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(R"({ "weights": "w" })")), o);
  Json saved;
  saveBattleState(f.b(), saved);
  Json units = Json::makeArray();
  for (size_t i = 0; i < saved["units"].size(); ++i) {
    Json u = saved["units"][i];
    if (i == (size_t)f.id("r")) {
      Json b = Json::makeObject();
      b.set("weights", Json::makeString("ghost"));
      u.set("behaviour", b);
    }
    units.push(u);
  }
  saved.set("units", units);
  Battle r;
  std::string err;
  CHECK(r.init(f.rig.db, f.rig.map, f.rig.formulas, 1, err));
  CHECK(!restoreBattleState(r, saved, err));
  CHECK(says(err, "weights") && says(err, "ghost"));
}

TEST(ai_scripted_data_set_behaviour_starts_the_counters_afresh) {
  Options o;
  o.events = R"({ "id": "again", "when": { "turn": 3 }, "do": [ { "setBehaviour": { "idleTurns": 1 }, "units": { "unit": "r" } } ] })";
  SField f("aisd_reset", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(R"({ "idleTurns": 2 })")), o);
  CHECK(f.redTurn());
  CHECK(f.redTurn());
  CHECK_EQ(f.unit("r").idleSeen, 0);  // round 3 began (blue first): the event handed out the new block and the counter starts over
  CHECK(f.redTurn());  // round 3, its first turn under idleTurns 1
  CHECK_EQ(f.unit("r").idleSeen, 1);
  CHECK_EQ(f.x("r"), 8);
  CHECK(f.redTurn());  // round 4: past its one idle turn
  CHECK(f.x("r") < 8);
}
