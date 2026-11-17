// The battle scene's clock (docs/BATTLE_SCENE.md "Timing"): a fight script plus how long each fighter's animations take
// become one timeline of poses, beats, hit-point changes and effects. Pure: no renderer, no Battle. The scene plays it
// and tests read it.
#pragma once
#include <string>
#include <vector>

#include "battle/fight_script.h"
#include "core/data/battle_art.h"

namespace ote {

// What one fighter's art offers, in milliseconds. `has[a]` false = no animation of that name: the fallback motion of the
// stand picture is used (fixed times below).
struct FighterTimes {
  bool has[kBattleAnims] = {};
  int total[kBattleAnims] = {};
  int hitAt[kBattleAnims] = {};    // from the start of the animation to the start of its hit frame
  int spawnAt[kBattleAnims] = {};  // ... of its spawn frame, -1 = none given
};

struct EffectTimes {
  bool has = false;
  bool travel = false;
  int travelMs = 250;
  int total = 0;
};

// Fallback motion times (ms), shared by the timeline and the drawing.
constexpr int kFallbackAttackMs = 450, kFallbackAttackHitMs = 280;
constexpr int kFallbackCritMs = 600, kFallbackCritHitMs = 380;
constexpr int kFallbackHurtMs = 320, kFallbackDodgeMs = 380, kFallbackDefeatMs = 700;
// The pacing around the animations (seconds).
constexpr double kIntro = 0.40, kOutro = 0.55, kStrikeGap = 0.30, kTagTime = 0.60, kFade = 0.20, kDrainTime = 0.45, kExtraTime = 0.40;

// Which animation the striker plays for a strike: crit (when it has one), else ranged (a ranged fight with a ranged
// animation), else cast (a weapon with an effect and a cast animation), else attack.
BattleAnimId attackAnimFor(const FighterTimes& t, bool crit, bool ranged, bool hasEffect);

struct FightPose {
  double start = 0, dur = 0;
  BattleAnimId anim = BaStand;
  bool fallback = false;  // no animation of that name: the stand picture moves
  int strike = -1;
};

enum class BeatKind { Begin, Tag, StrikeStart, Hit, Miss, Heal, Hurt, Broke, Defeated, End };

struct FightBeat {
  double t = 0;
  BeatKind kind = BeatKind::Begin;
  int strike = -1;
  int side = 0;        // whose beat (the one struck for Hit / Miss / Defeated, the striker for StrikeStart)
  int value = 0;       // damage for Hit, hit points for Heal / Hurt
  bool crit = false;   // Hit
  std::string text;    // Tag / Broke
};

struct HpSegment {
  double t0 = 0, t1 = 0;
  int side = 0;
  int from = 0, to = 0;
};

struct FightFx {
  double t0 = 0, dur = 0;
  int strike = -1;
  bool travel = false;
  int travelMs = 0;
  int fromSide = 0, toSide = 1;
};

struct FightTimeline {
  double total = 0;                  // the whole scene, fade out included
  std::vector<FightPose> poses[2];   // per side, in start order
  std::vector<FightBeat> beats;      // in time order
  std::vector<HpSegment> hp;
  std::vector<FightFx> fx;

  // The pose of `side` at `t`: the last one begun (its progress is t - start; once over, a defeated fighter stays down and
  // the others stand). Null = standing.
  const FightPose* poseAt(int side, double t) const;
  // The hit points shown for `side` at `t`.
  int hpAt(int side, double t, int startHp) const;
};

// `weaponFx(weapon)` says what the weapon's effect needs (EffectTimes::has false = none).
FightTimeline buildTimeline(const FightScript& s, const FighterTimes times[2], const std::vector<EffectTimes>& effectByStrike);

}  // namespace ote
