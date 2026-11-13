// View model for the battle screen. Battle::apply changes the rules state
// instantly; what is DRAWN comes from here, and this only changes by
// playing the Event queue in order (so the screen shows the fight as it
// unfolds). After the queue drains the view is re-synced from the Battle as
// a safety net.
#pragma once
#include <deque>
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "platform/platform.h"

namespace ote {

struct UnitView {
  float px = 0, py = 0;  // top-left of the unit's tile in map pixels (animated)
  float ox = 0, oy = 0;  // extra offset (attack lunge)
  int hp = 0;            // displayed hit points
  float alpha = 1;       // fades out on death
  float scale = 1;       // pops in when built
  bool visible = true;
};

struct FloatText {
  std::string text;
  float x = 0, y = 0;  // map pixels (centre x, top y), or screen pixels if `screen`
  float age = 0, life = 0.9f;
  Color color;
  bool screen = false;
};

struct LevelUpPanel {
  bool active = false;
  bool autoContinue = false;
  float timer = 0;
  std::string title;
  std::vector<std::string> lines;
};

class BattleView {
 public:
  void init(const Battle& b, int tileSize, bool autoplay);
  // Queue events produced by Battle::apply / start / aiTakeStep.
  void push(const std::vector<Event>& events);
  bool busy() const { return cur_.active || !queue_.empty(); }
  void update(double dt, const Battle& b);
  // Finishes the event currently playing (Confirm/Cancel/tap while animating).
  void skip(const Battle& b);
  // Snap everything to the Battle's real state (undo, safety net).
  void resync(const Battle& b);
  // Screen position where "+1000 Funds" floats spawn (the panel's resource area).
  void setResourceAnchor(int x, int y) { resX_ = x; resY_ = y; }

  std::vector<UnitView> units;  // indexed by unit id
  std::vector<FloatText> floats;
  LevelUpPanel levelUp;
  std::string banner;  // centred banner text, empty = none
  Color bannerColor;
  // While an event involving a unit plays, the camera should follow it.
  bool hasFocus = false;
  float focusX = 0, focusY = 0;  // map pixels (centre)

 private:
  struct Current {
    bool active = false;
    Event ev;
    float t = 0, dur = 0;
    bool fired = false;  // one-shot part of the event (attack impact) done
    std::vector<Cell> path;
    int hpFrom = 0, hpTo = 0;
    float dirX = 0, dirY = 0;  // attack lunge direction
  };
  void ensureUnits(const Battle& b);
  void startNext(const Battle& b);
  void begin(const Battle& b);     // sets up cur_ from its event
  void progress(const Battle& b);  // applies cur_ at time t
  void finish(const Battle& b);    // final state of cur_
  void impact(const Battle& b);    // attack: spawn numbers
  void addFloat(const std::string& text, float x, float y, Color c, float life = 0.9f, bool screen = false);
  void cellPos(const Battle& b, Cell c, float& x, float& y) const;
  bool showsResources(const Battle& b, int side) const;

  Current cur_;
  std::deque<Event> queue_;
  int tile_ = 24;
  bool autoplay_ = false;
  bool wasBusy_ = false;
  int resX_ = 0, resY_ = 0;
  int resFloats_ = 0;  // stagger counter for resource floats
};

// Side whose resources the panel shows: the active side if it is human,
// else the first human side (autoplay: the active side).
int viewSide(const Battle& b, bool autoplay);

}  // namespace ote
