// Art of the side-view battle scene (docs/BATTLE_SCENE.md): animation sets of fighters, per-weapon effects and
// backgrounds. Part of data/graphics.json (key "battle"). The core only describes it; the player draws it. Presentation
// only: nothing here is read by the rules, the saves or the simulator.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/data/sprite_cell.h"

namespace ote {

// The named animations of a fighter. Only `stand` is required; a missing one is played by the fallback motion of the
// stand picture (docs/BATTLE_SCENE.md "Fallback").
// The last four are optional extras that fall back (docs/BATTLE_SCENE.md "Which animation plays"): critRanged -> crit,
// dodgeRanged -> dodge, standRanged -> stand, miss (the attacker's whiff) -> attack.
enum BattleAnimId { BaStand, BaAttack, BaCrit, BaRanged, BaCast, BaDodge, BaHurt, BaDefeat, BaCritRanged, BaDodgeRanged, BaStandRanged, BaMiss, kBattleAnims };
const char* battleAnimKey(BattleAnimId id);  // "stand", "attack", ...
bool battleAnimFromKey(const std::string& key, BattleAnimId& out);

// One animation: cells of a sheet (the sheet's cell size is the frame size) with a time per frame and marked beats.
// An animation's frames list may hold this many cells (all of one sheet; a longer animation would not fit a 2048 pixel sheet
// of small cells anyway). Documented in docs/BATTLE_SCENE.md.
const int kMaxBattleFrames = 512;

struct BattleAnim {
  int sheet = -1;  // index into Graphics::sheets; -1 = this animation is not given
  // Per-side sheets: the fighter of that side (map side id) draws the same frames from another sheet of the same layout.
  // Already merged at load: a set-level "sides" entry is copied into every animation that has none of its own.
  std::vector<std::pair<std::string, int>> sideSheets;
  std::vector<SpriteCell> frames;
  std::vector<int> ms;  // per frame, milliseconds (same length as frames)
  bool loop = false;
  int hit = -1;    // frame on which the hit lands (-1 = the animation's last frame)
  int spawn = -1;  // frame on which a projectile / effect appears (-1 = none given)
  // Sounds at frames (docs/BATTLE_SCENE.md "Sounds"): { frame, sound id }, in frame order. The id is opaque here (presentation only);
  // the player checks it against data/audio.json and plays it when the frame begins.
  std::vector<std::pair<int, std::string>> sounds;

  // Effects only (docs/BATTLE_SCENE.md "Frames from several sheets"): the sheet of each frame, same length as `frames`, or empty = every
  // frame is from `sheet`. All the sheets have one cell size (checked at load).
  std::vector<int> frameSheets;
  int sheetOfFrame(int frame) const { return frame >= 0 && (size_t)frame < frameSheets.size() ? frameSheets[(size_t)frame] : sheet; }

  bool any() const { return sheet >= 0 && !frames.empty(); }
  // The sheet the fighter of `sideId` draws this animation from: its side's own sheet, else `sheet`.
  int sheetFor(const std::string& sideId) const {
    for (const auto& s : sideSheets)
      if (s.first == sideId) return s.second;
    return sheet;
  }
  int totalMs() const {
    int t = 0;
    for (int m : ms) t += m;
    return t;
  }
  // When frame `frame` starts, in ms from the start of the animation.
  int startMs(int frame) const {
    int t = 0;
    for (int i = 0; i < frame && i < (int)ms.size(); ++i) t += ms[(size_t)i];
    return t;
  }
  int hitFrame() const { return hit >= 0 ? hit : (int)frames.size() - 1; }
  // The frame showing at `t` ms (a looping animation wraps, another holds its last frame).
  int frameAt(int t) const;
};

struct BattleSet {
  std::string id;
  BattleAnim anims[kBattleAnims];
  int anchorX = -1, anchorY = -1;  // the foot point inside a frame, pixels from its top-left; -1 = bottom middle
  bool facesLeft = false;          // the art looks to the left (default: to the right)
  bool mirror = true;              // the fighter on the other side is the picture mirrored (false: never mirror)
};

// Where an effect is drawn (docs/BATTLE_SCENE.md "Effect layers"): over the fighters (today), behind both of them, or covering the whole stage.
enum class FxLayer { Front, Behind, Screen };

// A flash of colour over the whole scene / a shake of the stage, both timed from the start of the effect that owns them.
struct FxFlash {
  int atMs = 0, ms = 100;
  uint32_t rgb = 0xffffff;
};
struct FxShake {
  int atMs = 0, ms = 200;
  int strength = 2;  // stage / screen pixels before scaling
};

// Scene-wide parts of an effect (docs/BATTLE_SCENE.md "Hiding and fading"), timed from the start of the effect that owns them.
enum : unsigned { kHideBackdrop = 1, kHidePlatforms = 2 };
struct FxHide {                  // for `ms` from `atMs` the picked layers are not drawn; a hidden backdrop leaves the stage filled with `rgb` (black without one)
  int atMs = 0, ms = 100;
  unsigned what = 0;             // kHideBackdrop | kHidePlatforms
  bool hasColor = false;
  uint32_t rgb = 0;
};
struct FxFade {                  // a colour laid over the scene: in over `ms`, held `holdMs`, out over `ms`; peak opacity alpha / 255
  int atMs = 0, ms = 100, holdMs = 0;
  uint32_t rgb = 0;
  int alpha = 255;
  bool backdropOnly = false;     // "over": "backdrop" (else everything, the fighters too)
};
// Where a "screen" effect is drawn among the layers (docs/BATTLE_SCENE.md "Effect layers"): right after the backdrop (the default), with the
// "behind" layer (over the platforms, under the fighters), or over the fighters.
enum class FxOrder { Backdrop, Behind, Front };

// One step of an effect's "atRange" list: from a fight `from` squares apart, the whole effect starts `delayMs` later (docs/BATTLE_SCENE.md "Effect timing by distance").
struct FxRange {
  int from = 1, delayMs = 0;
};

// A spell or an arrow drawn over the fight for a weapon (or every weapon of a group). An entry with a weapon / group and no pictures
// only assigns the effects named in `list` / `byType`; an entry with an `id` and no weapon / group is a library effect that lists use.
struct BattleEffect {
  std::string id;    // optional name, used by the lists of other entries and in the log
  int weapon = -1;   // index into Database::weapons, or -1 when `group` (or `item`) is used
  int item = -1;     // index into Database::items: a STAFF item named by "weapon" (docs/BATTLE_SCENE.md "Staves in the scene"), else -1
  std::string group; // weapon group
  BattleAnim anim;
  bool travel = false;  // true: flies from the striker to the target; false: appears on the target
  int travelMs = 250;
  int anchorX = -1, anchorY = -1;  // the point of a frame that sits on the path / target (-1 = its middle)
  // The newer keys (all optional; an entry that sets none of them plays exactly as before).
  std::vector<std::pair<int, std::string>> sounds;  // { ms after the effect starts, sound id }, in time order
  int hitAtMs = 0;                                  // the effect starts this long before the hit lands (a "target" or "striker" effect)
  FxLayer layer = FxLayer::Front;
  bool mirror = false;               // layer "screen": turned round when the striker stands on the right
  std::vector<FxFlash> flash;
  std::vector<FxShake> shake;
  bool atStriker = false;            // "at": "striker": drawn on the striker, not the target (not for travel effects)
  bool returns = false;              // travel: flies back to the striker after arriving
  int lingerMs = 0;                  // travel: stays at the target this long after arriving
  std::vector<int> list;             // assignment: indices of further effects that play with this one (after its own pictures)
  std::vector<std::pair<int, std::vector<int>>> byType;  // assignment: striker's unit type index -> the effects that replace this entry's own
  std::vector<FxHide> hide;          // "hide": layers not drawn for a while
  std::vector<FxFade> fade;          // "fade": a colour over the scene or the backdrop
  FxOrder order = FxOrder::Backdrop; // "order" of a "screen" effect
  int speed = 0;                     // travel: "speed" in stage pixels per second instead of "travelMs" (0 = travelMs; needs a fixed stage)
  std::vector<FxRange> atRange;      // "atRange": delay of the whole effect by fight distance, sorted by `from`
  int endOffset = 0;                 // travel: "endOffset": the flight stops this many stage pixels short of the target
  bool additive = false;             // "blend": "add": its pixels brighten what is under them (docs/BATTLE_SCENE.md "Blending")
  bool extended = false;             // any of the newer keys was given (it then also logs its start)
  // The delay for a fight `distance` squares apart: the band with the largest `from` that is not above it, 0 when none fits.
  int delayAt(int distance) const {
    int d = 0;
    for (const FxRange& r : atRange)
      if (r.from <= distance) d = r.delayMs;
    return d;
  }
  bool assignsOnly() const { return !anim.any(); }
};

// Which set a fighter uses. The most specific rule that fits wins (the order is in docs/BATTLE_SCENE.md and BattleArt::resolveSet).
struct BattleAssign {
  int set = -1;
  int character = -1;  // index into Database::characters, or -1
  int type = -1;       // index into Database::unitTypes, or -1
  std::string group;   // weapon group of the weapon in use, or empty
  int weapon = -1;     // index into Database::weapons: that very weapon in use, or -1 (a rule names a weapon or a group, not both)
  std::string side;    // map side id of the fighter, or empty
};

struct BattleBackground {
  std::string id;
  int sheet = -1;
  SpriteCell cell;
};

// A picture drawn under each fighter of the battle scene (docs/BATTLE_SCENE.md "Platforms"); `far` is the picture used when the fight is at range 2+.
struct BattlePlatform {
  std::string id;
  int sheet = -1;
  SpriteCell cell;
  bool hasFar = false;
  SpriteCell farCell;
  // The right-hand fighter's own pictures (a fixed stage only): else the left one drawn mirrored (docs/BATTLE_SCENE.md "Fixed stage").
  bool hasRight = false, hasFarRight = false;
  SpriteCell rightCell, farRightCell;
};

// An optional fixed stage (docs/BATTLE_SCENE.md "Fixed stage"): the scene is laid out in the stage's own pixels and drawn at one whole-number
// scale. Points are [x, y] in stage pixels; index 0 = the left fighter, 1 = the right one.
// An extra layout of the fixed stage for fights from `from` squares apart (docs/BATTLE_SCENE.md "Range bands"). With `each` the stage shows one fighter at a
// time (the camera follows the acting fighter): points are then in a world wider than the stage and `leftView` / `rightView` are the world x of the
// stage's left edge while the camera is on the left / right fighter.
struct StageBand {
  int from = 2;
  int feet[2][2] = {{0, 0}, {0, 0}};
  bool hasPlatform = false;
  int platform[2][2] = {{0, 0}, {0, 0}};
  bool each = false;                // "view": "each"
  int panMs = 250, leftView = 0, rightView = 0;
  bool scrollBackdrop = false;      // pan "backdrop": "scroll": the backdrop moves with the camera and wraps (default "fixed")
  int panStartMs = -1;              // pan "startMs": the pan toward the struck fighter starts this long after the strike starts (-1 = timed to arrive at the hit)
};

struct BattleStage {
  bool on = false;
  std::vector<StageBand> ranges;           // sorted by `from`
  int width = 0, height = 0;
  int feet[2][2] = {{0, 0}, {0, 0}};      // where a fighter's anchor (foot point) stands, close
  bool hasFeetFar = false;
  int feetFar[2][2] = {{0, 0}, {0, 0}};   // at range 2+ (default: feet)
  bool hasPlatform = false;
  int platform[2][2] = {{0, 0}, {0, 0}};  // top-left of the platform picture, close (default: centred on the foot point)
  bool hasPlatformFar = false;
  int platformFar[2][2] = {{0, 0}, {0, 0}};
  enum Align { AlignTop, AlignCenter, AlignBottom } align = AlignCenter;  // the part that stays visible when the stage is cropped
  bool contain = false;                    // false = "cover", true = "contain"
  bool rangeLine = true;                   // the dotted line and label of a ranged fight
};

struct BattleArt {
  std::vector<BattleSet> sets;
  std::vector<BattleEffect> effects;
  std::vector<BattleAssign> assign;
  std::vector<BattleBackground> backgrounds;
  std::vector<int> terrainBackground;  // per terrain index: index into `backgrounds`, -1 = none
  int defaultBackground = -1;
  std::vector<BattlePlatform> platforms;
  std::vector<int> terrainPlatform;  // per terrain index: index into `platforms`, -1 = none
  int defaultPlatform = -1;
  BattleStage stage;  // stage.on = a fixed stage is declared

  int findPlatform(const std::string& id) const;
  bool any() const { return !sets.empty() || !backgrounds.empty(); }
  int findSet(const std::string& id) const;
  int findBackground(const std::string& id) const;
  // The set for a fighter: index into `sets`, -1 = none (the fallback picture is used). `character` / `type` as in
  // BattleAssign, `group` is the weapon group and `weapon` the index of the weapon in use ("" / -1 = none), `side` the
  // fighter's map side id. Best to worst: character+weapon, character+group, character (with a type too: a notch
  // higher), type+weapon, type+group, weapon, group, type, side alone. A `side` on a rule only breaks a tie between
  // rules of the same level (it never lifts a rule over a more specific one).
  int resolveSet(int character, int type, const std::string& group, int weapon = -1, const std::string& side = std::string()) const;
  // The effect for a weapon: the weapon's own entry, else its group's; -1 = none.
  // `item`: the staff item in use (a staff use in the scene; -1 = none): an entry for that very item beats the entry of its group.
  int resolveEffect(int weapon, const std::string& group, int item = -1) const;
  // The effects of a strike in the order they start: the entry for the weapon (else its group), then for a striker of unit type `type`
  // its byType list when it has one, else its own pictures followed by its list (docs/BATTLE_SCENE.md "Several effects, by unit type").
  std::vector<int> resolveEffects(int weapon, const std::string& group, int type, int item = -1) const;
  int findEffect(const std::string& id) const;
};

}  // namespace ote
