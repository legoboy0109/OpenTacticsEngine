#include "campaign/story_scene.h"

#include <algorithm>

#include "campaign/end_scenes.h"
#include "core/base/log.h"
#include "ui/chrome.h"
#include "ui/dialogue_portrait.h"
#include "ui/text.h"

namespace ote {

namespace {
constexpr float kCardSeconds = 2.5f;      // the title card leaves by itself after this (Confirm is quicker)
constexpr float kAutoplayCard = 1.0f;     // --autoplay: card and lines pass by themselves
constexpr float kAutoplayLine = 1.0f;
constexpr float kSceneHoldToSkip = 0.5f;  // Cancel / Menu held this long while a fade / wait plays skips the scene
}  // namespace

StoryScene::StoryScene(SceneHost& host, const std::string& heading, const std::string& title,
                       const std::vector<SceneLine>& lines, SceneResolver resolve)
    : host_(host), heading_(heading), title_(title), lines_(lines), resolve_(std::move(resolve)) {
  bg_ = storyBackground(host);
  consumeAudio(false);  // music asked for at the very top (a scene's "music") starts with the card
  onCard_ = hasCard();
  if (!onCard_) openLine();
}

const StageDrawCtx StoryScene::stageCtx() const {
  return StageDrawCtx{host_.session()->db, host_.sprites(), std::max(0, host_.styleIndex())};
}

// Sound requests (docs/SOUND.md) sit between the lines: they act when reached. Before the scene has been on screen
// for a frame the music director does not know about it yet, so they wait for the first update(). Scene presentation
// lines (Fx) act on the scene's own stage; they are left alone until the card is gone (`fx` = false).
void StoryScene::consumeAudio(bool fx) {
  using K = SceneLine::Kind;
  while (next_ < lines_.size() && lines_[next_].kind != K::Say && lines_[next_].kind != K::Choice) {
    if (lines_[next_].kind == K::Fx) {
      if (!fx) return;
      const SceneLine& l = lines_[next_++];
      applyFx(l, false);
      if (blockLeft_ > 0) return;  // a fade / wait / shake / transition holds the scene: the lines behind it come after
      continue;
    }
    const SceneLine& l = lines_[next_++];
    if (!entered_) queued_.push_back(l);
    else playAudio(l);
  }
}

void StoryScene::playAudio(const SceneLine& l) {
  GameAudio& a = host_.gameAudio();
  if (l.kind == SceneLine::Kind::PlayMusic) a.music().story(l.text, l.fadeMs);
  else if (l.kind == SceneLine::Kind::StopMusic) a.music().story("", l.fadeMs);
  else if (l.kind == SceneLine::Kind::PlaySound) a.sound(l.text);
}

// A scene command (docs/EVENTS.md "Scene presentation"). The time it takes holds the scene back (`blockLeft_`), except under
// --autoplay (smoke tests never wait) and when `instant` (the skip control: the end state is the same, only the time is gone).
void StoryScene::applyFx(const SceneLine& l, bool instant) {
  const bool quick = instant || host_.args().autoplay;
  const int ms = quick ? 0 : std::max(0, l.fadeMs);
  const char* note = instant ? " (skipped)" : "";
  switch (l.fx) {
    case SceneFx::Kind::Background: {
      const bool fade = l.fadeTransition && ms > 0;
      stage_.setBackground(l.text, fade, ms);
      if (fade) blockLeft_ = (float)ms / 1000.0f;
      const std::string how = l.fadeTransition ? " fade " + std::to_string(l.fadeMs) + " ms" : std::string(" cut");
      OTE_INFO("scene: background %s%s%s", l.text.empty() ? "(none)" : l.text.c_str(), how.c_str(), note);
      break;
    }
    case SceneFx::Kind::Fade:
      stage_.fade(l.out, l.color, ms);
      if (ms > 0) blockLeft_ = (float)ms / 1000.0f;
      OTE_INFO("scene: fade %s %d ms #%06x%s", l.out ? "out" : "in", l.fadeMs, (unsigned)l.color, note);
      break;
    case SceneFx::Kind::Wait:
      if (ms > 0) blockLeft_ = (float)ms / 1000.0f;
      OTE_INFO("scene: wait %d ms%s", l.fadeMs, note);
      break;
    case SceneFx::Kind::Shake:
      stage_.shake(ms, l.strength);
      if (ms > 0) blockLeft_ = (float)ms / 1000.0f;
      OTE_INFO("scene: shake %d ms strength %d%s", l.fadeMs, l.strength, note);
      break;
    case SceneFx::Kind::Leave: {
      const bool fade = l.fadeTransition && ms > 0;
      stage_.leave(l.text, fade ? ms : 0);
      if (fade) blockLeft_ = (float)ms / 1000.0f;
      OTE_INFO("scene: leave %s%s%s", l.text.empty() ? "(everybody)" : l.text.c_str(), fade ? " fade" : "", note);
      break;
    }
    case SceneFx::Kind::Enter: {  // a portrait on stage without a line (docs/EVENTS.md "Standing portraits")
      const bool fade = l.fadeTransition && ms > 0;
      stage_.enter(l.text, l.portrait.empty() ? l.text : l.portrait, l.position, fade ? ms : 0);
      if (fade) blockLeft_ = (float)ms / 1000.0f;
      OTE_INFO("scene: enter %s %s%s%s", l.text.c_str(), positionName(l.position), fade ? " fade" : "", note);
      break;
    }
    case SceneFx::Kind::Move: {
      const bool slide = l.fadeTransition && ms > 0;
      stage_.move(l.text, l.position, slide ? ms : 0);
      if (slide) blockLeft_ = (float)ms / 1000.0f;
      OTE_INFO("scene: move %s %s%s%s", l.text.c_str(), positionName(l.position), slide ? "" : " cut", note);
      break;
    }
    case SceneFx::Kind::Weather:  // (map events only: a scene never gets one)
    case SceneFx::Kind::None: break;
  }
  if (instant) stage_.finishNow();
}

// The skip control (docs/EVENTS.md "Scene presentation"): the rest of the scene takes effect at once. Music changes and the
// presentation state (background, fades, who stands where) end exactly as if played; one-shot sounds are not state and stay
// silent. Lines [from, to) only.
void StoryScene::applyRest(size_t from, size_t to) {
  for (size_t i = from; i < to && i < lines_.size(); ++i) {
    const SceneLine& l = lines_[i];
    if (l.kind == SceneLine::Kind::Fx) applyFx(l, true);
    else if (l.kind == SceneLine::Kind::PlayMusic || l.kind == SceneLine::Kind::StopMusic) {
      if (!entered_) queued_.push_back(l);
      else playAudio(l);
    } else if (l.kind == SceneLine::Kind::Say) {  // who is standing where when the scene ends
      stage_.say(l.speakerId.empty() ? l.speaker : l.speakerId, l.portrait.empty() ? l.speakerId : l.portrait, l.position);
    }
  }
  blockLeft_ = 0;
}

void StoryScene::openLine() {
  if (blockLeft_ > 0) return;  // a fade / wait / shake is under way: update() comes back here when it is over
  consumeAudio(true);
  if (blockLeft_ > 0) return;
  if (next_ >= lines_.size()) { finish(); return; }
  const SceneLine& l = lines_[next_++];
  // A question that was answered already (the scene was run again with the answers) is not shown twice.
  if (l.kind == SceneLine::Kind::Choice && !l.waiting) { openLine(); return; }
  DialogueLine d;
  d.name = l.narrate ? std::string() : l.speaker;
  d.text = l.text;
  d.portrait = l.portrait.empty() ? l.speakerId : l.portrait;  // like a battle line: the speaker's own picture
  d.narrate = l.narrate;
  d.autoMs = l.autoMs;
  // Staging (docs/EVENTS.md "Scene presentation"): a speaker on the stage has no portrait beside the box.
  const bool staged = l.kind == SceneLine::Kind::Say && !l.narrate &&
                      stage_.say(l.speakerId.empty() ? l.speaker : l.speakerId, d.portrait, l.position);
  if (l.kind == SceneLine::Kind::Say && l.narrate) stage_.say(std::string(), std::string(), 0);
  if (l.position != 0)
    OTE_INFO("scene: portrait %s %s", (l.speakerId.empty() ? l.speaker : l.speakerId).c_str(), positionName(l.position));
  if (l.narrate) OTE_INFO("scene: narrate");
  const Renderer& r = host_.renderer();
  int style = std::max(0, std::min(host_.styleIndex(), (int)host_.session()->db.styles.size() - 1));
  box_.setSkipInside(!stage_.portraits().empty());
  box_.open(r, r.width(), r.height(), r.height() - footerHeight(r), d,
            staged || l.narrate ? PortraitArt() : portraitArt(host_.session()->db, host_.sprites(), style, d.portrait));
  if (l.kind == SceneLine::Kind::Choice) {
    box_.setAnswers(r, l.answers);
    OTE_INFO("choice: scene question \"%s\": %s", l.text.c_str(), box_.describeAnswers().c_str());
  }
  lineTime_ = 0;
}

// The answer goes back to the game: the scene is run again from before it started with every pick so far, which hands
// back the same lines and then the ones the answer leads to (next_ already stands behind the question).
void StoryScene::answered(int pick) {
  picks_.push_back(pick);
  OTE_INFO("choice: scene answer %d", pick);
  std::vector<SceneLine> again;
  if (!resolve_ || !resolve_(picks_, again)) {
    OTE_WARN("campaign: the scene could not go on after an answer");
    finish();
    return;
  }
  lines_ = again;
  box_.close();
  openLine();
}

void StoryScene::skipRest() {
  onCard_ = false;
  box_.close();
  size_t stop = lines_.size();
  for (size_t i = next_; i < lines_.size(); ++i)
    if (lines_[i].kind == SceneLine::Kind::Choice && lines_[i].waiting) {
      stop = i;  // a question is never skipped: it is the next thing shown
      break;
    }
  applyRest(next_, stop);
  if (stop < lines_.size()) {
    next_ = stop;
    openLine();
    return;
  }
  next_ = lines_.size();
  finish();
}

void StoryScene::leaveCard() {
  onCard_ = false;
  openLine();
}

// A scene that ends faded out fades in by itself first (docs/EVENTS.md "Scene presentation"), so the next screen never
// starts from a black one.
void StoryScene::finish() {
  if (finished_ || ending_) return;
  box_.close();
  if (stage_.fadedOut() && !host_.args().autoplay) {
    ending_ = true;
    stage_.fade(false, stage_.fadeColor(), kDefaultFadeMs);
    blockLeft_ = (float)kDefaultFadeMs / 1000.0f;
    OTE_INFO("scene: fade in %d ms (automatic, the scene ended faded out)", kDefaultFadeMs);
    return;
  }
  endScene();
}

void StoryScene::endScene() {
  finished_ = true;
  host_.campaignSceneDone();  // replaces this scene: nothing after this line
}

void StoryScene::lineResult(DialogueBox::Result res) {
  if (res == DialogueBox::SkipAll) skipRest();
  else if (res == DialogueBox::Finished) openLine();
  else if (res == DialogueBox::Chosen) answered(box_.chosen());
}

void StoryScene::onInput(const InputEvent& e) {
  if (finished_) return;
  if (onCard_) {
    bool skip = e.type == InputEvent::ButtonDown && e.button == Button::Skip;
    bool next = e.type == InputEvent::PointerDown ||
                (e.type == InputEvent::ButtonDown && (e.button == Button::Confirm || e.button == Button::Cancel));
    if (skip) skipRest();
    else if (next) leaveCard();
    return;
  }
  if (blockLeft_ > 0) {  // a fade / wait / shake: Skip (or Cancel / Menu held) fast-forwards the scene, a tap ends this one wait
    if (ending_) {  // the closing fade-in: only a skip cuts it short
      if (e.type == InputEvent::ButtonDown && e.button == Button::Skip) {
        stage_.finishNow();
        blockLeft_ = 0;
        endScene();
      }
      return;
    }
    if (e.type == InputEvent::ButtonDown && e.button == Button::Skip) skipRest();
    else if (e.type == InputEvent::ButtonDown && (e.button == Button::Cancel || e.button == Button::Menu)) holdT_ = 0;
    else if (e.type == InputEvent::ButtonUp && (e.button == Button::Cancel || e.button == Button::Menu)) holdT_ = -1;
    else if (e.type == InputEvent::PointerDown || (e.type == InputEvent::ButtonDown && e.button == Button::Confirm)) {
      stage_.finishNow();  // only this wait ends
      blockLeft_ = 0;
      openLine();
    }
    return;
  }
  lineResult(box_.onInput(e));
}

void StoryScene::update(double dt) {
  if (!entered_) {  // the first frame: the director now knows this scene
    entered_ = true;
    for (const SceneLine& l : queued_) playAudio(l);
    queued_.clear();
  }
  if (finished_) return;
  stage_.update(dt);
  const bool autoplay = host_.args().autoplay;
  if (onCard_) {
    cardTime_ += (float)dt;
    if (cardTime_ >= (autoplay ? kAutoplayCard : kCardSeconds)) leaveCard();
    return;
  }
  if (blockLeft_ > 0) {
    if (holdT_ >= 0 && !ending_ && (holdT_ += (float)dt) >= kSceneHoldToSkip) {
      holdT_ = -1;
      skipRest();
      return;
    }
    blockLeft_ -= (float)dt;
    if (blockLeft_ <= 0) {
      blockLeft_ = 0;
      stage_.finishNow();  // (the entry that held the scene is over: its transition is complete)
      if (ending_) endScene();
      else openLine();
    }
    return;
  }
  lineResult(box_.update(dt));
  if (finished_ || ending_) return;
  if (autoplay) {
    lineTime_ += (float)dt;
    if (lineTime_ >= kAutoplayLine) openLine();
  }
}

void StoryScene::draw(Renderer& r) {
  r.clear(bg_);
  const StageDrawCtx ctx = stageCtx();
  if (onCard_) {
    // "Chapter N" small above the name, the name as large as the screen allows, both centred.
    int w = r.width(), h = r.height();
    std::string head = heading_, name = title_;
    int sc = fitScale(r, name, w - 24, r.lineHeight() <= 11 ? 3 : 2);
    int headH = head.empty() ? 0 : r.lineHeight() + 6;
    int total = headH + r.lineHeight(sc);
    int y = (h - total) / 2;
    if (!head.empty()) drawTextCentered(r, w / 2, y, fitText(r, head, w - 24), chrome::kDim);
    drawTextCentered(r, w / 2, y + headH, fitText(r, name, w - 24, sc), chrome::kTitle, sc);
    drawFooter(r, "Z / tap: continue");
    return;
  }
  drawStageBackground(r, ctx, stage_);  // nothing when there is none: the scene looks as it always did
  drawStagePortraits(r, ctx, stage_, box_.isOpen() ? box_.top() : r.height() - footerHeight(r) - 4);
  drawFooter(r, box_.choosing() ? kChoiceHint : kDialogueHint);
  box_.draw(r);
  drawStageFade(r, stage_);
}

}  // namespace ote
