#include "battle/fight_timeline.h"

#include <algorithm>
#include <cmath>

namespace ote {

namespace {
// An animation's own sounds (docs/BATTLE_SCENE.md "Sounds"): one beat per sound at the start of its frame, for a pose that plays the
// set's animation (a fallback motion has none).
void addSounds(FightTimeline& tl, const FightPose& pose, int side, const FighterTimes& t, int strike) {
  if (pose.fallback) return;
  for (const auto& s : t.soundAt[pose.anim]) tl.beats.push_back({pose.start + s.first / 1000.0, BeatKind::Sound, strike, side, 0, false, s.second});
}
}  // namespace

BattleAnimId attackAnimFor(const FighterTimes& t, bool crit, bool ranged, bool hasEffect, bool miss) {
  if (miss && t.has[BaMiss]) return BaMiss;
  if (crit && ranged && t.has[BaCritRanged]) return BaCritRanged;
  if (crit && t.has[BaCrit]) return BaCrit;
  if (ranged && t.has[BaRanged]) return BaRanged;
  if (hasEffect && t.has[BaCast]) return BaCast;
  return crit && !t.has[BaAttack] ? BaCrit : BaAttack;  // (with no attack animation either, the fallback crit shows its flash)
}

const FightPose* FightTimeline::poseAt(int side, double t) const {
  const FightPose* cur = nullptr;
  for (const FightPose& p : poses[side & 1])
    if (p.start <= t) cur = &p;
  if (!cur) return nullptr;
  if (t - cur->start >= cur->dur && cur->anim != BaDefeat) return nullptr;
  return cur;
}

int FightTimeline::hpAt(int side, double t, int startHp) const {
  int hpNow = startHp;
  for (const HpSegment& s : hp) {
    if (s.side != side) continue;
    if (t >= s.t1) hpNow = s.to;
    else if (t > s.t0) {
      const double k = (t - s.t0) / (s.t1 - s.t0);
      hpNow = s.from + (int)std::lround((s.to - s.from) * k);
      break;
    } else break;
  }
  return hpNow;
}

namespace {

double sec(int ms) { return ms / 1000.0; }

}  // namespace

FightTimeline buildTimeline(const FightScript& s, const FighterTimes times[2], const std::vector<EffectTimes>& effectByStrike) {
  std::vector<std::vector<EffectTimes>> lists;
  for (const EffectTimes& e : effectByStrike) lists.push_back(e.has ? std::vector<EffectTimes>(1, e) : std::vector<EffectTimes>());
  return buildTimelineList(s, times, lists);
}

FightTimeline buildTimelineList(const FightScript& s, const FighterTimes times[2], const std::vector<std::vector<EffectTimes>>& effectsByStrike) {
  FightTimeline tl;
  if (!s.valid) return tl;
  double t = kIntro;
  tl.beats.push_back({0, BeatKind::Begin, -1, s.initiator, 0, false, ""});
  for (size_t i = 0; i < s.strikes.size(); ++i) {
    const FightStrike& st = s.strikes[i];
    const int a = st.actor, d = 1 - a;
    const std::vector<EffectTimes> fxs = i < effectsByStrike.size() ? effectsByStrike[i] : std::vector<EffectTimes>();
    double t0 = t;
    // Skills that fired and conditions put on show first, over their owner, while the fighters stand.
    if (!st.tags.empty()) {
      for (const FightTag& tag : st.tags) tl.beats.push_back({t0, BeatKind::Tag, (int)i, tag.side, 0, false, tag.text});
      t0 += kTagTime;
    }
    tl.beats.push_back({t0, BeatKind::StrikeStart, (int)i, a, 0, false, ""});
    // The striker.
    const FighterTimes& ft = times[a];
    const BattleAnimId kind = attackAnimFor(ft, st.crit, s.ranged(), !fxs.empty(), !st.hit);
    FightPose ap;
    ap.start = t0;
    ap.anim = kind;
    ap.strike = (int)i;
    double hitOffset;
    if (ft.has[kind]) {
      ap.dur = sec(ft.total[kind]);
      hitOffset = sec(ft.hitAt[kind]);
    } else {
      ap.fallback = true;
      ap.dur = sec(kind == BaCrit ? kFallbackCritMs : kFallbackAttackMs);
      hitOffset = sec(kind == BaCrit ? kFallbackCritHitMs : kFallbackAttackHitMs);
    }
    tl.poses[a].push_back(ap);
    addSounds(tl, ap, a, ft, (int)i);
    double hitT = t0 + hitOffset;
    // The strike's effects (docs/BATTLE_SCENE.md "Timing of effects"). The hit lands at the strike animation's hit frame, but not before a
    // travelling effect has arrived, and not before an effect that starts `hitAtMs` ahead of it has had that long to run since the strike began.
    double fxEnd = 0;  // the end of an effect that flies back or lingers (the strike lasts until then)
    for (const EffectTimes& e : fxs) {
      if (e.travel) {
        const double spawn = t0 + (ft.has[kind] && ft.spawnAt[kind] >= 0 ? sec(ft.spawnAt[kind]) : hitOffset);
        hitT = std::max(hitT, spawn + sec(e.travelMs));  // the hit lands when the shot arrives
      } else if (e.hitAtMs > 0) {
        hitT = std::max(hitT, t0 + sec(e.hitAtMs));
      }
    }
    for (size_t k = 0; k < fxs.size(); ++k) {
      const EffectTimes& e = fxs[k];
      FightFx f;
      f.strike = (int)i;
      f.slot = (int)k;
      f.travel = e.travel;
      f.fromSide = a;
      f.toSide = e.striker ? a : d;
      if (e.travel) {
        f.t0 = t0 + (ft.has[kind] && ft.spawnAt[kind] >= 0 ? sec(ft.spawnAt[kind]) : hitOffset);
        f.travelMs = e.travelMs;
        f.returns = e.returns;
        f.lingerMs = e.lingerMs;
        f.dur = sec(e.travelMs + e.lingerMs + (e.returns ? e.travelMs : 0));
        if (e.returns || e.lingerMs > 0) fxEnd = std::max(fxEnd, f.t0 + f.dur);
      } else {
        f.t0 = hitT - sec(e.hitAtMs);
        f.dur = sec(std::max(e.total, 60));
      }
      tl.fx.push_back(f);
      if (e.extended) tl.beats.push_back({f.t0, BeatKind::FxStart, (int)i, a, 0, false, "", e.name});
      for (const auto& snd : e.sounds) tl.beats.push_back({f.t0 + sec(snd.first), BeatKind::FxSound, (int)i, a, 0, false, snd.second, e.name});
      for (const FxFlash& fl : e.flash) tl.flashes.push_back({f.t0 + sec(fl.atMs), sec(fl.ms), fl.rgb, 0, (int)i});
      for (const FxShake& sh : e.shake) tl.shakes.push_back({f.t0 + sec(sh.atMs), sec(sh.ms), 0, sh.strength, (int)i});
    }
    // The struck fighter.
    double end = std::max(ap.start + ap.dur, fxEnd);
    const FighterTimes& dt = times[d];
    if (st.hit) {
      tl.beats.push_back({hitT, BeatKind::Hit, (int)i, d, st.damage, st.crit, ""});
      FightPose dp;
      dp.start = hitT;
      dp.strike = (int)i;
      if (st.dies[d]) {
        dp.anim = BaDefeat;
        dp.dur = dt.has[BaDefeat] ? sec(dt.total[BaDefeat]) : sec(kFallbackDefeatMs);
        dp.fallback = !dt.has[BaDefeat];
        tl.beats.push_back({hitT + dp.dur, BeatKind::Defeated, (int)i, d, 0, false, ""});
      } else {
        dp.anim = BaHurt;
        dp.dur = dt.has[BaHurt] ? sec(dt.total[BaHurt]) : sec(kFallbackHurtMs);
        dp.fallback = !dt.has[BaHurt];
      }
      tl.poses[d].push_back(dp);
      addSounds(tl, dp, d, dt, (int)i);
      end = std::max(end, hitT + dp.dur + (st.dies[d] ? 0.3 : 0.0));
      if (st.damage > 0 || st.hpBefore[d] != st.hpAfterHit[d]) {
        const double drain = std::min(0.7, std::max(0.3, kDrainTime * st.damage / std::max(1, st.hpBefore[d] > 0 ? st.hpBefore[d] : 10) + 0.25));
        tl.hp.push_back({hitT, hitT + drain, d, st.hpBefore[d], st.hpAfterHit[d]});
        end = std::max(end, hitT + drain);
      }
    } else {
      tl.beats.push_back({hitT, BeatKind::Miss, (int)i, d, 0, false, ""});
      FightPose dp;
      dp.start = std::max(t0, hitT - 0.12);
      dp.anim = s.ranged() && dt.has[BaDodgeRanged] ? BaDodgeRanged : BaDodge;  // (dodgeRanged falls back to dodge)
      dp.dur = dt.has[dp.anim] ? sec(dt.total[dp.anim]) : sec(kFallbackDodgeMs);
      dp.fallback = !dt.has[dp.anim];
      dp.strike = (int)i;
      tl.poses[d].push_back(dp);
      addSounds(tl, dp, d, dt, (int)i);
      end = std::max(end, dp.start + dp.dur);
    }
    if (st.weaponBroke) {
      tl.beats.push_back({hitT + 0.1, BeatKind::Broke, (int)i, a, 0, false, st.brokeText});
      end = std::max(end, hitT + 0.6);
    }
    // Drain and recoil, one after the other once the hit has settled.
    int hpNow[2] = {st.hpAfterHit[0], st.hpAfterHit[1]};
    for (const FightHp& h : st.after) {
      tl.beats.push_back({end, h.delta >= 0 ? BeatKind::Heal : BeatKind::Hurt, (int)i, h.side, h.delta >= 0 ? h.delta : -h.delta, false, ""});
      tl.hp.push_back({end, end + sec(350), h.side, hpNow[h.side], hpNow[h.side] + h.delta});
      hpNow[h.side] += h.delta;
      end += kExtraTime;
    }
    t = end + kStrikeGap;
    if (st.dies[0] || st.dies[1]) break;  // nothing follows a fall
  }
  tl.beats.push_back({t + kOutro, BeatKind::End, -1, 0, 0, false, ""});
  tl.total = t + kOutro + kFade;
  std::stable_sort(tl.beats.begin(), tl.beats.end(), [](const FightBeat& x, const FightBeat& y) { return x.t < y.t; });
  return tl;
}

}  // namespace ote
