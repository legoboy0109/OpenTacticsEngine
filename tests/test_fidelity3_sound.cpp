// "Fidelity 3", item 6: the new sound slots (docs/SOUND.md "Shop music", "Boot jingle", "Level-up ticks", "Save sound", "Promotion sounds"):
// the audio.json keys and their refusals, and the music director's choices (shop music over the preparation music and back, the boot jingle
// before the title music, once per game). The director runs against an AudioService in Off mode (it logs the requests).
#include <string>

#include "core/base/log.h"
#include "core/data/loader.h"
#include "save_helpers.h"
#include "sound/audio_data.h"
#include "sound/cues.h"
#include "sound/music_director.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

const char* kMusic = R"("music": { "title": {"file":"audio/t.ogg"}, "boot": {"file":"audio/b.ogg","loop":false}, "shop": {"file":"audio/s.ogg"},
   "armory": {"file":"audio/a.ogg"}, "prep": {"file":"audio/p.ogg"}, "battle": {"file":"audio/bt.ogg"}, "loopy": {"file":"audio/y.ogg"} },
   "sounds": { "tick": {"file":"audio/tick.wav"}, "save": {"file":"audio/save.wav"}, "whoosh": {"file":"audio/w.wav"}, "boom": {"file":"audio/m.wav"} })";

std::string audioWith(const std::string& use) { return std::string("{ ") + kMusic + ", \"use\": { " + use + " } }"; }

// Loads data/audio.json of a temp folder; "" = it loaded.
std::string load(const char* folder, const std::string& text, AudioData& d) {
  const std::string dir = savetest::tempFolder(folder);
  makeDir(dir + "/data");
  CHECK(writeTextFile(dir + "/data/audio.json", text));
  std::string err;
  return loadAudioFile(dir, d, err) ? "" : err;
}

struct Director {
  AudioService audio;
  AudioData d;
  MusicDirector m;
  Director() {
    audio.log().keepLines(true);
    d.present = true;
    for (const char* id : {"title", "boot", "shop", "armory", "prep", "battle"}) {
      MusicDef def;
      def.id = id;
      def.file = std::string("audio/") + id + ".ogg";
      def.loop = std::string(id) != "boot";
      d.music.push_back(def);
    }
    d.titleMusic = d.findMusic("title");
    d.mapMusic = d.findMusic("battle");
    d.prepMusic = d.findMusic("prep");
    d.maps.assign(1, MapMusic());
    m.bind(&audio, &d, nullptr, "/g");
  }
  std::string played() {
    std::string s;
    for (const std::string& l : audio.log().lines()) {
      const size_t p = l.find("music play ");
      if (p != std::string::npos) {
        std::string name = l.substr(p + 11);
        name = name.substr(name.rfind('/') + 1, name.find(".ogg") - name.rfind('/') - 1);
        s += (s.empty() ? "" : " ") + name;
      } else if (l.find("music stop") != std::string::npos) {
        s += s.empty() ? "-" : " -";
      }
    }
    return s;
  }
  static SceneAudio battle() {
    SceneAudio a;
    a.kind = SceneAudio::Battle;
    a.map = 0;
    return a;
  }
  static SceneAudio title() {
    SceneAudio a;
    a.kind = SceneAudio::Title;
    return a;
  }
};

}  // namespace

TEST(fid3_sound_new_use_keys_load) {
  AudioData d;
  CHECK_STR(load("f3_snd_ok", audioWith(R"("shopMusic":"shop","bootJingle":"boot","promotionSounds":[{"atMs":400,"sound":"boom"},{"atMs":0,"sound":"whoosh"}],
     "sounds":{"levelUpTick":"tick","save":"save"})"), d), "");
  CHECK_EQ(d.shopMusic, d.findMusic("shop"));
  CHECK_EQ(d.bootJingle, d.findMusic("boot"));
  CHECK_EQ((int)d.promotionSounds.size(), 2);
  CHECK(d.promotionSounds[0].first == 0 && d.promotionSounds[0].second == d.findSound("whoosh"));  // kept in time order
  CHECK(d.promotionSounds[1].first == 400 && d.promotionSounds[1].second == d.findSound("boom"));
  CHECK_EQ(d.standard.sound[(int)Cue::LevelUpTick], d.findSound("tick"));
  CHECK_EQ(d.standard.sound[(int)Cue::Save], d.findSound("save"));
  CHECK_STR(cueKey(Cue::LevelUpTick), "levelUpTick");
  CHECK_STR(cueKey(Cue::Save), "save");
  // The old slots are untouched: a game that uses none of the new keys has them unset.
  AudioData old;
  CHECK_STR(load("f3_snd_old", audioWith(R"("titleMusic":"title")"), old), "");
  CHECK(old.shopMusic < 0 && old.bootJingle < 0 && old.promotionSounds.empty());
  CHECK_EQ(old.standard.sound[(int)Cue::Save], kNone);
}

TEST(fid3_sound_new_use_keys_are_refused) {
  auto bad = [&](const char* use, const char* part) {
    AudioData e;
    const std::string err = load("f3_snd_bad", audioWith(use), e);
    if (!has(err, part)) std::printf("  got: %s\n", err.c_str());
    CHECK(has(err, part));
  };
  bad(R"("shopMusic":"nope")", "names unknown music");
  bad(R"("bootJingle":"nope")", "names unknown music");
  bad(R"("bootJingle":"loopy")", "must have \"loop\": false");
  bad(R"("promotionSounds":[])", "1 to 16 entries");
  bad(R"("promotionSounds":{})", "1 to 16 entries");
  bad(R"("promotionSounds":[{"atMs":0}])", "\"sound\" must be the id");
  bad(R"("promotionSounds":[{"atMs":-1,"sound":"boom"}])", "from 0 to 60000");
  bad(R"("promotionSounds":[{"atMs":60001,"sound":"boom"}])", "from 0 to 60000");
  bad(R"("promotionSounds":[{"atMs":1.5,"sound":"boom"}])", "from 0 to 60000");
  bad(R"("promotionSounds":[{"atMs":0,"sound":"nope"}])", "names unknown sound");
  bad(R"("promotionSounds":[{"atMs":0,"sound":"boom","x":1}])", "unknown key \"x\"");
  bad(R"("sounds":{"levelUpTick":"nope"})", "names unknown sound");
  std::string many;
  for (int i = 0; i < 17; ++i) many += std::string(i ? "," : "") + R"({"atMs":0,"sound":"boom"})";
  bad(("\"promotionSounds\":[" + many + "]").c_str(), "1 to 16 entries");
}

TEST(fid3_sound_shop_music_over_the_preparation_music_and_back) {
  Director t;
  t.m.enter(Director::battle());
  t.m.prep(true);
  CHECK_STR(t.played(), std::string("battle prep"));
  t.m.shop(true, "armory");                       // the shop's own music
  CHECK_STR(t.played(), std::string("battle prep armory"));
  t.m.shop(true, "armory");                       // asked again: keeps playing
  CHECK_STR(t.played(), std::string("battle prep armory"));
  t.m.shop(false, "");                            // back to the preparation music
  CHECK_STR(t.played(), std::string("battle prep armory prep"));
  t.m.shop(true, "");                             // a shop with no music and no game default: nothing changes
  CHECK_STR(t.played(), std::string("battle prep armory prep"));
  t.d.shopMusic = t.d.findMusic("shop");          // the game's default shop music
  t.m.shop(false, "");
  t.m.shop(true, "");
  CHECK_STR(t.played(), std::string("battle prep armory prep shop"));
  t.m.shop(true, "armory");                       // a shop's own beats the default
  CHECK_STR(t.played(), std::string("battle prep armory prep shop armory"));
  t.m.shop(false, "");
  t.m.prep(false);
  CHECK_STR(t.played(), std::string("battle prep armory prep shop armory prep battle"));
}

TEST(fid3_sound_map_shop_plays_the_game_default_shop_music) {
  Director t;
  t.d.shopMusic = t.d.findMusic("shop");
  t.m.enter(Director::battle());
  t.m.shop(true, "");                             // a map shop (no prep): the default
  CHECK_STR(t.played(), std::string("battle shop"));
  t.m.shop(false, "");
  CHECK_STR(t.played(), std::string("battle shop battle"));
}

TEST(fid3_sound_boot_jingle_plays_before_the_title_music_once) {
  Director t;
  t.d.bootJingle = t.d.findMusic("boot");
  t.m.enter(Director::title());
  CHECK_STR(t.played(), std::string("boot"));  // the title music waits (the jingle is playing)
  t.m.tick();                                  // Off mode mixes nothing: the jingle is over at once, the title music follows
  CHECK_STR(t.played(), std::string("boot title"));
  t.m.enter(Director::battle());               // other scenes of the same game do not play it again
  t.m.enter(Director::title());
  CHECK_STR(t.played(), std::string("boot title battle title"));
  t.m.stop();                                  // the game is closed: the next time it opens is a new start
  t.m.bind(&t.audio, &t.d, nullptr, "/g");
  t.m.enter(Director::title());
  CHECK(t.played().rfind("boot") == t.played().size() - 4);
}

TEST(fid3_sound_a_game_without_the_boot_jingle_starts_the_title_music_at_once) {
  Director t;
  t.m.enter(Director::title());
  CHECK_STR(t.played(), std::string("title"));
}
