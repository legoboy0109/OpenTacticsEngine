// Story state in saves (docs/SAVES.md): a round trip in the middle of a battle with events half
// fired, refusal of damaged story fields, the data hash rules, determinism, and that games
// without a story keep their saves and hashes exactly.
#include <sys/stat.h>

#include <cstdio>

#include "save_helpers.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

std::vector<std::string> field() { return {".........", ".........", "........."}; }

const char* kEvents = R"([
  {"id":"setup","when":{"battle":"start"},"do":[{"setSwitch":"saved","to":true},{"setVariable":"gold","to":7},
     {"addUnit":{"type":"archer","side":"red","x":6,"y":2,"id":"scout"}}]},
  {"id":"second","when":{"turn":2},"do":[{"say":"Round two."},{"objective":{"text":"Hold on","win":[{"round":50}]}}]},
  {"id":"third","when":{"turn":3},"do":[{"say":"Round three."},{"addVariable":"gold","by":1}]},
  {"id":"walk","when":{"reach":{"x":3,"y":1}},"repeat":true,"do":[{"addVariable":"kills","by":1}]}])";

// A battle with events half fired: round 2 has begun, "third" has not run.
struct Half {
  StoryWorld w{field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"}};
  bool ok() {
    return w.play(kEvents, R"({"text":"Start","win":[{"defeated":"gorm"}]})") && w.act(Action::move(0, w.at(3, 1))) &&
           w.act(Action::endTurn()) && w.act(Action::endTurn());
  }
  // A fresh battle of the same map, not started (what restoring needs).
  bool fresh(Battle& b) {
    std::string err;
    return b.init(w.db, w.map, w.formulas, 1, err);
  }
};

std::string restoreError(Half& h, const Json& saved, std::string* stateBefore = nullptr, std::string* stateAfter = nullptr) {
  Battle r;
  std::string err;
  if (!h.fresh(r)) return "init failed";
  if (stateBefore) *stateBefore = savetest::stateText(r);
  bool ok = restoreBattleState(r, saved, err);
  if (stateAfter) *stateAfter = savetest::stateText(r);
  return ok ? "" : err;
}

// Replaces story.<key> / the story object / a unit's key in a copy of a save.
Json withStory(const Json& save, const char* key, const Json& value) {
  Json out = save, story = save["story"];
  story.set(key, value);
  out.set("story", story);
  return out;
}
Json withUnitKey(const Json& save, size_t unit, const char* key, const Json& value) {
  Json out = save, units = Json::makeArray();
  for (size_t i = 0; i < save["units"].size(); ++i) {
    Json u = save["units"][i];
    if (i == unit) u.set(key, value);
    units.push(u);
  }
  out.set("units", units);
  return out;
}
Json list(std::initializer_list<Json> items) {
  Json a = Json::makeArray();
  for (const Json& j : items) a.push(j);
  return a;
}
Json str(const char* s) { return Json::makeString(s); }
Json num(double d) { return Json::makeNumber(d); }

}  // namespace

TEST(story_save_round_trip_in_the_middle_of_a_battle) {
  Half h;
  CHECK(h.ok());
  const Battle& b = h.w.battle;
  CHECK(h.w.fired("setup") && h.w.fired("second") && !h.w.fired("third") && h.w.fired("walk"));
  Json saved;
  saveBattleState(b, saved);
  CHECK(saved.has("story"));
  CHECK_EQ(saved["story"]["fired"].size(), (size_t)3);
  CHECK_STR(saved["units"][2]["id"].asString(), "scout");  // the unit an event added keeps its id
  CHECK(saved["units"][0].has("id"));                      // map units carry theirs too: the list is rebuilt from the save

  Battle r;
  CHECK(h.fresh(r));
  std::string err;
  CHECK(restoreBattleState(r, saved, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK_STR(savetest::stateText(r), savetest::stateText(b));
  CHECK_EQ(r.storyState.variables[0], 7);
  CHECK(r.storyState.switches[0] == 1);
  CHECK_STR(r.objectiveText(), "Hold on");
  CHECK(r.unitByStoryId("scout") == 2 && r.unitByStoryId("eirik") == 0);

  // Played on from the same point, original and restored do exactly the same: "second" does not run
  // again, "third" does, "walk" (repeating) still can.
  Battle a = b;
  std::vector<Event> ea, er;
  for (int i = 0; i < 2; ++i) {
    CHECK(a.apply(Action::endTurn(), ea));
    CHECK(r.apply(Action::endTurn(), er));
  }
  CHECK(savetest::sameEvents(ea, er));
  int said = 0, changed = 0;
  for (const Event& e : er) {
    said += e.type == EventType::Dialogue;
    changed += e.type == EventType::ObjectiveChanged;
  }
  CHECK(said == 1 && changed == 0);
  CHECK_EQ(r.storyState.variables[0], 8);
  CHECK_STR(savetest::stateText(r), savetest::stateText(a));
}

TEST(story_save_of_a_game_without_story_is_unchanged) {
  fixture::World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 4, 0, false}});
  CHECK(w.begin());
  Json saved;
  saveBattleState(w.battle, saved);
  CHECK(!saved.has("story"));
  for (const Json& u : saved["units"].items()) CHECK(!u.has("id"));
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  Json bogus = saved;
  bogus.set("story", Json::makeObject());
  CHECK(!restoreBattleState(r, bogus, err) && err.find("battle.story") != std::string::npos);  // not from this game
}

TEST(story_save_refuses_damaged_story_fields_and_changes_nothing) {
  Half h;
  CHECK(h.ok());
  Json saved;
  saveBattleState(h.w.battle, saved);
  struct Case { const char* name; Json save; const char* needle; };
  Json noStory = saved;
  noStory.erase("story");
  Json storyNotObject = saved;
  storyNotObject.set("story", num(3));
  std::vector<Case> cases = {
      {"no story", noStory, "story"},
      {"story not an object", storyNotObject, "story"},
      {"switches wrong length", withStory(saved, "switches", list({Json::makeBool(true)})), "switches"},
      {"switch not true/false", withStory(saved, "switches", list({num(1), num(0)})), "switches[0]"},
      {"switches missing", withStory(saved, "switches", str("on")), "switches"},
      {"variable with a fraction", withStory(saved, "variables", list({num(1.5), num(0)})), "variables[0]"},
      {"variable as text", withStory(saved, "variables", list({num(1), str("x")})), "variables[1]"},
      {"variable too big", withStory(saved, "variables", list({num(2e9), num(0)})), "variables[0]"},
      {"variables wrong length", withStory(saved, "variables", list({num(1)})), "variables"},
      {"fired: unknown event", withStory(saved, "fired", list({str("nope")})), "no event"},
      {"fired: twice", withStory(saved, "fired", list({str("setup"), str("setup")})), "twice"},
      {"fired: not text", withStory(saved, "fired", list({num(1)})), "fired[0]"},
      {"fired: not a list", withStory(saved, "fired", str("all")), "fired"},
      {"objectives past the end", withStory(saved, "objectives", num(5)), "objectives"},
      {"objectives negative", withStory(saved, "objectives", num(-1)), "objectives"},
      {"objectives fraction", withStory(saved, "objectives", num(0.5)), "objectives"},
      {"unit id with spaces", withUnitKey(saved, 1, "id", str("a b")), "units[1].id"},
      {"unit id not text", withUnitKey(saved, 1, "id", num(4)), "units[1].id"},
      {"two units with one id", withUnitKey(saved, 1, "id", str("eirik")), "belongs to another unit"},
  };
  for (const Case& c : cases) {
    std::string before, after;
    std::string e = restoreError(h, c.save, &before, &after);
    if (e.empty() || e.find(c.needle) == std::string::npos) std::printf("  case '%s': got \"%s\", wanted \"%s\"\n", c.name, e.c_str(), c.needle);
    CHECK(!e.empty() && e.find(c.needle) != std::string::npos);
    CHECK_STR(after, before);                              // nothing applied
  }
  CHECK_STR(restoreError(h, saved), "");                   // and the undamaged one is fine
}

TEST(story_starting_switches_and_variables_can_be_handed_in_and_read_back) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.load(R"([
    {"id":"gate","when":{"battle":"start"},"if":[{"switch":"saved"},{"variable":"gold","op":"=","value":4}],"do":[{"addVariable":"gold","by":10}]},
    {"id":"nogate","when":{"battle":"start"},"if":[{"switch":"alarm"}],"do":[]}])").empty());
  std::string err;
  CHECK(w.battle.init(w.db, w.map, w.formulas, 1, err));
  CHECK(!w.battle.setStartingStory({1}, {4, 0}));          // sizes must match the game's lists
  CHECK(w.battle.setStartingStory({1, 0}, {4, 0}));        // a campaign carries these over
  w.battle.start(w.events);
  CHECK(w.fired("gate") && !w.fired("nogate"));
  CHECK_EQ(w.battle.storyState.variables[0], 14);          // and reads the final values after the battle

  StoryWorld plain(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(plain.begin());                                    // a standalone battle starts everything off / 0
  CHECK(!plain.sw("saved") && !plain.sw("alarm") && plain.var("gold") == 0 && plain.var("kills") == 0);
}

TEST(story_same_seed_same_event_stream) {
  auto run = [](std::vector<Event>& events, std::string& state) {
    StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 0, 2, false}, {"soldier", 1, 8, 1, false},
                           {"archer", 1, 8, 2, false}},
                 {"eirik", "lyra", "gorm", "sniper"});
    w.db.rules.progression = true;
    w.db.rules.fXpHit = "10";
    w.db.rules.fXpKill = "30";
    w.db.rules.fHit = "70";
    CHECK(w.play(R"([
      {"id":"every","when":{"turn":"every"},"do":[{"addVariable":"kills","by":1}]},
      {"id":"mid","when":{"reach":{"x":3,"y":0,"w":3,"h":3}},"repeat":true,"do":[{"addVariable":"gold","by":1},
         {"addUnit":{"type":"soldier","side":"red","x":7,"y":0}}]},
      {"id":"fall","when":{"defeated":{"side":"red"}},"repeat":true,"do":[{"say":"one down"}]},
      {"id":"fight","when":{"fight":{"side":"blue"}},"repeat":true,"do":[{"addVariable":"gold","by":2}]},
      {"id":"end","when":{"battle":"end"},"do":[{"say":"over"}]}])",
                 R"({"win":[{"defeated":{"side":"red"}}],"lose":[{"round":40}]})", 99));
    for (int i = 0; i < 4000 && !w.battle.over; ++i) aiTakeStep(w.battle, w.events);
    CHECK(w.battle.over);
    events = w.events;
    state = savetest::stateText(w.battle);
  };
  std::vector<Event> a, b;
  std::string sa, sb;
  run(a, sa);
  run(b, sb);
  CHECK(a.size() > 20);
  CHECK(savetest::sameEvents(a, b));
  CHECK_STR(sa, sb);
}

// ---- the data hash ----

TEST(story_hash_of_the_demo_games_is_exactly_what_it_was) {
  // Computed before story events existed. A game without events, objectives, switches or variables
  // must keep its hash, or every save made so far would be refused.
  struct Golden { const char* game; const char* map; const char* hash; };
  const Golden golden[] = {{"demo-srpg", "bridge", "5d561ac273e900ee"}, {"demo-economy", "twin_rivers", "1e44dae676a88a38"}};
  for (const Golden& g : golden) {
    Database db;
    std::string err, hash;
    CHECK(loadGame(ote_test::sourceDir() + "/games/" + g.game, db, err));
    CHECK(computeDataHash(db, g.map, hash, err));
    CHECK_STR(hash, g.hash);
    // And their saves carry nothing new.
    savetest::Rig rig;
    CHECK(rig.loadDemo(g.game, g.map));
    rig.start();
    Json saved;
    saveBattleState(rig.battle, saved);
    CHECK(!saved.has("story"));
    for (const Json& u : saved["units"].items()) CHECK(!u.has("id"));
  }
}

namespace {

// A tiny game folder whose map and game.json extras the test chooses; returns the data hash.
struct HashGame {
  std::string root = ote_test::tempDir() + "/story_hash_game";
  HashGame() {
    mkdir(root.c_str(), 0755);
    mkdir((root + "/data").c_str(), 0755);
    mkdir((root + "/maps").c_str(), 0755);
    CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"} ] }"));
    CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"sword\"}] }"));
    CHECK(writeTextFile(root + "/data/units.json",
                        "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"str\":2,\"mov\":3}}] }"));
  }
  std::string hash(const std::string& gameExtra, const std::string& mapExtra) {
    CHECK(writeTextFile(root + "/game.json",
                        "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"str\"},{\"id\":\"mov\"}],"
                        " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                        " \"formulas\": {\"damage\":\"a.str\"}," + gameExtra +
                        " \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
    CHECK(writeTextFile(root + "/maps/m.json",
                        "{ \"grid\": {\"type\":\"square\",\"width\":4,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"....\"],"
                        " \"sides\": [{\"id\":\"a\"},{\"id\":\"b\"}],"
                        " \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0,\"id\":\"hero\"},"
                        "            {\"type\":\"grunt\",\"side\":\"b\",\"x\":3,\"y\":0}]" + mapExtra + " }"));
    Database db;
    std::string err, h;
    if (!loadGame(root, db, err) || !computeDataHash(db, "m", h, err)) std::printf("  hash game: %s\n", err.c_str());
    return h;
  }
};

// Events and objectives with the wording kept apart: %s = say text, speaker name, label, objective text.
std::string eventsWith(const char* say, const char* name, const char* label, const char* objective) {
  std::string s = std::string(",\"events\":[{\"id\":\"e\",\"when\":{\"turn\":2},\"do\":[{\"say\":\"") + say +
                  "\",\"speaker\":\"hero\",\"portrait\":\"calm\"},{\"say\":\"Hm.\",\"speakerName\":\"" + name +
                  "\"},{\"setSwitch\":\"s\",\"to\":true}]},{\"id\":\"t\",\"when\":{\"talk\":\"hero\",\"to\":\"hero2\",\"label\":\"" +
                  label + "\"},\"do\":[]}],\"objectives\":{\"text\":\"" + objective + "\",\"win\":[{\"round\":5}]}";
  return s;
}

}  // namespace

TEST(story_hash_counts_the_rules_of_events_but_not_their_wording) {
  HashGame g;
  const std::string sw = "\"switches\":[{\"id\":\"s\",\"name\":\"Switch\"}],";
  // The hash only reads the map's text and never loads its events, so these need not be a valid story.
  std::string base = g.hash(sw, eventsWith("Hello", "Bob", "Talk", "Win it"));
  CHECK(!base.empty());
  CHECK_STR(g.hash(sw, eventsWith("Hello", "Bob", "Talk", "Win it")), base);
  // Wording: what is said, who says it (a free name), the Talk label, the objective text, a switch's name.
  CHECK_STR(g.hash(sw, eventsWith("Entirely different words", "Bob", "Talk", "Win it")), base);
  CHECK_STR(g.hash(sw, eventsWith("Hello", "Alice", "Talk", "Win it")), base);
  CHECK_STR(g.hash(sw, eventsWith("Hello", "Bob", "Speak", "Win it")), base);
  CHECK_STR(g.hash(sw, eventsWith("Hello", "Bob", "Talk", "Win it, quickly")), base);
  CHECK_STR(g.hash("\"switches\":[{\"id\":\"s\",\"name\":\"Another name\"}],", eventsWith("Hello", "Bob", "Talk", "Win it")), base);
  // Rules: anything else in an event changes the hash.
  const std::string good = eventsWith("Hello", "Bob", "Talk", "Win it");
  auto changed = [&](const char* from, const char* to) {
    std::string e = good;
    size_t at = e.find(from);
    CHECK(at != std::string::npos);
    e.replace(at, std::string(from).size(), to);
    return g.hash(sw, e) != base;
  };
  CHECK(changed("\"turn\":2", "\"turn\":3"));
  CHECK(changed("\"speaker\":\"hero\"", "\"speaker\":\"hero2\""));
  CHECK(changed("\"portrait\":\"calm\"", "\"portrait\":\"angry\""));
  CHECK(changed("\"to\":true", "\"to\":false"));
  CHECK(changed("\"round\":5", "\"round\":6"));
  CHECK(changed("\"id\":\"e\"", "\"id\":\"f\""));
  CHECK(changed("{\"say\":\"Hm.\",\"speakerName\":\"Bob\"},", ""));  // one line fewer
  CHECK(changed("{\"say\":\"Hm.\",\"speakerName\":\"Bob\"}", "{\"removeUnit\":\"hero\"}"));  // a line turned into another command
  CHECK(g.hash(sw + "\"variables\":[{\"id\":\"v\"}],", good) != base);
  CHECK(g.hash("\"switches\":[{\"id\":\"s2\"}],", good) != base);
  CHECK(g.hash("", good) != base);                         // switches count as rules when present
}
