// Step sounds by movement type (docs/SOUND.md) and the sounds a battle animation plays at its frames (docs/BATTLE_SCENE.md "Sounds"):
// the lookup order (unit type, then movement type, then the standard set), the step interval, the loader's refusals, and the timeline's
// sound beats at the start of the named frames.
#include "battle/fight_timeline.h"
#include "campaign_fixture.h"
#include "sound/audio_data.h"
#include "sound/sound_bank.h"

using namespace ote;

namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }


}  // namespace

TEST(step_sound_lookup_unit_type_then_movement_type_then_standard) {
  AudioData d;
  d.present = true;
  d.units.assign(2, CueSet());
  d.moveTypes.assign(2, CueSet());
  d.standard.sound[(int)Cue::Step] = 0;
  d.moveTypes[1].sound[(int)Cue::Step] = 1;
  d.units[1].sound[(int)Cue::Step] = 2;
  CueCtx c;
  CHECK_EQ(SoundBank::lookup(d, Cue::Step, c), 0);              // nothing named: the standard step
  c.moveType = 0;
  CHECK_EQ(SoundBank::lookup(d, Cue::Step, c), 0);              // a movement type that names nothing
  c.moveType = 1;
  CHECK_EQ(SoundBank::lookup(d, Cue::Step, c), 1);              // the movement type's
  c.unitType = 1;
  CHECK_EQ(SoundBank::lookup(d, Cue::Step, c), 2);              // the unit type overrides it
  c.unitType = 0;
  CHECK_EQ(SoundBank::lookup(d, Cue::Step, c), 1);              // a unit type that names nothing: the movement type's
  d.moveTypes[1].sound[(int)Cue::Step] = kSilent;               // "" on a movement type = silent steps (not the standard)
  CHECK_EQ(SoundBank::lookup(d, Cue::Step, c), -1);
}

namespace {

std::string loadWith(const char* folder, const std::string& moveTypes, AudioData& d) {
  camptest::Game g;
  if (!g.load(folder)) return "fixture";
  std::string t;
  readTextFile(g.dir + "/game.json", t);
  const std::string from = "\"moveTypes\": [ {\"id\":\"foot\"} ]";
  const size_t at = t.find(from);
  if (at == std::string::npos) return "fixture: no moveTypes";
  t.replace(at, from.size(), "\"moveTypes\": " + moveTypes);
  camptest::Game::write(g.dir + "/game.json", t);
  camptest::Game::write(g.dir + "/data/audio.json", R"({ "music": {}, "sounds": { "tread": {"file":"audio/t.wav"}, "hooves": {"file":"audio/h.wav"} }, "use": {} })");
  std::string err;
  Database db;
  if (!loadGame(g.dir, db, err)) return "game: " + err;
  if (!loadAudioFile(g.dir, d, err) || !loadAudioOverrides(db, d, err)) return err;
  return "";
}

}  // namespace

TEST(movement_type_sounds_load_and_are_refused) {
  AudioData d;
  CHECK_STR(loadWith("step_ok", R"([ {"id":"foot","sounds":{"step":"tread","stepInterval":0.25}} ])", d), std::string());
  CHECK_EQ(d.moveTypes.size(), size_t(1));
  CHECK(d.moveTypes[0].sound[(int)Cue::Step] == d.findSound("tread") && d.moveStepGap[0] > 0.249f && d.moveStepGap[0] < 0.251f);
  AudioData none;
  CHECK_STR(loadWith("step_none", R"([ {"id":"foot"} ])", none), std::string());
  CHECK(none.moveTypes[0].sound[(int)Cue::Step] == kNone && none.moveStepGap[0] == 0.0f);
  AudioData silent;
  CHECK_STR(loadWith("step_silent", R"([ {"id":"foot","sounds":{"step":""}} ])", silent), std::string());
  CHECK(silent.moveTypes[0].sound[(int)Cue::Step] == kSilent);
  AudioData e1, e2, e3, e4;
  CHECK(has(loadWith("step_bad1", R"([ {"id":"foot","sounds":{"step":"nope"}} ])", e1), "move type \"foot\": \"sounds\": \"step\" names unknown sound \"nope\""));
  CHECK(has(loadWith("step_bad2", R"([ {"id":"foot","sounds":{"hit":"tread"}} ])", e2), "unknown key \"hit\" (expected \"step\", \"stepInterval\")"));
  CHECK(has(loadWith("step_bad3", R"([ {"id":"foot","sounds":{"stepInterval":5}} ])", e3), "\"stepInterval\" is seconds between two steps"));
  CHECK(has(loadWith("step_bad4", R"([ {"id":"foot","sounds":{"stepInterval":"fast"}} ])", e4), "\"stepInterval\" is seconds"));
}

namespace {

FighterTimes withSounds() {
  FighterTimes t;
  t.has[BaAttack] = t.has[BaHurt] = true;
  t.total[BaAttack] = 430;
  t.hitAt[BaAttack] = 200;
  t.total[BaHurt] = 180;
  for (int& v : t.spawnAt) v = -1;
  t.soundAt[BaAttack].push_back(std::make_pair(140, std::string("swing")));
  t.soundAt[BaAttack].push_back(std::make_pair(230, std::string("clang")));
  t.soundAt[BaHurt].push_back(std::make_pair(0, std::string("ouch")));
  return t;
}

}  // namespace

TEST(animation_sounds_become_beats_at_the_start_of_their_frames) {
  Event e;
  e.type = EventType::Attack;
  e.a = 1;
  e.b = 2;
  e.d = 4;
  e.flags = kHit;
  FightInfo info;
  info.units[1] = {1, "Eirik", 3, 20, 24, 2};
  info.units[2] = {2, "Gorm", 5, 10, 30, 3};
  info.distance = 1;
  FightScript s = buildFightScript({e}, info);
  FighterTimes times[2] = {withSounds(), withSounds()};
  FightTimeline tl = buildTimeline(s, times, {});
  double start = -1, swing = -1, clang = -1, ouch = -1, hit = -1;
  for (const FightBeat& b : tl.beats) {
    if (b.kind == BeatKind::StrikeStart) start = b.t;
    if (b.kind == BeatKind::Hit) hit = b.t;
    if (b.kind == BeatKind::Sound && b.text == "swing") swing = b.t;
    if (b.kind == BeatKind::Sound && b.text == "clang") clang = b.t;
    if (b.kind == BeatKind::Sound && b.text == "ouch") ouch = b.t;
  }
  CHECK(start >= 0 && swing - start > 0.1399 && swing - start < 0.1401);
  CHECK(clang - start > 0.2299 && clang - start < 0.2301);
  CHECK(ouch - hit > -0.0001 && ouch - hit < 0.0001);  // the struck fighter's hurt animation starts at the hit
  // The beats stay in time order.
  for (size_t i = 1; i < tl.beats.size(); ++i) CHECK(tl.beats[i - 1].t <= tl.beats[i].t);
  // A fighter with no sounds adds no beat.
  FighterTimes quiet = withSounds();
  for (auto& v : quiet.soundAt) v.clear();
  FighterTimes times2[2] = {quiet, quiet};
  int n = 0;
  for (const FightBeat& b : buildTimeline(s, times2, {}).beats) n += b.kind == BeatKind::Sound;
  CHECK_EQ(n, 0);
}
