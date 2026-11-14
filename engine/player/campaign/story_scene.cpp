#include "campaign/story_scene.h"

#include <algorithm>

#include "campaign/end_scenes.h"
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
                       const std::vector<SceneLine>& lines)
    : host_(host), heading_(heading), title_(title), lines_(lines) {
  bg_ = storyBackground(host);
  onCard_ = hasCard();
  if (!onCard_) openLine();
}

void StoryScene::openLine() {
  if (next_ >= lines_.size()) { finish(); return; }
  const SceneLine& l = lines_[next_++];
  DialogueLine d;
  d.name = l.speaker;
  d.text = l.text;
  d.portrait = l.portrait.empty() ? l.speakerId : l.portrait;  // like a battle line: the speaker's own picture
  const Renderer& r = host_.renderer();
  int style = std::max(0, std::min(host_.styleIndex(), (int)host_.session()->db.styles.size() - 1));
  box_.open(r, r.width(), r.height(), r.height() - footerHeight(r), d,
            portraitArt(host_.session()->db, host_.sprites(), style, d.portrait));
  lineTime_ = 0;
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
  if (res == DialogueBox::SkipAll) finish();
  else if (res == DialogueBox::Finished) openLine();
}

void StoryScene::onInput(const InputEvent& e) {
  if (finished_) return;
  if (onCard_) {
    bool skip = e.type == InputEvent::ButtonDown && e.button == Button::Skip;
    bool next = e.type == InputEvent::PointerDown ||
                (e.type == InputEvent::ButtonDown && (e.button == Button::Confirm || e.button == Button::Cancel));
    if (skip) finish();
    else if (next) leaveCard();
    return;
  }
  lineResult(box_.onInput(e));
}

void StoryScene::update(double dt) {
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
  drawFooter(r, kDialogueHint);
  box_.draw(r);
}

}  // namespace ote
