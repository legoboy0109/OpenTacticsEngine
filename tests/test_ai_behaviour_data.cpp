// Computer opponent behaviour: the loader's keys and refusals, the story commands, and saves (docs/AI.md, docs/SAVES.md).
#include "ai_behaviour_fixture.h"

using namespace ote;
using namespace aitest;

namespace {

std::string redWith(const std::string& extra) {
  return "{ \"type\": \"soldier\", \"id\": \"r\", \"side\": \"red\", \"x\": 8, \"y\": 2" + (extra.empty() ? "" : ", " + extra) + " }";
}
std::string behaviour(const std::string& v) { return "\"behaviour\": " + v; }
bool says(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
std::string bad(const char* name, const std::string& extra, const std::string& events = "") {
  return Field::mapError(name, redWith(extra), events);
}

}  // namespace

TEST(ai_behaviour_data_every_mode_and_key_loads) {
  CHECK_STR(bad("aibd_ok1", behaviour("\"hold\"")), "");
  CHECK_STR(bad("aibd_ok2", behaviour("\"guard\"")), "");
  CHECK_STR(bad("aibd_ok3", behaviour(R"({ "mode": "guard", "wake": "react", "group": "pack", "fromTurn": 3 })")), "");
  CHECK_STR(bad("aibd_ok4", behaviour(R"({ "mode": "guardArea", "area": { "x": 3, "y": 1, "w": 2, "h": 2 }, "within": 4, "group": "pack" })")), "");
  CHECK_STR(bad("aibd_ok5", behaviour(R"({ "mode": "seek", "unit": "lord", "prefer": "weakest" })")), "");
  CHECK_STR(bad("aibd_ok6", behaviour(R"({ "mode": "seek", "place": "throne" })")), "");
  CHECK_STR(bad("aibd_ok7", behaviour(R"({ "mode": "seek", "x": 3, "y": 4, "prefer": { "type": "archer" } })")), "");
  CHECK_STR(bad("aibd_ok8", behaviour(R"({ "prefer": { "tag": "mounted" }, "fromTurn": 2 })")), "");
}

TEST(ai_behaviour_data_refusals_say_what_is_wrong_in_plain_words) {
  CHECK(says(bad("aibd_b1", behaviour("\"charge\"")), "\"behaviour\" must be \"advance\", \"hold\", \"guard\", \"guardArea\" or \"seek\", not \"charge\""));
  CHECK(says(bad("aibd_b2", behaviour("\"seek\"")), "needs more than a name"));
  CHECK(says(bad("aibd_b3", behaviour("5")), "must be a mode name"));
  CHECK(says(bad("aibd_b4", behaviour(R"({ "mode": "hold", "speed": 2 })")), "unknown key \"speed\""));
  CHECK(says(bad("aibd_b5", behaviour(R"({ "mode": "hold", "wake": "react" })")), "\"wake\" goes with mode \"guard\" only"));
  CHECK(says(bad("aibd_b6", behaviour(R"({ "mode": "guard", "wake": "never" })")), "\"wake\" must be \"once\""));
  CHECK(says(bad("aibd_b7", behaviour(R"({ "mode": "hold", "group": "pack" })")), "\"group\" goes with mode \"guard\" or \"guardArea\""));
  CHECK(says(bad("aibd_b8", behaviour(R"({ "mode": "guard", "group": "a b" })")), "letters, digits"));
  CHECK(says(bad("aibd_b9", behaviour(R"({ "mode": "guardArea" })")), "needs an \"area\" and / or \"within\""));
  CHECK(says(bad("aibd_b10", behaviour(R"({ "mode": "guard", "within": 3 })")), "go with mode \"guardArea\""));
  CHECK(says(bad("aibd_b11", behaviour(R"({ "mode": "seek" })")), "needs exactly one target"));
  CHECK(says(bad("aibd_b12", behaviour(R"({ "mode": "seek", "unit": "lord", "place": "throne" })")), "needs exactly one target"));
  CHECK(says(bad("aibd_b13", behaviour(R"({ "mode": "seek", "x": 3 })")), "both \"x\" and \"y\""));
  CHECK(says(bad("aibd_b14", behaviour(R"({ "mode": "hold", "unit": "lord" })")), "go with mode \"seek\""));
  CHECK(says(bad("aibd_b15", behaviour(R"({ "fromTurn": 0 })")), "\"fromTurn\" must be a whole number from 1 to 99"));
  CHECK(says(bad("aibd_b16", behaviour(R"({ "fromTurn": 100 })")), "from 1 to 99"));
  CHECK(says(bad("aibd_b17", behaviour(R"({ "mode": "seek", "unit": "ghost" })")), "unknown unit \"ghost\""));
  CHECK(says(bad("aibd_b18", behaviour(R"({ "mode": "seek", "place": "tower" })")), "not a structure of this map"));
  CHECK(says(bad("aibd_b19", behaviour(R"({ "mode": "seek", "x": 40, "y": 1 })")), "outside the map"));
  CHECK(says(bad("aibd_b20", behaviour(R"({ "mode": "guardArea", "area": { "x": 30, "y": 0 } })")), "outside the map"));
  CHECK(says(bad("aibd_b21", behaviour(R"({ "prefer": { "type": "dragon" } })")), "unknown unit type \"dragon\""));
  CHECK(says(bad("aibd_b22", behaviour(R"({ "prefer": "kindest" })")), "\"prefer\" must be \"weakest\""));
  CHECK(says(bad("aibd_b23", behaviour(R"({ "prefer": { "unit": "ghost" } })")), "unknown unit \"ghost\""));
  // A unit on a side a person plays has nothing for the computer to decide.
  CHECK(says(Field::mapError("aibd_b24", R"({ "type": "soldier", "id": "q", "side": "blue", "x": 2, "y": 2, "behaviour": "hold" })"),
             "this unit is on a side a person plays"));
}

TEST(ai_behaviour_data_the_story_commands_load_and_refuse) {
  const std::string ev = "{ \"id\": \"e\", \"when\": { \"turn\": 2 }, \"do\": [ ";
  CHECK_STR(bad("aibd_s1", "", ev + R"({ "setBehaviour": "hold", "units": { "unit": "r" } } ] })"), "");
  CHECK_STR(bad("aibd_s2", "", ev + R"({ "setBehaviour": { "mode": "seek", "place": "throne" }, "units": { "side": "red" } } ] })"), "");
  CHECK_STR(bad("aibd_s3", behaviour(R"({ "mode": "guard", "group": "pack" })"), ev + R"({ "wakeGroup": "pack" } ] })"), "");
  CHECK(says(bad("aibd_s4", "", ev + R"({ "setBehaviour": "hold" } ] })"), "needs \"units\""));
  CHECK(says(bad("aibd_s5", "", ev + R"({ "setBehaviour": "hold", "units": { "unit": "ghost" } } ] })"), "unknown unit \"ghost\""));
  CHECK(says(bad("aibd_s6", "", ev + R"({ "setBehaviour": "hold", "units": { "side": "blue" } } ] })"), "a side a person plays"));
  CHECK(says(bad("aibd_s7", "", ev + R"({ "setBehaviour": "dance", "units": {} } ] })"), "\"behaviour\" must be"));
  CHECK(says(bad("aibd_s8", "", ev + R"({ "wakeGroup": "pack" } ] })"), "no unit of this map is in a group"));
  CHECK(says(bad("aibd_s9", "", ev + R"({ "setBehaviour": "hold", "units": {}, "x": 1 } ] })"), "\"x\""));
  // addUnit takes the same key.
  CHECK_STR(bad("aibd_s10", "", ev + R"({ "addUnit": { "type": "soldier", "side": "red", "x": 10, "y": 1, "behaviour": "hold" } } ] })"), "");
  CHECK(says(bad("aibd_s11", "", ev + R"({ "addUnit": { "type": "soldier", "side": "red", "x": 10, "y": 1, "behaviour": "sulk" } } ] })"), "\"behaviour\" must be"));
  CHECK(says(bad("aibd_s12", "", ev + R"({ "addUnit": { "type": "soldier", "side": "blue", "x": 10, "y": 1, "behaviour": "hold" } } ] })"), "side a person plays"));
}

TEST(ai_behaviour_a_story_event_changes_behaviour_during_the_battle) {
  Field f("aibd_set", redWith(behaviour("\"hold\"")),
          R"({ "id": "release", "when": { "turn": 3 }, "do": [ { "setBehaviour": "advance", "units": { "unit": "r" } } ] })");
  CHECK(f.loaded);
  CHECK(f.redTurn());
  CHECK(f.redTurn());
  CHECK_EQ(f.x("r"), 8);  // rounds 1 and 2: it holds
  CHECK(f.redTurn());
  CHECK(f.x("r") < 8);    // round 3 began with the new behaviour
  CHECK(f.unit("r").behaviour.isDefault());
}

TEST(ai_behaviour_a_story_event_wakes_a_group_and_sets_a_new_behaviour_that_starts_asleep) {
  Field f("aibd_wake", redWith(behaviour(R"({ "mode": "guard", "group": "pack" })")),
          R"({ "id": "horn", "when": { "turn": 2 }, "do": [ { "wakeGroup": "pack" } ] })");
  CHECK(f.redTurn());  // the horn sounds as round 2 begins, before the unit has moved
  CHECK_EQ(f.x("r"), 8);
  CHECK(f.unit("r").awake);
  CHECK(f.redTurn());
  CHECK(f.x("r") < 8);
  // A guard handed out by a command starts asleep.
  Field g("aibd_set_guard", "{ \"type\": \"soldier\", \"id\": \"r\", \"side\": \"red\", \"x\": 12, \"y\": 4 }",
          R"({ "id": "ambush", "when": { "turn": 2 }, "do": [ { "setBehaviour": "guard", "units": { "unit": "r" } } ] })",
          false, "\"x\": 0, \"y\": 0");
  CHECK(g.redTurn());
  CHECK(g.unit("r").behaviour.mode == AiMode::Guard);
  CHECK(!g.unit("r").awake);
  CHECK(g.redTurn());
  CHECK_EQ(g.x("r"), 12);
  CHECK(!g.unit("r").awake);
}

TEST(ai_behaviour_an_added_unit_carries_its_behaviour) {
  Field f("aibd_add", "", R"({ "id": "reinforce", "when": { "turn": 2 }, "do": [ { "addUnit": { "type": "soldier", "id": "late", "side": "red", "x": 10, "y": 1, "behaviour": "hold" } } ] })");
  CHECK(f.redTurn());
  CHECK(f.redTurn());
  CHECK(f.id("late") >= 0);
  CHECK(f.unit("late").behaviour.mode == AiMode::Hold);
  CHECK_EQ(f.x("late"), 10);
}

TEST(ai_behaviour_reaching_a_square_can_remove_a_seeker_the_escape_recipe) {
  Field f("aibd_escape", "{ \"type\": \"soldier\", \"id\": \"runner\", \"side\": \"red\", \"x\": 9, \"y\": 4, " + behaviour(R"({ "mode": "seek", "x": 13, "y": 4 })") + " }",
          R"({ "id": "away", "when": { "reach": { "x": 13, "y": 4 }, "by": { "unit": "runner" } },
               "do": [ { "removeUnit": { "unit": "runner" } }, { "addVariable": "out", "by": 1 }, { "setSwitch": "escaped", "to": true } ] })",
          false, "\"x\": 0, \"y\": 0");
  CHECK(f.loaded);
  CHECK(f.redTurn());
  CHECK(f.sw("escaped"));
  CHECK(!f.unit("runner").alive);
  CHECK_EQ(f.b().storyState.variables[(size_t)f.rig.db.findVariable("out")], 1);
}

// ---- saves ----

TEST(ai_behaviour_a_map_without_the_keys_saves_exactly_as_before) {
  Field f("aibd_save_plain", redWith(""));
  CHECK(f.redTurn());
  const std::string text = savetest::stateText(f.b());
  CHECK(text.find("behaviour") == std::string::npos);
  CHECK(text.find("awake") == std::string::npos);
}

TEST(ai_behaviour_a_woken_guard_and_a_changed_behaviour_survive_a_save) {
  Field f("aibd_save", redWith(behaviour(R"({ "mode": "guard", "group": "pack", "fromTurn": 2 })")) + ", " +
                           "{ \"type\": \"soldier\", \"id\": \"k\", \"side\": \"red\", \"x\": 9, \"y\": 4, " + behaviour(R"({ "mode": "seek", "unit": "lord", "prefer": { "type": "archer" } })") + " }" + ", " +
                           "{ \"type\": \"soldier\", \"id\": \"w\", \"side\": \"red\", \"x\": 9, \"y\": 0, " + behaviour(R"({ "mode": "guardArea", "area": { "x": 1, "y": 1, "w": 2, "h": 2 }, "within": 2 })") + " }");
  f.put("lord", 6, 2);  // inside the guard's reach
  CHECK(f.redTurn());   // round 1: before its start turn
  CHECK(f.redTurn());   // round 2: it wakes
  CHECK(f.unit("r").awake);
  Json saved;
  saveBattleState(f.b(), saved);
  Battle r;
  std::string err;
  CHECK(r.init(f.rig.db, f.rig.map, f.rig.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  const Unit& g = r.units[(size_t)f.id("r")];
  CHECK(g.awake);
  CHECK(g.behaviour.mode == AiMode::Guard);
  CHECK_STR(g.behaviour.group, "pack");
  CHECK_EQ(g.behaviour.fromTurn, 2);
  const Unit& k = r.units[(size_t)f.id("k")];
  CHECK(k.behaviour.mode == AiMode::Seek);
  CHECK_STR(k.behaviour.seekUnit, "lord");
  CHECK(k.behaviour.prefer == AiPrefer::Type);
  CHECK_STR(k.behaviour.preferText, "archer");
  const Unit& w = r.units[(size_t)f.id("w")];
  CHECK(w.behaviour.hasArea && w.behaviour.ax == 1 && w.behaviour.aw == 2 && w.behaviour.within == 2);
  CHECK(!w.awake);
  // The same state saves to the same text.
  CHECK_STR(savetest::stateText(r), savetest::stateText(f.b()));
  // And the restored battle plays on exactly as the original does.
  std::vector<Event> e1, e2;
  f.b().apply(Action::endTurn(), e1);
  r.apply(Action::endTurn(), e2);
  for (int i = 0; i < 40 && f.b().activeSide == 1; ++i) aiTakeStep(f.b(), e1);
  for (int i = 0; i < 40 && r.activeSide == 1; ++i) aiTakeStep(r, e2);
  CHECK_STR(savetest::stateText(r), savetest::stateText(f.b()));
}

TEST(ai_behaviour_a_damaged_save_is_refused_in_plain_words) {
  Field f("aibd_save_bad", redWith(behaviour("\"hold\"")));
  Json saved;
  saveBattleState(f.b(), saved);
  Json units = Json::makeArray();
  for (size_t i = 0; i < saved["units"].size(); ++i) {
    Json u = saved["units"][i];
    if (i == (size_t)f.id("r")) u.set("behaviour", Json::makeString("charge"));
    units.push(u);
  }
  saved.set("units", units);
  Battle r;
  std::string err;
  CHECK(r.init(f.rig.db, f.rig.map, f.rig.formulas, 1, err));
  CHECK(!restoreBattleState(r, saved, err));
  CHECK(says(err, "behaviour") && says(err, "charge"));
}
