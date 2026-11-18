// The side-view battle scene (docs/BATTLE_SCENE.md): a full-screen 2D overlay that replays the events of one fight with the
// two fighters drawn from their animation sets (or the enlarged map picture with simple motion). Independent of the map's
// kind of view. Presentation only: it reads the fight script and the timeline (fight_script.h, fight_timeline.h) and
// changes nothing in the rules or the view; BattleScene tells the view when it is done (BattleView::fightFinished).
//   fight_scene.cpp       preparing a fight (art lookup, script, timeline), the clock, the beats (log lines, sounds, floats)
//   fight_scene_draw.cpp  drawing: layout for landscape and portrait, background, fighters, boxes, effects
//   fight_scene_stage.cpp the optional fixed stage (docs/BATTLE_SCENE.md "Fixed stage"): layout, backdrop and platforms
#pragma once
#include <map>
#include <string>
#include <vector>

#include "battle/battle_view.h"
#include "battle/fight_timeline.h"
#include "battle/sprites.h"
#include "core/battle/battle.h"
#include "platform/platform.h"

namespace ote {

class GameAudio;

class FightScene {
 public:
  // What a scene needs from the screen that shows it.
  struct Env {
    const Battle* battle = nullptr;
    const BattleView* view = nullptr;   // the hit points shown when the fight starts
    const Sprites* sprites = nullptr;   // textures of the game's sheets (null = placeholders only)
    int styleIndex = 0;                 // the visual style in use (the fallback picture is the map picture of that style)
    const MapDef* map = nullptr;        // the map being played: its own background and platform tables (core/data/battle_pick.h)
    GameAudio* audio = nullptr;         // null = silent
    const std::map<std::pair<int, int>, Forecast>* forecasts = nullptr;  // by (attacker, defender), taken before the fight
    // Per unit: levels (and maximum hit points) the fight's experience adds after it (BattleView::FightRun).
    std::map<int, int> levelsGained, hpGained;
  };

  // Prepares the fight of `run` (events for which belongsToFight held). False (and nothing active) when the run holds no strike.
  bool start(const Env& env, const std::vector<Event>& run);
  bool active() const { return active_; }
  bool finished() const { return active_ && t_ >= tl_.total; }
  void update(double dt);
  // Skip: the rest of the fight is over at once (its log lines are still written, in order).
  void skip();
  void close() { active_ = false; floats_.clear(); }
  void draw(Renderer& r) const;

  // For tests of the drawing and the layout.
  const FightScript& script() const { return script_; }
  const FightTimeline& timeline() const { return tl_; }

 private:
  struct Fighter {
    int unit = -1;
    int set = -1;                 // index into Graphics::battle.sets, -1 = the map picture
    std::string setName;
    std::string sideId;           // the map side the fighter belongs to (picks its set's per-side sheets)
    std::string name, weapon;
    int level = 1, maxHp = 1, startHp = 1;
    int hit = -1, crit = -1;      // the forecast percentages when the game has them to show (-1 = none)
    int type = -1;
    Color color;                  // the side's colour
    char glyph = '?';
    const Look* look = nullptr;   // the fallback: the unit type's map picture (null = a coloured block with its letter)
    FighterTimes times;
  };
  struct Float {
    std::string text;
    int side = 0;
    double born = 0, life = 1.0;
    Color color;
    bool plate = false;  // a skill label: on a dark plate
    int rise = 0;        // extra height in design pixels (stacked labels)
  };

  void fire(const FightBeat& b, bool quiet);
  void addFloat(const std::string& text, int side, Color c, double life, bool plate = false, int rise = 0);
  const BattleSet* setOf(int side) const;
  // The sheet a fighter draws an animation from: its side's own sheet when the set gives one (docs/BATTLE_SCENE.md "By side").
  int sheetOf(int side, const BattleAnim& a) const {
    const int s = a.sheetFor(fighter_[side].sideId);
    return env_.sprites && env_.sprites->texture(s) ? s : a.sheet;  // (a side sheet that failed to load: the set's own)
  }

  // fight_scene_draw.cpp
  struct Geo;  // positions for the size of the screen
  Geo geometry(const Renderer& r) const;
  void stageLayout(Geo& g) const;                      // fight_scene_stage.cpp: the fixed stage's layout
  void drawStageBackground(Renderer& r, const Geo& g) const;
  void drawStagePlatform(Renderer& r, const Geo& g, int side) const;
  void drawBackground(Renderer& r, const Geo& g, unsigned hidden, uint32_t hideRgb) const;
  void drawFades(Renderer& r, const Geo& g, bool backdropOnly) const;  // the effects' "fade" windows
  void drawPlatform(Renderer& r, const Geo& g, int side) const;
  void drawFighter(Renderer& r, const Geo& g, int side) const;
  void drawEffects(Renderer& r, const Geo& g, FxLayer layer) const;  // the effects of one layer (docs/BATTLE_SCENE.md "Effect layers")
  void drawEffectPicture(Renderer& r, const BattleEffect& be, TextureId tex, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh) const;  // blend mode
  void drawScreenEffect(Renderer& r, const Geo& g, const BattleEffect& be, const FightFx& fx) const;
  void applyShake(Geo& g) const;                    // moves the stage by the effects' shakes at this moment
  void drawFlashes(Renderer& r, const Geo& g) const;  // the crit flash and the effects' flashes
  void drawBox(Renderer& r, const Geo& g, int side) const;
  void drawFloats(Renderer& r, const Geo& g) const;
  void drawRange(Renderer& r, const Geo& g) const;
  int bgIndex() const;

  Env env_;
  bool active_ = false;
  FightScript script_;
  FightTimeline tl_;
  Fighter fighter_[2];
  std::vector<std::vector<int>> effectsOfStrike_;  // per strike: the indices into Graphics::battle.effects in the order they start
  double t_ = 0;
  size_t nextBeat_ = 0;
  std::vector<Float> floats_;
  int terrain_ = -1;                 // the terrain under the struck fighter (background and ground colour)
  int background_ = -1;
  int platform_ = -1;                // the picture under each fighter (index into Graphics::battle.platforms) or -1
  bool platformFar_ = false;         // the fight is at range 2+: the platform's `far` picture when it has one
  double flash_ = -10;               // when the last critical hit landed (white flash)
  bool skipped_ = false;
};

}  // namespace ote
