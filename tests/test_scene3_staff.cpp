// Staves in the battle scene (docs/BATTLE_SCENE.md "Staves in the scene"): which uses play as a scene, the script of a use (who stands where,
// the result on the target) and its timeline (cast, lead-in, result).
#include "battle/fight_timeline.h"
#include "test.h"

using namespace ote;

namespace {

Event ev(EventType t, int a, int b = -1, int c = -1, int d = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  return e;
}
Event used(int user, int target, int affected = 1) { return ev(EventType::StaffUsed, user, 0, target, affected); }

// Unit 1 "Eirik" (hp 20 of 24, a person's) at x 2, unit 2 "Gorm" (hp 10 of 30, a person's too) at x 3, unit 3 "Raider" (the computer's) at x 1.
FightInfo info(int sides = 0) {
  FightInfo f;
  f.units[1] = {1, "Eirik", 3, 20, 24, 2, true};
  f.units[2] = {2, "Gorm", 5, 10, 30, 3, true};
  f.units[3] = {3, "Raider", 2, 8, 8, 1, false};
  f.conditionNames = {"Poisoned", "Slow"};
  f.itemNames = {"Heal Staff", "Iron Sword"};
  f.sidesMode = sides;
  return f;
}

FighterTimes caster(bool cast = true) {
  FighterTimes t;
  t.has[cast ? BaCast : BaAttack] = true;
  t.total[cast ? BaCast : BaAttack] = 500;
  t.hitAt[cast ? BaCast : BaAttack] = 300;
  for (int& v : t.spawnAt) v = -1;
  return t;
}

const FightBeat* first(const FightTimeline& tl, BeatKind k) {
  for (const FightBeat& b : tl.beats)
    if (b.kind == k) return &b;
  return nullptr;
}

}  // namespace

TEST(scene_staff_which_uses_play_in_the_scene) {
  // A single other unit, nothing moved: a scene.
  CHECK(staffUseInScene(used(1, 2), {ev(EventType::UnitHealed, 2, 5)}));
  CHECK(staffUseInScene(used(1, 2), {ev(EventType::StaffMissed, 2, 1, 0)}));
  CHECK(staffUseInScene(used(1, 2), {}));
  // The user itself, nobody picked (a map-wide staff), more than one unit: on the map.
  CHECK(!staffUseInScene(used(1, 1), {}));
  CHECK(!staffUseInScene(used(1, -1), {}));
  CHECK(!staffUseInScene(used(1, 2, 3), {}));
  CHECK(!staffUseInScene(used(1, 2, 0), {}));
  // A warp, a pull or a swap moves a unit (UnitRelocated follows the heal, if any): on the map.
  CHECK(!staffUseInScene(used(1, 2), {ev(EventType::UnitHealed, 2, 5), ev(EventType::UnitRelocated, 2, 4, 9)}));
  // Entries after the use's own (the user's experience) are not looked at.
  CHECK(staffUseInScene(used(1, 2), {ev(EventType::UnitHealed, 2, 5), ev(EventType::XpGained, 1, 10), ev(EventType::UnitRelocated, 7, 1, 2)}));
  // Not a staff entry at all.
  CHECK(!staffUseInScene(ev(EventType::ItemUsed, 1, 0, 2), {}));
  // Which entries belong to the use.
  CHECK(belongsToStaffUse(ev(EventType::UnitHealed, 2, 5), 1, 2));
  CHECK(!belongsToStaffUse(ev(EventType::UnitHealed, 1, 5), 1, 2));
  CHECK(belongsToStaffUse(ev(EventType::StaffMissed, 2, 1, 0), 1, 2));
  CHECK(!belongsToStaffUse(ev(EventType::StaffMissed, 2, 3, 0), 1, 2));
  CHECK(!belongsToStaffUse(ev(EventType::XpGained, 1, 5), 1, 2));
  CHECK(!belongsToStaffUse(ev(EventType::ItemBroke, 1, 0), 1, 2));
}

TEST(scene_staff_script_heal_miss_and_conditions) {
  FightScript s = buildFightScript({used(1, 2), ev(EventType::UnitHealed, 2, 8)}, info());
  CHECK(s.valid && s.staff());
  CHECK_EQ((int)s.strikes.size(), 1);
  const FightStrike& st = s.strikes[0];
  CHECK(st.staff && st.hit && !st.missed);
  CHECK_EQ(st.item, 0);
  CHECK_EQ(st.healed, 8);
  CHECK_EQ(st.hpBefore[1 - st.actor], 10);
  CHECK_EQ(st.hpAfterHit[1 - st.actor], 18);
  CHECK_EQ(st.hpAfterHit[st.actor], 20);  // the user's own bar does not move
  CHECK_EQ(s.side[st.actor].unit, 1);
  // A heal is cut at the target's maximum.
  FightScript capped = buildFightScript({used(1, 2), ev(EventType::UnitHealed, 2, 99)}, info());
  CHECK_EQ(capped.strikes[0].healed, 20);
  // A miss.
  FightScript miss = buildFightScript({used(1, 2), ev(EventType::StaffMissed, 2, 1, 0)}, info());
  CHECK(miss.strikes[0].missed && !miss.strikes[0].hit && miss.strikes[0].healed == 0);
  // Conditions, resisted, cured, a mended item: labels on the target in order.
  FightScript c = buildFightScript({used(1, 2), ev(EventType::ConditionApplied, 2, 0, 3), ev(EventType::ConditionResisted, 2, 1, 1),
                                    ev(EventType::ConditionEnded, 2, 0, kConditionCured), ev(EventType::ConditionEnded, 2, 1, kConditionRanOut),
                                    ev(EventType::ItemRepaired, 2, 1, 5, 20)}, info());
  CHECK_EQ((int)c.strikes[0].notes.size(), 4);
  CHECK(c.strikes[0].notes[0].text == "Poisoned" && c.strikes[0].notes[0].kind == 0);
  CHECK(c.strikes[0].notes[1].text == "Slow resisted");
  CHECK(c.strikes[0].notes[2].text == "Poisoned cured");  // (a condition that merely ran out is not part of the use)
  CHECK(c.strikes[0].notes[3].text == "Iron Sword +5" && c.strikes[0].notes[3].kind == 1);
}

TEST(scene_staff_sides) {
  // "map": the unit nearer the left edge of the map (Eirik, x 2, user; Gorm x 3) stands on the left.
  FightScript m = buildFightScript({used(1, 2), ev(EventType::UnitHealed, 2, 1)}, info(0));
  CHECK(m.side[0].unit == 1 && m.side[1].unit == 2 && m.initiator == 0);
  FightScript m2 = buildFightScript({used(2, 1), ev(EventType::UnitHealed, 1, 1)}, info(0));  // Gorm uses it on Eirik: Eirik is nearer the left
  CHECK(m2.side[0].unit == 1 && m2.initiator == 1);
  // "playerRight": a person's user on the right, its ally target opposite (on the left) whatever the map says.
  FightScript r = buildFightScript({used(1, 2), ev(EventType::UnitHealed, 2, 1)}, info(1));
  CHECK(r.side[1].unit == 1 && r.side[0].unit == 2 && r.initiator == 1);
  FightScript r2 = buildFightScript({used(2, 1), ev(EventType::UnitHealed, 1, 1)}, info(1));
  CHECK(r2.side[1].unit == 2 && r2.side[0].unit == 1);
  // "playerLeft": the opposite.
  FightScript l = buildFightScript({used(1, 2), ev(EventType::UnitHealed, 2, 1)}, info(2));
  CHECK(l.side[0].unit == 1 && l.side[1].unit == 2 && l.initiator == 0);
  // The computer's own staff user (Raider heals Eirik): the computer on the left with "playerRight", on the right with "playerLeft".
  FightScript c = buildFightScript({used(3, 1), ev(EventType::UnitHealed, 1, 1)}, info(1));
  CHECK(c.side[0].unit == 3 && c.side[1].unit == 1);
  FightScript c2 = buildFightScript({used(3, 1), ev(EventType::UnitHealed, 1, 1)}, info(2));
  CHECK(c2.side[1].unit == 3 && c2.side[0].unit == 1);
}

TEST(scene_staff_timeline_cast_lead_in_and_result) {
  FightScript s = buildFightScript({used(1, 2), ev(EventType::UnitHealed, 2, 8)}, info());
  const int u = s.strikes[0].actor, t = 1 - u;
  FighterTimes times[2];
  times[u] = caster(true);
  FightTimeline tl = buildTimeline(s, times, {});
  // The user casts; the target plays nothing; the heal lands at the cast's hit frame (300 ms into the cast).
  CHECK(tl.poses[u].size() == 1 && tl.poses[u][0].anim == BaCast && !tl.poses[u][0].fallback);
  CHECK(tl.poses[t].empty());
  const FightBeat* start = first(tl, BeatKind::StrikeStart);
  const FightBeat* heal = first(tl, BeatKind::StaffHeal);
  CHECK(start && heal);
  if (!start || !heal) return;
  CHECK(heal->t - start->t > 0.2999 && heal->t - start->t < 0.3001);
  CHECK(heal->side == t && heal->value == 8);
  CHECK(first(tl, BeatKind::Hit) == nullptr && first(tl, BeatKind::Miss) == nullptr);
  // The bar rises from 10 to 18 starting on the result.
  CHECK(tl.hp.size() == 1 && tl.hp[0].side == t && tl.hp[0].from == 10 && tl.hp[0].to == 18 && tl.hp[0].t0 == heal->t);
  CHECK_EQ(tl.hpAt(t, tl.total, 10), 18);
  CHECK_EQ(tl.hpAt(t, heal->t - 0.01, 10), 10);
  CHECK(tl.total > heal->t + 0.5);
  // No cast animation: attack plays. Neither: the fallback motion of a cast.
  FighterTimes atk[2];
  atk[u] = caster(false);
  FightTimeline tl2 = buildTimeline(s, atk, {});
  CHECK(tl2.poses[u][0].anim == BaAttack && !tl2.poses[u][0].fallback);
  FighterTimes none[2];
  FightTimeline tl3 = buildTimeline(s, none, {});
  CHECK(tl3.poses[u][0].anim == BaCast && tl3.poses[u][0].fallback);
  // The staff's effect with a lead-in longer than the cast's hit frame makes the result wait (as for a weapon).
  EffectTimes fx;
  fx.has = true;
  fx.total = 600;
  fx.hitAtMs = 450;
  FightTimeline tl4 = buildTimeline(s, times, {fx});
  const FightBeat* start4 = first(tl4, BeatKind::StrikeStart);
  const FightBeat* heal4 = first(tl4, BeatKind::StaffHeal);
  CHECK(start4 && heal4 && heal4->t - start4->t > 0.4499 && heal4->t - start4->t < 0.4501);
  CHECK(tl4.fx.size() == 1 && tl4.fx[0].toSide == t && tl4.fx[0].fromSide == u);  // the effect lands on the target
  CHECK(tl4.fx[0].t0 >= start4->t - 0.0001 && tl4.fx[0].t0 <= start4->t + 0.0001);
}

TEST(scene_staff_timeline_miss_and_condition) {
  FightScript m = buildFightScript({used(1, 2), ev(EventType::StaffMissed, 2, 1, 0)}, info());
  const int u = m.strikes[0].actor, t = 1 - u;
  FighterTimes times[2];
  times[u] = caster();
  FightTimeline tl = buildTimeline(m, times, {});
  const FightBeat* miss = first(tl, BeatKind::StaffMiss);
  CHECK(miss && miss->side == t);
  CHECK(first(tl, BeatKind::StaffHeal) == nullptr && tl.hp.empty() && tl.poses[t].empty());
  FightScript c = buildFightScript({used(1, 2), ev(EventType::UnitHealed, 2, 3), ev(EventType::ConditionApplied, 2, 0, 3)}, info());
  FightTimeline tl2 = buildTimeline(c, times, {});
  const FightBeat* heal = first(tl2, BeatKind::StaffHeal);
  const FightBeat* note = first(tl2, BeatKind::StaffNote);
  CHECK(heal && note && note->text == "Poisoned" && note->t > heal->t);  // the label follows the number
  // Beats stay in time order, Begin first and End last.
  for (size_t i = 1; i < tl2.beats.size(); ++i) CHECK(tl2.beats[i - 1].t <= tl2.beats[i].t);
  CHECK(tl2.beats.front().kind == BeatKind::Begin && tl2.beats.back().kind == BeatKind::End);
}
