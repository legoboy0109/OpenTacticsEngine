#include "battle/fight_timeline.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

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

namespace {

// Lays the strikes of a script out in time, one after the other (buildTimelineList). Each strike goes through the same steps, in this
// order, because the beats, poses and windows are appended to the timeline as they are found and equal times keep that order.
class TimelineBuilder {
 public:
  TimelineBuilder(const FightScript& s, const FighterTimes times[2], const std::vector<std::vector<EffectTimes>>& effectsByStrike, int panMs, int panStartMs,
                  int resultHoldMs, int resultHoldAgainMs, const HitHold& hitHold)
      : s_(s), times_(times), effectsByStrike_(effectsByStrike), useCam_(panMs >= 0), pan_(panMs >= 0 ? sec(panMs) : 0), panStartMs_(panStartMs),
        resultHoldMs_(resultHoldMs), resultHoldAgainMs_(resultHoldAgainMs), hitHold_(hitHold) {}

  FightTimeline build() {
    if (!s_.valid) return tl_;
    camSide_ = s_.initiator;  // the camera of a band that shows one fighter at a time starts on the fighter that acts first
    tl_.camStart = camSide_;
    t_ = kIntro;
    tl_.beats.push_back({0, BeatKind::Begin, -1, s_.initiator, 0, false, ""});
    for (size_t i = 0; i < s_.strikes.size(); ++i) {
      const FightStrike& st = s_.strikes[i];
      addStrike(i);
      if (st.dies[0] || st.dies[1]) break;  // nothing follows a fall
    }
    tl_.beats.push_back({t_ + kOutro, BeatKind::End, -1, 0, 0, false, ""});
    tl_.total = t_ + kOutro + kFade;
    std::stable_sort(tl_.beats.begin(), tl_.beats.end(), [](const FightBeat& x, const FightBeat& y) { return x.t < y.t; });
    return tl_;
  }

 private:
  // Everything the steps of one strike hand to each other.
  struct Strike {
    Strike(size_t index, const FightStrike& strike, const FighterTimes& fighter, std::vector<EffectTimes> effects)
        : i(index), st(strike), a(strike.actor), d(1 - strike.actor), fxs(std::move(effects)), ft(fighter) {}
    size_t i;
    const FightStrike& st;
    int a, d;                       // the striker and the struck side
    std::vector<EffectTimes> fxs;   // the strike's effects
    const FighterTimes& ft;         // the striker's animations
    double t0 = 0;                  // when the strike begins (after the camera has moved and the skill tags have shown)
    BattleAnimId kind = BaAttack;   // the striker's pose
    FightPose ap;
    double hitOffset = 0;           // the blow's moment in that pose
    bool holdA = false;             // the striker's pose holds (its stalls are known once the effects are placed)
    double hitT = 0;                // when the blow lands (or the miss shows)
    double hitNoDelay = 0;          // the same without any "atRange" delay: the delay moves the effect and the hit, never the camera
    double fxEnd = 0;               // the end of an effect that flies back or lingers (the strike lasts until then)
    double fxBusy = 0;              // when the last part of any effect (pictures, sounds, flashes, shakes, hides, fades, a flight home) is over
    double camDone = 0;             // when the camera has arrived at the struck fighter
    int panFx = -1;                 // the first effect with its own "panAtMs": the camera leaves relative to its start
    std::vector<double> fxStart;
    double resultOver = 0;          // when the result's held poses may go on
    double end = 0;                 // the end of the strike's own action
  };

  void addStrike(size_t i) {
    const FightStrike& st = s_.strikes[i];
    Strike k(i, st, times_[st.actor], i < effectsByStrike_.size() ? effectsByStrike_[i] : std::vector<EffectTimes>());
    if (useCam_ && camSide_ != k.a) {  // the camera goes to the fighter that acts next, before it begins
      tl_.cam.push_back({t_, pan_, k.a});
      camSide_ = k.a;
      t_ += pan_;
    }
    k.t0 = t_;
    showTags(k);
    tl_.beats.push_back({k.t0, BeatKind::StrikeStart, (int)i, k.a, 0, false, ""});
    placeStriker(k);
    timeBlow(k);
    placeEffects(k);
    panByEffect(k);
    holdStrikerPose(k);
    k.end = std::max(std::max(k.ap.start + k.ap.dur, k.fxEnd), k.camDone);
    if (st.staff) placeStaffResult(k);
    else if (st.hit) placeBlow(k);
    else placeMiss(k);
    if (st.weaponBroke) {
      tl_.beats.push_back({k.hitT + 0.1, BeatKind::Broke, (int)i, k.a, 0, false, st.brokeText});
      k.end = std::max(k.end, k.hitT + 0.6);
    }
    placeDrainAndRecoil(k);
    t_ = k.end + kStrikeGap;
  }

  // Skills that fired and conditions put on show first, over their owner, while the fighters stand.
  void showTags(Strike& k) {
    if (k.st.tags.empty()) return;
    for (const FightTag& tag : k.st.tags) tl_.beats.push_back({k.t0, BeatKind::Tag, (int)k.i, tag.side, 0, false, tag.text});
    k.t0 += kTagTime;
  }

  // The striker's pose and its own sounds. A held strike animation is added once the strike's effects are known (its wait depends on
  // when they end); every other one now, as always.
  void placeStriker(Strike& k) {
    const FighterTimes& ft = k.ft;
    // A staff use plays `cast` (else `attack`; with neither, the fallback motion of a cast).
    k.kind = k.st.staff ? (ft.has[BaCast] ? BaCast : ft.has[BaAttack] ? BaAttack : BaCast)
                        : attackAnimFor(ft, k.st.crit, s_.ranged(), !k.fxs.empty(), !k.st.hit);
    k.ap.start = k.t0;
    k.ap.anim = k.kind;
    k.ap.strike = (int)k.i;
    if (ft.has[k.kind]) {
      k.ap.dur = sec(ft.total[k.kind]);
      k.hitOffset = sec(ft.hitAt[k.kind]);
    } else {
      k.ap.fallback = true;
      k.ap.dur = sec(k.kind == BaCrit ? kFallbackCritMs : kFallbackAttackMs);
      k.hitOffset = sec(k.kind == BaCrit ? kFallbackCritHitMs : kFallbackAttackHitMs);
    }
    k.holdA = ft.has[k.kind] && ft.hold[k.kind];
    if (!k.holdA) {
      tl_.poses[k.a].push_back(k.ap);
      addSounds(tl_, k.ap, k.a, ft, (int)k.i);
    }
  }

  // When the strike's effect starts to travel: at the animation's spawn frame, else at the blow.
  double spawnTime(const Strike& k) const { return k.t0 + (k.ft.has[k.kind] && k.ft.spawnAt[k.kind] >= 0 ? sec(k.ft.spawnAt[k.kind]) : k.hitOffset); }

  // When an effect's lead-in starts counting: the strike's start, or with "fromHitFrame" the striker animation's hit frame.
  double leadBase(const Strike& k, const EffectTimes& e) const { return e.fromHitFrame ? k.t0 + k.hitOffset : k.t0; }
  // How long after a shot leaves its hit lands: on arrival, or "hitAfterMs" (the shot may still be in the air).
  static int shotHitMs(const EffectTimes& e) { return e.hitAfterMs >= 0 ? e.hitAfterMs : e.travelMs; }

  // The strike's effects (docs/BATTLE_SCENE.md "Timing of effects"). The hit lands at the strike animation's hit frame, but not before a
  // travelling effect has arrived (or "hitAfterMs" has passed since it left), and not before an effect that starts `hitAtMs` ahead of it has
  // had that long to run since the strike began (with "fromHitFrame": since the hit frame).
  // Then the camera, and the "atRange" delay of the effects (which starts them later, and the hit they lead to with them).
  void timeBlow(Strike& k) {
    k.hitT = k.t0 + k.hitOffset;
    k.hitNoDelay = k.hitT;
    for (const EffectTimes& e : k.fxs) {
      if (e.travel) k.hitNoDelay = std::max(k.hitNoDelay, spawnTime(k) + sec(shotHitMs(e)));  // the hit lands when the shot arrives
      else if (e.hitAtMs > 0) k.hitNoDelay = std::max(k.hitNoDelay, leadBase(k, e) + sec(e.hitAtMs));
    }
    cameraToStruck(k);
    k.hitT = k.hitNoDelay;
    for (const EffectTimes& e : k.fxs) {
      if (e.delayMs <= 0) continue;
      if (e.travel) k.hitT = std::max(k.hitT, spawnTime(k) + sec(e.delayMs + shotHitMs(e)));
      else k.hitT = std::max(k.hitT, std::max(k.hitNoDelay, leadBase(k, e) + sec(e.hitAtMs)) + sec(e.delayMs));
    }
  }

  // A camera that shows one fighter at a time moves to the struck one over `pan`, arriving as the hit lands (the hit waits when the blow is sooner).
  // With a "startMs" it leaves that long after the strike starts instead and the hit does not wait for it (only the next strike does).
  void cameraToStruck(Strike& k) {
    for (size_t j = 0; useCam_ && k.panFx < 0 && j < k.fxs.size(); ++j)
      if (k.fxs[j].panAtMs >= 0) k.panFx = (int)j;
    if (useCam_ && k.panFx >= 0) {
      camSide_ = k.d;  // (it leaves relative to an effect: panByEffect, once the effects are placed)
    } else if (useCam_) {
      if (panStartMs_ >= 0) {
        tl_.cam.push_back({k.t0 + sec(panStartMs_), pan_, k.d});
        k.camDone = k.t0 + sec(panStartMs_) + pan_;
      } else {
        k.hitNoDelay = std::max(k.hitNoDelay, k.t0 + pan_);
        tl_.cam.push_back({k.hitNoDelay - pan_, pan_, k.d});
      }
      camSide_ = k.d;
    }
  }

  void placeEffects(Strike& k) {
    for (size_t j = 0; j < k.fxs.size(); ++j) placeEffect(k, k.fxs[j], (int)j);
  }

  void placeEffect(Strike& k, const EffectTimes& e, int slot) {
    FightFx f;
    f.strike = (int)k.i;
    f.slot = slot;
    f.travel = e.travel;
    f.fromSide = k.a;
    f.toSide = e.striker ? k.a : k.d;
    if (e.travel) {
      f.t0 = spawnTime(k) + sec(e.delayMs);
      f.travelMs = e.travelMs;
      f.returns = e.returns;
      f.lingerMs = e.lingerMs;
      f.dur = sec(e.travelMs + e.lingerMs + (e.returns ? e.travelMs : 0));
      if (e.returns || e.lingerMs > 0) k.fxEnd = std::max(k.fxEnd, f.t0 + f.dur);
    } else {
      f.t0 = (e.rangeFixed ? k.hitNoDelay : k.hitT) - sec(e.hitAtMs);  // "atRangeFixed": as if no effect of the strike were delayed
      f.dur = sec(std::max(e.total, 60));
    }
    tl_.fx.push_back(f);
    k.fxStart.push_back(f.t0);
    k.fxBusy = std::max(k.fxBusy, f.t0 + f.dur);
    placeEffectRows(k, e, f);
  }

  // The effect's sounds, flashes, shakes, hides and fades; each pushes the strike's busy time out to its own end.
  void placeEffectRows(Strike& k, const EffectTimes& e, const FightFx& f) {
    // The start a row marked "atRangeFixed" is timed from: the effect's start without its own "atRange" delay.
    const double base = f.t0 - sec(e.delayMs);
    auto rowT = [&](bool fixed) { return fixed ? base : f.t0; };
    const int i = (int)k.i;
    if (e.extended) tl_.beats.push_back({f.t0, BeatKind::FxStart, i, k.a, 0, false, "", e.name});
    for (const auto& snd : e.sounds) {
      tl_.beats.push_back({f.t0 + sec(snd.first), BeatKind::FxSound, i, k.a, 0, false, snd.second, e.name});
      k.fxBusy = std::max(k.fxBusy, f.t0 + sec(snd.first));
    }
    for (const FxFlash& fl : e.flash) {
      tl_.flashes.push_back({rowT(fl.fixed) + sec(fl.atMs), sec(fl.ms), fl.rgb, 0, i});
      k.fxBusy = std::max(k.fxBusy, rowT(fl.fixed) + sec(fl.atMs + fl.ms));
    }
    for (const FxShake& sh : e.shake) {
      tl_.shakes.push_back({rowT(sh.fixed) + sec(sh.atMs), sec(sh.ms), 0, sh.strength, i});
      k.fxBusy = std::max(k.fxBusy, rowT(sh.fixed) + sec(sh.atMs + sh.ms));
    }
    if (e.travel && e.bySpeed) tl_.beats.push_back({f.t0, BeatKind::FxFlight, i, k.a, e.travelMs, false, "", e.name});
    for (const FxHide& h : e.hide) {
      tl_.hides.push_back({rowT(h.fixed) + sec(h.atMs), sec(h.ms), h.what, h.hasColor, h.rgb});
      k.fxBusy = std::max(k.fxBusy, rowT(h.fixed) + sec(h.atMs + h.ms));
    }
    for (const FxFade& fd : e.fade) {
      tl_.fades.push_back({rowT(fd.fixed) + sec(fd.atMs), sec(fd.ms), sec(fd.holdMs), fd.rgb, fd.alpha / 255.0, fd.backdropOnly});
      k.fxBusy = std::max(k.fxBusy, rowT(fd.fixed) + sec(fd.atMs + 2 * fd.ms + fd.holdMs));
    }
  }

  // A camera that leaves relative to an effect's own start (after its "atRange" delay): like "startMs", the blow does not wait for it.
  void panByEffect(Strike& k) {
    if (!useCam_ || k.panFx < 0) return;
    const double leave = k.fxStart[(size_t)k.panFx] + sec(k.fxs[(size_t)k.panFx].panAtMs);
    tl_.cam.push_back({leave, pan_, k.d});
    k.camDone = leave + pan_;
  }

  // Holding a pose (docs/BATTLE_SCENE.md "Holding a pose"): the result is over `resultHoldMs` after the blow / miss moment (with "hitHold", a hit
  // waits by the hit points it changed instead), or when the effects are all over if later, and `resultHoldAgainMs` more when the same fighter strikes
  // again next.
  double resultHold(const Strike& k) const {
    const FightStrike& st = k.st;
    if (!hitHold_.on || !st.hit || (st.staff && st.missed)) return sec(resultHoldMs_);
    const int points = st.staff ? st.healed : std::abs(st.hpBefore[k.d] - st.hpAfterHit[k.d]);
    return hitHold_.ms(points, !st.staff && st.kill) / 1000.0;
  }
  void holdStrikerPose(Strike& k) {
    const bool again = k.i + 1 < s_.strikes.size() && s_.strikes[k.i + 1].actor == k.a;
    k.resultOver = std::max(k.hitT + resultHold(k), k.fxBusy) + (again ? sec(resultHoldAgainMs_) : 0.0);
    if (!k.holdA) return;
    StallMaker m{k.ap.start, &k.ap};
    m.holdUntil(k.ft.holdAt[k.kind], k.ft.holdLen[k.kind], k.resultOver);
    k.ap.dur += m.extraMs / 1000.0;
    tl_.poses[k.a].push_back(k.ap);
    addSounds(tl_, k.ap, k.a, k.ft, (int)k.i);
  }

  // The result of a staff on the target: no blow, no pose; the number or the label shows where the effect lands.
  void placeStaffResult(Strike& k) {
    const FightStrike& st = k.st;
    const int i = (int)k.i, d = k.d;
    if (st.missed) {
      tl_.beats.push_back({k.hitT, BeatKind::StaffMiss, i, d, 0, false, ""});
      k.end = std::max(k.end, k.hitT + 0.6);
      return;
    }
    double at = k.hitT;
    if (st.healed > 0) {
      tl_.beats.push_back({k.hitT, BeatKind::StaffHeal, i, d, st.healed, false, ""});
      const double rise = std::min(0.7, std::max(0.3, kDrainTime * st.healed / std::max(1, st.hpBefore[d] > 0 ? st.hpBefore[d] : 10) + 0.25));
      tl_.hp.push_back({k.hitT, k.hitT + rise, d, st.hpBefore[d], st.hpAfterHit[d]});
      k.end = std::max(k.end, k.hitT + std::max(rise, 0.6));
      at += kExtraTime;
    }
    for (const FightTag& n : st.notes) {
      tl_.beats.push_back({at, BeatKind::StaffNote, i, d, n.kind, false, n.text});
      at += kExtraTime;
      k.end = std::max(k.end, at + 0.4);
    }
    if (st.healed <= 0 && st.notes.empty()) k.end = std::max(k.end, k.hitT + 0.4);
  }

  // The struck fighter's pose (hurt or defeat), the hit beat and its hit-point drain.
  void placeBlow(Strike& k) {
    const FightStrike& st = k.st;
    const int i = (int)k.i, d = k.d;
    const FighterTimes& dt = times_[d];
    tl_.beats.push_back({k.hitT, BeatKind::Hit, i, d, st.damage, st.crit, ""});
    FightPose dp;
    dp.start = k.hitT;
    dp.strike = i;
    if (st.dies[d]) {
      dp.anim = BaDefeat;
      dp.dur = dt.has[BaDefeat] ? sec(dt.total[BaDefeat]) : sec(kFallbackDefeatMs);
      dp.fallback = !dt.has[BaDefeat];
      tl_.beats.push_back({k.hitT + dp.dur, BeatKind::Defeated, i, d, 0, false, ""});
    } else {
      dp.anim = BaHurt;
      dp.dur = dt.has[BaHurt] ? sec(dt.total[BaHurt]) : sec(kFallbackHurtMs);
      dp.fallback = !dt.has[BaHurt];
      if (dt.has[BaHurt] && dt.hold[BaHurt]) {
        StallMaker m{dp.start, &dp};
        m.holdUntil(dt.holdAt[BaHurt], dt.holdLen[BaHurt], k.resultOver);
        dp.dur += m.extraMs / 1000.0;
      }
    }
    tl_.poses[d].push_back(dp);
    addSounds(tl_, dp, d, dt, i);
    k.end = std::max(k.end, k.hitT + dp.dur + (st.dies[d] ? 0.3 : 0.0));
    if (st.damage > 0 || st.hpBefore[d] != st.hpAfterHit[d]) {
      const double drain = std::min(0.7, std::max(0.3, kDrainTime * st.damage / std::max(1, st.hpBefore[d] > 0 ? st.hpBefore[d] : 10) + 0.25));
      tl_.hp.push_back({k.hitT, k.hitT + drain, d, st.hpBefore[d], st.hpAfterHit[d]});
      k.end = std::max(k.end, k.hitT + drain);
    }
  }

  // The struck fighter dodges.
  void placeMiss(Strike& k) {
    const int i = (int)k.i, d = k.d;
    const FighterTimes& dt = times_[d];
    tl_.beats.push_back({k.hitT, BeatKind::Miss, i, d, 0, false, ""});
    FightPose dp;
    dp.start = std::max(k.t0, k.hitT - 0.12);
    dp.anim = s_.ranged() && dt.has[BaDodgeRanged] ? BaDodgeRanged : BaDodge;  // (dodgeRanged falls back to dodge)
    dp.dur = dt.has[dp.anim] ? sec(dt.total[dp.anim]) : sec(kFallbackDodgeMs);
    dp.fallback = !dt.has[dp.anim];
    dp.strike = i;
    // A dodge that holds (docs/BATTLE_SCENE.md "Holding a pose"): the frames before "startFrame" are the brace, played from the start of the strike with the last one
    // held until the miss; the dodge proper starts at the miss plus "startDelayMs"; "holdFrame" waits until the result is over. Without "startFrame" the whole dodge starts at the miss.
    if (dt.has[dp.anim] && (dt.hold[dp.anim] || dt.start[dp.anim])) {
      const BattleAnimId id = dp.anim;
      const double properAt = k.hitT + sec(dt.startDelay[id]);
      const bool braced = dt.start[id] && dt.brace[id];
      dp.start = braced ? k.t0 : properAt;
      StallMaker m{dp.start, &dp};
      if (braced) m.holdUntil(dt.braceAt[id], dt.braceLen[id], properAt);
      if (dt.hold[id]) m.holdUntil(dt.holdAt[id], dt.holdLen[id], k.resultOver);
      dp.dur += m.extraMs / 1000.0;
    }
    tl_.poses[d].push_back(dp);
    addSounds(tl_, dp, d, dt, i);
    k.end = std::max(k.end, dp.start + dp.dur);
  }

  // Drain and recoil, one after the other once the hit has settled.
  void placeDrainAndRecoil(Strike& k) {
    int hpNow[2] = {k.st.hpAfterHit[0], k.st.hpAfterHit[1]};
    for (const FightHp& h : k.st.after) {
      tl_.beats.push_back({k.end, h.delta >= 0 ? BeatKind::Heal : BeatKind::Hurt, (int)k.i, h.side, h.delta >= 0 ? h.delta : -h.delta, false, ""});
      tl_.hp.push_back({k.end, k.end + sec(350), h.side, hpNow[h.side], hpNow[h.side] + h.delta});
      hpNow[h.side] += h.delta;
      k.end += kExtraTime;
    }
  }

  const FightScript& s_;
  const FighterTimes* times_;
  const std::vector<std::vector<EffectTimes>>& effectsByStrike_;
  const bool useCam_;
  const double pan_;
  const int panStartMs_, resultHoldMs_, resultHoldAgainMs_;
  const HitHold hitHold_;
  FightTimeline tl_;
  int camSide_ = 0;
  double t_ = 0;
};

}  // namespace

FightTimeline buildTimelineList(const FightScript& s, const FighterTimes times[2], const std::vector<std::vector<EffectTimes>>& effectsByStrike, int panMs, int panStartMs, int resultHoldMs, int resultHoldAgainMs,
                                const HitHold& hitHold) {
  return TimelineBuilder(s, times, effectsByStrike, panMs, panStartMs, resultHoldMs, resultHoldAgainMs, hitHold).build();
}

}  // namespace ote
