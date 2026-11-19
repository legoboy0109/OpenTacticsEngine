// Loading story events, objectives, switches, variables and unit ids (docs/EVENTS.md): that
// everything is read, and that each kind of mistake is refused with a message that names the map,
// the event and the command.
#include "posix_compat.h"

#include <cstdio>

#include "core/data/loader.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// eirik (blue soldier, 0,1) and gorm (red soldier, 8,1) on a 9 x 3 field.
StoryWorld world() {
  return StoryWorld({".........", ".........", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}},
                    {"eirik", "gorm"});
}

// The loader's message for one event (as the first of the map's events) / one objectives block.
std::string errorOf(const std::string& event) {
  StoryWorld w = world();
  return w.load("[" + event + "]");
}
std::string commandError(const std::string& command) {
  StoryWorld w = world();
  fixture::addItems(w);
  return w.load("[{\"id\":\"ev\",\"when\":{\"battle\":\"start\"},\"do\":[" + command + "]}]");
}
std::string objectiveError(const std::string& objectives) {
  StoryWorld w = world();
  return w.load("", objectives);
}

#define REFUSES(name, error, needle)                                                       \
  TEST(story_loader_refuses_##name) {                                                      \
    std::string e = (error);                                                               \
    if (!has(e, needle)) std::printf("  got: %s\n  wanted: %s\n", e.c_str(), needle);       \
    CHECK(!e.empty() && has(e, needle));                                                   \
  }

}  // namespace

TEST(story_loader_reads_events_objectives_and_the_text_table) {
  StoryWorld w = world();
  fixture::addItems(w);
  std::string err = w.load(R"([
    {"id":"a","when":{"turn":3,"side":"red"},"if":[{"switch":"saved","is":false},{"variable":"gold","op":">=","value":2}],
     "repeat":true,"do":[{"say":"Now!","speaker":"gorm","portrait":"gorm_angry"},
                         {"addUnit":{"type":"archer","side":"red","x":7,"y":1,"id":"sniper","name":"Sniper","level":2,"leader":true,"stats":{"atk":9}}},
                         {"giveItem":"potion","to":{"tag":"armor"}},{"giveResource":{"funds":-5},"side":"red"},
                         {"if":[{"alive":"gorm"}],"then":[{"focus":"gorm"}],"else":[{"focus":{"x":1,"y":2}}]}]},
    {"id":"b","when":{"talk":"eirik","to":"gorm","label":"Parley"},"do":[{"endTurn":true}]},
    {"id":"c","when":{"fight":{"type":"soldier"},"with":{"team":"red"}},"do":[]},
    {"id":"d","when":{"turn":"every"},"do":[]}])",
                           R"({"text":"Hold","win":[{"defeated":{"unit":"gorm"}},{"reach":{"x":8,"y":1,"w":1,"h":2},"by":"eirik"},{"survive":9}],
                               "lose":[{"defeated":"eirik"},{"round":15}]})");
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK(err.empty());
  if (!err.empty()) return;
  const StoryData& s = *w.map.story;
  CHECK_EQ(s.events.size(), (size_t)4);
  const StoryEvent& a = s.events[0];
  CHECK(a.id == "a" && a.repeat && a.when.kind == Trigger::Kind::Turn && a.when.round == 3 && a.when.side == 1);
  CHECK(a.cond.size() == 2 && a.cond[0].kind == Condition::Kind::Switch && !a.cond[0].is);
  CHECK(a.cond[1].kind == Condition::Kind::Variable && a.cond[1].op == CmpOp::Ge && a.cond[1].value == 2);
  CHECK_EQ(a.commands.size(), (size_t)5);
  CHECK(a.commands[0].kind == Command::Kind::Say && a.commands[0].unit == "gorm");
  CHECK_STR(s.texts[(size_t)a.commands[0].portrait], "gorm_angry");
  const MapUnitDef& u = a.commands[1].newUnit;
  CHECK(u.id == "sniper" && u.name == "Sniper" && u.level == 2 && u.leader && u.stats[1] == 9 && u.x == 7);
  CHECK(a.commands[2].filter.kind == UnitFilter::Kind::Tag && a.commands[2].filter.unit == "armor");
  CHECK(a.commands[3].amounts.size() == 1 && a.commands[3].amounts[0].amount == -5 && a.commands[3].side == 1);
  CHECK(a.commands[4].thenDo.size() == 1 && a.commands[4].elseDo.size() == 1 && a.commands[4].elseDo[0].y == 2);
  CHECK(s.events[1].when.kind == Trigger::Kind::Talk && s.events[1].endsTurnOnTalk);
  CHECK_STR(s.texts[(size_t)s.events[1].when.label], "Parley");
  CHECK(s.events[2].when.who.kind == UnitFilter::Kind::Type && s.events[2].when.other.kind == UnitFilter::Kind::Team);
  CHECK(s.events[3].repeat);                               // "every" round means every round
  const ObjectiveSet& o = s.objectives[0];
  CHECK_STR(s.texts[(size_t)o.text], "Hold");
  CHECK(o.win.size() == 3 && o.win[1].hasBy && o.win[1].rect.h == 2 && o.win[2].rounds == 9);
  CHECK(o.lose.size() == 2 && o.lose[0].filter.unit == "eirik" && o.lose[1].kind == ObjectiveRule::Kind::Round);
}

TEST(story_loader_leaves_a_map_without_events_alone) {
  StoryWorld w = world();
  CHECK(w.load("", "").empty());
  CHECK(!w.map.story);
}

TEST(story_loader_game_and_map_files) {
  std::string root = ote_test::tempDir() + "/story_game";
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  auto game = [&](const std::string& extra) {
    CHECK(writeTextFile(root + "/game.json",
                        "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"str\"},{\"id\":\"mov\"}],"
                        " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                        " \"formulas\": {\"damage\":\"a.str\"}," + extra +
                        " \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
  };
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"} ] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"sword\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"str\":2,\"mov\":3}}] }"));
  auto map = [&](const std::string& unitA, const std::string& extra) {
    CHECK(writeTextFile(root + "/maps/m.json",
                        "{ \"grid\": {\"type\":\"square\",\"width\":3,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"...\"],"
                        " \"sides\": [{\"id\":\"a\"},{\"id\":\"b\"}],"
                        " \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0" + unitA + "},"
                        "            {\"type\":\"grunt\",\"side\":\"b\",\"x\":2,\"y\":0}]" + extra + " }"));
  };
  auto problem = [&]() {
    Database db;
    MapDef m;
    std::string err;
    if (!loadGame(root, db, err)) return err;
    if (!loadMap(db, "m", m, err)) return err;
    return std::string();
  };
  const std::string good = "\"switches\":[{\"id\":\"s1\",\"name\":\"One\"}],\"variables\":[{\"id\":\"v1\",\"name\":\"V\"}],";

  game(good);
  map(",\"id\":\"hero\"",
      ",\"events\":[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"if\":[{\"switch\":\"s1\"}],\"do\":[{\"say\":\"Hi\",\"speaker\":\"hero\"}]}]");
  CHECK_STR(problem(), "");
  Database db;
  MapDef m;
  std::string err;
  CHECK(loadGame(root, db, err) && loadMap(db, "m", m, err));
  CHECK(db.switches.size() == 1 && db.switches[0].name == "One" && db.variables.size() == 1);
  CHECK(m.units[0].id == "hero" && m.units[1].id.empty() && m.story && m.story->events.size() == 1);

  std::string e;
  game("\"switches\":[{\"id\":\"s\"},{\"id\":\"s\"}],");
  map("", "");
  e = problem();
  CHECK(has(e, "switches \"s\"") && has(e, "duplicate id"));
  game("\"variables\":[{\"id\":\"has space\"}],");
  e = problem();
  CHECK(has(e, "variables \"has space\"") && has(e, "letters, digits"));
  game(good);
  map(",\"id\":\"bad id!\"", "");
  e = problem();
  CHECK(has(e, "units[0]") && has(e, "\"id\" must be letters, digits"));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"grid\": {\"type\":\"square\",\"width\":3,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"...\"],"
                      " \"sides\": [{\"id\":\"a\"},{\"id\":\"b\"}],"
                      " \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0,\"id\":\"x\"},"
                      "            {\"type\":\"grunt\",\"side\":\"b\",\"x\":2,\"y\":0,\"id\":\"x\"}] }"));
  e = problem();
  CHECK(has(e, "units[1]") && has(e, "already used by units[0]"));
  map(",\"id\":\"hero\"", ",\"events\":[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":[{\"say\":\"Hi\",\"speaker\":\"nobody\"}]}]");
  e = problem();
  CHECK(has(e, "m.json") && has(e, "map \"m\"") && has(e, "event \"e\"") && has(e, "command 1 (say)") && has(e, "unknown unit \"nobody\""));
}

// ---- events ----
REFUSES(an_event_without_an_id, errorOf(R"({"when":{"battle":"start"},"do":[]})"), "missing required \"id\"")
REFUSES(an_event_id_with_spaces, errorOf(R"({"id":"two words","when":{"battle":"start"},"do":[]})"), "letters, digits")
REFUSES(an_unknown_event_key, errorOf(R"({"id":"e","when":{"battle":"start"},"do":[],"repat":true})"), "unknown key \"repat\"")
REFUSES(an_event_without_when, errorOf(R"({"id":"e","do":[]})"), "missing required \"when\"")
REFUSES(an_event_without_do, errorOf(R"({"id":"e","when":{"battle":"start"}})"), "missing required \"do\"")
REFUSES(a_repeat_that_is_not_true_or_false, errorOf(R"({"id":"e","when":{"battle":"start"},"do":[],"repeat":"yes"})"), "\"repeat\" must be true or false")
REFUSES(a_duplicate_event_id, world().load(R"([{"id":"e","when":{"battle":"start"},"do":[]},{"id":"e","when":{"battle":"won"},"do":[]}])"),
        "event \"e\": duplicate id")

// ---- triggers ----
REFUSES(an_unknown_trigger_key, errorOf(R"({"id":"e","when":{"battle":"start","oops":1},"do":[]})"), "unknown key \"oops\"")
REFUSES(two_triggers, errorOf(R"({"id":"e","when":{"battle":"start","turn":1},"do":[]})"), "has two triggers")
REFUSES(no_trigger, errorOf(R"({"id":"e","when":{},"do":[]})"), "needs a trigger")
REFUSES(an_unknown_battle_trigger, errorOf(R"({"id":"e","when":{"battle":"begin"},"do":[]})"), "\"battle\" must be")
REFUSES(a_turn_below_one, errorOf(R"({"id":"e","when":{"turn":0},"do":[]})"), "\"turn\" must be a round number")
REFUSES(a_turn_with_an_unknown_side, errorOf(R"({"id":"e","when":{"turn":1,"side":"purple"},"do":[]})"), "unknown side \"purple\"")
REFUSES(an_area_outside_the_map, errorOf(R"({"id":"e","when":{"reach":{"x":8,"y":1,"w":2}},"do":[]})"), "does not fit inside the map")
REFUSES(an_area_without_a_corner, errorOf(R"({"id":"e","when":{"reach":{"w":2}},"do":[]})"), "missing required \"x\"")
REFUSES(an_empty_area, errorOf(R"({"id":"e","when":{"reach":{"x":1,"y":1,"w":0}},"do":[]})"), "at least 1")
REFUSES(a_reach_by_an_unknown_unit, errorOf(R"({"id":"e","when":{"reach":{"x":1,"y":1},"by":"zed"},"do":[]})"), "unknown unit \"zed\"")
REFUSES(a_filter_with_an_unknown_side, errorOf(R"({"id":"e","when":{"defeated":{"side":"purple"}},"do":[]})"), "unknown side \"purple\"")
REFUSES(a_filter_with_an_unknown_team, errorOf(R"({"id":"e","when":{"defeated":{"team":"purple"}},"do":[]})"), "unknown team \"purple\"")
REFUSES(a_filter_with_an_unknown_type, errorOf(R"({"id":"e","when":{"fight":{"type":"dragon"}},"do":[]})"), "unknown unit type \"dragon\"")
REFUSES(a_filter_naming_two_things, errorOf(R"({"id":"e","when":{"fight":{"type":"soldier","side":"red"}},"do":[]})"), "more than one thing")
REFUSES(a_filter_with_an_unknown_key, errorOf(R"({"id":"e","when":{"fight":{"colour":"red"}},"do":[]})"), "unknown key \"colour\"")
REFUSES(a_talk_without_a_partner, errorOf(R"({"id":"e","when":{"talk":"eirik"},"do":[]})"), "needs \"to\"")
REFUSES(talking_to_yourself, errorOf(R"({"id":"e","when":{"talk":"eirik","to":"eirik"},"do":[]})"), "cannot talk to itself")
REFUSES(a_talk_with_an_unknown_unit, errorOf(R"({"id":"e","when":{"talk":"eirik","to":"zed"},"do":[]})"), "unknown unit \"zed\"")

// ---- conditions ----
REFUSES(an_unknown_switch_in_a_condition, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"switch":"nope"}],"do":[]})"), "unknown switch \"nope\"")
REFUSES(an_unknown_variable_in_a_condition, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"variable":"nope","value":1}],"do":[]})"), "unknown variable \"nope\"")
REFUSES(an_unknown_comparison, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"variable":"gold","op":"~","value":1}],"do":[]})"), "\"op\" must be one of")
REFUSES(a_variable_condition_without_a_value, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"variable":"gold"}],"do":[]})"), "missing required \"value\"")
REFUSES(two_conditions_in_one_entry, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"switch":"saved","alive":"eirik"}],"do":[]})"), "has two conditions")
REFUSES(something_that_is_not_a_condition, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"colour":1}],"do":[]})"), "is not a condition")
REFUSES(an_unknown_item_in_carries, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"carries":"sock"}],"do":[]})"), "unknown item \"sock\"")
REFUSES(an_unknown_key_in_a_condition, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"switch":"saved","iss":true}],"do":[]})"), "unknown key \"iss\"")
REFUSES(an_unknown_unit_in_alive, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"alive":"zed"}],"do":[]})"), "unknown unit \"zed\"")
REFUSES(a_round_condition_that_is_a_bare_number, errorOf(R"({"id":"e","when":{"battle":"start"},"if":[{"round":5}],"do":[]})"), "\"round\" must be")

// ---- commands ----
REFUSES(something_that_is_not_a_command, commandError(R"({"shout":"hey"})"), "is not a command")
REFUSES(two_commands_in_one_entry, commandError(R"({"say":"hi","removeUnit":"eirik"})"), "has two commands")
REFUSES(an_unknown_key_in_a_command, commandError(R"({"say":"hi","speeker":"eirik"})"), "unknown key \"speeker\"")
REFUSES(a_command_that_is_not_an_object, commandError(R"("say")"), "must be an object")
REFUSES(a_say_with_two_kinds_of_speaker, commandError(R"({"say":"hi","speaker":"eirik","speakerName":"Bob"})"), "not both")
REFUSES(a_say_by_an_unknown_unit, commandError(R"({"say":"hi","speaker":"zed"})"), "unknown unit \"zed\"")
REFUSES(a_say_without_text, commandError(R"({"say":3})"), "must be the text to show")
REFUSES(an_add_unit_of_an_unknown_type, commandError(R"({"addUnit":{"type":"dragon","side":"red","x":1,"y":1}})"), "unknown unit type \"dragon\"")
REFUSES(an_add_unit_for_an_unknown_side, commandError(R"({"addUnit":{"type":"soldier","side":"purple","x":1,"y":1}})"), "unknown side \"purple\"")
REFUSES(an_add_unit_outside_the_map, commandError(R"({"addUnit":{"type":"soldier","side":"red","x":9,"y":1}})"), "position is outside the map")
REFUSES(an_add_unit_with_a_taken_id, commandError(R"({"addUnit":{"type":"soldier","side":"red","x":1,"y":1,"id":"gorm"}})"), "already used by another unit")
REFUSES(an_add_unit_with_a_stray_key, commandError(R"({"addUnit":{"type":"soldier","side":"red","x":1,"y":1,"hp":5}})"), "unknown key \"hp\"")
REFUSES(an_add_unit_that_is_not_an_object, commandError(R"({"addUnit":"soldier"})"), "\"addUnit\" must be a unit")
REFUSES(a_remove_of_an_unknown_unit, commandError(R"({"removeUnit":"zed"})"), "unknown unit \"zed\"")
REFUSES(a_move_of_an_unknown_unit, commandError(R"({"moveUnit":"zed","x":1,"y":1})"), "unknown unit \"zed\"")
REFUSES(a_move_outside_the_map, commandError(R"({"moveUnit":"eirik","x":1,"y":3})"), "is outside the map")
REFUSES(a_move_without_a_square, commandError(R"({"moveUnit":"eirik","x":1})"), "missing required \"y\"")
REFUSES(a_change_to_an_unknown_side, commandError(R"({"changeSide":{"unit":"gorm"},"side":"purple"})"), "unknown side \"purple\"")
REFUSES(a_change_without_a_side, commandError(R"({"changeSide":{"unit":"gorm"}})"), "missing required \"side\"")
REFUSES(an_unknown_item_to_give, commandError(R"({"giveItem":"sock","to":"eirik"})"), "unknown item \"sock\"")
REFUSES(an_item_gift_without_a_receiver, commandError(R"({"giveItem":"potion"})"), "missing required \"to\"")
REFUSES(an_unknown_resource_to_give, commandError(R"({"giveResource":{"gems":3},"side":"blue"})"), "unknown resource \"gems\"")
REFUSES(a_resource_gift_for_an_unknown_side, commandError(R"({"giveResource":{"funds":3},"side":"purple"})"), "unknown side \"purple\"")
REFUSES(a_resource_gift_that_is_not_whole, commandError(R"({"giveResource":{"funds":1.5},"side":"blue"})"), "must be a whole number")
REFUSES(an_unknown_switch_to_set, commandError(R"({"setSwitch":"nope","to":true})"), "unknown switch \"nope\"")
REFUSES(a_switch_set_without_a_value, commandError(R"({"setSwitch":"saved"})"), "missing required \"to\"")
REFUSES(a_switch_set_to_a_number, commandError(R"({"setSwitch":"saved","to":1})"), "\"to\" must be true or false")
REFUSES(an_unknown_variable_to_set, commandError(R"({"setVariable":"nope","to":1})"), "unknown variable \"nope\"")
REFUSES(a_variable_set_to_a_fraction, commandError(R"({"setVariable":"gold","to":1.5})"), "\"to\" must be a whole number")
REFUSES(a_variable_set_to_something_huge, commandError(R"({"setVariable":"gold","to":1500000000})"), "unreasonably large")
REFUSES(a_variable_add_without_by, commandError(R"({"addVariable":"gold"})"), "missing required \"by\"")
REFUSES(a_variable_add_with_to, commandError(R"({"addVariable":"gold","to":1})"), "unknown key \"to\"")
REFUSES(an_objective_with_a_stray_key, commandError(R"({"objective":{"wins":[]}})"), "unknown key \"wins\"")
REFUSES(a_win_for_an_unknown_team, commandError(R"({"win":"purple"})"), "unknown team \"purple\"")
REFUSES(an_if_without_then, commandError(R"({"if":[{"switch":"saved"}]})"), "missing required \"then\"")
REFUSES(an_if_with_a_bad_condition_inside, commandError(R"({"if":[{"switch":"nope"}],"then":[]})"), "unknown switch \"nope\"")
REFUSES(a_bad_command_inside_then, commandError(R"({"if":[],"then":[{"say":"a"},{"win":"purple"}]})"), "\"then\": command 2 (win)")
REFUSES(an_end_turn_outside_a_talk, commandError(R"({"endTurn":true})"), "only works in an event whose trigger is \"talk\"")
REFUSES(a_focus_outside_the_map, commandError(R"({"focus":{"x":20,"y":1}})"), "is outside the map")
REFUSES(a_focus_on_nothing_sensible, commandError(R"({"focus":5})"), "must be a unit id or a square")

// ---- objectives ----
REFUSES(an_unknown_key_in_objectives, objectiveError(R"({"text":"x","winn":[]})"), "unknown key \"winn\"")
REFUSES(objectives_that_are_not_an_object, objectiveError(R"([])"), "must be an object")
REFUSES(win_that_is_not_a_list, objectiveError(R"({"win":{}})"), "\"win\" must be a list")
REFUSES(an_objective_entry_that_is_not_an_object, objectiveError(R"({"win":["x"]})"), "must be an object")
REFUSES(an_objective_entry_of_no_known_kind, objectiveError(R"({"win":[{"kill":"gorm"}]})"), "is not an objective")
REFUSES(an_objective_entry_of_two_kinds, objectiveError(R"({"win":[{"round":3,"survive":3}]})"), "has two kinds")
REFUSES(an_objective_that_defeats_an_unknown_unit, objectiveError(R"({"win":[{"defeated":{"unit":"zed"}}]})"), "unknown unit \"zed\"")
REFUSES(an_objective_reach_without_an_area, objectiveError(R"({"win":[{"reach":5}]})"), "must be an area")
REFUSES(an_objective_round_of_zero, objectiveError(R"({"lose":[{"round":0}]})"), "round number from 1")
REFUSES(survive_as_a_way_to_lose, objectiveError(R"({"lose":[{"survive":5}]})"), "way to win")
REFUSES(an_objective_text_that_is_not_text, objectiveError(R"({"text":5})"), "\"text\" must be a string")

TEST(story_loader_messages_name_the_map_the_event_and_the_command) {
  std::string e = commandError(R"({"say":"ok"},{"giveItem":"sock","to":"eirik"})");
  CHECK(has(e, "test.json: map \"test\": event \"ev\": \"do\": command 2 (giveItem): "));
}

// The "two commands" message, exactly, for a pair drawn from each table of command keys (the first key is always
// the one found first, so this also pins the order of the tables).
TEST(story_loader_names_both_commands_when_an_entry_has_two) {
  const char* later[] = {"removeUnit", "promote", "openDoor", "addCondition", "setBehaviour",
                         "setFog", "setPhaseMusic", "showUnit", "wait"};
  const char* values[] = {"\"eirik\"", "\"eirik\"", "\"d\"", "\"x\"", "\"x\"", "true", "{}", "\"eirik\"", "5"};
  for (size_t i = 0; i < 9; ++i) {
    std::string e = commandError(std::string("{\"say\":\"hi\",\"") + later[i] + "\":" + values[i] + "}");
    const std::string tail = std::string("has two commands (say and ") + later[i] + "); use one entry for each";
    CHECK(e.size() > tail.size() + 2 && e.compare(e.size() - tail.size(), tail.size(), tail) == 0);
    if (e.size() > tail.size() + 2) CHECK(e.compare(e.size() - tail.size() - 2, 2, ": ") == 0);
  }
}
