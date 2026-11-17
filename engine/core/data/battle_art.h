// Art of the side-view battle scene (docs/BATTLE_SCENE.md): animation sets of fighters, per-weapon effects and
// backgrounds. Part of data/graphics.json (key "battle"). The core only describes it; the player draws it. Presentation
// only: nothing here is read by the rules, the saves or the simulator.
#pragma once
#include <string>
#include <vector>

#include "core/data/sprite_cell.h"

namespace ote {

// The named animations of a fighter. Only `stand` is required; a missing one is played by the fallback motion of the
// stand picture (docs/BATTLE_SCENE.md "Fallback").
enum BattleAnimId { BaStand, BaAttack, BaCrit, BaRanged, BaCast, BaDodge, BaHurt, BaDefeat, kBattleAnims };
const char* battleAnimKey(BattleAnimId id);  // "stand", "attack", ...
bool battleAnimFromKey(const std::string& key, BattleAnimId& out);

// One animation: cells of a sheet (the sheet's cell size is the frame size) with a time per frame and marked beats.
struct BattleAnim {
  int sheet = -1;  // index into Graphics::sheets; -1 = this animation is not given
  std::vector<SpriteCell> frames;
  std::vector<int> ms;  // per frame, milliseconds (same length as frames)
  bool loop = false;
  int hit = -1;    // frame on which the hit lands (-1 = the animation's last frame)
  int spawn = -1;  // frame on which a projectile / effect appears (-1 = none given)

  bool any() const { return sheet >= 0 && !frames.empty(); }
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

// A spell or an arrow drawn over the fight for a weapon (or every weapon of a group).
struct BattleEffect {
  int weapon = -1;   // index into Database::weapons, or -1 when `group` is used
  std::string group; // weapon group
  BattleAnim anim;
  bool travel = false;  // true: flies from the striker to the target; false: appears on the target
  int travelMs = 250;
  int anchorX = -1, anchorY = -1;  // the point of a frame that sits on the path / target (-1 = its middle)
};

// Which set a fighter uses. The most specific rule that fits wins: character > weapon group > class (docs/BATTLE_SCENE.md).
struct BattleAssign {
  int set = -1;
  int character = -1;  // index into Database::characters, or -1
  int type = -1;       // index into Database::unitTypes, or -1
  std::string group;   // weapon group of the weapon in use, or empty
};

struct BattleBackground {
  std::string id;
  int sheet = -1;
  SpriteCell cell;
};

struct BattleArt {
  std::vector<BattleSet> sets;
  std::vector<BattleEffect> effects;
  std::vector<BattleAssign> assign;
  std::vector<BattleBackground> backgrounds;
  std::vector<int> terrainBackground;  // per terrain index: index into `backgrounds`, -1 = none
  int defaultBackground = -1;

  bool any() const { return !sets.empty() || !backgrounds.empty(); }
  int findSet(const std::string& id) const;
  int findBackground(const std::string& id) const;
  // The set for a fighter: index into `sets`, -1 = none (the fallback picture is used). `character` / `type` as in
  // BattleAssign, `group` is the weapon group of the weapon in use ("" = none). Priority: character with the weapon
  // group, character, unit type with the group, the group alone, unit type alone.
  int resolveSet(int character, int type, const std::string& group) const;
  // The effect for a weapon: the weapon's own entry, else its group's; -1 = none.
  int resolveEffect(int weapon, const std::string& group) const;
};

}  // namespace ote
