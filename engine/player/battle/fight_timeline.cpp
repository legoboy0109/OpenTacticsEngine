#include "battle/fight_timeline.h"

#include <algorithm>
#include <cmath>

namespace ote {

namespace {
// An animation's own sounds (docs/BATTLE_SCENE.md "Sounds"): one beat per sound at the start of its frame, for a pose that plays the
// set's animation (a fallback motion has none).
void addSounds(FightTimeline& tl, const FightPose& pose, int side, const FighterTimes& t, int strike) {
  if (pose.fallback) return;
  for (const auto& s : t.soundAt[pose.anim]) tl.beats.push_back({pose.start + pose.localSecOf(s.first), BeatKind::Sound, strike, side, 0, false, s.second});
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

double FightPose::animMsAt(double localSec) const {
  const double local = localSec * 1000.0;
  double extra = 0;
  for (const Stall& s : stalls) {
    const double begin = s.atMs + extra;
    if (local <= begin) break;
    if (local < begin + s.pauseMs) return s.atMs;  // frozen on the frame that starts at atMs
    extra += s.pauseMs;
  }
  return local - extra;
}

double FightPose::localSecOf(int animMs) const {
  double extra = 0;
  for (const Stall& s : stalls)
    if (s.atMs < animMs) extra += s.pauseMs;
  return (animMs + extra) / 1000.0;
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

// Builds the stalls of one pose in animation order (docs/BATTLE_SCENE.md "Holding a pose"): a frame that begins at `frameAtMs` of the animation and lasts
// `frameLenMs` is kept on screen until at least `untilAbs` (scene seconds); the frames after it come later by the extra time.
struct StallMaker {
  double startAbs;
  FightPose* pose;
  double extraMs = 0;
  void holdUntil(int frameAtMs, int frameLenMs, double untilAbs) {
    const double begins = startAbs + (frameAtMs + extraMs) / 1000.0;
    const double pause = (untilAbs - begins) * 1000.0 - frameLenMs;
    if (pause <= 0.5) return;  // it ends later than that on its own
    pose->stalls.push_back({frameAtMs, pause});
    extraMs += pause;
  }
};

}  // namespace

FightTimeline buildTimeline(const FightScript& s, const FighterTimes times[2], const std::vector<EffectTimes>& effectByStrike) {
  std::vector<std::vector<EffectTimes>> lists;
  for (const EffectTimes& e : effectByStrike) lists.push_back(e.has ? std::vector<EffectTimes>(1, e) : std::vector<EffectTimes>());
  return buildTimelineList(s, times, lists);
}

FightTimeline buildTimelineList(const FightScript& s, const FighterTimes times[2], const std::vector<std::vector<EffectTimes>>& effectsByStrike, int panMs, int panStartMs, int resultHoldMs, int resultHoldAgainMs) {
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
    // A held strike animation is added once the strike's effects are known (its wait depends on when they end); every other one now, as always.
    const bool holdA = ft.has[kind] && ft.hold[kind];
    if (!holdA) {
      tl.poses[a].push_back(ap);
      addSounds(tl, ap, a, ft, (int)i);
    }
    double hitT = t0 + hitOffset;
    // The strike's effects (docs/BATTLE_SCENE.md "Timing of effects"). The hit lands at the strike animation's hit frame, but not before a
    // travelling effect has arrived, and not before an effect that starts `hitAtMs` ahead of it has had that long to run since the strike began.
    double fxEnd = 0;  // the end of an effect that flies back or lingers (the strike lasts until then)
    // The hit time and the camera's timing as they are without any "atRange" delay: the delay moves the effect and the hit, never the camera.
    double hitNoDelay = hitT;
    for (const EffectTimes& e : fxs) {
      if (e.travel) {
        const double spawn = t0 + (ft.has[kind] && ft.spawnAt[kind] >= 0 ? sec(ft.spawnAt[kind]) : hitOffset);
        hitNoDelay = std::max(hitNoDelay, spawn + sec(e.travelMs));  // the hit lands when the shot arrives
      } else if (e.hitAtMs > 0) {
        hitNoDelay = std::max(hitNoDelay, t0 + sec(e.hitAtMs));
      }
    }
    // A camera that shows one fighter at a time moves to the struck one over `pan`, arriving as the hit lands (the hit waits when the blow is sooner).
    // With a "startMs" it leaves that long after the strike starts instead and the hit does not wait for it (only the next strike does).
    double camDone = 0;
    int panFx = -1;  // the first effect with its own "panAtMs": the camera leaves relative to its start (set after the effects are placed)
    for (size_t k = 0; useCam && panFx < 0 && k < fxs.size(); ++k)
      if (fxs[k].panAtMs >= 0) panFx = (int)k;
    if (useCam && panFx >= 0) {
      camSide = d;
    } else if (useCam) {
      if (panStartMs >= 0) {
        tl.cam.push_back({t0 + sec(panStartMs), pan, d});
        camDone = t0 + sec(panStartMs) + pan;
      } else {
        hitNoDelay = std::max(hitNoDelay, t0 + pan);
        tl.cam.push_back({hitNoDelay - pan, pan, d});
      }
      camSide = d;
    }
    // An effect with an "atRange" delay for this distance starts later, and the hit it leads to with it.
    hitT = hitNoDelay;
    for (const EffectTimes& e : fxs) {
      if (e.delayMs <= 0) continue;
      if (e.travel) {
        const double spawn = t0 + (ft.has[kind] && ft.spawnAt[kind] >= 0 ? sec(ft.spawnAt[kind]) : hitOffset);
        hitT = std::max(hitT, spawn + sec(e.delayMs + e.travelMs));
      } else {
        hitT = std::max(hitT, std::max(hitNoDelay, t0 + sec(e.hitAtMs)) + sec(e.delayMs));
      }
    }
    double fxBusy = 0;  // when the last part of any of the strike's effects (pictures, sounds, flashes, shakes, hides, fades, a flight home) is over
    std::vector<double> fxStart;
    for (size_t k = 0; k < fxs.size(); ++k) {
      const EffectTimes& e = fxs[k];
      FightFx f;
      f.strike = (int)i;
      f.slot = (int)k;
      f.travel = e.travel;
      f.fromSide = a;
      f.toSide = e.striker ? a : d;
      if (e.travel) {
        f.t0 = t0 + (ft.has[kind] && ft.spawnAt[kind] >= 0 ? sec(ft.spawnAt[kind]) : hitOffset) + sec(e.delayMs);
        f.travelMs = e.travelMs;
        f.returns = e.returns;
        f.lingerMs = e.lingerMs;
        f.dur = sec(e.travelMs + e.lingerMs + (e.returns ? e.travelMs : 0));
        if (e.returns || e.lingerMs > 0) fxEnd = std::max(fxEnd, f.t0 + f.dur);
      } else {
        f.t0 = (e.rangeFixed ? hitNoDelay : hitT) - sec(e.hitAtMs);  // "atRangeFixed": as if no effect of the strike were delayed
        f.dur = sec(std::max(e.total, 60));
      }
      tl.fx.push_back(f);
      fxStart.push_back(f.t0);
      fxBusy = std::max(fxBusy, f.t0 + f.dur);
      // The start a row marked "atRangeFixed" is timed from: the effect's start without its own "atRange" delay.
      const double base = f.t0 - sec(e.delayMs);
      auto rowT = [&](bool fixed) { return fixed ? base : f.t0; };
      if (e.extended) tl.beats.push_back({f.t0, BeatKind::FxStart, (int)i, a, 0, false, "", e.name});
      for (const auto& snd : e.sounds) {
        tl.beats.push_back({f.t0 + sec(snd.first), BeatKind::FxSound, (int)i, a, 0, false, snd.second, e.name});
        fxBusy = std::max(fxBusy, f.t0 + sec(snd.first));
      }
      for (const FxFlash& fl : e.flash) {
        tl.flashes.push_back({rowT(fl.fixed) + sec(fl.atMs), sec(fl.ms), fl.rgb, 0, (int)i});
        fxBusy = std::max(fxBusy, rowT(fl.fixed) + sec(fl.atMs + fl.ms));
      }
      for (const FxShake& sh : e.shake) {
        tl.shakes.push_back({rowT(sh.fixed) + sec(sh.atMs), sec(sh.ms), 0, sh.strength, (int)i});
        fxBusy = std::max(fxBusy, rowT(sh.fixed) + sec(sh.atMs + sh.ms));
      }
      if (e.travel && e.bySpeed) tl.beats.push_back({f.t0, BeatKind::FxFlight, (int)i, a, e.travelMs, false, "", e.name});
      for (const FxHide& h : e.hide) {
        tl.hides.push_back({rowT(h.fixed) + sec(h.atMs), sec(h.ms), h.what, h.hasColor, h.rgb});
        fxBusy = std::max(fxBusy, rowT(h.fixed) + sec(h.atMs + h.ms));
      }
      for (const FxFade& fd : e.fade) {
        tl.fades.push_back({rowT(fd.fixed) + sec(fd.atMs), sec(fd.ms), sec(fd.holdMs), fd.rgb, fd.alpha / 255.0, fd.backdropOnly});
        fxBusy = std::max(fxBusy, rowT(fd.fixed) + sec(fd.atMs + 2 * fd.ms + fd.holdMs));
      }
    }
    // A camera that leaves relative to an effect's own start (after its "atRange" delay): like "startMs", the blow does not wait for it.
    if (useCam && panFx >= 0) {
      const double leave = fxStart[(size_t)panFx] + sec(fxs[(size_t)panFx].panAtMs);
      tl.cam.push_back({leave, pan, d});
      camDone = leave + pan;
    }
    // Holding a pose (docs/BATTLE_SCENE.md "Holding a pose"): the result is over `resultHoldMs` after the blow / miss moment, or when the effects
    // are all over if later, and `resultHoldAgainMs` more when the same fighter strikes again next.
    const bool again = i + 1 < s.strikes.size() && s.strikes[i + 1].actor == a;
    const double resultOver = std::max(hitT + sec(resultHoldMs), fxBusy) + (again ? sec(resultHoldAgainMs) : 0.0);
    if (holdA) {
      StallMaker m{ap.start, &ap};
      m.holdUntil(ft.holdAt[kind], ft.holdLen[kind], resultOver);
      ap.dur += m.extraMs / 1000.0;
      tl.poses[a].push_back(ap);
      addSounds(tl, ap, a, ft, (int)i);
    }
    // The struck fighter.
    double end = std::max(std::max(ap.start + ap.dur, fxEnd), camDone);
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
        if (dt.has[BaHurt] && dt.hold[BaHurt]) {
          StallMaker m{dp.start, &dp};
          m.holdUntil(dt.holdAt[BaHurt], dt.holdLen[BaHurt], resultOver);
          dp.dur += m.extraMs / 1000.0;
        }
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
      // A dodge that holds (docs/BATTLE_SCENE.md "Holding a pose"): the frames before "startFrame" are the brace, played from the start of the strike with the last one
      // held until the miss; the dodge proper starts at the miss plus "startDelayMs"; "holdFrame" waits until the result is over. Without "startFrame" the whole dodge starts at the miss.
      if (dt.has[dp.anim] && (dt.hold[dp.anim] || dt.start[dp.anim])) {
        const BattleAnimId id = dp.anim;
        const double properAt = hitT + sec(dt.startDelay[id]);
        const bool braced = dt.start[id] && dt.brace[id];
        dp.start = braced ? t0 : properAt;
        StallMaker m{dp.start, &dp};
        if (braced) m.holdUntil(dt.braceAt[id], dt.braceLen[id], properAt);
        if (dt.hold[id]) m.holdUntil(dt.holdAt[id], dt.holdLen[id], resultOver);
        dp.dur += m.extraMs / 1000.0;
      }
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
