// The player's state for story scene presentation (docs/EVENTS.md "Scene presentation"): the background picture and its
// transition, the fade, the shake, the weather and the portraits standing on the stage. Pure state with no drawing (so it can
// be tested): the battle view and the campaign's scene screen each own one, feed it the commands in order and call update();
// scene_stage_draw.cpp draws it. Time-taking commands (fade, wait, background transition, shake) block the commands behind
// them in the owner's queue; the stage itself only animates the result. finishNow() is the skip: everything under way
// completes at once, so the final state is exactly the one a played scene reaches.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ote {

class SceneStage {
 public:
  struct Portrait {
    std::string key;  // the speaker: a unit's story id, or the free name of the line
    std::string art;  // the portrait id to look up in the graphics
    int position = 0; // kPos* (scene_fx.h)
    // Enter / leave / move transitions (docs/EVENTS.md "Standing portraits"): fading in or out, sliding from another slot.
    float alpha = 1;          // 0..1
    float alphaSpeed = 0;     // per second; 0 = at rest
    bool leaving = false;     // fading out: removed when alpha reaches 0
    float fromSlot = 0;       // while sliding: the slot (0..6) it slid from
    float slideT = 1;         // 0..1 of the slide; 1 = at rest
    float slideSpeed = 0;
    // The slot to draw it at right now (fractional while sliding), 0..6 (stageSlot()).
    float slotNow() const;
  };

  // ---- commands
  // A picture over the map / scene ("" = take it away). `fade`: cross-fade over `ms` instead of a cut.
  void setBackground(const std::string& id, bool fade, int ms);
  void fade(bool out, uint32_t color, int ms);          // to the colour (out) or back (in); stays until faded in
  void shake(int ms, int strength);
  void setWeather(const std::string& kind);             // "rain" | "snow" | "" (none)
  // A line spoken by `key`. position 0 = the classic portrait beside the box (nothing staged, unless `key` already
  // stands on the stage: then that portrait lights up). Returns true when the line's speaker stands on the stage (the
  // box then draws no portrait of its own).
  bool say(const std::string& key, const std::string& art, int position);
  // `leave` with `ms` > 0 fades the portrait out first ("" = everybody); 0 = at once.
  void leave(const std::string& key, int ms = 0);
  // A portrait on stage without a line (docs/EVENTS.md "Standing portraits"): `fadeMs` > 0 fades it in. It is not lit.
  void enter(const std::string& key, const std::string& art, int position, int fadeMs);
  // Slides a portrait on stage to another slot over `ms` (0 = jumps); one that is not on stage is ignored.
  // Whoever stands in the new slot leaves (one portrait per place).
  void move(const std::string& key, int position, int ms);
  void clearPortraits();
  // What a resumed battle / a resync shows: the saved background and weather at once, nothing mid-way, nobody on stage.
  void syncFrom(const std::string& background, const std::string& weather);

  // ---- time
  void update(double dt);
  // Skip: transitions complete, the shake ends, the fade reaches its target. (Weather keeps falling.)
  void finishNow();

  // ---- state (drawing and tests)
  const std::string& background() const { return bg_; }      // the picture the scene is heading for ("" = none)
  const std::string& previousBackground() const { return prev_; }  // the one still fading out ("" = none)
  float backgroundProgress() const { return bgT_; }          // 0..1 of the transition; 1 = settled
  bool backgroundShown() const { return !bg_.empty() || !prev_.empty(); }  // something covers the map
  float fadeAmount() const { return fadeAmt_; }              // 0 = clear .. 1 = the whole screen in fadeColor()
  bool fadedOut() const { return fadeOut_; }                 // the fade's target is "out" (stays until faded in)
  bool fading() const { return fadeAmt_ != (fadeOut_ ? 1.0f : 0.0f); }
  uint32_t fadeColor() const { return color_; }
  const std::string& weather() const { return weather_; }
  float clock() const { return clock_; }                     // seconds, for the weather's movement
  bool shaking() const { return shakeLeft_ > 0; }
  int shakeX() const;                                        // the picture's current offset in logical pixels
  int shakeY() const;
  const std::vector<Portrait>& portraits() const { return portraits_; }
  const std::string& speaker() const { return speaker_; }    // the lit portrait (the others are dimmed)
  bool staged(const std::string& key) const;
  // True while any of the four added slots (farLeft, midLeft, midRight, farRight) is occupied:
  // the layout is then the wide one (scene_stage_layout.h).
  bool wide() const;
  // Anything still moving (a transition, a fade, a shake).
  bool animating() const;

 private:
  std::string bg_, prev_;
  float bgT_ = 1, bgSpeed_ = 0;  // progress and its speed per second (0 = instant)
  float fadeAmt_ = 0, fadeSpeed_ = 0;
  bool fadeOut_ = false;
  uint32_t color_ = 0;
  float shakeLeft_ = 0, shakeTotal_ = 0;  // seconds
  int strength_ = 0;
  float shakeElapsed_ = 0;
  std::string weather_;
  float clock_ = 0;
  std::vector<Portrait> portraits_;
  std::string speaker_;
};

}  // namespace ote
