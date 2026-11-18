// The battle scene's clock (docs/BATTLE_SCENE.md "Timing"): a fight script plus how long each fighter's animations take
// become one timeline of poses, beats, hit-point changes and effects. Pure: no renderer, no Battle. The scene plays it
// and tests read it.
#pragma once
#include <cstdint>
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
  std::vector<std::pair<int, std::string>> soundAt[kBattleAnims];  // the animation's sounds: ms from its start, sound id
};

struct EffectTimes {
  bool has = false;
  bool travel = false;
  int travelMs = 250;
  int total = 0;
  // The newer keys (docs/BATTLE_SCENE.md "Effects"); all off = the effect plays exactly as it always did.
  int hitAtMs = 0;        // starts this long before the hit lands (not for travel)
  bool striker = false;   // drawn on the striker, not the target
  bool returns = false;   // travel: flies back after arriving
  int lingerMs = 0;       // travel: stays at the target this long after arriving
  bool extended = false;  // some newer key is used: the effect logs its start
  std::string name;       // for the log lines
  std::vector<std::pair<int, std::string>> sounds;  // ms after the effect starts, sound id
  std::vector<FxFlash> flash;
  std::vector<FxShake> shake;
  std::vector<FxHide> hide;  // scene-wide parts (docs/BATTLE_SCENE.md "Hiding and fading")
  std::vector<FxFade> fade;
  int delayMs = 0;           // "atRange": for this fight's distance the whole effect (its start, sounds, flashes, hides, fades and the hit it leads to) comes this much later
  bool bySpeed = false;      // travel: `travelMs` was worked out from the distance and "speed" (the effect then logs its flight time)
};

// Fallback motion times (ms), shared by the timeline and the drawing.
constexpr int kFallbackAttackMs = 450, kFallbackAttackHitMs = 280;
constexpr int kFallbackCritMs = 600, kFallbackCritHitMs = 380;
constexpr int kFallbackHurtMs = 320, kFallbackDodgeMs = 380, kFallbackDefeatMs = 700;
// The pacing around the animations (seconds).
constexpr double kIntro = 0.40, kOutro = 0.55, kStrikeGap = 0.30, kTagTime = 0.60, kFade = 0.20, kDrainTime = 0.45, kExtraTime = 0.40;

// Which animation the striker plays for a strike: `miss` (a blow that does not land, when the set has one), else
// critRanged (a ranged crit), else crit (when it has one), else ranged (a ranged fight with a ranged animation), else cast
// (a weapon with an effect and a cast animation), else attack.
BattleAnimId attackAnimFor(const FighterTimes& t, bool crit, bool ranged, bool hasEffect, bool miss = false);

struct FightPose {
  double start = 0, dur = 0;
  BattleAnimId anim = BaStand;
  bool fallback = false;  // no animation of that name: the stand picture moves
  int strike = -1;
};

enum class BeatKind { Begin, Tag, StrikeStart, Hit, Miss, Heal, Hurt, Broke, Defeated, End, Sound, FxSound, FxStart, StaffHeal, StaffMiss, StaffNote, FxFlight };
// Sound: an animation's own sound (text = its id). FxSound: an effect's sound (text = its id, label = the effect's name). FxStart: an effect
// that uses the newer keys begins (label = its name). A staff use's results (docs/BATTLE_SCENE.md "Staves in the scene"), on the target `side`:
// FxFlight: a travel effect whose time came from "speed" (value = the flight time in ms, label = its name). StaffHeal (value = hit points), StaffMiss, StaffNote (text = the label; value 1 = an item mended, else a condition).

struct FightBeat {
  double t = 0;
  BeatKind kind = BeatKind::Begin;
  int strike = -1;
  int side = 0;        // whose beat (the one struck for Hit / Miss / Defeated, the striker for StrikeStart)
  int value = 0;       // damage for Hit, hit points for Heal / Hurt
  bool crit = false;   // Hit
  std::string text;    // Tag / Broke / Sound / FxSound
  std::string label = std::string();  // FxSound / FxStart: the effect's name
};

struct HpSegment {
  double t0 = 0, t1 = 0;
  int side = 0;
  int from = 0, to = 0;
};

struct FightFx {
  double t0 = 0, dur = 0;  // dur: the whole time it is drawn (a returning projectile: out, linger, back)
  int strike = -1;
  int slot = 0;            // which of the strike's effects (the position in its list)
  bool travel = false;
  int travelMs = 0;
  bool returns = false;
  int lingerMs = 0;
  int fromSide = 0, toSide = 1;  // a striker effect has toSide == fromSide
};

// A flash of colour over the scene / a shake of the stage, from `t0` for `dur` seconds.
struct FightPulse {
  double t0 = 0, dur = 0;
  uint32_t rgb = 0xffffff;  // flash
  int strength = 0;         // shake: stage pixels before scaling
  int strike = -1;
};

// A window of an effect's "hide" / "fade" (seconds on the scene's clock).
struct FightHideWin {
  double t0 = 0, dur = 0;
  unsigned what = 0;
  bool hasColor = false;
  uint32_t rgb = 0;
};
struct FightFadeWin {
  double t0 = 0, ms = 0, hold = 0;  // in over `ms`, held `hold`, out over `ms`
  uint32_t rgb = 0;
  double alpha = 1;
  bool backdropOnly = false;
  // The opacity (0 to alpha) at scene time `t`.
  double opacityAt(double t) const;
};
// The camera of a range band that shows one fighter at a time (docs/BATTLE_SCENE.md "Range bands"): from `t` it moves to `side` over `dur` seconds.
struct FightCamKey {
  double t = 0, dur = 0;
  int side = 0;
};

struct FightTimeline {
  double total = 0;                  // the whole scene, fade out included
  std::vector<FightPose> poses[2];   // per side, in start order
  std::vector<FightBeat> beats;      // in time order
  std::vector<HpSegment> hp;
  std::vector<FightFx> fx;
  std::vector<FightPulse> flashes, shakes;  // an effect's flash and shake lists
  std::vector<FightHideWin> hides;
  std::vector<FightFadeWin> fades;
  int camStart = 0;                         // the side the camera begins on (the fighter that acts first)
  std::vector<FightCamKey> cam;             // empty = no camera (both fighters in one picture)

  // Where the camera is at `t`: 0 = on the left fighter, 1 = on the right fighter, in between while it moves.
  double cameraAt(double t) const;
  // The layers hidden at `t` (kHideBackdrop | kHidePlatforms), and the colour the stage is filled with while the backdrop is hidden (the window that began last
  // among those hiding it that give a colour; black otherwise).
  unsigned hiddenAt(double t, uint32_t& rgb) const;

  // The pose of `side` at `t`: the last one begun (its progress is t - start; once over, a defeated fighter stays down and
  // the others stand). Null = standing.
  const FightPose* poseAt(int side, double t) const;
  // The hit points shown for `side` at `t`.
  int hpAt(int side, double t, int startHp) const;
};

// `effectsByStrike[i]` lists the effects of strike i in the order they start (empty = none; docs/BATTLE_SCENE.md "Timing of effects").
// `panMs` >= 0: the range band shows one fighter at a time; the camera follows the acting fighter and moves to the struck one over `panMs` so that it has
// arrived when the hit lands (the hit waits for it when the blow comes sooner); -1: no camera.
// `panStartMs` >= 0 (with a camera): the move to the struck fighter starts that long after the strike starts, whatever the hit time (the hit does not wait for it).
FightTimeline buildTimelineList(const FightScript& s, const FighterTimes times[2], const std::vector<std::vector<EffectTimes>>& effectsByStrike, int panMs = -1,
                                int panStartMs = -1);
// At most one effect per strike (EffectTimes::has false = none): the form the first scene used.
FightTimeline buildTimeline(const FightScript& s, const FighterTimes times[2], const std::vector<EffectTimes>& effectByStrike);

}  // namespace ote
