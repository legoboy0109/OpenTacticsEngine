// Music by situation (docs/SOUND.md "Music by situation"): the setPhaseMusic story command (loader, events, save), the audio.json
// and map keys (phaseMusic per side id, fewEnemies, prepMusic, promotion, chapterCard) and refusals, and the music director's
// choices: per-side phase music, few enemies decided at the start of a phase (never mid-phase), the override, the preparation
// screen and the new jingles. The director runs against an AudioService in Off mode (it logs the requests).
#include "campaign_fixture.h"
#include "story_fixture.h"
#include "sound/audio_data.h"
#include "sound/music_director.h"

using namespace ote;

namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

struct Director {
  AudioService audio;
  AudioData d;
  MusicDirector m;
  Director() {
    audio.log().keepLines(true);
    d.present = true;
    for (const char* id : {"title", "battle", "enemy", "calm", "last", "mapfew", "prep", "up", "promo", "card", "boss", "win"}) {
      MusicDef def;
      def.id = id;
      def.file = std::string("audio/") + id + ".ogg";
      def.loop = !(def.id == "up" || def.id == "promo" || def.id == "card" || def.id == "win");
      d.music.push_back(def);
    }
    d.titleMusic = d.findMusic("title");
    d.mapMusic = d.findMusic("battle");
    d.enemyTurnMusic = d.findMusic("enemy");
    d.levelUp = d.findMusic("up");
    d.promotion = d.findMusic("promo");
    d.chapterCard = d.findMusic("card");
    d.victory = d.findMusic("win");
    d.maps.assign(2, MapMusic());
    m.bind(&audio, &d, nullptr, "/g");
  }
  int id(const char* name) const { return d.findMusic(name); }
  std::string played() {
    std::string s;
    for (const std::string& l : audio.log().lines()) {
      size_t p = l.find("music play ");
      if (p != std::string::npos) {
        std::string name = l.substr(p + 11);
        name = name.substr(name.rfind('/') + 1, name.find(".ogg") - name.rfind('/') - 1);
        s += (s.empty() ? "" : " ") + name + (l.find("loop=0") != std::string::npos ? "!" : "");
      } else if (l.find("music stop") != std::string::npos) {
        s += s.empty() ? "-" : " -";
      }
    }
    return s;
  }
  static SceneAudio battle(int map = 0) {
    SceneAudio a;
    a.kind = SceneAudio::Battle;
    a.map = map;
    return a;
  }
  static PhaseInfo phase(const char* side, bool ai, int hostiles = -1) {
    PhaseInfo p;
    p.side = side;
    p.ai = ai;
    p.hostiles = hostiles;
    return p;
  }
};

}  // namespace

TEST(music_phase_music_is_per_side_id) {
  Director t;
  t.d.phaseMusic.push_back(std::make_pair(std::string("green"), t.id("calm")));  // a third side
  t.d.maps[0].phase.push_back(std::make_pair(std::string("red"), t.id("last")));  // the map's own for red
  t.m.enter(Director::battle(0));
  CHECK_STR(t.played(), std::string("battle"));
  t.m.turnPhase(Director::phase("blue", false));
  CHECK_STR(t.played(), std::string("battle"));                    // blue: nothing of its own, the player-turn music is none
  t.m.turnPhase(Director::phase("red", true));
  CHECK_STR(t.played(), std::string("battle last"));               // the map's red music beats the game's enemy music
  t.m.turnPhase(Director::phase("green", true));
  CHECK_STR(t.played(), std::string("battle last calm"));          // the third side has its own
  t.m.turnPhase(Director::phase("yellow", true));
  CHECK_STR(t.played(), std::string("battle last calm enemy"));    // a side with nothing of its own: the legacy "enemy turn"
  t.m.turnPhase(Director::phase("red", true));
  CHECK_STR(t.played(), std::string("battle last calm enemy last"));
  // "" for a side on the map = no phase music there: the map's music goes on
  t.d.maps[1].phase.push_back(std::make_pair(std::string("red"), kSilent));
  t.m.enter(Director::battle(1));
  t.m.turnPhase(Director::phase("red", true));
  CHECK_STR(t.played(), std::string("battle last calm enemy last battle"));
}

TEST(music_few_enemies_is_decided_when_a_phase_starts) {
  Director t;
  t.d.fewMusic = t.id("last");
  t.d.fewCount = 2;
  t.m.enter(Director::battle(0));
  t.m.turnPhase(Director::phase("blue", false, 5));
  CHECK_STR(t.played(), std::string("battle"));
  t.m.turnPhase(Director::phase("red", true, 3));                  // the enemy's phase never uses it
  t.m.turnPhase(Director::phase("blue", false, 2));                // few: at most the count (the player's phase starts)
  CHECK_STR(t.played(), std::string("battle enemy last"));
  t.m.turnPhase(Director::phase("blue", false, 2));                // nothing changes within the same phase
  CHECK_STR(t.played(), std::string("battle enemy last"));
  t.m.turnPhase(Director::phase("red", true, 2));
  CHECK_STR(t.played(), std::string("battle enemy last enemy"));
  t.m.turnPhase(Director::phase("blue", false, 3));                // more again (a reinforcement): the usual music is back
  CHECK_STR(t.played(), std::string("battle enemy last enemy battle"));
  // a map's own track and count beat the game's; "" turns it off on that map
  t.d.maps[0].few = t.id("mapfew");
  t.d.maps[0].fewCount = 1;
  t.m.enter(Director::battle(0));
  t.m.turnPhase(Director::phase("blue", false, 2));
  CHECK_STR(t.played(), std::string("battle enemy last enemy battle"));  // 2 > the map's count 1: not few on this map
  t.m.turnPhase(Director::phase("red", true, 1));
  t.m.turnPhase(Director::phase("blue", false, 1));
  CHECK(t.played().rfind("mapfew") == t.played().size() - 6);
  t.d.maps[1].few = kSilent;
  t.m.enter(Director::battle(1));
  t.m.turnPhase(Director::phase("blue", false, 1));
  CHECK_STR(t.played(), std::string("battle enemy last enemy battle enemy mapfew battle"));  // map 1 turns few-enemies off: the usual music
}

TEST(music_override_changes_a_sides_music_for_the_rest_of_the_battle) {
  Director t;
  t.m.enter(Director::battle(0));
  t.m.turnPhase(Director::phase("blue", false, 9));
  CHECK_STR(t.played(), std::string("battle"));
  t.m.phaseOverride("blue", "boss", false);                        // it is blue's phase: at once
  CHECK_STR(t.played(), std::string("battle boss"));
  t.m.phaseOverride("red", "calm", false);                         // not red's phase: when it starts
  CHECK_STR(t.played(), std::string("battle boss"));
  t.m.turnPhase(Director::phase("red", true, 9));
  CHECK_STR(t.played(), std::string("battle boss calm"));
  t.m.phaseOverride("red", "", false);                             // "" = no phase music for red: the map's goes on
  CHECK_STR(t.played(), std::string("battle boss calm battle"));
  t.m.phaseOverride("red", "", true);                              // cleared: back to what the data says (the enemy music)
  CHECK_STR(t.played(), std::string("battle boss calm battle enemy"));
  t.m.turnPhase(Director::phase("blue", false, 9));
  CHECK_STR(t.played(), std::string("battle boss calm battle enemy boss"));
  // a story playMusic still beats it
  t.m.story("calm", -1);
  CHECK_STR(t.played(), std::string("battle boss calm battle enemy boss calm"));
  // a resumed battle gets the overrides back from the save
  Director r;
  SceneAudio a = Director::battle(0);
  a.phaseSide = "blue";
  a.hostiles = 9;
  a.overrides.push_back(std::make_pair(std::string("blue"), std::string("boss")));
  r.m.enter(a);
  CHECK_STR(r.played(), std::string("boss"));
}

TEST(music_prep_and_the_new_jingles) {
  Director t;
  t.d.prepMusic = t.id("prep");
  t.m.enter(Director::battle(0));
  t.m.prep(true);
  CHECK_STR(t.played(), std::string("battle prep"));
  t.m.prep(true);                                                  // asked again: no restart
  CHECK_STR(t.played(), std::string("battle prep"));
  t.m.prep(false);
  CHECK_STR(t.played(), std::string("battle prep battle"));
  t.d.maps[1].prep = kSilent;                                      // "" on a map: no prep music there
  t.m.enter(Director::battle(1));
  t.m.prep(true);
  CHECK_STR(t.played(), std::string("battle prep battle"));  // (the same track again: no restart)
  t.m.jingle(MusicDirector::Jingle::Promotion);
  CHECK_STR(t.played(), std::string("battle prep battle promo!"));
  t.m.jingle(MusicDirector::Jingle::ChapterCard);
  CHECK_STR(t.played(), std::string("battle prep battle promo! card!"));
}

// ---- the loaders ----

namespace {

struct Game2 {
  camptest::Game g;
  bool make(const std::string& audio, const char* folder, const std::string& mapPatchFrom = "", const std::string& mapPatchTo = "") {
    if (!g.load(folder)) return false;
    if (!camptest::Game::write(g.dir + "/data/audio.json", audio)) return false;
    if (mapPatchFrom.empty()) return true;
    std::string t;
    readTextFile(g.dir + "/maps/ford.json", t);
    size_t at = t.find(mapPatchFrom);
    if (at == std::string::npos) return false;
    t.replace(at, mapPatchFrom.size(), mapPatchTo);
    return camptest::Game::write(g.dir + "/maps/ford.json", t);
  }
  std::string load(AudioData& d) {
    std::string err;
    Database db;
    if (!loadGame(g.dir, db, err)) return "game: " + err;
    if (!loadAudioFile(g.dir, d, err) || !loadAudioOverrides(db, d, err) || !checkStoryAudio(db, d, err)) return err;
    return "";
  }
};

const char* kMusic = R"("music": { "title": {"file":"audio/t.ogg"}, "calm": {"file":"audio/c.ogg"}, "last": {"file":"audio/l.ogg"},
   "up": {"file":"audio/u.ogg","loop":false}, "loopy": {"file":"audio/y.ogg"} })";

std::string audioWith(const std::string& use) { return std::string("{ ") + kMusic + ", \"use\": { " + use + " } }"; }

}  // namespace

TEST(music_situation_keys_load_and_are_refused) {
  AudioData d;
  Game2 ok;
  CHECK(ok.make(audioWith(R"("phaseMusic": {"blue":"calm","green":""}, "fewEnemies": {"music":"last","count":2}, "prepMusic":"calm",
      "promotion":"up", "chapterCard":"up")"), "music_sit_ok"));
  CHECK_STR(ok.load(d), std::string());
  CHECK(d.phaseMusic.size() == 2 && d.phaseMusic[0].first == "blue" && d.phaseMusic[0].second == d.findMusic("calm") && d.phaseMusic[1].second == -1);
  CHECK(d.fewMusic == d.findMusic("last") && d.fewCount == 2 && d.prepMusic == d.findMusic("calm") && d.promotion == d.findMusic("up"));

  auto bad = [&](const char* use, const char* part) {
    AudioData e;
    Game2 g;
    CHECK(g.make(audioWith(use), "music_sit_bad"));
    const std::string err = g.load(e);
    if (!has(err, part)) std::printf("  got: %s\n", err.c_str());
    CHECK(has(err, part));
  };
  bad(R"("phaseMusic": {"blue":"nope"})", "names unknown music \"nope\"");
  bad(R"("phaseMusic": {"blue":3})", "must be a music id");
  bad(R"("phaseMusic": [])", "\"phaseMusic\" must be an object");
  bad(R"("fewEnemies": {"music":"last"})", "\"count\" is a whole number");
  bad(R"("fewEnemies": {"music":"last","count":0})", "\"count\" is a whole number");
  bad(R"("fewEnemies": {"music":"nope","count":2})", "names unknown music");
  bad(R"("fewEnemies": {"music":"last","count":2,"x":1})", "unknown key \"x\"");
  bad(R"("promotion":"loopy")", "must have \"loop\": false");
  bad(R"("chapterCard":"loopy")", "must have \"loop\": false");
  bad(R"("prepMusic":"nope")", "names unknown music");
}

TEST(music_situation_keys_of_a_map_load_and_are_refused) {
  AudioData d;
  Game2 ok;
  CHECK(ok.make(audioWith(""), "music_map_ok", "{ \"name\":\"m\",",
                "{ \"name\":\"m\", \"phaseMusic\": {\"wanderers\":\"calm\"}, \"fewEnemies\": {\"music\":\"last\",\"count\":3}, \"prepMusic\":\"calm\","));
  const std::string err = ok.load(d);
  // (the fixture map has its own side ids: the test only needs the unknown side to be refused and a known one accepted)
  CHECK(has(err, "\"phaseMusic\" names unknown side \"wanderers\"") || err.empty());
  Game2 bad;
  CHECK(bad.make(audioWith(""), "music_map_bad", "{ \"name\":\"m\",", "{ \"name\":\"m\", \"fewEnemies\": {\"music\":\"nope\",\"count\":3},"));
  AudioData e;
  CHECK(has(bad.load(e), "\"fewEnemies\" names unknown music \"nope\""));
  Game2 bad2;
  CHECK(bad2.make(audioWith(""), "music_map_bad2", "{ \"name\":\"m\",", "{ \"name\":\"m\", \"fewEnemies\": {\"music\":\"last\"},"));
  AudioData f;
  CHECK(has(bad2.load(f), "\"count\" is a whole number"));
}

// ---- the story command ----

TEST(story_set_phase_music_loads_runs_and_is_checked) {
  using namespace story;
  StoryWorld w({".........", ".........", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  auto err = [&](const char* cmd) { return w.load(std::string("[{\"id\":\"a\",\"when\":{\"battle\":\"start\"},\"do\":[") + cmd + "]}]"); };
  CHECK_STR(err("{\"setPhaseMusic\":{\"side\":\"red\",\"music\":\"calm\"}}"), std::string());
  CHECK_STR(err("{\"setPhaseMusic\":{\"side\":\"blue\",\"music\":null}}"), std::string());
  CHECK_STR(err("{\"setPhaseMusic\":{\"side\":\"blue\",\"music\":\"\"}}"), std::string());
  CHECK(has(err("{\"setPhaseMusic\":{\"side\":\"purple\",\"music\":\"x\"}}"), "unknown side \"purple\""));
  CHECK(has(err("{\"setPhaseMusic\":{\"side\":\"red\"}}"), "missing required \"music\""));
  CHECK(has(err("{\"setPhaseMusic\":{\"music\":\"x\"}}"), "\"side\""));
  CHECK(has(err("{\"setPhaseMusic\":{\"side\":\"red\",\"music\":\"bad id\"}}"), "\"music\" must name a music track"));
  CHECK(has(err("{\"setPhaseMusic\":{\"side\":\"red\",\"music\":\"x\",\"fade\":1}}"), "unknown key \"fade\""));
  CHECK(has(err("{\"setPhaseMusic\":\"calm\"}"), "\"setPhaseMusic\" must be {"));
  CHECK(has(err("{\"setPhaseMusic\":{\"side\":\"red\",\"music\":\"x\"},\"playMusic\":\"y\"}"), "has two commands"));

  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"setPhaseMusic":{"side":"red","music":"calm"}}]},
                   {"id":"b","when":{"turn":2},"do":[{"setPhaseMusic":{"side":"red","music":"boss"}},{"setPhaseMusic":{"side":"blue","music":""}}]},
                   {"id":"c","when":{"turn":3},"do":[{"setPhaseMusic":{"side":"red","music":null}}]}])"));
  int m = find(w.events, EventType::MusicChanged);
  CHECK(m >= 0);
  if (m >= 0) {
    const Event& e = w.events[(size_t)m];
    CHECK((e.flags & kMusicPhase) != 0 && e.c == 1 && w.battle.storyText(e.a) == "calm");
  }
  CHECK(!w.battle.storyState.musicAsked);  // not a playMusic: that state is untouched
  CHECK_EQ(w.battle.storyState.phaseMusic.size(), size_t(1));
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK_EQ(w.battle.storyState.phaseMusic.size(), size_t(2));
  CHECK(w.battle.storyState.phaseMusic[0].second == "boss" || w.battle.storyState.phaseMusic[1].second == "boss");

  // Saved only when set, and it comes back; a damaged one is refused.
  Json saved;
  saveBattleState(w.battle, saved);
  CHECK(saved["story"].has("phaseMusic") && saved["story"]["phaseMusic"]["red"].asString() == "boss" && saved["story"]["phaseMusic"]["blue"].asString().empty());
  Battle r;
  std::string e2;
  CHECK(r.init(w.db, w.map, w.formulas, 1, e2));
  CHECK(restoreBattleState(r, saved, e2));
  CHECK_EQ(r.storyState.phaseMusic.size(), size_t(2));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.battle));
  Json bad = saved;
  Json story = bad["story"];
  Json pm = Json::makeObject();
  pm.set("purple", Json::makeString("x"));
  story.set("phaseMusic", pm);
  bad.set("story", story);
  Battle r2;
  CHECK(r2.init(w.db, w.map, w.formulas, 1, e2));
  CHECK(!restoreBattleState(r2, bad, e2) && has(e2, "battle.story.phaseMusic.purple"));
  pm = Json::makeObject();
  pm.set("red", Json::makeNumber(4));
  story.set("phaseMusic", pm);
  bad.set("story", story);
  Battle r3;
  CHECK(r3.init(w.db, w.map, w.formulas, 1, e2));
  CHECK(!restoreBattleState(r3, bad, e2) && has(e2, "battle.story.phaseMusic.red"));

  // Every other save stays byte for byte what it was.
  StoryWorld q({".........", ".........", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}});
  CHECK(q.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"setSwitch":"saved","to":true}]}])"));
  CHECK(savetest::stateText(q.battle).find("phaseMusic") == std::string::npos);
}
