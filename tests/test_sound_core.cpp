// The rules core's part of sound (docs/SOUND.md): the sound keys of weapons / units / items / maps and the scene
// shape load without being interpreted, they change neither the data hash nor a save, the story commands
// playMusic / stopMusic / playSound run as presentation events, and "the music the story asked for" is saved
// only when set and comes back. (The player's data/audio.json loader is tested in test_audio_data.cpp.)
#include "campaign_run.h"
#include "story_fixture.h"

using namespace camptest;

namespace {

bool replaceOnce(std::string& s, const std::string& from, const std::string& to) {
  size_t at = s.find(from);
  if (at == std::string::npos) return false;
  s.replace(at, from.size(), to);
  return true;
}

// The hash of a map, the hash of the campaign and the bytes of a started battle's save, for the game in `g.dir`.
struct Snapshot {
  std::string mapHash, campaignHash, save, error;
};

Snapshot snapshotOf(const std::string& dir) {
  Snapshot s;
  Database db;
  Formulas formulas;
  MapDef map;
  Battle b;
  std::vector<Event> events;
  std::string err;
  if (!loadGame(dir, db, err) || !formulas.init(db, err) || !loadMap(db, "ford", map, err) || !b.init(db, map, formulas, 1, err)) {
    s.error = err;
    return s;
  }
  b.start(events);
  s.save = savetest::stateText(b);
  if (!computeDataHash(db, "ford", s.mapHash, err) || !computeCampaignHash(db, s.campaignHash, err)) s.error = err;
  return s;
}

}  // namespace

TEST(sound_keys_load_and_change_neither_hash_nor_save) {
  Game g;
  CHECK(g.load("sound_core_a"));
  Snapshot before = snapshotOf(g.dir);
  CHECK(before.error.empty());
  CHECK_EQ(before.mapHash.size(), size_t(16));

  // The same game with sounds and music everywhere the contract puts them.
  std::string weapons = kWeapons, units = kUnits, items = kItems, ford = fordMap(), campaign = kCampaignJson;
  CHECK(replaceOnce(weapons, "\"id\":\"sword\",", "\"id\":\"sword\",\"sounds\":{\"hit\":\"sword_hit\",\"miss\":\"sword_miss\",\"critical\":\"sword_crit\"},"));
  CHECK(replaceOnce(units, "\"id\":\"hero\",", "\"id\":\"hero\",\"sounds\":{\"step\":\"hooves\",\"defeated\":\"horse_fall\",\"select\":\"neigh\"},"));
  CHECK(replaceOnce(items, "\"id\":\"potion\",", "\"id\":\"potion\",\"sounds\":{\"use\":\"potion\"},"));
  CHECK(replaceOnce(ford, "{ \"name\":\"m\",", "{ \"name\":\"m\", \"music\":\"boss\", \"playerTurnMusic\":\"calm\", \"enemyTurnMusic\":\"tense\","));
  // Scene shape: { "music": ..., "commands": [...] } in place of the plain list.
  CHECK(replaceOnce(campaign, "\"before\": [ {\"say\":\"We hold the ford.\"", "\"before\": { \"music\": \"story\", \"commands\": [ {\"say\":\"We hold the ford.\""));
  CHECK(replaceOnce(campaign, "{\"addVariable\":\"chapters\",\"by\":1} ],\n      \"after\":  [ {\"say\":\"It is done.\"",
                    "{\"addVariable\":\"chapters\",\"by\":1} ] },\n      \"after\":  [ {\"say\":\"It is done.\""));
  CHECK(Game::write(g.dir + "/data/weapons.json", weapons));
  CHECK(Game::write(g.dir + "/data/units.json", units));
  CHECK(Game::write(g.dir + "/data/items.json", items));
  CHECK(Game::write(g.dir + "/maps/ford.json", ford));
  CHECK(Game::write(g.dir + "/data/campaign.json", campaign));

  Snapshot after = snapshotOf(g.dir);
  if (!after.error.empty()) std::printf("  %s\n", after.error.c_str());
  CHECK(after.error.empty());
  CHECK_STR(after.mapHash, before.mapHash);
  CHECK_STR(after.campaignHash, before.campaignHash);
  CHECK_STR(after.save, before.save);
}

TEST(scene_with_music_loads_as_the_same_scene_plus_a_playMusic_line) {
  Run r;
  std::string campaign = kCampaignJson;
  CHECK(replaceOnce(campaign, "\"before\": [ {\"say\":\"We hold the ford.\",\"speaker\":\"eirik\"}, {\"say\":\"Stay sharp.\",\"speakerName\":\"Narrator\",\"portrait\":\"narr\"},\n                  {\"addVariable\":\"chapters\",\"by\":1} ],",
                    "\"before\": { \"music\": \"story\", \"commands\": [ {\"say\":\"We hold the ford.\",\"speaker\":\"eirik\"}, {\"playSound\":\"thunder\"}, {\"say\":\"Stay sharp.\",\"speakerName\":\"Narrator\",\"portrait\":\"narr\"},\n                  {\"stopMusic\":true,\"fade\":1.5}, {\"addVariable\":\"chapters\",\"by\":1} ] },"));
  CHECK(r.begin("sound_core_scene", campaign));
  CHECK(r.scene(SceneKind::Before));
  CHECK_EQ(r.lines.size(), size_t(5));
  if (r.lines.size() == 5) {
    CHECK(r.lines[0].kind == SceneLine::Kind::PlayMusic && r.lines[0].text == "story" && r.lines[0].fadeMs == -1);
    CHECK(r.lines[1].kind == SceneLine::Kind::Say && r.lines[1].text == "We hold the ford.");
    CHECK(r.lines[2].kind == SceneLine::Kind::PlaySound && r.lines[2].text == "thunder");
    CHECK(r.lines[3].kind == SceneLine::Kind::Say);
    CHECK(r.lines[4].kind == SceneLine::Kind::StopMusic && r.lines[4].fadeMs == 1500);
  }
  CHECK_EQ(r.pt.variables[(size_t)r.g.db.findVariable("chapters")], 1);  // the commands around them still ran
}

TEST(scene_object_form_is_checked) {
  Game g;
  std::string c = kCampaignJson;
  CHECK(replaceOnce(c, "\"before\": [ {\"say\":\"The keep, by the road.\"} ]", "\"before\": { \"musik\": \"x\", \"commands\": [] }"));
  std::string err = g.loadError("sound_core_scene_bad", c);
  CHECK(err.find("unknown key \"musik\"") != std::string::npos);
  c = kCampaignJson;
  CHECK(replaceOnce(c, "\"before\": [ {\"say\":\"The keep, by the road.\"} ]", "\"before\": { \"music\": \"bad id\", \"commands\": [] }"));
  CHECK(g.loadError("sound_core_scene_bad", c).find("\"music\" must name a music track") != std::string::npos);
  c = kCampaignJson;
  CHECK(replaceOnce(c, "\"before\": [ {\"say\":\"The keep, by the road.\"} ]", "\"before\": [ {\"playMusic\":\"x\",\"fade\":-1} ]"));
  CHECK(g.loadError("sound_core_scene_bad", c).find("\"fade\" is seconds") != std::string::npos);
}

TEST(story_music_commands_run_as_events_and_are_remembered) {
  using namespace story;
  StoryWorld w({".........", ".........", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([
    {"id":"a","when":{"battle":"start"},"do":[{"playMusic":"boss","fade":1.0},{"playSound":"thunder"},{"say":"Hark."}]},
    {"id":"b","when":{"turn":2},"do":[{"stopMusic":true,"fade":0.25}]}])"));
  int m = find(w.events, EventType::MusicChanged), s = find(w.events, EventType::SoundRequested), d = find(w.events, EventType::Dialogue);
  CHECK(m >= 0 && s > m && d > s);  // in the order the commands ran
  if (m >= 0 && s >= 0) {
    CHECK_STR(w.battle.storyText(w.events[(size_t)m].a), std::string("boss"));
    CHECK_EQ(w.events[(size_t)m].b, 1000);
    CHECK_STR(w.battle.storyText(w.events[(size_t)s].a), std::string("thunder"));
  }
  CHECK(w.battle.storyState.musicAsked && w.battle.storyState.music == "boss");
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));  // round 2: the second event
  int stop = find(w.events, EventType::MusicChanged, (size_t)m + 1);
  CHECK(stop >= 0 && w.events[(size_t)stop].a == -1 && w.events[(size_t)stop].b == 250);
  CHECK(w.battle.storyState.musicAsked && w.battle.storyState.music.empty());
}

TEST(story_music_commands_are_checked) {
  using namespace story;
  StoryWorld w({".........", ".........", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}});
  auto err = [&](const char* cmd) { return w.load(std::string("[{\"id\":\"a\",\"when\":{\"battle\":\"start\"},\"do\":[") + cmd + "]}]"); };
  CHECK_STR(err("{\"playMusic\":\"boss\",\"fade\":2}"), std::string());
  CHECK_STR(err("{\"stopMusic\":true}"), std::string());
  CHECK_STR(err("{\"playSound\":\"x-1_y\"}"), std::string());
  CHECK(err("{\"playMusic\":\"\"}").find("\"playMusic\" must name a music track") != std::string::npos);
  CHECK(err("{\"playMusic\":3}").find("\"playMusic\" must name a music track") != std::string::npos);
  CHECK(err("{\"playSound\":\"a\",\"fade\":1}").find("unknown key \"fade\"") != std::string::npos);
  CHECK(err("{\"stopMusic\":false}").find("write { \"stopMusic\": true }") != std::string::npos);
  CHECK(err("{\"playMusic\":\"a\",\"fade\":99}").find("\"fade\" is seconds") != std::string::npos);
  CHECK(err("{\"playMusic\":\"a\",\"loop\":true}").find("unknown key \"loop\"") != std::string::npos);
  CHECK(err("{\"playMusic\":\"a\",\"stopMusic\":true}").find("has two commands") != std::string::npos);
}

TEST(story_music_save_key_is_written_only_when_set_and_resumes) {
  using namespace story;
  const char* quiet = R"([{"id":"a","when":{"battle":"start"},"do":[{"setSwitch":"saved","to":true}]}])";
  const char* loud = R"([{"id":"a","when":{"battle":"start"},"do":[{"playMusic":"boss"}]}])";
  const char* stopped = R"([{"id":"a","when":{"battle":"start"},"do":[{"stopMusic":true}]}])";
  const std::vector<std::string> rows = {".........", ".........", "........."};
  auto make = [&](StoryWorld& w, const char* events) { return w.play(events); };

  StoryWorld q(rows, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}});
  CHECK(make(q, quiet));
  CHECK(savetest::stateText(q.battle).find("\"music\"") == std::string::npos);  // saves of every other game stay as they were

  StoryWorld l(rows, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}});
  CHECK(make(l, loud));
  Json saved;
  saveBattleState(l.battle, saved);
  CHECK(saved["story"].has("music") && saved["story"]["music"].asString() == "boss");
  Battle r;
  std::string err;
  CHECK(r.init(l.db, l.map, l.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  CHECK(r.storyState.musicAsked && r.storyState.music == "boss");
  CHECK_STR(savetest::stateText(r), savetest::stateText(l.battle));

  StoryWorld s(rows, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}});
  CHECK(make(s, stopped));
  Json sv;
  saveBattleState(s.battle, sv);
  CHECK(sv["story"].has("music") && sv["story"]["music"].asString().empty());
  Battle r2;
  CHECK(r2.init(s.db, s.map, s.formulas, 1, err));
  CHECK(restoreBattleState(r2, sv, err));
  CHECK(r2.storyState.musicAsked && r2.storyState.music.empty());

  // A damaged value is refused.
  Json bad = saved;
  Json story = bad["story"];
  story.set("music", Json::makeNumber(5));
  bad.set("story", story);
  Battle r3;
  CHECK(r3.init(l.db, l.map, l.formulas, 1, err));
  CHECK(!restoreBattleState(r3, bad, err) && err.find("battle.story.music") != std::string::npos);
}

TEST(story_music_commands_count_in_the_hash_but_the_track_does_not) {
  // Which track plays is presentation; whether a command is there is part of the story (like a line of dialogue).
  Game g;
  CHECK(g.load("sound_core_hash"));
  std::string base = savetest::tempFolder("sound_core_hash");
  std::string ford = fordMap();
  auto hashWith = [&](const char* extra) {
    std::string m = ford;
    if (extra[0]) replaceOnce(m, "{\"changeSide\":\"wanderer\"", std::string(extra) + ",{\"changeSide\":\"wanderer\"");
    Game::write(g.dir + "/maps/ford.json", m);
    Database db;
    std::string err, h;
    if (!loadGame(g.dir, db, err) || !computeDataHash(db, "ford", h, err)) return "error: " + err;
    return h;
  };
  std::string none = hashWith(""), a = hashWith("{\"playMusic\":\"one\"}"), b = hashWith("{\"playMusic\":\"two\"}");
  CHECK(none != a);
  CHECK_STR(a, b);
}
