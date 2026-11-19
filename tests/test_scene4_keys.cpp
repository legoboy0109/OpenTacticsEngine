// The fourth batch of opt-in battle scene keys (docs/BATTLE_SCENE.md): a range band's pan "backdrop" and "startMs", an effect's "atRange" (delay by fight
// distance) and a travel effect's "endOffset". The loader and its refusals, the pure layout, and the timeline (what a delay shifts and what it leaves).
#include <sys/stat.h>

#include <cmath>
#include <cstdio>

#include "battle/fight_timeline.h"
#include "core/base/json.h"
#include "core/data/battle_stage.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

const char* kStage = "{ \"width\": 240, \"height\": 160, \"feet\": {\"left\":[88,104],\"right\":[152,104]}, \"feetFar\": {\"left\":[56,104],\"right\":[184,104]} }";

// Same little game as test_scene3_keys.cpp: knight and archer, sword (blade) and longbow (bow, range 2), sheets "fx" (16 x 16 cells) and "bg".
std::string makeGame(const char* name, const std::string& effectsJson, const std::string& stageJson) {
  std::string root = ote_test::tempDir() + "/scene4_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}], \"rules\": {\"items\": {\"weapons\": \"carried\"}},"
                      " \"formulas\": {\"damage\":\"1\"}, \"maps\": [{\"id\":\"m\",\"name\":\"M\",\"file\":\"maps/m.json\"}] }"));
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
  png("bg.png", 240, 68);
  CHECK(writeTextFile(root + "/data/graphics.json",
                      "{ \"sheets\": [ {\"id\":\"fighters\",\"file\":\"art/fighters.png\",\"cellWidth\":32,\"cellHeight\":32},"
                      "                {\"id\":\"fx\",\"file\":\"art/fx.png\",\"cellWidth\":16,\"cellHeight\":16},"
                      "                {\"id\":\"bg\",\"file\":\"art/bg.png\",\"cellWidth\":240,\"cellHeight\":68} ],"
                      " \"battle\": { \"sets\": { \"knight\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]} } } },"
                      "   \"effects\": " + effectsJson + (stageJson.empty() ? std::string() : ", \"stage\": " + stageJson) + " } }"));
  return root;
}

std::string one(const std::string& extra) { return "[ {\"weapon\":\"sword\",\"sheet\":\"fx\",\"frames\":[[0,0],[1,0]]," + extra + "} ]"; }
std::string travel(const std::string& extra) { return "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"mode\":\"travel\"," + extra + "} ]"; }

void expectError(const char* name, const std::string& effects, const char* a, const char* b = "", const std::string& stage = std::string(kStage)) {
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame(name, effects, stage), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json") && has(err, a) && has(err, b));
}

bool load(const char* name, const std::string& effects, Database& db, const std::string& stage = std::string(kStage)) {
  std::string err;
  const bool ok = loadGame(makeGame(name, effects, stage), db, err);
  if (!ok) std::printf("  [%s] %s\n", name, err.c_str());
  CHECK(ok);
  return ok;
}

// A stage with one panning band from 5 squares; `pan` is the inside of its "pan" object.
std::string panStage(const std::string& pan) {
  return "{ \"width\": 240, \"height\": 160, \"feet\": {\"left\":[88,104],\"right\":[152,104]}, \"ranges\": [ { \"from\": 5, \"feet\": {\"left\":[40,100],\"right\":[400,100]},"
         " \"view\": \"each\", \"pan\": { \"panMs\": 250, \"leftView\": 0, \"rightView\": 200" + pan + " } } ] }";
}

Event ev(EventType t, int a, int b = -1, int c = -1, int d = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  return e;
}
FightInfo info(int dist) {
  FightInfo f;
  f.units[1] = {1, "Eirik", 3, 20, 24, 2, true};
  f.units[2] = {2, "Gorm", 5, 10, 30, 3, true};
  f.distance = dist;
  return f;
}
FighterTimes animated() {
  FighterTimes t;
  t.has[BaAttack] = t.has[BaHurt] = t.has[BaDefeat] = t.has[BaDodge] = true;
  t.total[BaAttack] = 430;
  t.hitAt[BaAttack] = 200;
  t.total[BaHurt] = 180;
  t.total[BaDefeat] = 500;
  t.total[BaDodge] = 250;
  for (int& v : t.spawnAt) v = -1;
  return t;
}
Event attack(int a, int b, int flags, int damage) {
  Event e = ev(EventType::Attack, a, b, 0, damage);
  e.flags = flags;
  return e;
}
const FightBeat* beat(const FightTimeline& tl, BeatKind k, int strike = -1) {
  for (const FightBeat& b : tl.beats)
    if (b.kind == k && (strike < 0 || b.strike == strike)) return &b;
  return nullptr;
}
bool near(double a, double b) { return std::fabs(a - b) < 1e-9; }

}  // namespace

// ---- pan "backdrop" and "startMs" ----

TEST(scene4_pan_backdrop_and_start_load) {
  Database plain;
  if (!load("pan_plain", "[]", plain, panStage(""))) return;
  const StageBand& p = plain.graphics.battle.stage.ranges[0];
  CHECK(!p.scrollBackdrop && p.panStartMs == -1);  // absent = today
  CHECK(!stageLayoutFor(plain.graphics.battle.stage, 6).scrollBackdrop && stageLayoutFor(plain.graphics.battle.stage, 6).panStartMs == -1);
  Database db;
  if (!load("pan_keys", "[]", db, panStage(", \"backdrop\": \"scroll\", \"startMs\": 400"))) return;
  const StageBand& b = db.graphics.battle.stage.ranges[0];
  CHECK(b.scrollBackdrop && b.panStartMs == 400);
  const StageLayout lay = stageLayoutFor(db.graphics.battle.stage, 6);
  CHECK(lay.each && lay.scrollBackdrop && lay.panStartMs == 400);
  CHECK(!stageLayoutFor(db.graphics.battle.stage, 1).scrollBackdrop);  // a fight outside the band is untouched
  Database fixed;
  if (!load("pan_fixed", "[]", fixed, panStage(", \"backdrop\": \"fixed\", \"startMs\": 0"))) return;
  CHECK(!fixed.graphics.battle.stage.ranges[0].scrollBackdrop && fixed.graphics.battle.stage.ranges[0].panStartMs == 0);
}

TEST(scene4_pan_backdrop_and_start_refusals) {
  expectError("pan1", "[]", "battle stage range 1 \"pan\"", "\"backdrop\" must be \"fixed\"", panStage(", \"backdrop\": \"slide\""));
  expectError("pan2", "[]", "\"backdrop\"", "a string", panStage(", \"backdrop\": 3"));
  expectError("pan3", "[]", "\"startMs\" must be between 0 and 15000", "", panStage(", \"startMs\": -1"));
  expectError("pan4", "[]", "\"startMs\" must be between 0 and 15000", "", panStage(", \"startMs\": 15001"));
  expectError("pan5", "[]", "\"startMs\"", "whole number", panStage(", \"startMs\": 2.5"));
  expectError("pan6", "[]", "unknown key \"scroll\"", "\"backdrop\", \"startMs\"", panStage(", \"scroll\": true"));
}

// ---- an effect's "atRange" ----

TEST(scene4_at_range_load) {
  Database db;
  if (!load("range", one("\"atRange\":[{\"from\":4,\"delayMs\":400},{\"from\":2,\"delayMs\":267}]"), db)) return;
  const BattleEffect& e = db.graphics.battle.effects[0];
  CHECK_EQ((int)e.atRange.size(), 2);
  CHECK(e.extended);
  if (e.atRange.size() != 2) return;
  CHECK(e.atRange[0].from == 2 && e.atRange[0].delayMs == 267 && e.atRange[1].from == 4 && e.atRange[1].delayMs == 400);  // sorted by "from"
  // The last band at or below the distance wins; below the first, none.
  CHECK_EQ(e.delayAt(1), 0);
  CHECK_EQ(e.delayAt(2), 267);
  CHECK_EQ(e.delayAt(3), 267);
  CHECK_EQ(e.delayAt(4), 400);
  CHECK_EQ(e.delayAt(9), 400);
  // A "from": 1 band covers melee; a 0 delay is allowed (a band can switch the delay back off).
  Database d2;
  if (!load("range_one", one("\"atRange\":[{\"from\":1,\"delayMs\":100},{\"from\":3,\"delayMs\":0}]"), d2)) return;
  CHECK_EQ(d2.graphics.battle.effects[0].delayAt(1), 100);
  CHECK_EQ(d2.graphics.battle.effects[0].delayAt(3), 0);
  // Without it nothing changes: not extended, no delay at any distance.
  Database plain;
  if (!load("range_plain", one("\"hitAtMs\":0"), plain)) return;
  CHECK(plain.graphics.battle.effects[0].atRange.empty() && plain.graphics.battle.effects[0].delayAt(5) == 0);
  Database none;
  if (!load("range_none", one("\"x\":0"), none)) return;
  CHECK(!none.graphics.battle.effects[0].extended);
  // A travel effect takes it too.
  Database tr;
  if (!load("range_travel", travel("\"atRange\":[{\"from\":2,\"delayMs\":150}]"), tr)) return;
  CHECK_EQ(tr.graphics.battle.effects[0].delayAt(2), 150);
}

TEST(scene4_at_range_refusals) {
  expectError("ar1", one("\"atRange\":[]"), "\"atRange\" must be a list of 1 to 16");
  expectError("ar2", one("\"atRange\":{}"), "\"atRange\" must be a list of 1 to 16");
  expectError("ar3", one("\"atRange\":[3]"), "atRange 1", "must be { \"from\": 2, \"delayMs\": 267 }");
  expectError("ar4", one("\"atRange\":[{\"from\":2,\"delayMs\":5,\"x\":1}]"), "atRange 1", "unknown key \"x\"");
  expectError("ar5", one("\"atRange\":[{\"delayMs\":5}]"), "\"from\"");
  expectError("ar6", one("\"atRange\":[{\"from\":2}]"), "\"delayMs\"");
  expectError("ar7", one("\"atRange\":[{\"from\":0,\"delayMs\":5}]"), "\"from\" must be between 1 and 99");
  expectError("ar8", one("\"atRange\":[{\"from\":100,\"delayMs\":5}]"), "\"from\" must be between 1 and 99");
  expectError("ar9", one("\"atRange\":[{\"from\":2,\"delayMs\":-1}]"), "\"delayMs\" must be between 0 and 15000");
  expectError("ar10", one("\"atRange\":[{\"from\":2,\"delayMs\":15001}]"), "\"delayMs\" must be between 0 and 15000");
  expectError("ar11", one("\"atRange\":[{\"from\":2,\"delayMs\":5},{\"from\":2,\"delayMs\":9}]"), "atRange 2", "a second entry starts at 2");
  expectError("ar12", one("\"atRange\":[{\"from\":2.5,\"delayMs\":5}]"), "\"from\"", "whole number");
  std::string many = "[";
  for (int i = 0; i < 17; ++i) many += std::string(i ? "," : "") + "{\"from\":" + std::to_string(i + 2) + ",\"delayMs\":1}";
  expectError("ar13", one("\"atRange\":" + many + "]"), "\"atRange\" must be a list of 1 to 16");
}

// ---- a travel effect's "endOffset" ----

TEST(scene4_end_offset) {
  Database db;
  if (!load("offset", travel("\"endOffset\":24"), db)) return;
  CHECK(db.graphics.battle.effects[0].endOffset == 24 && db.graphics.battle.effects[0].extended);
  Database zero;
  if (!load("offset0", travel("\"endOffset\":0"), zero)) return;
  CHECK_EQ(zero.graphics.battle.effects[0].endOffset, 0);
  Database plain;
  if (!load("offset_plain", travel("\"travelMs\":300"), plain)) return;
  CHECK(plain.graphics.battle.effects[0].endOffset == 0 && !plain.graphics.battle.effects[0].extended);
  expectError("eo1", travel("\"endOffset\":-401"), "\"endOffset\" must be a whole number from -400 to 400");
  expectError("eo2", travel("\"endOffset\":401"), "\"endOffset\" must be a whole number from -400 to 400");
  expectError("eo3", travel("\"endOffset\":1.5"), "\"endOffset\"", "whole number");
  expectError("eo4", one("\"endOffset\":10"), "\"endOffset\" is only for a \"travel\" effect");
}

// ---- the timeline ----

namespace {
EffectTimes target(int total, int hitAtMs = 0) {
  EffectTimes e;
  e.has = true;
  e.total = total;
  e.hitAtMs = hitAtMs;
  e.sounds.push_back(std::make_pair(50, std::string("zap")));
  FxFlash fl;
  fl.atMs = 10;
  e.flash.push_back(fl);
  FxHide h;
  h.atMs = 20;
  h.what = kHideBackdrop;
  e.hide.push_back(h);
  return e;
}
}  // namespace

TEST(scene4_timeline_delay_shifts_the_effect_and_the_hit_only) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 3)}, info(5));
  FighterTimes times[2] = {animated(), animated()};
  EffectTimes base = target(300);
  base.extended = true;
  EffectTimes late = base;
  late.delayMs = 400;
  const FightTimeline a = buildTimelineList(s, times, {{base}}, -1);
  const FightTimeline b = buildTimelineList(s, times, {{late}}, -1);
  CHECK(a.fx.size() == 1 && b.fx.size() == 1 && a.flashes.size() == 1 && b.flashes.size() == 1 && a.hides.size() == 1 && b.hides.size() == 1);
  if (a.fx.size() != 1 || b.fx.size() != 1) return;
  // The effect (its start, sounds, flash, hide) and the hit come 0.4 s later ...
  CHECK(near(b.fx[0].t0 - a.fx[0].t0, 0.4));
  CHECK(near(beat(b, BeatKind::Hit)->t - beat(a, BeatKind::Hit)->t, 0.4));
  CHECK(near(beat(b, BeatKind::FxStart)->t - beat(a, BeatKind::FxStart)->t, 0.4));
  CHECK(near(beat(b, BeatKind::FxSound)->t - beat(a, BeatKind::FxSound)->t, 0.4));
  CHECK(near(b.flashes[0].t0 - a.flashes[0].t0, 0.4) && near(b.hides[0].t0 - a.hides[0].t0, 0.4));
  // ... the striker's animation does not move.
  CHECK(near(beat(b, BeatKind::StrikeStart)->t, beat(a, BeatKind::StrikeStart)->t));
  CHECK(near(b.poses[s.strikes[0].actor][0].start, a.poses[s.strikes[0].actor][0].start));
  // With a lead-in the effect starts before the hit as before: the lead-in is kept, both are shifted.
  EffectTimes lead = target(300, 300), leadLate = lead;
  leadLate.delayMs = 400;
  const FightTimeline c = buildTimelineList(s, times, {{lead}}, -1), d = buildTimelineList(s, times, {{leadLate}}, -1);
  CHECK(near(d.fx[0].t0 - c.fx[0].t0, 0.4) && near(beat(d, BeatKind::Hit)->t - beat(c, BeatKind::Hit)->t, 0.4));
  CHECK(near(beat(d, BeatKind::Hit)->t - d.fx[0].t0, 0.3));
  // A travel effect leaves later and arrives later.
  EffectTimes arrow;
  arrow.has = arrow.travel = true;
  arrow.travelMs = 500;
  EffectTimes arrowLate = arrow;
  arrowLate.delayMs = 400;
  const FightTimeline e = buildTimelineList(s, times, {{arrow}}, -1), f = buildTimelineList(s, times, {{arrowLate}}, -1);
  CHECK(near(f.fx[0].t0 - e.fx[0].t0, 0.4) && near(beat(f, BeatKind::Hit)->t - beat(e, BeatKind::Hit)->t, 0.4));
  CHECK(near(beat(f, BeatKind::Hit)->t - f.fx[0].t0, 0.5));  // the flight still takes travelMs
  // A delay of 0 is no delay at all.
  EffectTimes zero = base;
  zero.delayMs = 0;
  const FightTimeline z = buildTimelineList(s, times, {{zero}}, -1);
  CHECK(near(z.fx[0].t0, a.fx[0].t0) && near(beat(z, BeatKind::Hit)->t, beat(a, BeatKind::Hit)->t) && near(z.total, a.total));
}

TEST(scene4_timeline_delay_leaves_the_camera) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 3)}, info(5));
  FighterTimes times[2] = {animated(), animated()};
  EffectTimes base = target(300), late = base;
  late.delayMs = 400;
  const FightTimeline a = buildTimelineList(s, times, {{base}}, 250);
  const FightTimeline b = buildTimelineList(s, times, {{late}}, 250);
  CHECK(a.cam.size() == 1 && b.cam.size() == 1);
  if (a.cam.size() != 1 || b.cam.size() != 1) return;
  CHECK(near(a.cam[0].t, b.cam[0].t) && near(a.cam[0].dur, b.cam[0].dur));  // the pan is timed as without the delay
  CHECK(near(beat(b, BeatKind::Hit)->t - beat(a, BeatKind::Hit)->t, 0.4));
}

TEST(scene4_timeline_pan_start) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 3), attack(2, 1, kHit | kCounter, 3)}, info(5));
  FighterTimes times[2] = {animated(), animated()};
  const FightTimeline plain = buildTimelineList(s, times, {}, 250), absent = buildTimelineList(s, times, {}, 250, -1);
  CHECK(plain.cam.size() == absent.cam.size() && near(plain.total, absent.total) && near(plain.cam[0].t, absent.cam[0].t));  // absent = today
  const FightTimeline tl = buildTimelineList(s, times, {}, 250, 600);
  CHECK_EQ((int)tl.cam.size(), 2);
  if (tl.cam.size() != 2) return;
  const double start0 = beat(tl, BeatKind::StrikeStart, 0)->t;
  CHECK(near(tl.cam[0].t, start0 + 0.6) && near(tl.cam[0].dur, 0.25));  // it leaves 600 ms after the strike started
  CHECK(tl.cam[0].side == 1 - s.initiator);
  // The hit does not wait for it, the camera is not there yet at the hit's time (0.2 s) but is on the struck fighter 0.25 s after it left.
  CHECK(near(beat(tl, BeatKind::Hit, 0)->t - start0, 0.2));
  CHECK(tl.cameraAt(start0 + 0.5) == (double)s.initiator);
  CHECK(tl.cameraAt(start0 + 0.6 + 0.25) == (double)(1 - s.initiator));
  // The next strike (the counter) starts after the camera has arrived, with the camera already on its actor.
  CHECK(beat(tl, BeatKind::StrikeStart, 1)->t >= start0 + 0.6 + 0.25);
  // Without a startMs it stays timed to arrive at the hit.
  CHECK(near(plain.cam[0].t + plain.cam[0].dur, beat(plain, BeatKind::Hit, 0)->t));
}
