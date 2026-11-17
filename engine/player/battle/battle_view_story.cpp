// How the battle view plays the story entries of the event stream (docs/EVENTS.md "What the player
// gets"): Dialogue (waits for the player), Focus, UnitRemoved, UnitRelocated, SideChanged and
// ObjectiveChanged. UnitAdded shares the pop-in of UnitBuilt in battle_view.cpp.
// Each hook returns true when the entry was one of these (and so fully handled here).
#include <algorithm>

#include "battle/battle_view.h"
#include "core/base/log.h"
#include "core/data/scene_fx.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr float kDialogueAutoSeconds = 1.0f;  // autoplay: a line dismisses itself after this long
constexpr float kRemoveFade = 0.4f;
constexpr float kSlideSecPerCell = 0.05f;
constexpr int kSlideMaxCells = 8;             // a unit moved further than this is snapped there
constexpr float kFlashDur = 0.5f;
constexpr float kFocusDur = 0.5f;             // the camera has this long to arrive
constexpr float kNoticeDur = 2.4f;
constexpr float kWalkSecPerCell = 0.035f;     // a walked moveUnit goes at the speed of an ordinary move (battle_view.cpp kMoveSecPerCell)

}  // namespace

bool BattleView::beginStory(const Battle& b) {
  const Event& e = cur_.ev;
  switch (e.type) {
    case EventType::Dialogue: {
      dialogue.line = dialogueLine(b, e);
      ++dialogue.serial;
      // For the tests and for whoever debugs a story: which line reaches the player, in which order.
      OTE_INFO("dialogue: %s | %s%s", dialogue.line.name.c_str(), dialogue.line.text.c_str(), skipping_ ? " (skipped)" : "");
      // Staging (docs/EVENTS.md "Scene presentation"): a positioned line puts its speaker on the stage; the log line only
      // exists for those, so a game that does not use the keys logs exactly what it always did.
      const bool staged = stage.say(dialogue.line.key, dialogue.line.portrait, dialogue.line.position);
      if (dialogue.line.position != 0)
        OTE_INFO("scene: portrait %s %s", dialogue.line.key.c_str(), positionName(dialogue.line.position));
      dialogue.staged = staged;
      if (dialogue.line.narrate) OTE_INFO("scene: narrate");
      // An empty line has nothing to show; a skipped run passes every line without showing it.
      if (skipping_ || (dialogue.line.text.empty() && dialogue.line.name.empty())) {
        cur_.dur = 0;
        return true;
      }
      dialogue.active = true;
      // Everything after this entry waits: the view's queue does not advance until the player dismisses
      // the line (the scene calls skip()), or, in autoplay, until the timer runs out.
      cur_.dur = autoplay_ ? kDialogueAutoSeconds : 1e9f;
      return true;
    }
    case EventType::Choice: {
      // A question always waits for the player and is never skipped: a skipped run goes back to showing lines after it.
      dialogue.line = dialogueLine(b, e);
      dialogue.answers.clear();
      for (int t : e.data) dialogue.answers.push_back(b.storyText(t));
      ++dialogue.serial;
      dialogue.active = true;
      skipping_ = false;
      cur_.dur = 1e9f;  // until the scene has the answer (BattleScene::answerChosen calls skip())
      return true;
    }
    case EventType::Focus: {
      if (e.b >= 0 && (size_t)e.b < units.size() && units[(size_t)e.b].visible) {
        stickyX_ = units[(size_t)e.b].px + tile_ / 2.0f;
        stickyY_ = units[(size_t)e.b].py + tile_ / 2.0f;
      } else if (b.grid.valid(e.a) && !dimmed(e.a)) {  // (fog: the camera does not go to a square the viewer cannot see)
        cellPos(b, e.a, stickyX_, stickyY_);
        stickyX_ += tile_ / 2.0f;
        stickyY_ += tile_ / 2.0f;
      } else {
        cur_.dur = 0;
        return true;
      }
      stickyFocus_ = true;
      // "ms" / "wait" (docs/EVENTS.md "Scene presentation"): c = scroll milliseconds (-1 = not given: the usual half
      // second wait and quick ease), flags kFocusNoWait = the next entry does not wait. A skipped run jumps.
      focusMs = skipping_ ? 0 : e.c;
      ++focusSerial;
      cur_.dur = skipping_ ? 0.0f : (e.flags & kFocusNoWait) ? 0.0f : e.c >= 0 ? (float)e.c / 1000.0f : kFocusDur;
      if (e.c >= 0 || (e.flags & kFocusNoWait))
        OTE_INFO("scene: focus %d,%d %s ms%s", b.grid.x(e.a), b.grid.y(e.a), e.c >= 0 ? std::to_string(e.c).c_str() : "default",
                 (e.flags & kFocusNoWait) ? " (no wait)" : "");
      return true;
    }
    case EventType::SceneEffect: return beginSceneEffect(b);
    case EventType::UnitRemoved: cur_.dur = kRemoveFade; return true;
    case EventType::UnitRelocated: {
      UnitView& v = units[(size_t)e.a];
      if ((e.flags & kRelocateWalk) && e.data.size() >= 2) {  // "walk": true (docs/EVENTS.md): along the path, at the speed of a move
        std::vector<Cell> path(e.data.begin(), e.data.end());
        const float dur = std::max(0.02f, kWalkSecPerCell * (float)(path.size() - 1));
        OTE_INFO("scene: walk %s %d steps%s", b.units[(size_t)e.a].storyId.c_str(), (int)path.size() - 1,
                 (e.flags & kFocusNoWait) ? " (no wait)" : "");
        cellPos(b, path[0], v.px, v.py);
        if (skipping_) { cellPos(b, path.back(), v.px, v.py); cur_.dur = 0; return true; }
        if (e.flags & kFocusNoWait) {  // it keeps walking while the next entries play
          Walker w;
          w.unit = e.a;
          w.path = path;
          w.dur = dur;
          walkers_.push_back(w);
          cur_.dur = 0;
          return true;
        }
        cur_.path = path;
        cur_.dur = dur;
        return true;
      }
      cellPos(b, e.b, v.px, v.py);  // starts from the square it left
      int dist = b.grid.distance(e.b, e.c);
      cur_.path.assign({e.b, e.c});
      cur_.dur = dist > kSlideMaxCells ? 0.0f : std::max(0.12f, kSlideSecPerCell * (float)dist);
      return true;
    }
    case EventType::SideChanged: {
      UnitView& v = units[(size_t)e.a];
      v.side = e.c;
      v.flash = 1;
      if (e.c >= 0 && (size_t)e.c < b.sides.size())
        addFloat("Joins " + b.sides[(size_t)e.c].name, v.px + tile_ / 2.0f, v.py - 2, Color::rgb(b.sides[(size_t)e.c].color),
                 1.2f);
      cur_.dur = kFlashDur;
      return true;
    }
    case EventType::ObjectiveChanged:
      if (e.a < 0) {  // the objective was cleared: nothing to announce
        cur_.dur = 0;
        return true;
      }
      banner = "New objective";
      bannerSub = b.storyText(e.a);
      bannerColor = Color(70, 110, 200);
      cur_.dur = kNoticeDur;
      return true;
    default: return false;
  }
}

bool BattleView::progressStory(const Battle& b) {
  const Event& e = cur_.ev;
  float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  switch (e.type) {
    case EventType::Dialogue:
    case EventType::Choice:
    case EventType::ObjectiveChanged: return true;
    case EventType::Focus:
      hasFocus = true;
      focusX = stickyX_;
      focusY = stickyY_;
      return true;
    case EventType::UnitRemoved: units[(size_t)e.a].alpha = 1 - p; return true;
    case EventType::UnitRelocated: {
      UnitView& v = units[(size_t)e.a];
      if ((e.flags & kRelocateWalk) && cur_.path.size() >= 2) {  // a walked move: along its path with the walking frames
        walkStep(b, v, cur_.path, p, true);
        hasFocus = true;
        focusX = v.px + tile_ / 2.0f;
        focusY = v.py + tile_ / 2.0f;
        return true;
      }
      float x0, y0, x1, y1;
      cellPos(b, e.b, x0, y0);
      cellPos(b, e.c, x1, y1);
      v.px = x0 + (x1 - x0) * p;
      v.py = y0 + (y1 - y0) * p;
      hasFocus = true;
      focusX = v.px + tile_ / 2.0f;
      focusY = v.py + tile_ / 2.0f;
      return true;
    }
    case EventType::SideChanged: {
      UnitView& v = units[(size_t)e.a];
      v.flash = 1 - p;
      hasFocus = true;
      focusX = v.px + tile_ / 2.0f;
      focusY = v.py + tile_ / 2.0f;
      return true;
    }
    default: return false;
  }
}

bool BattleView::finishStory(const Battle& b) {
  const Event& e = cur_.ev;
  switch (e.type) {
    case EventType::Dialogue:
    case EventType::Choice: dialogue.active = false; dialogue.answers.clear(); return true;
    case EventType::Focus: return true;  // the camera stays (stickyFocus_) until the run is over
    case EventType::ObjectiveChanged:
      banner.clear();
      bannerSub.clear();
      return true;
    case EventType::UnitRemoved: {
      UnitView& v = units[(size_t)e.a];
      v.visible = false;
      v.hp = 0;
      v.alpha = 1;
      return true;
    }
    case EventType::UnitRelocated: {
      UnitView& v = units[(size_t)e.a];
      for (const Walker& w : walkers_)
        if (w.unit == e.a) return true;  // it is still walking in the background ("wait": false): its walker finishes it
      cellPos(b, e.c, v.px, v.py);
      v.walking = false;
      v.walkLogged = -1;
      return true;
    }
    case EventType::SceneEffect: stage.finishNow(); return true;  // the transition / wait / shake this entry started is over
    case EventType::SideChanged: {
      UnitView& v = units[(size_t)e.a];
      v.side = e.c;
      v.flash = 0;
      return true;
    }
    default: return false;
  }
}

// ---- scene presentation (docs/EVENTS.md "Scene presentation")

bool BattleView::beginSceneEffect(const Battle& b) {
  const Event& e = cur_.ev;
  const std::string text = e.b >= 0 ? b.storyText(e.b) : std::string();
  // A skipped run (and the simulator, which never gets here) takes no time: every transition is a cut.
  const int ms = skipping_ ? 0 : std::max(0, e.c);
  const char* note = skipping_ ? " (skipped)" : "";
  switch ((SceneFx::Kind)e.a) {
    case SceneFx::Kind::Background: {
      const bool fade = (e.flags & kSceneFadeTrans) != 0 && ms > 0;
      stage.setBackground(text, fade, ms);
      cur_.dur = fade ? (float)ms / 1000.0f : 0.0f;
      const std::string how = (e.flags & kSceneFadeTrans) ? " fade " + std::to_string(e.c) + " ms" : std::string(" cut");
      OTE_INFO("scene: background %s%s%s", text.empty() ? "(none)" : text.c_str(), how.c_str(), note);
      break;
    }
    case SceneFx::Kind::Fade: {
      const bool out = (e.flags & kSceneOut) != 0;
      stage.fade(out, (uint32_t)e.d, ms);
      cur_.dur = (float)ms / 1000.0f;
      OTE_INFO("scene: fade %s %d ms #%06x%s%s", out ? "out" : "in", e.c, (unsigned)e.d, (e.flags & kSceneAuto) ? " (automatic)" : "", note);
      break;
    }
    case SceneFx::Kind::Wait:
      cur_.dur = (float)ms / 1000.0f;
      OTE_INFO("scene: wait %d ms%s", e.c, note);
      break;
    case SceneFx::Kind::Shake:
      stage.shake(ms, e.d);
      cur_.dur = (float)ms / 1000.0f;
      OTE_INFO("scene: shake %d ms strength %d%s", e.c, e.d, note);
      break;
    case SceneFx::Kind::Leave: {
      const bool fade = (e.flags & kSceneFadeTrans) != 0 && ms > 0;
      stage.leave(text, fade ? ms : 0);
      cur_.dur = fade ? (float)ms / 1000.0f : 0.0f;
      OTE_INFO("scene: leave %s%s%s", text.empty() ? "(everybody)" : text.c_str(), fade ? " fade" : "", note);
      break;
    }
    case SceneFx::Kind::Enter: {  // a portrait on stage without a line (docs/EVENTS.md "Standing portraits")
      const bool fade = (e.flags & kSceneFadeTrans) != 0 && ms > 0;
      const std::string art = !e.data.empty() ? b.storyText(e.data[0]) : text;
      stage.enter(text, art, e.d, fade ? ms : 0);
      cur_.dur = fade ? (float)ms / 1000.0f : 0.0f;
      OTE_INFO("scene: enter %s %s%s%s", text.c_str(), positionName(e.d), fade ? " fade" : "", note);
      break;
    }
    case SceneFx::Kind::Move: {
      const bool slide = (e.flags & kSceneFadeTrans) != 0 && ms > 0;
      stage.move(text, e.d, slide ? ms : 0);
      cur_.dur = slide ? (float)ms / 1000.0f : 0.0f;
      OTE_INFO("scene: move %s %s%s%s", text.c_str(), positionName(e.d), slide ? "" : " cut", note);
      break;
    }
    case SceneFx::Kind::Weather:
      stage.setWeather(text);
      OTE_INFO("scene: weather %s", text.c_str());
      break;
    case SceneFx::Kind::None: break;
  }
  return true;
}

// One frame of a walk along `path` at progress p (0..1): the unit's place, the direction of the step under way (the walking
// frames follow it) and, for the walk the scene waits on, the footstep sound.
void BattleView::walkStep(const Battle& b, UnitView& v, const std::vector<Cell>& path, float p, bool sound) {
  const float f = p * (float)(path.size() - 1);
  const size_t i = std::min((size_t)f, path.size() - 1), j = std::min(i + 1, path.size() - 1);
  if (sound) soundStep(b, i);
  float x0, y0, x1, y1;
  cellPos(b, path[i], x0, y0);
  cellPos(b, path[j], x1, y1);
  if (j > i) {
    v.walkDx = x1 - x0;
    v.walkDy = y1 - y0;
  }
  v.walking = path.size() > 1;
  const float t = f - (float)i;
  v.px = x0 + (x1 - x0) * t;
  v.py = y0 + (y1 - y0) * t;
}

void BattleView::updateScene(double dt, const Battle& b) {
  stage.update(dt);
  for (size_t k = 0; k < walkers_.size();) {
    Walker& w = walkers_[k];
    w.t += (float)dt;
    const float p = std::min(1.0f, w.t / w.dur);
    UnitView& v = units[(size_t)w.unit];
    walkStep(b, v, w.path, p, false);
    if (p >= 1) {
      cellPos(b, w.path.back(), v.px, v.py);
      v.walking = false;
      v.walkLogged = -1;
      walkers_.erase(walkers_.begin() + (long)k);
    } else {
      ++k;
    }
  }
}

// The skip control: every fade, wait, shake, walk and scroll under way completes at once (the entries behind it then take no
// time either, see skipping_). The end state is the one a played scene reaches.
void BattleView::skipScene(const Battle& b) {
  stage.finishNow();
  for (const Walker& w : walkers_) {
    UnitView& v = units[(size_t)w.unit];
    cellPos(b, w.path.back(), v.px, v.py);
    v.walking = false;
    v.walkLogged = -1;
  }
  walkers_.clear();
  focusMs = 0;  // a scroll under way jumps
  ++focusSerial;
}

bool BattleView::inScene() const {
  if (!walkers_.empty()) return true;
  auto sceneEntry = [](const Event& e) { return e.type == EventType::SceneEffect || (e.type == EventType::UnitRelocated && (e.flags & kRelocateWalk)); };
  if (cur_.active && sceneEntry(cur_.ev)) return true;
  for (const Event& e : queue_)
    if (sceneEntry(e)) return true;
  return false;
}

}  // namespace ote
