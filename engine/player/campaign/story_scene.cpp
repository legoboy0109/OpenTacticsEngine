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
}  // namespace

StoryScene::StoryScene(SceneHost& host, const std::string& heading, const std::string& title,
                       const std::vector<SceneLine>& lines, SceneResolver resolve)
    : host_(host), heading_(heading), title_(title), lines_(lines), resolve_(std::move(resolve)) {
  bg_ = storyBackground(host);
  consumeAudio();  // music asked for at the very top (a scene's "music") starts with the card
  onCard_ = hasCard();
  if (!onCard_) openLine();
}

// Sound requests (docs/SOUND.md) sit between the lines: they act when reached. Before the scene has been on screen
// for a frame the music director does not know about it yet, so they wait for the first update().
void StoryScene::consumeAudio() {
  using K = SceneLine::Kind;
  while (next_ < lines_.size() && lines_[next_].kind != K::Say && lines_[next_].kind != K::Choice) {
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

void StoryScene::openLine() {
  consumeAudio();
  if (next_ >= lines_.size()) { finish(); return; }
  const SceneLine& l = lines_[next_++];
  // A question that was answered already (the scene was run again with the answers) is not shown twice.
  if (l.kind == SceneLine::Kind::Choice && !l.waiting) { openLine(); return; }
  DialogueLine d;
  d.name = l.speaker;
  d.text = l.text;
  d.portrait = l.portrait.empty() ? l.speakerId : l.portrait;  // like a battle line: the speaker's own picture
  const Renderer& r = host_.renderer();
  int style = std::max(0, std::min(host_.styleIndex(), (int)host_.session()->db.styles.size() - 1));
  box_.open(r, r.width(), r.height(), r.height() - footerHeight(r), d,
            portraitArt(host_.session()->db, host_.sprites(), style, d.portrait));
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
  for (size_t i = next_; i < lines_.size(); ++i)
    if (lines_[i].kind == SceneLine::Kind::Choice && lines_[i].waiting) {
      next_ = i;  // a question is never skipped: it is the next thing shown
      openLine();
      return;
    }
  finish();
}

void StoryScene::leaveCard() {
  onCard_ = false;
  openLine();
}

void StoryScene::finish() {
  if (finished_) return;
  finished_ = true;
  box_.close();
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
  lineResult(box_.onInput(e));
}

void StoryScene::update(double dt) {
  if (!entered_) {  // the first frame: the director now knows this scene
    entered_ = true;
    for (const SceneLine& l : queued_) playAudio(l);
    queued_.clear();
  }
  if (finished_) return;
  const bool autoplay = host_.args().autoplay;
  if (onCard_) {
    cardTime_ += (float)dt;
    if (cardTime_ >= (autoplay ? kAutoplayCard : kCardSeconds)) leaveCard();
    return;
  }
  lineResult(box_.update(dt));
  if (finished_) return;
  if (autoplay) {
    lineTime_ += (float)dt;
    if (lineTime_ >= kAutoplayLine) openLine();
  }
}

void StoryScene::draw(Renderer& r) {
  r.clear(bg_);
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
  drawFooter(r, box_.choosing() ? kChoiceHint : kDialogueHint);
  box_.draw(r);
}

}  // namespace ote
