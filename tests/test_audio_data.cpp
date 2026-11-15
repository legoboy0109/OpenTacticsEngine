// The player's sound data (docs/SOUND.md step 2): data/audio.json and the override keys load strictly, the cue
// lookup runs weapon / item, unit type, standard set, silence, and the music director follows the priority
// (jingle, story, turn phase, map, game default) without restarting a track that is asked for again.
// The director is driven against an AudioService in Off mode: it only logs what it was asked, which is all
// these tests need (the player's real behaviour is in tests/sound_player_test.py).
#include "campaign_fixture.h"
#include "sound/audio_data.h"
#include "sound/music_director.h"
#include "sound/sound_bank.h"

using namespace ote;
using namespace camptest;

namespace {

const char* kAudio = R"({
  "music": { "title": {"file":"audio/title.ogg","loopStart":2.5}, "battle": {"file":"audio/battle.ogg","volume":80},
             "enemy": {"file":"audio/enemy.ogg"}, "boss": {"file":"audio/boss.ogg"}, "calm": {"file":"audio/calm.ogg"},
             "scene": {"file":"audio/scene.ogg"}, "win": {"file":"audio/win.ogg","loop":false},
             "lose": {"file":"audio/lose.ogg","loop":false}, "up": {"file":"audio/up.ogg","loop":false} },
  "sounds": { "hit": {"file":"audio/hit.wav"}, "sword_hit": {"file":"audio/sword_hit.wav","volume":50},
              "step": {"files":["audio/s1.wav","audio/s2.wav"]}, "hooves": {"file":"audio/hooves.wav"},
              "potion": {"file":"audio/potion.wav"}, "thunder": {"file":"audio/thunder.wav"} },
  "use": { "titleMusic":"title", "mapMusic":"battle", "playerTurnMusic":"", "enemyTurnMusic":"enemy", "sceneMusic":"scene",
           "victory":"win", "defeat":"lose", "levelUp":"up",
           "sounds": { "hit":"hit", "step":"step", "itemUse":"hit" } } })";

// The fixture game with its audio.json and (optionally) the contract's override keys.
struct SoundGame {
  Game g;
  bool make(const std::string& audio, const char* folder, bool overrides = true) {
    if (!g.load(folder)) return false;
    if (!audio.empty() && !Game::write(g.dir + "/data/audio.json", audio)) return false;
    if (!overrides) return true;
    auto patch = [&](const char* file, const char* from, const char* to) {
      std::string t;
      readTextFile(g.dir + "/" + file, t);
      size_t at = t.find(from);
      if (at == std::string::npos) return false;
      t.replace(at, std::string(from).size(), to);
      return Game::write(g.dir + "/" + file, t);
    };
    return patch("data/weapons.json", "\"id\":\"sword\",", "\"id\":\"sword\",\"sounds\":{\"hit\":\"sword_hit\"},") &&
           patch("data/units.json", "\"id\":\"hero\",", "\"id\":\"hero\",\"sounds\":{\"step\":\"hooves\"},") &&
           patch("data/items.json", "\"id\":\"potion\",", "\"id\":\"potion\",\"sounds\":{\"use\":\"potion\"},") &&
           patch("maps/ford.json", "{ \"name\":\"m\",", "{ \"name\":\"m\", \"music\":\"boss\", \"playerTurnMusic\":\"calm\",");
  }
};

std::string loadAll(SoundGame& s, AudioData& d) {
  std::string err;
  Database db;
  if (!loadGame(s.g.dir, db, err)) return "game: " + err;
  if (!loadAudioFile(s.g.dir, d, err) || !loadAudioOverrides(db, d, err) || !checkStoryAudio(db, d, err)) return err;
  return "";
}

}  // namespace

TEST(audio_data_loads_the_contract_shape) {
  SoundGame s;
  CHECK(s.make(kAudio, "audio_data_a"));
  AudioData d;
  CHECK_STR(loadAll(s, d), std::string());
  CHECK(d.present && d.music.size() == 9 && d.sounds.size() == 6);
  CHECK(d.music[0].id == "title" && d.music[0].loopStart == 2.5f && d.music[1].volume == 80 && !d.music[(size_t)d.findMusic("win")].loop);
  CHECK(d.titleMusic == d.findMusic("title") && d.playerTurnMusic == -1 && d.enemyTurnMusic == d.findMusic("enemy"));
  CHECK(d.sounds[(size_t)d.findSound("step")].files.size() == 2 && d.sounds[(size_t)d.findSound("sword_hit")].volume == 50);
  CHECK(d.standard.sound[(int)Cue::Hit] == d.findSound("hit") && d.standard.sound[(int)Cue::Miss] == kNone);
  // The overrides are indexed like the Database.
  CHECK(d.weapons[(size_t)s.g.db.findWeapon("sword")].sound[(int)Cue::Hit] == d.findSound("sword_hit"));
  CHECK(d.units[(size_t)s.g.db.findUnitType("hero")].sound[(int)Cue::Step] == d.findSound("hooves"));
  CHECK(d.items[(size_t)s.g.db.findItem("potion")].sound[(int)Cue::ItemUse] == d.findSound("potion"));
  const int ford = s.g.db.findMap("ford");
  CHECK(d.maps[(size_t)ford].music == d.findMusic("boss") && d.maps[(size_t)ford].playerTurn == d.findMusic("calm") &&
        d.maps[(size_t)ford].enemyTurn == kNone);
}

TEST(audio_data_without_a_file_is_silent_and_fine) {
  SoundGame s;
  CHECK(s.make("", "audio_data_none", false));
  AudioData d;
  CHECK_STR(loadAll(s, d), std::string());
  CHECK(!d.present && d.silent());
}

TEST(audio_data_refusals_name_file_entry_and_key) {
  SoundGame s;
  struct Case { const char* from; const char* to; const char* expect; } cases[] = {
      {"\"use\": {", "\"usee\": {", "unknown key \"usee\" (expected \"music\", \"sounds\", \"use\")"},
      {"\"loopStart\":2.5", "\"loopstart\":2.5", "music \"title\": unknown key \"loopstart\""},
      {"\"loopStart\":2.5", "\"loopStart\":-1", "music \"title\": \"loopStart\" is seconds"},
      {"\"volume\":80", "\"volume\":180", "\"volume\" is a percent between 0 and 100"},
      {"\"file\":\"audio/hit.wav\"", "\"file\":\"audio/hit.mp3\"", "sound \"hit\": \"file\" must be a path"},
      {"\"file\":\"audio/hit.wav\"", "\"file\":\"audio/hit.wav\",\"files\":[\"a.wav\"]", "needs exactly one of \"file\" or \"files\""},
      {"\"files\":[\"audio/s1.wav\",\"audio/s2.wav\"]", "\"files\":[]", "\"files\" must list between 1 and 16 paths"},
      {"\"mapMusic\":\"battle\"", "\"mapMusic\":\"nope\"", "use.mapMusic: names unknown music \"nope\""},
      {"\"victory\":\"win\"", "\"victory\":\"battle\"", "the jingle \"battle\" must have \"loop\": false"},
      {"\"hit\":\"hit\", \"step\"", "\"hits\":\"hit\", \"step\"", "use.sounds: unknown key \"hits\""},
      {"\"itemUse\":\"hit\"", "\"itemUse\":\"nope\"", "\"itemUse\" names unknown sound \"nope\""},
      {"\"battle\": {", "\"bad id\": {", "music \"bad id\": an id is letters"},
  };
  for (const Case& c : cases) {
    std::string text = kAudio;
    size_t at = text.find(c.from);
    CHECK(at != std::string::npos);
    if (at == std::string::npos) continue;
    text.replace(at, std::string(c.from).size(), c.to);
    CHECK(s.make(text, "audio_data_bad", false));
    AudioData d;
    std::string err = loadAll(s, d);
    if (err.find(c.expect) == std::string::npos) std::printf("  got: %s\n  want: %s\n", err.c_str(), c.expect);
    CHECK(err.find(c.expect) != std::string::npos);
    CHECK(err.find("audio.json") != std::string::npos);
  }
}

TEST(audio_data_override_refusals) {
  SoundGame s;
  CHECK(s.make(kAudio, "audio_data_ov"));
  std::string t;
  readTextFile(s.g.dir + "/data/weapons.json", t);
  struct Case { const char* file; const char* from; const char* to; const char* expect; } cases[] = {
      {"data/weapons.json", "\"hit\":\"sword_hit\"", "\"hit\":\"ghost\"", "weapon \"sword\": \"sounds\": \"hit\" names unknown sound \"ghost\""},
      {"data/weapons.json", "\"hit\":\"sword_hit\"", "\"step\":\"hooves\"", "weapon \"sword\": \"sounds\": unknown key \"step\" (expected \"hit\", \"miss\", \"critical\")"},
      {"data/units.json", "\"step\":\"hooves\"", "\"use\":\"hooves\"", "unit \"hero\": \"sounds\": unknown key \"use\""},
      {"data/items.json", "\"use\":\"potion\"", "\"use\":7", "\"use\" must be a sound id"},
      {"maps/ford.json", "\"music\":\"boss\"", "\"music\":\"ghost\"", "map \"ford\": \"music\" names unknown music \"ghost\""},
  };
  for (const Case& c : cases) {
    CHECK(s.make(kAudio, "audio_data_ov"));
    std::string text;
    readTextFile(s.g.dir + "/" + c.file, text);
    size_t at = text.find(c.from);
    CHECK(at != std::string::npos);
    if (at == std::string::npos) continue;
    text.replace(at, std::string(c.from).size(), c.to);
    CHECK(Game::write(s.g.dir + "/" + c.file, text));
    AudioData d;
    std::string err = loadAll(s, d);
    if (err.find(c.expect) == std::string::npos) std::printf("  got: %s\n  want: %s\n", err.c_str(), c.expect);
    CHECK(err.find(c.expect) != std::string::npos);
  }
  // "" on an override is silence on purpose (a different thing from "not set").
  CHECK(s.make(kAudio, "audio_data_ov"));
  std::string w;
  readTextFile(s.g.dir + "/data/weapons.json", w);
  w.replace(w.find("\"hit\":\"sword_hit\""), 17, "\"hit\":\"\"");
  CHECK(Game::write(s.g.dir + "/data/weapons.json", w));
  AudioData d;
  CHECK_STR(loadAll(s, d), std::string());
  CHECK_EQ(d.weapons[(size_t)s.g.db.findWeapon("sword")].sound[(int)Cue::Hit], kSilent);
}

TEST(audio_data_story_ids_are_checked_in_maps_and_scenes) {
  SoundGame s;
  CHECK(s.make(kAudio, "audio_data_story", false));
  std::string ford = fordMap();
  ford.replace(ford.find("{\"changeSide\":\"wanderer\""), 0, "{\"if\":[{\"switch\":\"met_wanderer\"}],\"then\":[{\"playMusic\":\"ghost\"}]},");
  CHECK(Game::write(s.g.dir + "/maps/ford.json", ford));
  AudioData d;
  std::string err = loadAll(s, d);
  CHECK(err.find("maps/ford.json: event \"recruit\": \"playMusic\" names unknown music \"ghost\"") != std::string::npos);
  ford = fordMap();
  ford.replace(ford.find("{\"changeSide\":\"wanderer\""), 0, "{\"playMusic\":\"boss\"},{\"playSound\":\"thunder\"},");
  CHECK(Game::write(s.g.dir + "/maps/ford.json", ford));
  CHECK_STR(loadAll(s, d), std::string());
  // A scene.
  std::string c = kCampaignJson;
  c.replace(c.find("\"before\": [ {\"say\":\"The keep, by the road.\"} ]"), 46, "\"before\": { \"music\": \"nowhere\", \"commands\": [] }");
  CHECK(Game::write(s.g.dir + "/data/campaign.json", c));
  err = loadAll(s, d);
  CHECK(err.find("chapter \"ch3a\" \"before\": music \"nowhere\" is not in data/audio.json") != std::string::npos);
  // No audio.json at all, but a story command: refused (it would never be heard).
  SoundGame n;
  CHECK(n.make("", "audio_data_story2", false));
  CHECK(Game::write(n.g.dir + "/maps/ford.json", ford));
  err = loadAll(n, d);
  CHECK(err.find("this game has no data/audio.json") != std::string::npos);
}

TEST(sound_lookup_order_weapon_item_unit_standard_silence) {
  AudioData d;
  d.present = true;
  d.sounds.resize(5);
  d.standard.sound[(int)Cue::Hit] = 0;
  d.units.assign(2, CueSet());
  d.weapons.assign(2, CueSet());
  d.items.assign(2, CueSet());
  d.units[1].sound[(int)Cue::Hit] = 1;
  d.weapons[1].sound[(int)Cue::Hit] = 2;
  d.items[1].sound[(int)Cue::ItemUse] = 3;
  d.units[1].sound[(int)Cue::Miss] = kSilent;
  d.standard.sound[(int)Cue::Miss] = 4;
  CueCtx c;
  CHECK_EQ(SoundBank::lookup(d, Cue::Hit, c), 0);                 // the standard set
  c.unitType = 1;
  CHECK_EQ(SoundBank::lookup(d, Cue::Hit, c), 1);                 // the unit type beats it
  c.weapon = 1;
  CHECK_EQ(SoundBank::lookup(d, Cue::Hit, c), 2);                 // the weapon beats the unit type
  c.weapon = 0;                                                    // a weapon with no override falls through
  CHECK_EQ(SoundBank::lookup(d, Cue::Hit, c), 1);
  CueCtx item;
  item.item = 1;
  CHECK_EQ(SoundBank::lookup(d, Cue::ItemUse, item), 3);
  CHECK_EQ(SoundBank::lookup(d, Cue::Heal, c), -1);               // nothing defined anywhere: silence
  CHECK_EQ(SoundBank::lookup(d, Cue::Miss, c), -1);               // "" on the unit type is silence, not the standard miss
  CHECK_EQ(SoundBank::lookup(d, Cue::Miss, CueCtx()), 4);
}

namespace {

// A music director on an AudioService in Off mode (it logs the requests, nothing else happens).
struct Director {
  AudioService audio;
  AudioData d;
  MusicDirector m;
  Director() {
    audio.log().keepLines(true);
    d.present = true;
    for (const char* id : {"title", "battle", "enemy", "boss", "calm", "scene", "win", "lose", "up"}) {
      MusicDef def;
      def.id = id;
      def.file = std::string("audio/") + id + ".ogg";
      def.loop = !(def.id == "win" || def.id == "lose" || def.id == "up");
      d.music.push_back(def);
    }
    d.titleMusic = d.findMusic("title");
    d.mapMusic = d.findMusic("battle");
    d.enemyTurnMusic = d.findMusic("enemy");
    d.sceneMusic = d.findMusic("scene");
    d.victory = d.findMusic("win");
    d.defeat = d.findMusic("lose");
    d.levelUp = d.findMusic("up");
    d.maps.assign(2, MapMusic());
    m.bind(&audio, &d, nullptr, "/g");
  }
  // The names of the tracks started so far, "-" for a stop.
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
  static SceneAudio scene(SceneAudio::Kind k, int map = -1) {
    SceneAudio a;
    a.kind = k;
    a.map = map;
    return a;
  }
};

}  // namespace

TEST(music_director_priority_and_no_restart) {
  Director t;
  t.m.enter(Director::scene(SceneAudio::Title));
  t.m.enter(Director::scene(SceneAudio::Title));  // the same again: keeps playing
  CHECK_STR(t.played(), std::string("title"));
  t.m.enter(Director::scene(SceneAudio::Keep));  // options: nothing changes
  CHECK_STR(t.played(), std::string("title"));
  t.m.enter(Director::scene(SceneAudio::Battle, 0));
  CHECK_STR(t.played(), std::string("title battle"));  // the game default
  t.m.turnPhase(true);                                  // the enemy's turn
  t.m.turnPhase(true);
  CHECK_STR(t.played(), std::string("title battle enemy"));
  t.m.story("boss", 1000);                              // the story beats the turn phase
  CHECK_STR(t.played(), std::string("title battle enemy boss"));
  t.m.turnPhase(false);                                 // ... which stays until it changes or the battle ends
  CHECK_STR(t.played(), std::string("title battle enemy boss"));
  t.m.story("boss", -1);                                // the same track asked again: no restart
  CHECK_STR(t.played(), std::string("title battle enemy boss"));
  t.m.story("", 250);                                   // stopMusic: silence, not the map music
  CHECK_STR(t.played(), std::string("title battle enemy boss -"));
  t.m.jingle(MusicDirector::Jingle::Victory);
  CHECK_STR(t.played(), std::string("title battle enemy boss - win!"));
  t.m.turnPhase(true);                                  // nothing starts under a victory jingle
  CHECK_STR(t.played(), std::string("title battle enemy boss - win!"));
  t.m.enter(Director::scene(SceneAudio::Title));
  CHECK_STR(t.played(), std::string("title battle enemy boss - win! title"));
  t.m.enter(Director::scene(SceneAudio::Silent));
  CHECK_STR(t.played(), std::string("title battle enemy boss - win! title -"));
}

TEST(music_director_map_and_phase_music_of_a_map) {
  Director t;
  t.d.maps[1].music = t.d.findMusic("boss");
  t.d.maps[1].playerTurn = t.d.findMusic("calm");
  t.d.maps[0].enemyTurn = kSilent;  // map 0: no enemy-turn music at all (the map's music goes on)
  t.m.enter(Director::scene(SceneAudio::Battle, 0));
  t.m.turnPhase(true);
  CHECK_STR(t.played(), std::string("battle"));
  t.m.enter(Director::scene(SceneAudio::Battle, 1));
  CHECK_STR(t.played(), std::string("battle calm"));  // the map's player-turn music beats its own map music
  t.m.turnPhase(true);
  CHECK_STR(t.played(), std::string("battle calm enemy"));  // the game's enemy-turn music (the map gave none)
  t.m.turnPhase(false);
  CHECK_STR(t.played(), std::string("battle calm enemy calm"));
}

TEST(music_director_scene_and_level_up_and_resume) {
  Director t;
  t.m.enter(Director::scene(SceneAudio::Scene));
  t.m.story("boss", -1);
  t.m.story("boss", -1);
  CHECK_STR(t.played(), std::string("scene boss"));
  t.m.jingle(MusicDirector::Jingle::LevelUp);  // pauses; the track under it stays the one asked for
  t.m.story("boss", -1);
  CHECK_STR(t.played(), std::string("scene boss up!"));
  // A resumed battle whose story had asked for a track, mid-way through the enemy's turn.
  SceneAudio a = Director::scene(SceneAudio::Battle, 0);
  a.storyAsked = true;
  a.storyMusic = "calm";
  a.enemyTurn = true;
  t.m.enter(a);
  CHECK_STR(t.played(), std::string("scene boss up! calm"));
  // Silent: no scene music in the game keeps what plays.
  Director u;
  u.d.sceneMusic = -1;
  u.m.enter(Director::scene(SceneAudio::Title));
  u.m.enter(Director::scene(SceneAudio::Scene));
  CHECK_STR(u.played(), std::string("title"));
}
