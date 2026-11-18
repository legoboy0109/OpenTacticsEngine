// Richer battle scene effects (docs/BATTLE_SCENE.md "Effects"): the loader and its refusals for every newer key (sounds, hitAtMs, layer, mirror,
// flash, shake, at, returns, lingerMs, frames from several sheets, lists, byType), which effects a strike gets, the sound check against
// data/audio.json, and the timeline of effects (when each effect, sound, flash and the hit start).
#include <sys/stat.h>

#include <cmath>
#include <cstdio>

#include "battle/fight_timeline.h"
#include "core/base/json.h"
#include "core/data/loader.h"
#include "sound/audio_data.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// Unit types knight and archer, weapons sword (group blade) and longbow (group bow). Sheets: "fx" and "fx2" 4 x 1 cells of 16 x 16,
// "odd" 2 x 1 cells of 8 x 8, "bg" 240 x 68 (one cell: a whole-stage picture).
std::string makeGame(const char* name, const std::string& effectsJson) {
  std::string root = ote_test::tempDir() + "/battlefx_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"formulas\": {\"damage\":\"1\"},"
                      " \"maps\": [{\"id\":\"m\",\"name\":\"M\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json",
                      "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"},{\"id\":\"longbow\",\"group\":\"bow\",\"range\":[2,2]}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"sword\",\"longbow\"]},"
                      "{\"id\":\"archer\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"longbow\"]}] }"));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"name\": \"M\", \"grid\": {\"type\":\"square\",\"width\":2,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"..\"],"
                      " \"sides\": [{\"id\":\"a\",\"controller\":\"human\"},{\"id\":\"b\",\"controller\":\"ai\"}],"
                      " \"units\": [{\"type\":\"knight\",\"side\":\"a\",\"x\":0,\"y\":0},{\"type\":\"archer\",\"side\":\"b\",\"x\":1,\"y\":0}] }"));
  auto png = [&](const char* file, int w, int h) {
    unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, (unsigned char)(w >> 8),
                           (unsigned char)w, 0, 0, (unsigned char)(h >> 8), (unsigned char)h, 8, 6};
    FILE* f = std::fopen((root + "/art/" + file).c_str(), "wb");
    CHECK(f != nullptr);
    if (f) {
      std::fwrite(b, 1, sizeof b, f);
      std::fclose(f);
    }
  };
  png("fighters.png", 64, 32);
  png("fx.png", 64, 16);
  png("fx2.png", 64, 16);
  png("odd.png", 16, 8);
  png("bg.png", 240, 68);
  CHECK(writeTextFile(root + "/data/graphics.json",
                      "{ \"sheets\": [ {\"id\":\"fighters\",\"file\":\"art/fighters.png\",\"cellWidth\":32,\"cellHeight\":32},"
                      "                {\"id\":\"fx\",\"file\":\"art/fx.png\",\"cellWidth\":16,\"cellHeight\":16},"
                      "                {\"id\":\"fx2\",\"file\":\"art/fx2.png\",\"cellWidth\":16,\"cellHeight\":16},"
                      "                {\"id\":\"odd\",\"file\":\"art/odd.png\",\"cellWidth\":8,\"cellHeight\":8},"
                      "                {\"id\":\"bg\",\"file\":\"art/bg.png\",\"cellWidth\":240,\"cellHeight\":68} ],"
                      " \"battle\": { \"sets\": { \"knight\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]} } } },"
                      "   \"effects\": " + effectsJson + " } }"));
  return root;
}

// One effect on the longbow with the given extra keys.
std::string one(const std::string& extra) { return "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0],[1,0]]," + extra + "} ]"; }

void expectError(const char* name, const std::string& effects, const char* a, const char* b = "") {
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame(name, effects), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json") && has(err, a) && has(err, b));
}

}  // namespace

TEST(battle_effects_load_every_new_key) {
  Database db;
  std::string err;
  const std::string fx = "[ {\"id\":\"boom\",\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0],[1,0,\"fx2\"],[2,0]],\"ms\":80,\"mode\":\"target\","
                         " \"sounds\":{\"300\":\"hit2\",\"0\":\"zap\"},\"hitAtMs\":233,\"layer\":\"behind\","
                         " \"flash\":[{\"atMs\":100,\"ms\":80,\"color\":\"#ff8000\"},{\"atMs\":0,\"ms\":50}],"
                         " \"shake\":[{\"atMs\":100,\"ms\":200,\"strength\":3}],\"at\":\"striker\"},"
                         " {\"id\":\"wall\",\"sheet\":\"bg\",\"frames\":[[0,0]],\"layer\":\"screen\",\"mirror\":true},"
                         " {\"id\":\"orb\",\"group\":\"blade\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"mode\":\"travel\",\"travelMs\":300,\"returns\":true,\"lingerMs\":120} ]";
  const bool ok = loadGame(makeGame("all", fx), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleArt& a = db.graphics.battle;
  CHECK_EQ((int)a.effects.size(), 3);
  const BattleEffect& e = a.effects[0];
  CHECK(e.id == "boom" && e.extended);
  CHECK_EQ((int)e.sounds.size(), 2);
  CHECK(e.sounds[0].first == 0 && e.sounds[0].second == "zap" && e.sounds[1].first == 300 && e.sounds[1].second == "hit2");  // in time order
  CHECK_EQ(e.hitAtMs, 233);
  CHECK(e.layer == FxLayer::Behind && e.atStriker && !e.travel);
  CHECK_EQ((int)e.flash.size(), 2);
  CHECK(e.flash[0].atMs == 100 && e.flash[0].ms == 80 && e.flash[0].rgb == 0xff8000);
  CHECK(e.flash[1].rgb == 0xffffff);  // the default colour
  CHECK(e.shake.size() == 1 && e.shake[0].strength == 3 && e.shake[0].ms == 200);
  // Frames from several sheets: the second frame is from fx2.
  CHECK_EQ(e.anim.sheetOfFrame(0), db.graphics.findSheet("fx"));
  CHECK_EQ(e.anim.sheetOfFrame(1), db.graphics.findSheet("fx2"));
  CHECK_EQ(e.anim.sheetOfFrame(2), db.graphics.findSheet("fx"));
  const BattleEffect& w = a.effects[1];
  CHECK(w.layer == FxLayer::Screen && w.mirror && w.weapon < 0 && w.group.empty());  // a library effect
  const BattleEffect& o = a.effects[2];
  CHECK(o.travel && o.returns && o.lingerMs == 120 && o.travelMs == 300);
  // A plain effect has none of it and logs nothing new.
  Database db2;
  CHECK(loadGame(makeGame("plain", one("\"mode\":\"travel\"")), db2, err));
  const BattleEffect& p = db2.graphics.battle.effects[0];
  CHECK(!p.extended && p.sounds.empty() && p.hitAtMs == 0 && p.layer == FxLayer::Front && p.flash.empty() && p.shake.empty() && !p.atStriker && !p.returns &&
        p.lingerMs == 0 && p.anim.frameSheets.empty());
}

TEST(battle_effects_refusals) {
  expectError("snd1", one("\"sounds\":[\"zap\"]"), "\"sounds\" must be an object");
  expectError("snd2", one("\"sounds\":{\"soon\":\"zap\"}"), "\"sounds\" key \"soon\"");
  expectError("snd3", one("\"sounds\":{\"-5\":\"zap\"}"), "\"sounds\" key \"-5\"");
  expectError("snd4", one("\"sounds\":{\"5\":\"no way\"}"), "must name a sound");
  expectError("snd5", one("\"sounds\":{\"5\":3}"), "must name a sound");
  expectError("hit1", one("\"hitAtMs\":-1"), "\"hitAtMs\" must be between 0 and 15000");
  expectError("hit2", one("\"hitAtMs\":19000"), "\"hitAtMs\" must be between 0 and 15000");
  expectError("hit3", one("\"mode\":\"travel\",\"hitAtMs\":100"), "\"hitAtMs\" is for an effect that appears");
  expectError("hit4", one("\"hitAtMs\":1.5"), "hitAtMs", "whole number");
  expectError("lay1", one("\"layer\":\"above\""), "\"layer\" must be \"front\", \"behind\" or \"screen\"");
  expectError("lay2", one("\"layer\":\"screen\",\"mode\":\"travel\""), "screen");
  expectError("mir1", one("\"mirror\":true"), "\"mirror\" is only for \"layer\": \"screen\"");
  expectError("mir2", one("\"mirror\":\"yes\""), "mirror");
  expectError("at1", one("\"at\":\"sky\""), "\"at\" must be \"target\" or \"striker\"");
  expectError("at2", one("\"at\":\"striker\",\"mode\":\"travel\""), "not a \"travel\" effect");
  expectError("at3", one("\"at\":\"striker\",\"layer\":\"screen\""), "no \"at\"");
  expectError("ret1", one("\"returns\":true"), "\"returns\" is only for a \"travel\" effect");
  expectError("ret2", one("\"mode\":\"travel\",\"returns\":1"), "returns");
  expectError("lin1", one("\"lingerMs\":100"), "\"lingerMs\" is only for a \"travel\" effect");
  expectError("lin2", one("\"mode\":\"travel\",\"lingerMs\":6000"), "\"lingerMs\" must be between 0 and 5000");
  expectError("fl1", one("\"flash\":{}"), "\"flash\" must be a list");
  expectError("fl2", one("\"flash\":[3]"), "flash 1", "must be {");
  expectError("fl3", one("\"flash\":[{\"atMs\":0,\"ms\":5}]"), "flash 1", "\"ms\" must be between 10 and 5000");
  expectError("fl4", one("\"flash\":[{\"atMs\":-1,\"ms\":50}]"), "\"atMs\" must be between 0 and 60000");
  expectError("fl5", one("\"flash\":[{\"atMs\":0,\"ms\":50,\"color\":\"red\"}]"), "\"color\" must be a colour such as");
  expectError("fl6", one("\"flash\":[{\"atMs\":0,\"ms\":50,\"color\":\"#12345g\"}]"), "\"color\" must be a colour such as");
  expectError("fl7", one("\"flash\":[{\"ms\":50}]"), "atMs");
  expectError("sh1", one("\"shake\":\"hard\""), "\"shake\" must be a list");
  expectError("sh2", one("\"shake\":[{\"atMs\":0,\"ms\":50,\"strength\":0}]"), "shake 1", "\"strength\" must be between 1 and 64");
  expectError("sh3", one("\"shake\":[{\"atMs\":0,\"ms\":50}]"), "strength");
  expectError("sh4", one("\"shake\":[{\"atMs\":0,\"ms\":99999,\"strength\":2}]"), "\"ms\" must be between 10 and 5000");
  // Frames from several sheets.
  expectError("fr1", "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0,\"nowhere\"]]} ]", "frame 0 names unknown sheet \"nowhere\"");
  expectError("fr2", "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0,\"odd\"]]} ]", "same cell size");
  expectError("fr3", "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0,7]]} ]", "third entry must be a sheet id");
  expectError("fr4", "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[9,0,\"fx2\"]]} ]", "outside sheet \"fx2\"");
  // Ids, lists, byType.
  expectError("id1", "[ {\"id\":\"a b\",\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0]]} ]", "\"id\" must be letters");
  expectError("id2", "[ {\"id\":\"a\",\"sheet\":\"fx\",\"frames\":[[0,0]]}, {\"id\":\"a\",\"sheet\":\"fx\",\"frames\":[[0,0]]} ]", "a second effect with the id \"a\"");
  expectError("id3", "[ {\"sheet\":\"fx\",\"frames\":[[0,0]]} ]", "needs exactly one of");
  expectError("li1", "[ {\"weapon\":\"longbow\",\"effects\":[\"ghost\"]} ]", "there is no effect with the id \"ghost\"");
  expectError("li2", "[ {\"id\":\"lib\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"effects\":[\"lib\"]} ]", "belong to an entry with a \"weapon\" or a \"group\"");
  expectError("li3", "[ {\"weapon\":\"longbow\",\"effects\":3} ]", "must be an effect id or a list");
  expectError("li4", "[ {\"id\":\"a\",\"sheet\":\"fx\",\"frames\":[[0,0]]}, {\"weapon\":\"longbow\",\"effects\":[\"a\",\"x\"]} ]", "there is no effect with the id \"x\"");
  expectError("li5", "[ {\"weapon\":\"longbow\",\"effects\":[\"b\"]}, {\"id\":\"b\",\"group\":\"blade\",\"effects\":[\"a\"]}, {\"id\":\"a\",\"sheet\":\"fx\",\"frames\":[[0,0]]} ]",
              "has no pictures of its own", "\"b\"");
  expectError("li6", "[ {\"weapon\":\"longbow\",\"effects\":[\"a\",\"a\",\"a\",\"a\",\"a\",\"a\",\"a\",\"a\",\"a\"]} ]", "up to 8");
  expectError("by1", "[ {\"weapon\":\"longbow\",\"byType\":[\"a\"]} ]", "\"byType\" must be {");
  expectError("by2", "[ {\"weapon\":\"longbow\",\"byType\":{\"wizard\":\"a\"}} ]", "there is no unit type with the id \"wizard\"");
  expectError("by3", "[ {\"id\":\"a\",\"sheet\":\"fx\",\"frames\":[[0,0]]}, {\"weapon\":\"longbow\",\"byType\":{\"knight\":\"nope\"}} ]", "\"byType\" \"knight\"", "nope");
  expectError("by4", "[ {\"weapon\":\"longbow\",\"byType\":{\"knight\":[1]}} ]", "must be an effect id or a list");
  expectError("by5", "[ {\"weapon\":\"longbow\"} ]", "sheet");  // no pictures and no lists: the old rule
  expectError("dup", "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0]]}, {\"weapon\":\"longbow\",\"effects\":[]} ]", "a second effect for \"longbow\"");
}

TEST(battle_effects_which_effects_a_strike_gets) {
  Database db;
  std::string err;
  const std::string fx =
      "[ {\"id\":\"charge\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"at\":\"striker\"},"
      "  {\"id\":\"bolt\",\"sheet\":\"fx\",\"frames\":[[1,0]],\"mode\":\"travel\"},"
      "  {\"id\":\"ember\",\"sheet\":\"fx\",\"frames\":[[2,0]]},"
      "  {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[3,0]],\"effects\":[\"charge\",\"bolt\"],\"byType\":{\"knight\":[\"ember\"],\"archer\":[]}},"
      "  {\"group\":\"blade\",\"effects\":\"bolt\"} ]";
  const bool ok = loadGame(makeGame("pick", fx), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleArt& a = db.graphics.battle;
  const int knight = db.findUnitType("knight"), archer = db.findUnitType("archer");
  const int bow = db.findWeapon("longbow"), sword = db.findWeapon("sword");
  // A byType entry replaces the entry's own pictures and its list; an empty list means none for that type.
  CHECK(a.resolveEffects(bow, "bow", knight) == std::vector<int>({2}));
  CHECK(a.resolveEffects(bow, "bow", archer).empty());
  // Another type: the entry's own pictures, then its list, in that order. (Nobody else here: use a type index that is not listed.)
  CHECK(a.resolveEffects(bow, "bow", -1) == std::vector<int>({3, 0, 1}));
  // An entry with no pictures of its own and a list; found by group.
  CHECK(a.resolveEffects(sword, "blade", knight) == std::vector<int>({1}));
  CHECK(a.resolveEffects(-1, "none", knight).empty());
  CHECK_EQ(a.findEffect("ember"), 2);
  CHECK_EQ(a.findEffect("nobody"), -1);
}

TEST(battle_effects_sounds_are_checked_against_the_audio_file) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("snd", one("\"sounds\":{\"0\":\"zap\",\"90\":\"boom\"}")), db, err));
  AudioData d;
  CHECK(!checkStoryAudio(db, d, err));  // no data/audio.json at all
  CHECK(has(err, "battle effect 1: sound \"zap\" is not in this game"));
  d.present = true;
  SoundDef zap;
  zap.id = "zap";
  d.sounds.push_back(zap);
  err.clear();
  CHECK(!checkStoryAudio(db, d, err));
  CHECK(has(err, "battle effect 1: sound \"boom\" is not in data/audio.json \"sounds\""));
  SoundDef boom;
  boom.id = "boom";
  d.sounds.push_back(boom);
  CHECK(checkStoryAudio(db, d, err));
}

// ---- the timeline ------------------------------------------------------------------------------------------------------------------------

namespace {

Event strikeEv(int a, int b, int flags, int damage) {
  Event e;
  e.type = EventType::Attack;
  e.a = a;
  e.b = b;
  e.d = damage;
  e.flags = flags;
  return e;
}
FightInfo info() {
  FightInfo f;
  f.units[1] = {1, "Eirik", 3, 20, 24, 2};
  f.units[2] = {2, "Gorm", 5, 10, 30, 3};
  return f;
}
// The attack: 430 ms, the hit frame at 200 ms, the spawn frame at 100 ms.
FighterTimes animated() {
  FighterTimes t;
  t.has[BaAttack] = t.has[BaHurt] = t.has[BaDefeat] = t.has[BaDodge] = true;
  t.total[BaAttack] = 430;
  t.hitAt[BaAttack] = 200;
  t.total[BaHurt] = 180;
  t.total[BaDefeat] = 500;
  t.total[BaDodge] = 250;
  for (int& v : t.spawnAt) v = -1;
  t.spawnAt[BaAttack] = 100;
  return t;
}
EffectTimes target(int hitAtMs = 0, int total = 160) {
  EffectTimes e;
  e.has = true;
  e.total = total;
  e.hitAtMs = hitAtMs;
  e.name = "fx";
  return e;
}
EffectTimes travelFx(int ms = 250) {
  EffectTimes e;
  e.has = true;
  e.travel = true;
  e.travelMs = ms;
  e.total = 100;
  e.name = "arrow";
  return e;
}
const FightBeat* beat(const FightTimeline& tl, BeatKind k, int nth = 0) {
  for (const FightBeat& b : tl.beats)
    if (b.kind == k && nth-- == 0) return &b;
  return nullptr;
}
bool near(double a, double b) { return std::fabs(a - b) < 0.0005; }

FightTimeline build(const std::vector<std::vector<EffectTimes>>& fx, int flags = kHit, int strikes = 1) {
  std::vector<Event> run;
  for (int i = 0; i < strikes; ++i) run.push_back(strikeEv(i % 2 ? 2 : 1, i % 2 ? 1 : 2, flags | (i % 2 ? kCounter : 0), 4));
  FightScript s = buildFightScript(run, info());
  FighterTimes times[2] = {animated(), animated()};
  return buildTimelineList(s, times, fx);
}

}  // namespace

TEST(effect_timeline_without_new_keys_is_what_it_was) {
  FightScript s = buildFightScript({strikeEv(1, 2, kHit, 4)}, info());
  FighterTimes times[2] = {animated(), animated()};
  const FightTimeline oldForm = buildTimeline(s, times, std::vector<EffectTimes>{travelFx()});
  const FightTimeline list = buildTimelineList(s, times, {{travelFx()}});
  CHECK_EQ(oldForm.fx.size(), (size_t)1);
  CHECK(near(oldForm.total, list.total) && oldForm.beats.size() == list.beats.size());
  const FightBeat* start = beat(list, BeatKind::StrikeStart);
  const FightBeat* hit = beat(list, BeatKind::Hit);
  CHECK(start && hit);
  if (!start || !hit) return;
  CHECK(near(list.fx[0].t0, start->t + 0.1));  // the spawn frame
  CHECK(near(hit->t, start->t + 0.35));          // the hit waits for the arrival
  CHECK(!beat(list, BeatKind::FxStart) && !beat(list, BeatKind::FxSound));  // no new log lines unless a new key is used
  // A target effect starts at the hit, as always.
  const FightTimeline t2 = buildTimelineList(s, times, {{target()}});
  CHECK(near(t2.fx[0].t0, beat(t2, BeatKind::Hit)->t));
}

TEST(effect_timeline_hit_at_ms_starts_the_effect_ahead_of_the_hit) {
  EffectTimes e = target(150);
  e.extended = true;
  e.sounds = {{0, "zap"}, {150, "boom"}};
  e.flash = {{100, 80, 0xff0000}};
  e.shake = {{100, 200, 2}};
  const FightTimeline tl = build({{e}});
  const double start = beat(tl, BeatKind::StrikeStart)->t, hit = beat(tl, BeatKind::Hit)->t;
  CHECK(near(hit, start + 0.2));          // the strike's own hit moment is the reference
  CHECK(near(tl.fx[0].t0, hit - 0.150));  // the effect leads in by 150 ms
  CHECK(tl.fx[0].t0 >= start);
  const FightBeat* fxStart = beat(tl, BeatKind::FxStart);
  CHECK(fxStart && near(fxStart->t, hit - 0.15) && fxStart->label == "fx");
  const FightBeat* s0 = beat(tl, BeatKind::FxSound, 0);
  const FightBeat* s1 = beat(tl, BeatKind::FxSound, 1);
  CHECK(s0 && s1);
  if (!s0 || !s1) return;
  CHECK(s0->text == "zap" && near(s0->t, hit - 0.15) && s1->text == "boom" && near(s1->t, hit));  // the second lands with the hit
  CHECK(s0->label == "fx");
  CHECK_EQ(tl.flashes.size(), (size_t)1);
  CHECK(near(tl.flashes[0].t0, hit - 0.15 + 0.1) && near(tl.flashes[0].dur, 0.08) && tl.flashes[0].rgb == 0xff0000u);
  CHECK_EQ(tl.shakes.size(), (size_t)1);
  CHECK(near(tl.shakes[0].t0, hit - 0.15 + 0.1) && near(tl.shakes[0].dur, 0.2) && tl.shakes[0].strength == 2);
  // The order of the beats: the effect (and its sound) before the hit.
  size_t iFx = 0, iHit = 0, iSnd = 0;
  for (size_t i = 0; i < tl.beats.size(); ++i) {
    if (tl.beats[i].kind == BeatKind::FxStart) iFx = i;
    if (tl.beats[i].kind == BeatKind::FxSound && tl.beats[i].text == "zap") iSnd = i;
    if (tl.beats[i].kind == BeatKind::Hit) iHit = i;
  }
  CHECK(iFx < iHit && iSnd < iHit);
}

TEST(effect_timeline_a_long_lead_in_makes_the_hit_wait) {
  const FightTimeline tl = build({{target(500)}});
  const double start = beat(tl, BeatKind::StrikeStart)->t;
  CHECK(near(beat(tl, BeatKind::Hit)->t, start + 0.5));  // 500 ms ahead cannot start before the strike did: the hit waits
  CHECK(near(tl.fx[0].t0, start));
}

TEST(effect_timeline_plays_for_a_miss_too) {
  const FightTimeline tl = build({{target(150)}}, 0);
  const FightBeat* miss = beat(tl, BeatKind::Miss);
  CHECK(miss && !beat(tl, BeatKind::Hit));
  if (!miss) return;
  CHECK(near(tl.fx[0].t0, miss->t - 0.15));
}

TEST(effect_timeline_striker_effects_and_lists) {
  EffectTimes charge = target(300);  // a charge-up on the caster, 300 ms before the hit
  charge.striker = true;
  charge.name = "charge";
  EffectTimes bolt = travelFx(250);
  const FightTimeline tl = build({{charge, bolt}});
  const double start = beat(tl, BeatKind::StrikeStart)->t, hit = beat(tl, BeatKind::Hit)->t;
  CHECK_EQ(tl.fx.size(), (size_t)2);
  CHECK(tl.fx[0].fromSide == tl.fx[0].toSide);  // on the striker
  CHECK(tl.fx[1].travel && tl.fx[1].fromSide != tl.fx[1].toSide);
  CHECK(near(tl.fx[1].t0, start + 0.1));        // the shot leaves at the spawn frame
  CHECK(near(hit, start + 0.35));                // and lands 250 ms later (later than the strike's own hit frame and the charge's lead-in)
  CHECK(near(tl.fx[0].t0, hit - 0.3));
  CHECK_EQ(tl.fx[0].slot, 0);
  CHECK_EQ(tl.fx[1].slot, 1);
  // Two target effects with different lead-ins: the longer one decides when the hit may land.
  const FightTimeline t2 = build({{target(100), target(260)}});
  const double st2 = beat(t2, BeatKind::StrikeStart)->t, hit2 = beat(t2, BeatKind::Hit)->t;
  CHECK(near(hit2, st2 + 0.26));
  CHECK(near(t2.fx[0].t0, hit2 - 0.1) && near(t2.fx[1].t0, hit2 - 0.26));
}

TEST(effect_timeline_returning_and_lingering_projectiles) {
  EffectTimes back = travelFx(250);
  back.returns = true;
  back.extended = true;
  const FightTimeline tl = build({{back}}, kHit, 2);
  const double start = beat(tl, BeatKind::StrikeStart, 0)->t, hit = beat(tl, BeatKind::Hit, 0)->t;
  CHECK(near(hit, start + 0.35));         // the hit lands on arrival, as before
  CHECK(near(tl.fx[0].dur, 0.5));         // out 250 ms, back 250 ms
  CHECK(near(tl.fx[0].t0 + tl.fx[0].dur, start + 0.1 + 0.5));
  // The strike is not over before the projectile is home: the next strike starts after it.
  const FightBeat* next = beat(tl, BeatKind::StrikeStart, 1);
  CHECK(next && next->t >= tl.fx[0].t0 + tl.fx[0].dur);
  EffectTimes stay = travelFx(250);
  stay.lingerMs = 200;
  stay.extended = true;
  const FightTimeline t2 = build({{stay}});
  CHECK(near(t2.fx[0].dur, 0.45) && t2.fx[0].lingerMs == 200 && !t2.fx[0].returns);
  EffectTimes both = back;
  both.lingerMs = 100;
  const FightTimeline t3 = build({{both}});
  CHECK(near(t3.fx[0].dur, 0.6));  // out, stays, back
}

TEST(effect_timeline_sounds_of_an_effect_come_at_their_own_times) {
  EffectTimes e = travelFx(250);
  e.extended = true;
  e.sounds = {{0, "whoosh"}, {250, "thud"}};
  const FightTimeline tl = build({{e}});
  const double spawn = tl.fx[0].t0;
  CHECK(near(beat(tl, BeatKind::FxSound, 0)->t, spawn) && near(beat(tl, BeatKind::FxSound, 1)->t, spawn + 0.25));
  CHECK(near(beat(tl, BeatKind::FxSound, 1)->t, beat(tl, BeatKind::Hit)->t));  // the arrival is the hit
  for (size_t i = 1; i < tl.beats.size(); ++i) CHECK(tl.beats[i - 1].t <= tl.beats[i].t);  // the beats stay in time order
}
