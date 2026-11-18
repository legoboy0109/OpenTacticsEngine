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

double FightFadeWin::opacityAt(double t) const {
  const double d = t - t0;
  if (d < 0 || ms <= 0) return 0;
  if (d < ms) return alpha * d / ms;
  if (d < ms + hold) return alpha;
  if (d < 2 * ms + hold) return alpha * (1 - (d - ms - hold) / ms);
  return 0;
}

double FightTimeline::cameraAt(double t) const {
  double pos = camStart;
  for (const FightCamKey& k : cam) {
    if (t <= k.t) break;
    if (k.dur <= 0 || t >= k.t + k.dur) {
      pos = k.side;
      continue;
    }
    const double p = (t - k.t) / k.dur;
    return pos + (k.side - pos) * (p * p * (3 - 2 * p));  // eased, so the picture does not start and stop with a jerk
  }
  return pos;
}

unsigned FightTimeline::hiddenAt(double t, uint32_t& rgb) const {
  unsigned mask = 0;
  double latest = -1e9;
  rgb = 0;
  for (const FightHideWin& h : hides) {
    if (t < h.t0 || t >= h.t0 + h.dur) continue;
    mask |= h.what;
    if ((h.what & kHideBackdrop) && h.hasColor && h.t0 >= latest) {
      latest = h.t0;
      rgb = h.rgb;
    }
  }
  return mask;
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

FightTimeline buildTimelineList(const FightScript& s, const FighterTimes times[2], const std::vector<std::vector<EffectTimes>>& effectsByStrike, int panMs) {
  FightTimeline tl;
  if (!s.valid) return tl;
  const bool useCam = panMs >= 0;
  const double pan = useCam ? sec(panMs) : 0;
  int camSide = s.initiator;  // the camera of a band that shows one fighter at a time starts on the fighter that acts first
  tl.camStart = camSide;
  double t = kIntro;
  tl.beats.push_back({0, BeatKind::Begin, -1, s.initiator, 0, false, ""});
  for (size_t i = 0; i < s.strikes.size(); ++i) {
    const FightStrike& st = s.strikes[i];
    const int a = st.actor, d = 1 - a;
    const std::vector<EffectTimes> fxs = i < effectsByStrike.size() ? effectsByStrike[i] : std::vector<EffectTimes>();
    if (useCam && camSide != a) {  // the camera goes to the fighter that acts next, before it begins
      tl.cam.push_back({t, pan, a});
      camSide = a;
      t += pan;
    }
    double t0 = t;
    // Skills that fired and conditions put on show first, over their owner, while the fighters stand.
    if (!st.tags.empty()) {
      for (const FightTag& tag : st.tags) tl.beats.push_back({t0, BeatKind::Tag, (int)i, tag.side, 0, false, tag.text});
      t0 += kTagTime;
    }
    tl.beats.push_back({t0, BeatKind::StrikeStart, (int)i, a, 0, false, ""});
    // The striker.
    const FighterTimes& ft = times[a];
    // A staff use plays `cast` (else `attack`; with neither, the fallback motion of a cast).
    const BattleAnimId kind = st.staff ? (ft.has[BaCast] ? BaCast : ft.has[BaAttack] ? BaAttack : BaCast)
                                       : attackAnimFor(ft, st.crit, s.ranged(), !fxs.empty(), !st.hit);
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
    // A camera that shows one fighter at a time moves to the struck one over `pan`, arriving as the hit lands (the hit waits when the blow is sooner).
    if (useCam) {
      hitT = std::max(hitT, t0 + pan);
      tl.cam.push_back({hitT - pan, pan, d});
      camSide = d;
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
      if (e.travel && e.bySpeed) tl.beats.push_back({f.t0, BeatKind::FxFlight, (int)i, a, e.travelMs, false, "", e.name});
      for (const FxHide& h : e.hide) tl.hides.push_back({f.t0 + sec(h.atMs), sec(h.ms), h.what, h.hasColor, h.rgb});
      for (const FxFade& fd : e.fade) tl.fades.push_back({f.t0 + sec(fd.atMs), sec(fd.ms), sec(fd.holdMs), fd.rgb, fd.alpha / 255.0, fd.backdropOnly});
    }
    // The struck fighter.
    double end = std::max(ap.start + ap.dur, fxEnd);
    const FighterTimes& dt = times[d];
    if (st.staff) {  // the result of a staff on the target: no blow, no pose; the number or the label shows where the effect lands
      if (st.missed) {
        tl.beats.push_back({hitT, BeatKind::StaffMiss, (int)i, d, 0, false, ""});
        end = std::max(end, hitT + 0.6);
      } else {
        double at = hitT;
        if (st.healed > 0) {
          tl.beats.push_back({hitT, BeatKind::StaffHeal, (int)i, d, st.healed, false, ""});
          const double rise = std::min(0.7, std::max(0.3, kDrainTime * st.healed / std::max(1, st.hpBefore[d] > 0 ? st.hpBefore[d] : 10) + 0.25));
          tl.hp.push_back({hitT, hitT + rise, d, st.hpBefore[d], st.hpAfterHit[d]});
          end = std::max(end, hitT + std::max(rise, 0.6));
          at += kExtraTime;
        }
        for (const FightTag& n : st.notes) {
          tl.beats.push_back({at, BeatKind::StaffNote, (int)i, d, n.kind, false, n.text});
          at += kExtraTime;
          end = std::max(end, at + 0.4);
        }
        if (st.healed <= 0 && st.notes.empty()) end = std::max(end, hitT + 0.4);
      }
    } else if (st.hit) {
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
