// The fifth batch of opt-in battle scene keys (docs/BATTLE_SCENE.md "Holding a pose", "Platforms", "Effect timing by distance", "Range bands"):
//   an animation's "holdFrame" / "startFrame" / "startDelayMs" and the scene's "resultHoldMs" / "resultHoldAgainMs",
//   game.json "battlePlatformFrom", a travel effect's "endOffset" by distance (and the wider range), "atRangeFixed" and an effect's "panAtMs".
// The loader and its refusals, and the timeline (exactly when a held pose starts, waits and ends, and what leaves the old timing alone).
#include "posix_compat.h"

#include <cmath>
#include <cstdio>

#include "battle/fight_timeline.h"
#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// A game with knight and archer, sword and longbow, and sheets "fighters" (6 x 1 cells of 32), "fx" (4 x 1 of 16) and "bg".
// `sets` is the inside of "sets", `effects` the effects list, `battle` more keys of "battle", `gameKeys` more keys of game.json (each ending in a comma).
std::string makeGame(const char* name, const std::string& sets, const std::string& effects, const std::string& battle = std::string(),
                     const std::string& gameKeys = std::string()) {
  std::string root = ote_test::tempDir() + "/scene5_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\"," + gameKeys + " \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
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
  png("fighters.png", 192, 32);
  png("fx.png", 64, 16);
  CHECK(writeTextFile(root + "/data/graphics.json",
                      "{ \"sheets\": [ {\"id\":\"fighters\",\"file\":\"art/fighters.png\",\"cellWidth\":32,\"cellHeight\":32},"
                      "                {\"id\":\"fx\",\"file\":\"art/fx.png\",\"cellWidth\":16,\"cellHeight\":16} ],"
                      " \"battle\": { \"sets\": { " + sets + " }, \"effects\": " + effects + (battle.empty() ? std::string() : ", " + battle) + " } }"));
  return root;
}

const std::string kStand = "\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}";
std::string knight(const std::string& anims) { return "\"knight\": { \"animations\": { " + kStand + (anims.empty() ? "" : ", " + anims) + " } }"; }
// Six frames of the fighters sheet, the given extra keys.
std::string anim(const char* slot, const std::string& extra, int frames = 6) {
  std::string f;
  for (int i = 0; i < frames; ++i) f += std::string(i ? "," : "") + "[" + std::to_string(i) + ",0]";
  return std::string("\"") + slot + "\": {\"sheet\":\"fighters\",\"frames\":[" + f + "]" + (extra.empty() ? "" : ", " + extra) + "}";
}
std::string one(const std::string& extra) { return "[ {\"weapon\":\"sword\",\"sheet\":\"fx\",\"frames\":[[0,0],[1,0]]," + extra + "} ]"; }
std::string travel(const std::string& extra) { return "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"mode\":\"travel\"," + extra + "} ]"; }

void expectError(const char* name, const std::string& sets, const std::string& effects, const char* a, const char* b = "", const std::string& battle = std::string(),
                 const std::string& gameKeys = std::string()) {
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame(name, sets, effects, battle, gameKeys), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, a) && has(err, b));
}
bool load(const char* name, const std::string& sets, const std::string& effects, Database& db, const std::string& battle = std::string(),
          const std::string& gameKeys = std::string()) {
  std::string err;
  const bool ok = loadGame(makeGame(name, sets, effects, battle, gameKeys), db, err);
  if (!ok) std::printf("  [%s] %s\n", name, err.c_str());
  CHECK(ok);
  return ok;
}
const BattleSet& knightSet(const Database& db) { return db.graphics.battle.sets[(size_t)db.graphics.battle.findSet("knight")]; }

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
bool near(double a, double b) { return std::fabs(a - b) < 1e-6; }

FighterTimes plain() {
  FighterTimes t;
  for (int& v : t.spawnAt) v = -1;
  return t;
}
// The striker of the worked example in docs/BATTLE_SCENE.md: four frames of 200, 200, 200, 100 ms, the blow on frame 2 (at 400), "holdFrame": 3 (at 600).
FighterTimes holdingStriker() {
  FighterTimes t = plain();
  t.has[BaAttack] = true;
  t.total[BaAttack] = 700;
  t.hitAt[BaAttack] = 400;
  t.hold[BaAttack] = true;
  t.holdAt[BaAttack] = 600;
  t.holdLen[BaAttack] = 100;
  return t;
}
// Its opponent: six frames of 100 ms, "startFrame": 2 (brace = frames 0 and 1), "holdFrame": 3.
FighterTimes holdingDodger() {
  FighterTimes t = plain();
  t.has[BaDodge] = true;
  t.total[BaDodge] = 600;
  t.hold[BaDodge] = true;
  t.holdAt[BaDodge] = 300;
  t.holdLen[BaDodge] = 100;
  t.start[BaDodge] = true;
  t.startAt[BaDodge] = 200;
  t.brace[BaDodge] = true;
  t.braceAt[BaDodge] = 100;
  t.braceLen[BaDodge] = 100;
  return t;
}

}  // namespace

// ---- holdFrame / startFrame / startDelayMs ----

TEST(scene5_hold_keys_load) {
  Database db;
  if (!load("hold", knight(anim("attack", "\"hit\":2,\"holdFrame\":3", 4) + "," + anim("dodge", "\"startFrame\":2,\"holdFrame\":3,\"startDelayMs\":80") + "," +
                           anim("hurt", "\"holdFrame\":1", 2)),
            "[]", db))
    return;
  const BattleSet& s = knightSet(db);
  CHECK(s.anims[BaAttack].holdFrame == 3 && s.anims[BaAttack].startFrame == -1 && s.anims[BaAttack].startDelayMs == 0);
  CHECK(s.anims[BaDodge].holdFrame == 3 && s.anims[BaDodge].startFrame == 2 && s.anims[BaDodge].startDelayMs == 80);
  CHECK(s.anims[BaHurt].holdFrame == 1);
  CHECK(s.anims[BaStand].holdFrame == -1 && !s.anims[BaStand].holds());
  // A dodge with only a hold (the whole dodge then starts at the miss), a dodge with only a brace, and dodgeRanged.
  Database d2;
  if (!load("hold2", knight(anim("dodge", "\"holdFrame\":4") + "," + anim("dodgeRanged", "\"startFrame\":1,\"startDelayMs\":5000")), "[]", d2)) return;
  CHECK(knightSet(d2).anims[BaDodge].holdFrame == 4 && knightSet(d2).anims[BaDodge].startFrame == -1);
  CHECK(knightSet(d2).anims[BaDodgeRanged].startFrame == 1 && knightSet(d2).anims[BaDodgeRanged].holdFrame == -1 && knightSet(d2).anims[BaDodgeRanged].startDelayMs == 5000);
  // Without any of them: nothing is held, and the scene timing keeps its default (only read when something holds).
  Database d3;
  if (!load("hold_none", knight(anim("attack", "\"hit\":2") + "," + anim("dodge", "")), "[]", d3)) return;
  CHECK(!knightSet(d3).anims[BaAttack].holds() && !knightSet(d3).anims[BaDodge].holds());
  CHECK(d3.graphics.battle.timing.resultHoldMs == 533 && d3.graphics.battle.timing.resultHoldAgainMs == 0);
}

TEST(scene5_hold_keys_refusals) {
  const std::string where = "battle set \"knight\": animation ";
  expectError("h1", knight(anim("attack", "\"holdFrame\":6")), "[]", "animation \"attack\"", "\"holdFrame\" must be a whole number from 0 to 5");
  expectError("h2", knight(anim("attack", "\"holdFrame\":-1")), "[]", "\"holdFrame\" must be a whole number from 0 to 5");
  expectError("h3", knight(anim("attack", "\"holdFrame\":1.5")), "[]", "\"holdFrame\" must be a whole number from 0 to 5");
  expectError("h4", knight(anim("dodge", "\"startFrame\":6")), "[]", "\"startFrame\" must be a whole number from 0 to 5");
  expectError("h5", knight(anim("dodge", "\"holdFrame\":2,\"startDelayMs\":5001")), "[]", "\"startDelayMs\" must be a whole number from 0 to 5000");
  expectError("h6", knight(anim("dodge", "\"startDelayMs\":10")), "[]", "\"startDelayMs\" needs a \"startFrame\" or a \"holdFrame\"");
  expectError("h7", knight(anim("attack", "\"startFrame\":1")), "[]", "\"startFrame\" is only for \"dodge\" and \"dodgeRanged\"");
  expectError("h8", knight(anim("hurt", "\"holdFrame\":1,\"startDelayMs\":10")), "[]", "\"startDelayMs\" is only for \"dodge\"");
  expectError("h9", knight(anim("dodge", "\"startFrame\":4,\"holdFrame\":2")), "[]", "\"holdFrame\" 2 comes before \"startFrame\" 4");
  expectError("h10", knight(anim("attack", "\"hit\":3,\"holdFrame\":2")), "[]", "\"holdFrame\" 2 comes before the hit frame 3");
  expectError("h11", knight(anim("attack", "\"holdFrame\":2")), "[]", "comes before the hit frame 5");  // (no "hit": the blow is on the last frame)
  expectError("h12", knight(anim("defeat", "\"holdFrame\":2")), "[]", "a standing animation or a fall never waits");
  expectError("h13", "\"knight\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0],[1,0]],\"holdFrame\":1} } }", "[]", "a standing animation or a fall never waits");
  expectError("h14", knight(""), one("\"holdFrame\":1"), "battle effect 1", "only for an animation of a battle set");
}

TEST(scene5_scene_timing_load_and_refusals) {
  Database db;
  if (!load("timing", knight(""), "[]", db, "\"scene\": { \"resultHoldMs\": 700, \"resultHoldAgainMs\": 120 }")) return;
  CHECK(db.graphics.battle.timing.resultHoldMs == 700 && db.graphics.battle.timing.resultHoldAgainMs == 120);
  Database d2;
  if (!load("timing2", knight(""), "[]", d2, "\"scene\": { \"resultHoldMs\": 0 }")) return;
  CHECK(d2.graphics.battle.timing.resultHoldMs == 0 && d2.graphics.battle.timing.resultHoldAgainMs == 0);
  expectError("t1", knight(""), "[]", "battle scene", "\"resultHoldMs\" must be a whole number from 0 to 5000", "\"scene\": { \"resultHoldMs\": 5001 }");
  expectError("t2", knight(""), "[]", "\"resultHoldAgainMs\" must be a whole number from 0 to 5000", "", "\"scene\": { \"resultHoldAgainMs\": -1 }");
  expectError("t3", knight(""), "[]", "unknown key \"holdMs\"", "\"resultHoldMs\", \"resultHoldAgainMs\"", "\"scene\": { \"holdMs\": 5 }");
  expectError("t4", knight(""), "[]", "battle scene", "must be an object", "\"scene\": 5");
}

// What a held pose does, exactly (docs/BATTLE_SCENE.md "Holding a pose" has the same worked example).
TEST(scene5_timeline_worked_example) {
  // Strike 0: the first fighter attacks and MISSES; strike 1: the other one counters (a plain hit).
  FightScript s = buildFightScript({attack(1, 2, 0, 0), attack(2, 1, kHit | kCounter, 3)}, info(1));
  const int a = s.strikes[0].actor, d = 1 - a;
  FighterTimes times[2];
  times[a] = holdingStriker();
  times[d] = holdingDodger();
  const FightTimeline tl = buildTimelineList(s, times, {}, -1, -1, 533, 0);
  // The strike starts at 0.400 (the intro), the miss moment is the striker's hit frame: 0.800.
  CHECK(near(beat(tl, BeatKind::StrikeStart, 0)->t, 0.4));
  CHECK(near(beat(tl, BeatKind::Miss, 0)->t, 0.8));
  // The striker: frame 3 begins at 1.000 and is held until the result is over, 0.800 + 533 ms = 1.333; the pose ends there (0.4 + 0.933).
  CHECK(!tl.poses[a].empty() && tl.poses[a][0].anim == BaAttack);
  const FightPose& sp = tl.poses[a][0];
  CHECK(near(sp.start, 0.4) && near(sp.dur, 0.933));
  CHECK(sp.stalls.size() == 1 && sp.stalls[0].atMs == 600 && std::fabs(sp.stalls[0].pauseMs - 233) < 1e-3);
  // The dodger: the brace (frames 0 and 1) plays from the start of the strike, frame 1 is held until the miss; frame 2 starts at 0.800,
  // frame 3 at 0.900 and is held until 1.333; frames 4 and 5 play 1.333 to 1.533.
  CHECK(!tl.poses[d].empty() && tl.poses[d][0].anim == BaDodge);
  const FightPose& dp = tl.poses[d][0];
  CHECK(near(dp.start, 0.4) && near(dp.start + dp.dur, 1.533));
  CHECK(dp.stalls.size() == 2 && dp.stalls[0].atMs == 100 && std::fabs(dp.stalls[0].pauseMs - 200) < 1e-3 && dp.stalls[1].atMs == 300 &&
        std::fabs(dp.stalls[1].pauseMs - 333) < 1e-3);
  const BattleAnim frames6 = [] {
    BattleAnim an;
    an.sheet = 0;
    for (int i = 0; i < 6; ++i) {
      an.frames.push_back(SpriteCell());
      an.ms.push_back(100);
    }
    return an;
  }();
  auto frameAt = [&](double t) { return frames6.frameAt((int)dp.animMsAt(t - dp.start)); };
  CHECK_EQ(frameAt(0.45), 0);
  CHECK_EQ(frameAt(0.55), 1);   // the brace's last frame ...
  CHECK_EQ(frameAt(0.79), 1);   // ... held until the miss
  CHECK_EQ(frameAt(0.81), 2);   // the dodge proper begins at the miss
  CHECK_EQ(frameAt(0.95), 3);
  CHECK_EQ(frameAt(1.30), 3);   // frozen until the result is over
  CHECK_EQ(frameAt(1.34), 4);
  CHECK_EQ(frameAt(1.45), 5);
  CHECK(tl.poseAt(d, 1.50) != nullptr && tl.poseAt(d, 1.54) == nullptr);
  // The next strike begins after every held pose has played its remaining frames, plus the usual 0.3 s gap: 1.533 + 0.3.
  CHECK(near(beat(tl, BeatKind::StrikeStart, 1)->t, 1.833));
}

TEST(scene5_timeline_absent_keys_change_nothing) {
  FightScript s = buildFightScript({attack(1, 2, 0, 0), attack(2, 1, kHit | kCounter, 3)}, info(1));
  const int a = s.strikes[0].actor, d = 1 - a;
  FighterTimes times[2];
  times[a] = holdingStriker();
  times[d] = holdingDodger();
  for (BattleAnimId id : {BaAttack, BaDodge}) {  // the same animations with the keys left out
    times[a].hold[id] = times[a].start[id] = times[a].brace[id] = false;
    times[d].hold[id] = times[d].start[id] = times[d].brace[id] = false;
  }
  // Whatever the scene-wide durations say, nothing waits: the dodge starts 120 ms before the miss and plays once, the striker plays its frames at their times.
  const FightTimeline tl = buildTimelineList(s, times, {}, -1, -1, 533, 0), other = buildTimelineList(s, times, {}, -1, -1, 4000, 900);
  CHECK(tl.poses[a][0].stalls.empty() && tl.poses[d][0].stalls.empty());
  CHECK(near(tl.poses[d][0].start, 0.8 - 0.12) && near(tl.poses[d][0].dur, 0.6));
  CHECK(near(tl.poses[a][0].start, 0.4) && near(tl.poses[a][0].dur, 0.7));
  CHECK(near(tl.total, other.total) && near(beat(tl, BeatKind::StrikeStart, 1)->t, beat(other, BeatKind::StrikeStart, 1)->t));
  CHECK(near(beat(tl, BeatKind::StrikeStart, 1)->t, 0.68 + 0.6 + 0.3));  // the dodge's own end (it began at 0.68), then the gap
  // The default call (no durations passed) is the same as passing them.
  const FightTimeline dflt = buildTimelineList(s, times, {});
  CHECK(near(dflt.total, tl.total));
}

TEST(scene5_timeline_dodge_without_a_brace_starts_at_the_miss) {
  FightScript s = buildFightScript({attack(1, 2, 0, 0)}, info(1));
  const int a = s.strikes[0].actor, d = 1 - a;
  FighterTimes times[2];
  times[a] = plain();  // (no animations: the fallback strike, 280 ms to the blow)
  times[d] = holdingDodger();
  times[d].start[BaDodge] = times[d].brace[BaDodge] = false;  // "holdFrame" alone
  times[d].startDelay[BaDodge] = 80;                          // "startDelayMs": 80
  const FightTimeline tl = buildTimelineList(s, times, {}, -1, -1, 533, 0);
  const double miss = beat(tl, BeatKind::Miss, 0)->t;
  CHECK(near(miss, 0.4 + 0.28));
  const FightPose& dp = tl.poses[d][0];
  CHECK(near(dp.start, miss + 0.08));  // the whole dodge starts at the miss plus startDelayMs
  // Frame 3 starts 300 ms later and is held until miss + 533 ms; frames 4 and 5 follow.
  CHECK(dp.stalls.size() == 1 && dp.stalls[0].atMs == 300);
  CHECK(near(dp.start + dp.dur, miss + 0.533 + 0.2));  // frame 3 ends at the result, then frames 4 and 5 (200 ms)
  // A brace only (startFrame, no holdFrame): the miss starts the dodge proper and nothing waits after it.
  FighterTimes brace = holdingDodger();
  brace.hold[BaDodge] = false;
  times[d] = brace;
  const FightTimeline b = buildTimelineList(s, times, {}, -1, -1, 533, 0);
  const FightPose& bp = b.poses[d][0];
  CHECK(near(bp.start, 0.4) && bp.stalls.size() == 1 && bp.stalls[0].atMs == 100);
  CHECK(near(bp.start + 0.2 + bp.stalls[0].pauseMs / 1000.0, miss));  // frame 2 starts at the miss
  CHECK(near(bp.start + bp.dur, miss + 0.4));                           // and the remaining frames 2 to 5 play straight through
  // A brace that is already longer than the wait to the miss does not hold at all.
  FighterTimes slow = holdingDodger();
  slow.hold[BaDodge] = false;
  slow.braceAt[BaDodge] = 400;  // the brace's last frame begins 400 ms in
  slow.braceLen[BaDodge] = 100;
  times[d] = slow;
  const FightTimeline c = buildTimelineList(s, times, {}, -1, -1, 533, 0);
  CHECK(c.poses[d][0].stalls.empty());
}

TEST(scene5_timeline_result_over_waits_for_the_effect_and_the_next_strike) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 3)}, info(1));
  const int a = s.strikes[0].actor;
  FighterTimes times[2];
  times[a] = holdingStriker();
  times[1 - a] = plain();
  EffectTimes e;
  e.has = true;
  e.total = 200;
  e.sounds.push_back(std::make_pair(1500, std::string("boom")));  // the effect is busy until its last sound: 0.8 + 1.5 = 2.3
  const FightTimeline tl = buildTimelineList(s, times, {{e}}, -1, -1, 533, 0);
  const FightPose& sp = tl.poses[a][0];
  CHECK(near(sp.start + sp.dur, 2.3));  // later than hit + 533 ms (1.333): the pose waits for the effect
  // A delay by distance moves the hit, the effect and so the end: 0.2 s later.
  EffectTimes late = e;
  late.delayMs = 200;
  const FightTimeline tl2 = buildTimelineList(s, times, {{late}}, -1, -1, 533, 0);
  CHECK(near(tl2.poses[a][0].start + tl2.poses[a][0].dur, 2.5));
  // An effect that ends sooner than the result hold changes nothing: 0.8 + 533 ms.
  EffectTimes brief;
  brief.has = true;
  brief.total = 200;
  const FightTimeline tl3 = buildTimelineList(s, times, {{brief}}, -1, -1, 533, 0);
  CHECK(near(tl3.poses[a][0].start + tl3.poses[a][0].dur, 0.8 + 0.533));
  // A fade's ramp counts too (in, hold, out).
  EffectTimes dim = brief;
  FxFade fd;
  fd.atMs = 100;
  fd.ms = 300;
  fd.holdMs = 400;
  dim.fade.push_back(fd);
  const FightTimeline tl4 = buildTimelineList(s, times, {{dim}}, -1, -1, 533, 0);
  CHECK(near(tl4.poses[a][0].start + tl4.poses[a][0].dur, 0.8 + 0.1 + 0.3 + 0.4 + 0.3));
}

TEST(scene5_timeline_again_and_the_next_strike) {
  // The same fighter strikes twice (a follow-up): "resultHoldAgainMs" lengthens the first hold only.
  FightScript s = buildFightScript({attack(1, 2, kHit, 3), attack(1, 2, kHit, 3)}, info(1));
  const int a = s.strikes[0].actor;
  FighterTimes times[2];
  times[a] = holdingStriker();
  times[1 - a] = plain();
  const FightTimeline base = buildTimelineList(s, times, {}, -1, -1, 533, 0), again = buildTimelineList(s, times, {}, -1, -1, 533, 150);
  CHECK(near(base.poses[a][0].start + base.poses[a][0].dur, 0.8 + 0.533));
  CHECK(near(again.poses[a][0].start + again.poses[a][0].dur, 0.8 + 0.533 + 0.15));
  // The second strike's pose is its own (the last strike has no "again").
  CHECK(near(again.poses[a][1].dur, base.poses[a][1].dur));
  // The second strike begins after the first has finished, plus the gap.
  CHECK(near(beat(base, BeatKind::StrikeStart, 1)->t, 0.8 + 0.533 + 0.3));
  CHECK(near(beat(again, BeatKind::StrikeStart, 1)->t, 0.8 + 0.533 + 0.15 + 0.3));
  // A counter (the other fighter strikes next) is not "again".
  FightScript c = buildFightScript({attack(1, 2, kHit, 3), attack(2, 1, kHit | kCounter, 3)}, info(1));
  FighterTimes ct[2];
  ct[c.strikes[0].actor] = holdingStriker();
  ct[1 - c.strikes[0].actor] = plain();
  const FightTimeline k = buildTimelineList(c, ct, {}, -1, -1, 533, 150);
  CHECK(near(k.poses[c.strikes[0].actor][0].start + k.poses[c.strikes[0].actor][0].dur, 0.8 + 0.533));
}

TEST(scene5_timeline_hurt_hold_and_sounds) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 3)}, info(1));
  const int a = s.strikes[0].actor, d = 1 - a;
  FighterTimes times[2];
  times[a] = plain();
  times[d] = plain();
  times[d].has[BaHurt] = true;
  times[d].total[BaHurt] = 300;  // three frames of 100 ms, "holdFrame": 1
  times[d].hold[BaHurt] = true;
  times[d].holdAt[BaHurt] = 100;
  times[d].holdLen[BaHurt] = 100;
  times[d].soundAt[BaHurt].push_back(std::make_pair(200, std::string("late")));  // on frame 2, after the hold
  times[d].soundAt[BaHurt].push_back(std::make_pair(100, std::string("early")));  // on the held frame itself
  const FightTimeline tl = buildTimelineList(s, times, {}, -1, -1, 533, 0);
  const double hit = beat(tl, BeatKind::Hit)->t;
  const FightPose& hp = tl.poses[d][0];
  CHECK(hp.anim == BaHurt && near(hp.start, hit) && near(hp.start + hp.dur, hit + 0.533 + 0.1));  // frame 1 held to hit + 533 ms, then frame 2
  // A sound of a frame begins with the frame: the held frame's at its start, the later frame's after the wait.
  double early = -1, late = -1;
  for (const FightBeat& b : tl.beats) {
    if (b.text == "early") early = b.t;
    if (b.text == "late") late = b.t;
  }
  CHECK(near(early, hit + 0.1) && near(late, hit + 0.533));
}

// ---- game.json "battlePlatformFrom" ----

TEST(scene5_platform_from_load) {
  Database d0;
  if (!load("plat0", knight(""), "[]", d0)) return;
  CHECK(!d0.battlePlatformOwn);  // absent = "same"
  Database same;
  if (!load("plat1", knight(""), "[]", same, "", "\"battlePlatformFrom\": \"same\",")) return;
  CHECK(!same.battlePlatformOwn);
  Database own;
  if (!load("plat2", knight(""), "[]", own, "", "\"battlePlatformFrom\": \"own\",")) return;
  CHECK(own.battlePlatformOwn);
  expectError("plat3", knight(""), "[]", "\"battlePlatformFrom\" must be \"same\"", "got \"both\"", "", "\"battlePlatformFrom\": \"both\",");
  expectError("plat4", knight(""), "[]", "\"battlePlatformFrom\"", "a string", "", "\"battlePlatformFrom\": 1,");
}

// ---- endOffset by distance, and the wider range ----

TEST(scene5_end_offset_by_distance) {
  Database db;
  if (!load("eo", knight(""),
            travel("\"endOffset\":10,\"atRange\":[{\"from\":3,\"endOffset\":-40},{\"from\":5,\"delayMs\":100},{\"from\":7,\"delayMs\":50,\"endOffset\":0}]"), db))
    return;
  const BattleEffect& e = db.graphics.battle.effects[0];
  CHECK_EQ(e.endOffsetAt(1), 10);  // no step reaches it: the plain endOffset
  CHECK_EQ(e.endOffsetAt(2), 10);
  CHECK_EQ(e.endOffsetAt(3), -40);  // a step that gives one wins, negative = past the target
  CHECK_EQ(e.endOffsetAt(4), -40);
  CHECK_EQ(e.endOffsetAt(5), 10);   // the counting step (from 5) gives none: the plain one
  CHECK_EQ(e.endOffsetAt(7), 0);
  CHECK_EQ(e.delayAt(3), 0);        // a step with no delayMs delays nothing
  CHECK_EQ(e.delayAt(5), 100);
  CHECK_EQ(e.delayAt(9), 50);
  CHECK(e.extended);
  // Only "endOffset" in a step (no delay) is fine; the widened plain limits.
  Database lim;
  if (!load("eo_lim", knight(""), travel("\"endOffset\":-400,\"atRange\":[{\"from\":2,\"endOffset\":400}]"), lim)) return;
  CHECK_EQ(lim.graphics.battle.effects[0].endOffset, -400);
  CHECK_EQ(lim.graphics.battle.effects[0].endOffsetAt(2), 400);
  Database plain;
  if (!load("eo_plain", knight(""), travel("\"travelMs\":300"), plain)) return;
  CHECK(plain.graphics.battle.effects[0].endOffsetAt(1) == 0 && plain.graphics.battle.effects[0].endOffsetAt(9) == 0);
  expectError("eo1", knight(""), travel("\"endOffset\":401"), "\"endOffset\" must be a whole number from -400 to 400");
  expectError("eo2", knight(""), travel("\"atRange\":[{\"from\":2,\"endOffset\":-401}]"), "atRange 1", "\"endOffset\" must be a whole number from -400 to 400");
  expectError("eo3", knight(""), travel("\"atRange\":[{\"from\":2,\"endOffset\":1.5}]"), "atRange 1", "whole number");
  expectError("eo4", knight(""), travel("\"atRange\":[{\"from\":2}]"), "atRange 1", "needs a \"delayMs\"");
  expectError("eo5", knight(""), one("\"atRange\":[{\"from\":2,\"endOffset\":5}]"), "atRange 1", "\"endOffset\" is only for a \"travel\" effect");
  expectError("eo6", knight(""), travel("\"atRange\":[{\"from\":2,\"delayMs\":5,\"x\":1}]"), "unknown key \"x\"", "\"endOffset\"");
}

// ---- atRangeFixed ----

TEST(scene5_at_range_fixed_load) {
  Database db;
  if (!load("fixed", knight(""),
            "[ {\"weapon\":\"sword\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"atRange\":[{\"from\":2,\"delayMs\":300}],"
            "\"fade\":[{\"atMs\":0,\"ms\":200,\"atRangeFixed\":true},{\"atMs\":0,\"ms\":200}],"
            "\"hide\":[{\"atMs\":0,\"ms\":100,\"what\":[\"backdrop\"],\"atRangeFixed\":true}],"
            "\"flash\":[{\"atMs\":0,\"ms\":100,\"atRangeFixed\":true}],\"shake\":[{\"atMs\":0,\"ms\":100,\"strength\":2,\"atRangeFixed\":true}]},"
            "  {\"id\":\"dim\",\"sheet\":\"fx\",\"frames\":[[1,0]],\"layer\":\"screen\",\"atRangeFixed\":true} ]",
            db))
    return;
  const BattleEffect& e = db.graphics.battle.effects[0];
  CHECK(e.fade.size() == 2 && e.fade[0].fixed && !e.fade[1].fixed);
  CHECK(e.hide[0].fixed && e.flash[0].fixed && e.shake[0].fixed && !e.rangeFixed);
  CHECK(db.graphics.battle.effects[1].rangeFixed && db.graphics.battle.effects[1].layer == FxLayer::Screen && db.graphics.battle.effects[1].extended);
  expectError("f1", knight(""), one("\"atRangeFixed\":1"), "\"atRangeFixed\"", "true or false");
  expectError("f2", knight(""), one("\"atRangeFixed\":true,\"atRange\":[{\"from\":2,\"delayMs\":5}]"), "\"atRangeFixed\" is for an effect that is not delayed by distance");
  expectError("f3", knight(""), one("\"fade\":[{\"atMs\":0,\"ms\":100,\"atRangeFixed\":\"yes\"}]"), "fade 1", "\"atRangeFixed\"");
  expectError("f4", knight(""), one("\"sounds\":{\"0\":\"zap\"},\"hide\":[{\"atMs\":0,\"ms\":100,\"what\":[\"backdrop\"],\"fixed\":true}]"), "hide 1", "unknown key \"fixed\"");
}

TEST(scene5_timeline_at_range_fixed) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 3)}, info(5));
  const int a = s.strikes[0].actor;
  FighterTimes times[2];
  times[a] = plain();
  times[a].has[BaAttack] = true;  // 430 ms with the blow at 200 ms: the hit moment is 0.4 + 0.2 = 0.6 without any delay
  times[a].total[BaAttack] = 430;
  times[a].hitAt[BaAttack] = 200;
  times[1 - a] = plain();
  EffectTimes main;  // delayed 400 ms by distance; its fade rows: one fixed, one not
  main.has = true;
  main.total = 300;
  main.delayMs = 400;
  main.extended = true;
  FxFade f1, f2;
  f1.ms = f2.ms = 100;
  f1.fixed = true;
  main.fade = {f1, f2};
  FxFlash fl;
  fl.ms = 50;
  fl.fixed = true;
  main.flash.push_back(fl);
  FxHide hd;
  hd.ms = 50;
  hd.what = kHideBackdrop;
  hd.fixed = true;
  main.hide.push_back(hd);
  EffectTimes dim;  // a second effect with a lead-in, not delayed itself: normally the hit's delay would carry it along
  dim.has = true;
  dim.total = 200;
  dim.hitAtMs = 100;
  EffectTimes dimFixed = dim;
  dimFixed.rangeFixed = true;
  const FightTimeline moved = buildTimelineList(s, times, {{main, dim}}, -1), fixed = buildTimelineList(s, times, {{main, dimFixed}}, -1);
  CHECK(moved.fx.size() == 2 && fixed.fx.size() == 2);
  if (moved.fx.size() != 2 || fixed.fx.size() != 2) return;
  CHECK(near(beat(fixed, BeatKind::Hit)->t, 1.0) && near(beat(moved, BeatKind::Hit)->t, 1.0));  // the delay still moves the hit
  CHECK(near(fixed.fx[0].t0, 1.0));                                                             // the delayed effect starts with the hit
  CHECK(near(moved.fx[1].t0, 0.9));                                                             // the unmarked one follows the hit (lead-in 100 ms)
  CHECK(near(fixed.fx[1].t0, 0.5));                                                             // the marked one starts as if no effect were delayed: 0.6 - 0.1
  // The marked fade row is timed from the effect's start before its delay (1.0 - 0.4); the other from its start.
  CHECK(fixed.fades.size() == 2 && near(fixed.fades[0].t0, 0.6) && near(fixed.fades[1].t0, 1.0));
  CHECK(near(fixed.flashes[0].t0, 0.6) && near(fixed.hides[0].t0, 0.6));
  // Without any mark everything is as before.
  EffectTimes plainMain = main;
  plainMain.fade[0].fixed = false;
  plainMain.flash[0].fixed = false;
  plainMain.hide[0].fixed = false;
  const FightTimeline p = buildTimelineList(s, times, {{plainMain}}, -1);
  CHECK(near(p.fades[0].t0, 1.0) && near(p.flashes[0].t0, 1.0) && near(p.hides[0].t0, 1.0));
}

// ---- panAtMs ----

TEST(scene5_pan_at_ms_load) {
  Database db;
  if (!load("panat", knight(""), one("\"panAtMs\":250"), db)) return;
  CHECK(db.graphics.battle.effects[0].panAtMs == 250 && db.graphics.battle.effects[0].extended);
  Database none;
  if (!load("panat_none", knight(""), one("\"hitAtMs\":0"), none)) return;
  CHECK_EQ(none.graphics.battle.effects[0].panAtMs, -1);
  Database zero;
  if (!load("panat0", knight(""), one("\"panAtMs\":15000"), zero)) return;
  CHECK_EQ(zero.graphics.battle.effects[0].panAtMs, 15000);
  expectError("pa1", knight(""), one("\"panAtMs\":-1"), "\"panAtMs\" must be a whole number from 0 to 15000");
  expectError("pa2", knight(""), one("\"panAtMs\":15001"), "\"panAtMs\" must be a whole number from 0 to 15000");
  expectError("pa3", knight(""), one("\"panAtMs\":2.5"), "\"panAtMs\" must be a whole number from 0 to 15000");
}

TEST(scene5_timeline_pan_at_ms) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 3)}, info(5));
  const int a = s.strikes[0].actor, d = 1 - a;
  FighterTimes times[2];
  times[a] = plain();
  times[a].has[BaAttack] = true;
  times[a].total[BaAttack] = 430;
  times[a].hitAt[BaAttack] = 200;
  times[d] = plain();
  EffectTimes e;
  e.has = true;
  e.total = 300;
  e.delayMs = 400;  // the effect (and the hit) start 0.4 s late: effect start 1.0
  const FightTimeline band = buildTimelineList(s, times, {{e}}, 250, 900);  // the band's own startMs: 900 after the strike starts (0.4)
  CHECK(band.cam.size() == 1 && near(band.cam[0].t, 1.3));
  EffectTimes own = e;
  own.panAtMs = 100;  // 100 ms after the effect's own start (after its delay)
  const FightTimeline tl = buildTimelineList(s, times, {{own}}, 250, 900);
  CHECK(tl.cam.size() == 1 && near(tl.cam[0].t, 1.1) && near(tl.cam[0].dur, 0.25) && tl.cam[0].side == d);
  CHECK(near(beat(tl, BeatKind::Hit)->t, 1.0));  // the blow does not wait for the camera
  // Without startMs on the band the effect's panAtMs still decides (and the blow no longer waits for the pan to arrive).
  EffectTimes early = e;
  early.delayMs = 0;
  early.panAtMs = 0;
  const FightTimeline t2 = buildTimelineList(s, times, {{early}}, 250, -1), t3 = buildTimelineList(s, times, {{e}}, 250, -1);
  CHECK(near(t2.cam[0].t, 0.6));                       // the effect starts at the hit, 0.6
  CHECK(near(beat(t2, BeatKind::Hit)->t, 0.6));        // "arrive at the hit" would have made the hit wait for the camera (0.4 + 0.25 = 0.65)
  CHECK(near(beat(t3, BeatKind::Hit)->t, 1.05) && near(t3.cam[0].t, 0.4));  // the band's own rule when no effect gives one: it waits for the pan (0.4 + 0.25), the delay then moves the hit
  // The next strike waits for the camera to arrive, as with startMs.
  CHECK(beat(tl, BeatKind::End)->t >= 1.1 + 0.25);
  // No camera (a band that is not "each"): panAtMs does nothing.
  const FightTimeline nocam = buildTimelineList(s, times, {{own}}, -1, -1);
  CHECK(nocam.cam.empty());
}

// "hitHold" (docs/BATTLE_SCENE.md "Holding a pose"): a hit waits by the hit points it changed; a miss keeps resultHoldMs.
TEST(scene5_hit_hold_load_and_refusals) {
  Database db;
  if (!load("hithold", knight(""), "[]", db,
            "\"scene\": { \"resultHoldMs\": 550, \"hitHold\": { \"minMs\": 566.667, \"baseMs\": 50, \"perHpMs\": 33.333, \"killMs\": 816.667, \"noChangeMs\": 183.333 } }"))
    return;
  const HitHold& h = db.graphics.battle.timing.hitHold;
  CHECK(h.on && std::fabs(h.minMs - 566.667) < 1e-9 && h.baseMs == 50 && std::fabs(h.perHpMs - 33.333) < 1e-9 && std::fabs(h.killMs - 816.667) < 1e-9 &&
        std::fabs(h.noChangeMs - 183.333) < 1e-9);
  CHECK(std::fabs(h.ms(1, false) - 566.667) < 1e-9);            // the minimum
  CHECK(std::fabs(h.ms(20, false) - (50 + 20 * 33.333)) < 1e-9);  // grows with the points
  CHECK(std::fabs(h.ms(3, true) - (566.667 + 816.667)) < 1e-9);   // a defeat adds killMs
  CHECK(std::fabs(h.ms(0, false) - 183.333) < 1e-9);             // nothing changed
  // Left-out keys: the minimum is resultHoldMs, nothing grows, no change waits the minimum.
  Database d2;
  if (!load("hithold2", knight(""), "[]", d2, "\"scene\": { \"resultHoldMs\": 600, \"hitHold\": { \"perHpMs\": 10 } }")) return;
  const HitHold& h2 = d2.graphics.battle.timing.hitHold;
  CHECK(h2.on && h2.minMs == 600 && h2.baseMs == 0 && h2.killMs == 0 && h2.noChangeMs == 600 && h2.ms(80, false) == 800);
  Database d3;
  if (!load("hithold3", knight(""), "[]", d3, "\"scene\": { \"resultHoldMs\": 600 }")) return;
  CHECK(!d3.graphics.battle.timing.hitHold.on);
  expectError("hh1", knight(""), "[]", "battle scene hitHold", "must be an object", "\"scene\": { \"hitHold\": 5 }");
  expectError("hh2", knight(""), "[]", "unknown key \"perHp\"", "\"perHpMs\"", "\"scene\": { \"hitHold\": { \"perHp\": 5 } }");
  expectError("hh3", knight(""), "[]", "\"perHpMs\" must be from 0 to 1000", "", "\"scene\": { \"hitHold\": { \"perHpMs\": 1001 } }");
  expectError("hh4", knight(""), "[]", "\"killMs\" must be from 0 to 10000", "", "\"scene\": { \"hitHold\": { \"killMs\": -1 } }");
  expectError("hh5", knight(""), "[]", "minMs", "number", "\"scene\": { \"hitHold\": { \"minMs\": \"long\" } }");
}

TEST(scene5_hit_hold_timeline) {
  HitHold hh;
  hh.on = true;
  hh.minMs = 566.667;
  hh.baseMs = 50;
  hh.perHpMs = 33.333;
  hh.killMs = 816.667;
  hh.noChangeMs = 183.333;
  auto endOfStrike = [&](int flags, int damage) {
    FightScript s = buildFightScript({attack(1, 2, flags, damage)}, info(1));
    const int a = s.strikes[0].actor;
    FighterTimes times[2] = {plain(), plain()};
    times[a] = holdingStriker();
    const FightTimeline tl = buildTimelineList(s, times, {}, -1, -1, 550, 0, hh);
    const FightPose& sp = tl.poses[a][0];
    return sp.start + sp.dur;  // the held frame is the last: the pose ends when the result is over
  };
  // The blow is at 0.800 (0.4 intro + the hit frame at 400 ms). Gorm has 10 hit points.
  CHECK(near(endOfStrike(kHit, 3), 0.8 + 0.566667));                 // 3 points: the minimum
  CHECK(near(endOfStrike(kHit, 9), 0.8 + 0.566667));                 // 9 points: 50 + 300 is still under it
  CHECK(near(endOfStrike(kHit | kKill, 10), 0.8 + 0.566667 + 0.816667));  // a defeat
  CHECK(near(endOfStrike(0, 0), 0.8 + 0.55));                        // a miss keeps resultHoldMs
  // Without hitHold a hit waits resultHoldMs as before.
  FightScript s = buildFightScript({attack(1, 2, kHit, 3)}, info(1));
  const int a = s.strikes[0].actor;
  FighterTimes times[2] = {plain(), plain()};
  times[a] = holdingStriker();
  const FightTimeline tl = buildTimelineList(s, times, {}, -1, -1, 550, 0);
  CHECK(near(tl.poses[a][0].start + tl.poses[a][0].dur, 0.8 + 0.55));
}
