// A story scene on another map (docs/EVENTS.md "Showing another map"): showMap / returnMap, cut-scene maps, the loader's checks, and that
// the battle itself is left as it was. Game: tests/fixtures/scene-map ("keep" plays a flashback on the cut-scene map "flashback").
#include <cstring>

#include "core/ai/ai.h"
#include "core/campaign/playthrough.h"
#include "save_helpers.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// A writable copy of the fixture.
struct SCopy {
  std::string dir;
  explicit SCopy(const std::string& name) {
    dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    static const char* const files[] = {"game.json", "data/terrain.json", "data/weapons.json", "data/items.json", "data/units.json", "data/skills.json",
                                        "data/structures.json", "maps/keep.json", "maps/field.json", "maps/flashback.json"};
    for (const char* f : files) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/scene-map/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
  void put(const std::string& rel, const std::string& text) { writeTextFile(dir + "/" + rel, text); }
  std::string read(const std::string& rel) const {
    std::string text;
    readTextFile(dir + "/" + rel, text);
    return text;
  }
  void swap(const std::string& rel, const std::string& from, const std::string& to) {
    std::string text = read(rel);
    const size_t at = text.find(from);
    if (at == std::string::npos) std::printf("  the test's text to replace is missing: %s\n", from.c_str());
    else put(rel, text.replace(at, from.size(), to));
  }
  // The keep's events replaced by `events` (a JSON list).
  void events(const std::string& events) {
    std::string text = read("maps/keep.json");
    put("maps/keep.json", text.substr(0, text.find("\"events\"")) + "\"events\": " + events + " }\n");
  }
  std::string loadError(const char* map = "keep") const {
    Database db;
    MapDef m;
    std::string err;
    if (!loadGame(dir, db, err)) return err;
    if (!loadMap(db, map, m, err)) return err;
    return "";
  }
};

// The keep with one battle-start event whose commands are `commands`: the loader's message ("" = loads).
std::string withStart(const char* name, const std::string& commands, const char* when = "{ \"battle\": \"start\" }") {
  SCopy c(name);
  c.events(std::string("[ { \"id\": \"e\", \"when\": ") + when + ", \"do\": " + commands + " } ]");
  return c.loadError();
}

struct Played {
  SCopy copy;
  savetest::Rig rig;
  std::vector<Event> events;
  explicit Played(const char* name, const std::string& eventsJson = "") : copy(name) {
    if (!eventsJson.empty()) copy.events(eventsJson);
    const bool loaded = rig.load(copy.dir, "keep");
    CHECK(loaded);
    if (loaded) rig.battle.start(events);  // (a battle that did not load cannot start)
  }
  Battle& b() { return rig.battle; }
  int count(EventType t) const {
    int n = 0;
    for (const Event& e : events) n += e.type == t;
    return n;
  }
  int index(EventType t, size_t from = 0) const {
    for (size_t i = from; i < events.size(); ++i)
      if (events[i].type == t) return (int)i;
    return -1;
  }
};

// Every unit's square, hit points and life, as text.
std::string unitsText(const Battle& b) {
  std::string s;
  for (const Unit& u : b.units)
    s += u.storyId + "@" + std::to_string(u.cell) + ":" + std::to_string(u.hp) + (u.alive ? "" : "x") + " ";
  return s;
}

}  // namespace

// ---- loader ----

TEST(show_map_fixture_loads_and_marks_the_cut_scene_map) {
  SCopy c("sm_load");
  CHECK_STR(c.loadError("keep"), "");
  CHECK_STR(c.loadError("flashback"), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.cutSceneMap("flashback"));
  CHECK(!db.cutSceneMap("keep"));
  CHECK(!freeBattleAllowed(db, "flashback"));  // never offered as a battle
  CHECK(freeBattleAllowed(db, "keep"));
  MapDef keep;
  CHECK(loadMap(db, "keep", keep, err));
  const Command& show = keep.story->events[0].commands[2];
  CHECK(show.kind == Command::Kind::ShowMap);
  CHECK(show.shownMap != nullptr && show.shownMap->id == "flashback");
  CHECK(!show.shownMap->story);  // loaded without events
  CHECK(keep.story->events[0].commands[12].kind == Command::Kind::ReturnMap);
}

TEST(show_map_cut_scene_flag_must_be_a_bool_and_its_map_has_no_story) {
  SCopy c("sm_flag");
  c.swap("game.json", "\"cutScene\": true", "\"cutScene\": \"yes\"");
  CHECK(has(c.loadError(), "cutScene"));
  SCopy d("sm_story");
  d.swap("maps/flashback.json", "\"units\": [", "\"events\": [], \"units\": [");
  CHECK(has(d.loadError("flashback"), "\"events\" makes no sense on a cut-scene map"));
  SCopy e("sm_obj");
  e.swap("maps/flashback.json", "\"units\": [", "\"objectives\": {}, \"units\": [");
  CHECK(has(e.loadError("flashback"), "\"objectives\" makes no sense on a cut-scene map"));
}

TEST(show_map_loader_refuses_wrong_maps_and_nesting) {
  CHECK(has(withStart("sm_l1", "[ { \"showMap\": \"nowhere\" } ]"), "names unknown map \"nowhere\""));
  CHECK(has(withStart("sm_l2", "[ { \"showMap\": \"keep\" } ]"), "names this map itself"));
  CHECK(has(withStart("sm_l3", "[ { \"showMap\": \"flashback\" }, { \"showMap\": \"field\" } ]"), "is shown already"));
  CHECK(has(withStart("sm_l4", "[ { \"returnMap\": true } ]"), "nothing to return from"));
  CHECK(has(withStart("sm_l5", "[ { \"showMap\": \"flashback\" }, { \"returnMap\": false } ]"), "write { \"returnMap\": true }"));
  CHECK(has(withStart("sm_l6", "[ { \"if\": [ { \"switch\": \"remembered\" } ], \"then\": [ { \"showMap\": \"flashback\" } ] } ]"),
            "goes in the event's own \"do\" list"));
  CHECK(has(withStart("sm_l7", "[ { \"showMap\": \"flashback\", \"camera\": 1 } ]"), "unknown key \"camera\""));
  CHECK(has(withStart("sm_l8", "[ { \"showMap\": \"flashback\", \"focus\": { \"x\": 8, \"y\": 0 } } ]"), "outside map \"flashback\""));
  CHECK(has(withStart("sm_l9", "[ { \"showMap\": \"flashback\", \"focus\": \"hero\" } ]"), "a unit of the battle's own map"));
  CHECK(has(withStart("sm_l10", "[ { \"showMap\": \"flashback\", \"say\": \"x\" } ]"), "has two commands"));
  CHECK_STR(withStart("sm_l11", "[ { \"showMap\": \"field\", \"focus\": { \"x\": 5, \"y\": 2 } }, { \"moveUnit\": \"r1\", \"x\": 0, \"y\": 0 } ]"), "");
}

TEST(show_map_loader_checks_the_section_against_the_shown_map) {
  // Units of the shown map and the section's own additions are known; the battle's own are not (and the message says why).
  CHECK_STR(withStart("sm_s1", "[ { \"showMap\": \"flashback\" }, { \"addUnit\": { \"id\": \"kid\", \"type\": \"thief\", \"side\": \"elders\", \"x\": 0, \"y\": 0 } },"
                                " { \"moveUnit\": \"kid\", \"x\": 2, \"y\": 3 }, { \"say\": \"Run!\", \"speaker\": \"kid\" }, { \"removeUnit\": \"bandit\" } ]"), "");
  CHECK(has(withStart("sm_s2", "[ { \"showMap\": \"flashback\" }, { \"moveUnit\": \"hero\", \"x\": 0, \"y\": 0 } ]"), "\"hero\", a unit of the battle's own map"));
  CHECK(has(withStart("sm_s3", "[ { \"showMap\": \"flashback\" }, { \"say\": \"x\", \"speaker\": \"wing\" } ]"), "speakerName"));
  CHECK(has(withStart("sm_s4", "[ { \"showMap\": \"flashback\" }, { \"addUnit\": { \"type\": \"thief\", \"side\": \"blue\", \"x\": 0, \"y\": 0 } } ]"),
            "\"blue\""));  // the shown map's sides are elders / bandits
  CHECK(has(withStart("sm_s5", "[ { \"showMap\": \"flashback\" }, { \"moveUnit\": \"elder\", \"x\": 8, \"y\": 0 } ]"), "outside the map"));
  // A unit the section adds is not a unit of the battle's map after the return.
  CHECK(has(withStart("sm_s6", "[ { \"showMap\": \"flashback\" }, { \"addUnit\": { \"id\": \"kid\", \"type\": \"thief\", \"side\": \"elders\", \"x\": 0, \"y\": 0 } },"
                               " { \"returnMap\": true }, { \"moveUnit\": \"kid\", \"x\": 0, \"y\": 0 } ]"),
            "unknown unit \"kid\""));
  // After the return the battle's own units are known again.
  CHECK_STR(withStart("sm_s7", "[ { \"showMap\": \"flashback\" }, { \"returnMap\": true }, { \"moveUnit\": \"hero\", \"x\": 2, \"y\": 1 } ]"), "");
  // An `if` judges the battle itself: its conditions name the battle's units; its branches act on the shown map.
  CHECK_STR(withStart("sm_s8", "[ { \"showMap\": \"flashback\" }, { \"if\": [ { \"alive\": \"hero\" } ], \"then\": [ { \"moveUnit\": \"elder\", \"x\": 2, \"y\": 0 } ] } ]"), "");
  CHECK_STR(withStart("sm_s9", "[ { \"showMap\": \"flashback\" }, { \"setVariable\": \"seen\", \"level\": \"hero\" } ]"), "");
}

TEST(show_map_loader_refuses_what_acts_on_the_battle_inside_a_section) {
  const char* const refused[] = {"{ \"win\": \"blue\" }", "{ \"giveItem\": \"potion\", \"to\": \"elder\" }", "{ \"giveResource\": { \"gold\": 5 }, \"side\": \"blue\" }",
                                 "{ \"objective\": {} }", "{ \"setFog\": true }"};
  int n = 0;
  for (const char* r : refused) {
    const std::string msg = withStart(("sm_r" + std::to_string(n++)).c_str(), std::string("[ { \"showMap\": \"flashback\" }, ") + r + " ]");
    CHECK(has(msg, "acts on the battle itself and cannot run while another map is shown"));
  }
  CHECK(has(withStart("sm_r9", "[ { \"showMap\": \"flashback\" }, { \"removeUnit\": \"elder\", \"hide\": true } ]"), "\"hide\" keeps a unit"));
  // ... and after the return they are fine again.
  CHECK_STR(withStart("sm_r10", "[ { \"showMap\": \"flashback\" }, { \"returnMap\": true }, { \"giveItem\": \"potion\", \"to\": \"hero\" } ]"), "");
}

TEST(show_map_before_prep_allows_a_staged_fight_on_the_shown_map_only) {
  const char* bp = "{ \"battle\": \"beforePrep\" }";
  CHECK_STR(withStart("sm_bp1", "[ { \"showMap\": \"flashback\" }, { \"fight\": { \"attacker\": \"elder\", \"defender\": \"bandit\" } } ]", bp), "");
  CHECK(has(withStart("sm_bp2", "[ { \"fight\": { \"attacker\": \"hero\", \"defender\": \"r1\" } } ]", bp), "makes no sense in a \"beforePrep\" event"));
  CHECK(has(withStart("sm_bp3", "[ { \"showMap\": \"flashback\" }, { \"returnMap\": true }, { \"fight\": { \"attacker\": \"hero\", \"defender\": \"r1\" } } ]", bp),
            "makes no sense in a \"beforePrep\" event"));
}

TEST(show_map_a_cut_scene_map_is_never_a_chapter_or_a_free_battle) {
  SCopy c("sm_camp");
  c.put("data/campaign.json", R"({ "chapters": [ { "id": "c1", "map": "flashback" } ] })");
  CHECK(has(c.loadError(), "a cut-scene map"));
  SCopy d("sm_camp2");
  d.put("data/campaign.json", R"({ "chapters": [ { "id": "c1", "map": "keep" } ], "freeBattles": ["field", "flashback"] })");
  CHECK(has(d.loadError(), "a cut-scene map"));
  SCopy e("sm_camp3");
  e.put("data/campaign.json", R"({ "chapters": [ { "id": "c1", "map": "keep" } ], "freeBattles": "all" })");
  CHECK_STR(e.loadError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(e.dir, db, err));
  CHECK(!freeBattleAllowed(db, "flashback"));  // "all" means the playable ones
  CHECK(freeBattleAllowed(db, "field"));
}

// ---- the battle ----

TEST(show_map_plays_the_scene_on_the_other_map_and_leaves_the_battle_as_it_was) {
  Played p("sm_play");
  Played plain("sm_plain", "[]");  // the same battle with no scene
  // The battle's own units, squares and hit points are what they would be without the scene.
  CHECK_STR(unitsText(p.b()), unitsText(plain.b()));
  CHECK_EQ((int)p.b().units.size(), 3);
  CHECK(p.b().rng.state == plain.b().rng.state);  // and its random numbers are where they were
  // Game state set on purpose stays; the event ran once.
  CHECK(p.b().storyState.switches[(size_t)p.rig.db.findSwitch("remembered")] != 0);
  CHECK_EQ(p.b().storyState.variables[(size_t)p.rig.db.findVariable("seen")], 1);
  CHECK(p.b().storyState.fired[0] != 0);
  CHECK(!p.b().over);
  // The stream: the hero's line, the map shown, the scene's entries, the return, the hero's line.
  const int shown = p.index(EventType::MapShown), back = p.index(EventType::MapReturned);
  CHECK(shown > 0 && back > shown);
  CHECK_EQ(p.count(EventType::MapShown), 1);
  CHECK_EQ(p.events[(size_t)shown].a, 0);
  CHECK_EQ(p.events[(size_t)shown].b, p.rig.db.findMap("flashback"));
  CHECK_EQ(p.events[(size_t)back].a, 0);
  CHECK(p.index(EventType::Attack, (size_t)shown) > shown && p.index(EventType::Attack, (size_t)shown) < back);
  CHECK(p.index(EventType::UnitDied, (size_t)shown) > shown && p.index(EventType::UnitDied, (size_t)shown) < back);
  CHECK(p.index(EventType::UnitAdded, (size_t)shown) < back && p.index(EventType::UnitRelocated, (size_t)shown) < back);
  CHECK_EQ(p.index(EventType::BattleEnded), -1);  // the bandits lost their last unit on the shown map: nothing ends
  CHECK_EQ(p.index(EventType::SideDefeated), -1);
  CHECK(p.index(EventType::Dialogue, (size_t)back) > back);  // "Never again." after the return
  // The shown map as set up and as the scene left it (what the player plays the entries on).
  CHECK_EQ((int)p.b().shownMaps.size(), 1);
  const Battle::ShownMap& sm = p.b().shownMaps[0];
  CHECK(sm.start && sm.end && sm.def);
  CHECK(sm.end->sceneOnly());
  CHECK_EQ((int)sm.start->units.size(), 2);
  CHECK_EQ((int)sm.end->units.size(), 3);  // + the brute
  // UnitAdded carries the hit points the brute appeared with (the player's bars read them, not the end state, where a scene's unit
  // may already be gone: a flashback's fight showed 0 HP bars before).
  const int added = p.index(EventType::UnitAdded, (size_t)shown);
  const int brute = sm.end->unitByStoryId("brute");
  CHECK(added > shown && brute >= 0);
  if (added > shown && brute >= 0) {
    CHECK_EQ(p.events[(size_t)added].a, brute);
    CHECK_EQ(p.events[(size_t)added].c, sm.end->maxHp(sm.end->units[(size_t)brute]));
  }
  const int elder = sm.end->unitByStoryId("elder"), bandit = sm.end->unitByStoryId("bandit");
  CHECK(sm.end->units[(size_t)bandit].alive == false);
  CHECK_EQ(sm.end->units[(size_t)elder].cell, sm.end->grid.at(4, 1));
  CHECK_EQ(sm.start->units[(size_t)elder].cell, sm.start->grid.at(1, 1));
  CHECK_EQ(sm.focus, sm.start->grid.at(1, 1));  // "focus": "elder"
  CHECK_EQ(p.events[(size_t)shown].c, sm.focus);
  // The entries of the scene name the shown map's units: the dialogue's speaker is the elder there.
  const int line = p.index(EventType::Dialogue, (size_t)shown);
  CHECK(line < back && p.events[(size_t)line].a == elder);
  CHECK_STR(sm.end->storyText(p.events[(size_t)line].b), "Leave this village.");
  // The fade out before the return is faded in by the scene itself; nothing was left dark.
  CHECK(!p.b().storyState.faded);
}

TEST(show_map_the_end_of_the_event_implies_the_return) {
  Played p("sm_implied", R"([ { "id": "a", "when": { "battle": "start" }, "do": [ { "showMap": "flashback" }, { "moveUnit": "elder", "x": 3, "y": 3 } ] },
                            { "id": "b", "when": { "battle": "start" }, "do": [ { "say": "Later.", "speaker": "hero" } ] } ])");
  const int back = p.index(EventType::MapReturned);
  CHECK(back > p.index(EventType::UnitRelocated));
  CHECK(p.index(EventType::Dialogue) > back);  // the next event plays on the battle's map
  CHECK_EQ(p.events[(size_t)p.index(EventType::Dialogue)].a, p.b().unitByStoryId("hero"));
  CHECK_EQ(p.b().units[(size_t)p.b().unitByStoryId("hero")].cell, p.b().grid.at(1, 1));
}

TEST(show_map_real_rolls_on_the_shown_map_leave_the_battles_dice_alone) {
  const char* scene = R"([ { "id": "a", "when": { "battle": "start" }, "do": [ { "showMap": "flashback" }, { "moveUnit": "elder", "x": 4, "y": 1 },
                           { "fight": { "attacker": "elder", "defender": "bandit" } }, { "fight": { "attacker": "elder", "defender": "bandit" } } ] } ])";
  Played p("sm_dice", scene);
  Played again("sm_dice2", scene);
  Played plain("sm_dice3", "[]");
  CHECK(p.count(EventType::Attack) > 0);
  CHECK(p.b().rng.state == plain.b().rng.state);
  CHECK_EQ(p.count(EventType::Attack), again.count(EventType::Attack));  // the same scene plays the same way
  CHECK_STR(unitsText(*p.b().shownMaps[0].end), unitsText(*again.b().shownMaps[0].end));
}

TEST(show_map_twice_in_one_event_and_in_a_later_action) {
  Played p("sm_twice", R"([ { "id": "a", "when": { "battle": "start" }, "do": [ { "showMap": "flashback" }, { "returnMap": true },
                            { "showMap": "field", "focus": { "x": 5, "y": 2 } }, { "removeUnit": "r1" } ] },
                            { "id": "late", "when": { "turn": 2 }, "do": [ { "showMap": "flashback" }, { "removeUnit": "bandit" } ] } ])");
  CHECK_EQ(p.count(EventType::MapShown), 2);
  CHECK_EQ(p.count(EventType::MapReturned), 2);
  CHECK_EQ((int)p.b().shownMaps.size(), 2);
  CHECK(p.b().units[(size_t)p.b().unitByStoryId("r1")].alive);  // the field's r1 was removed, not the keep's
  CHECK_EQ(p.b().shownMaps[1].focus, p.b().shownMaps[1].start->grid.at(5, 2));
  // A later action: the turn-2 event shows the map again, a fresh copy of it.
  std::vector<Event> ev;
  CHECK(p.b().apply(Action::endTurn(), ev));
  for (int i = 0; i < 40 && !p.b().over && p.b().activeSide == 1; ++i) aiTakeStep(p.b(), ev);
  int shownNow = 0;
  for (const Event& e : ev) shownNow += e.type == EventType::MapShown;
  CHECK_EQ(shownNow, 1);
  CHECK_EQ((int)p.b().shownMaps.size(), 3);
  CHECK(p.b().shownMaps[2].start->units[(size_t)p.b().shownMaps[2].start->unitByStoryId("bandit")].alive);
}

TEST(show_map_entries_are_seen_by_everyone_under_fog) {
  SCopy c("sm_fog");
  c.swap("maps/keep.json", "\"sides\":", "\"fog\": true, \"sides\":");
  savetest::Rig rig;
  CHECK(rig.load(c.dir, "keep"));
  std::vector<Event> ev;
  rig.battle.start(ev);
  bool inside = false;
  int checked = 0;
  for (const Event& e : ev) {
    if (e.type == EventType::MapShown) inside = true;
    if (inside) {
      CHECK(e.seen == kSeenByAll);
      ++checked;
    }
    if (e.type == EventType::MapReturned) inside = false;
  }
  CHECK(checked > 5);
}

TEST(show_map_saves_nothing_new) {
  Played p("sm_save");
  Played plain("sm_save2", "[]");
  Json a, b;
  saveBattleState(p.b(), a);
  saveBattleState(plain.b(), b);
  // The units (and everything else but the story's own record of switches, variables and what fired) are the same; no key mentions a shown map.
  CHECK_STR(a["units"].dump(), b["units"].dump());
  const std::string text = a.dump();
  CHECK(!has(text, "shown") && !has(text, "flashback"));
}
