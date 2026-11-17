// The battle scene's clock (engine/player/battle/fight_timeline.*): beats in order, hit frames, fallbacks.
#include "battle/fight_timeline.h"
#include "test.h"

using namespace ote;

namespace {

Event strike(int a, int b, int flags, int damage) {
  Event e;
  e.type = EventType::Attack;
  e.a = a;
  e.b = b;
  e.d = damage;
  e.flags = flags;
  return e;
}
FightInfo info(int dist = 1) {
  FightInfo f;
  f.units[1] = {1, "Eirik", 3, 20, 24, 2};
  f.units[2] = {2, "Gorm", 5, 10, 30, 3};
  f.distance = dist;
  return f;
}
// A fighter with an attack animation of 4 frames (hit on frame 2), hurt and defeat.
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
int count(const FightTimeline& tl, BeatKind k) {
  int n = 0;
  for (const FightBeat& b : tl.beats) n += b.kind == k;
  return n;
}
const FightBeat* first(const FightTimeline& tl, BeatKind k, int strike = -1) {
  for (const FightBeat& b : tl.beats)
    if (b.kind == k && (strike < 0 || b.strike == strike)) return &b;
  return nullptr;
}

}  // namespace

TEST(fight_timeline_hit_lands_on_the_marked_frame) {
  FightScript s = buildFightScript({strike(1, 2, kHit, 4)}, info());
  FighterTimes times[2] = {animated(), animated()};
  FightTimeline tl = buildTimeline(s, times, {});
  const FightBeat* hit = first(tl, BeatKind::Hit);
  const FightBeat* start = first(tl, BeatKind::StrikeStart);
  CHECK(hit && start);
  if (!hit || !start) return;
  CHECK(hit->t - start->t > 0.1999 && hit->t - start->t < 0.2001);  // the attack's hit frame starts 200 ms in
  CHECK_EQ(hit->value, 4);
  CHECK_EQ(hit->side, 1);
  // The struck fighter's hurt pose starts on the beat; the bar starts to drain on it.
  CHECK(tl.poses[1].size() == 1 && tl.poses[1][0].anim == BaHurt && tl.poses[1][0].start == hit->t);
  CHECK(tl.hp.size() == 1 && tl.hp[0].t0 == hit->t && tl.hp[0].from == 10 && tl.hp[0].to == 6);
  CHECK_EQ(tl.hpAt(1, hit->t - 0.01, 10), 10);
  CHECK_EQ(tl.hpAt(1, tl.total, 10), 6);
  const int mid = tl.hpAt(1, tl.hp[0].t0 + (tl.hp[0].t1 - tl.hp[0].t0) / 2, 10);
  CHECK(mid <= 10 && mid >= 6);
  CHECK(count(tl, BeatKind::Begin) == 1 && count(tl, BeatKind::End) == 1);
  CHECK(tl.beats.front().kind == BeatKind::Begin && tl.beats.back().kind == BeatKind::End);
}

TEST(fight_timeline_beats_are_in_time_order_and_strikes_follow_each_other) {
  FightScript s = buildFightScript({strike(1, 2, 0, 0), strike(2, 1, kHit | kCounter, 3), strike(1, 2, kHit | kCrit, 5)}, info());
  FighterTimes times[2] = {animated(), FighterTimes()};
  FightTimeline tl = buildTimeline(s, times, {});
  for (size_t i = 1; i < tl.beats.size(); ++i) CHECK(tl.beats[i - 1].t <= tl.beats[i].t);
  CHECK_EQ(count(tl, BeatKind::StrikeStart), 3);
  CHECK(first(tl, BeatKind::Miss, 0) != nullptr);
  CHECK(first(tl, BeatKind::Hit, 1) != nullptr && !first(tl, BeatKind::Hit, 1)->crit);
  CHECK(first(tl, BeatKind::Hit, 2) != nullptr && first(tl, BeatKind::Hit, 2)->crit);
  // Each strike starts after the one before has played out.
  CHECK(first(tl, BeatKind::StrikeStart, 1)->t > first(tl, BeatKind::Miss, 0)->t);
  CHECK(first(tl, BeatKind::StrikeStart, 2)->t > first(tl, BeatKind::Hit, 1)->t);
  // Fighter 1 has no art: its dodge and its counter use the fallback motion; fighter 0 uses its animations.
  CHECK(tl.poses[1][0].anim == BaDodge && tl.poses[1][0].fallback);
  CHECK(tl.poses[1][1].anim == BaAttack && tl.poses[1][1].fallback);
  CHECK(tl.poses[0][0].anim == BaAttack && !tl.poses[0][0].fallback);
  CHECK(tl.poses[0][1].anim == BaHurt && !tl.poses[0][1].fallback);
  // The crit with no crit animation uses the attack animation.
  CHECK(tl.poses[0][2].anim == BaAttack && !tl.poses[0][2].fallback);
}

TEST(fight_timeline_kill_ends_the_scene_with_a_defeat) {
  FightScript s = buildFightScript({strike(1, 2, kHit | kKill, 12)}, info());
  FighterTimes times[2] = {animated(), animated()};
  FightTimeline tl = buildTimeline(s, times, {});
  const FightBeat* hit = first(tl, BeatKind::Hit);
  const FightBeat* dead = first(tl, BeatKind::Defeated);
  CHECK(hit && dead);
  if (!hit || !dead) return;
  CHECK(dead->t > hit->t);
  CHECK_EQ(dead->side, 1);
  CHECK_EQ(tl.poses[1][0].anim, BaDefeat);
  CHECK(tl.poseAt(1, tl.total) != nullptr && tl.poseAt(1, tl.total)->anim == BaDefeat);  // it stays down
  CHECK(tl.poseAt(0, tl.total) == nullptr);                                              // the winner stands
  CHECK_EQ(tl.hpAt(1, tl.total, 10), 0);
  CHECK(tl.beats.back().kind == BeatKind::End && tl.beats.back().t > dead->t);
}

TEST(fight_timeline_ranged_projectile_sets_the_hit_time) {
  FightScript s = buildFightScript({strike(1, 2, kHit, 4)}, info(3));
  FighterTimes times[2] = {animated(), animated()};
  times[0].has[BaRanged] = true;
  times[0].total[BaRanged] = 400;
  times[0].hitAt[BaRanged] = 150;
  times[0].spawnAt[BaRanged] = 100;
  EffectTimes arrow;
  arrow.has = true;
  arrow.travel = true;
  arrow.travelMs = 300;
  arrow.total = 100;
  FightTimeline tl = buildTimeline(s, times, {arrow});
  CHECK_EQ(tl.poses[0][0].anim, BaRanged);  // ranged fight, ranged animation
  CHECK_EQ((int)tl.fx.size(), 1);
  const FightBeat* start = first(tl, BeatKind::StrikeStart);
  CHECK(tl.fx[0].travel && tl.fx[0].t0 - start->t > 0.0999 && tl.fx[0].t0 - start->t < 0.1001);  // spawn frame
  const FightBeat* hit = first(tl, BeatKind::Hit);
  CHECK(hit->t - start->t > 0.3999 && hit->t - start->t < 0.4001);  // 100 spawn + 300 flight, later than the hit frame
  // An effect that appears on the target starts on the hit.
  arrow.travel = false;
  FightTimeline on = buildTimeline(s, times, {arrow});
  CHECK(on.fx[0].t0 == first(on, BeatKind::Hit)->t && !on.fx[0].travel);
}

TEST(fight_timeline_animation_choice) {
  FighterTimes t;
  t.has[BaAttack] = true;
  CHECK_EQ(attackAnimFor(t, false, false, false), BaAttack);
  CHECK_EQ(attackAnimFor(t, true, false, false), BaAttack);   // a crit with no crit animation: the attack
  t.has[BaCrit] = true;
  CHECK_EQ(attackAnimFor(t, true, false, false), BaCrit);
  CHECK_EQ(attackAnimFor(t, false, true, false), BaAttack);   // ranged with no ranged animation
  t.has[BaRanged] = true;
  CHECK_EQ(attackAnimFor(t, false, true, false), BaRanged);
  CHECK_EQ(attackAnimFor(t, true, true, false), BaCrit);      // a crit beats the range
  t.has[BaCast] = true;
  CHECK_EQ(attackAnimFor(t, false, false, true), BaCast);     // a weapon with an effect: cast
  CHECK_EQ(attackAnimFor(t, false, false, false), BaAttack);
  FighterTimes none;
  CHECK_EQ(attackAnimFor(none, true, false, false), BaCrit);  // fallback flash
  CHECK_EQ(attackAnimFor(none, false, false, false), BaAttack);
}

TEST(fight_timeline_tags_drain_and_broken_weapon) {
  Event tag;
  tag.type = EventType::SkillActivated;
  tag.a = 1;
  tag.b = 0;
  Event heal;
  heal.type = EventType::UnitHealed;
  heal.a = 1;
  heal.b = 3;
  Event broke;
  broke.type = EventType::ItemBroke;
  broke.a = 1;
  broke.b = 0;
  FightInfo inf = info();
  inf.skillNames = {"Heavy Blow"};
  inf.itemNames = {"Iron Sword"};
  FightScript s = buildFightScript({tag, strike(1, 2, kHit, 4), heal, broke}, inf);
  FighterTimes times[2];
  FightTimeline tl = buildTimeline(s, times, {});
  const FightBeat* tg = first(tl, BeatKind::Tag);
  CHECK(tg && tg->text == "Heavy Blow" && tg->side == 0);
  CHECK(first(tl, BeatKind::StrikeStart)->t > tg->t);  // the label first, then the swing
  const FightBeat* hl = first(tl, BeatKind::Heal);
  CHECK(hl && hl->value == 3 && hl->side == 0 && hl->t > first(tl, BeatKind::Hit)->t);
  CHECK(first(tl, BeatKind::Broke) && first(tl, BeatKind::Broke)->text == "Iron Sword broke");
  CHECK_EQ((int)tl.hp.size(), 2);  // the foe's bar, then the striker's
  CHECK_EQ(tl.hpAt(0, tl.total, 20), 23);
}

TEST(fight_timeline_invalid_script_gives_an_empty_clock) {
  FightTimeline tl = buildTimeline(FightScript(), nullptr, {});
  CHECK(tl.beats.empty() && tl.total == 0);
}
