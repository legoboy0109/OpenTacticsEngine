#include "ui/dialogue_box.h"

#include <algorithm>
#include <cmath>

#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/dialogue_portrait.h"
#include "ui/text.h"

namespace ote {

void DialogueBox::open(const Renderer& r, int screenW, int screenH, int bottom, const DialogueLine& line,
                       const PortraitArt& art) {
  open_ = true;
  name_ = line.name;
  art_ = art;
  page_ = 0;
  letters_ = 0;
  time_ = 0;
  held_ = false;
  heldFor_ = 0;
  centred_ = line.narrate;
  autoMs_ = line.autoMs;
  autoT_ = 0;
  answers_.clear();
  answerLines_.clear();
  choosing_ = false;
  sel_ = 0;
  chosen_ = -1;
  shape_ = frame_.shape;

  // Everything is measured from the active font, like the other panels.
  lh_ = r.lineHeight();
  pad_ = std::max(4, lh_ / 2);
  step_ = lh_ + 2;  // line pitch (a little air between lines, like the other panels)
  skipW_ = textW(r, kDialogueSkipLabel) + 8;
  skipH_ = lh_ + 4;
  // A bubble and a notice stay inside the map's part of the screen (docs/EVENTS.md "Speech bubbles", "Notices").
  const bool area = frame_.areaW > 0 && frame_.areaH > 0;
  areaX_ = area ? frame_.areaX : 0;
  areaY_ = area ? frame_.areaY : 0;
  areaW_ = area ? frame_.areaW : screenW;
  areaH_ = area ? frame_.areaH : bottom;
  if (shape_ == DialogueFrame::Shape::Bubble) layoutBubble(r, screenW, bottom, line.text);
  else if (shape_ == DialogueFrame::Shape::Notice) layoutNotice(r, screenW, bottom, line.text);
  else layoutBox(r, screenW, screenH, bottom, line.text);
}

// The classic box across the bottom (every game before speech bubbles).
void DialogueBox::layoutBox(const Renderer& r, int screenW, int screenH, int bottom, const std::string& text) {
  const PortraitArt& art = art_;
  headerH_ = name_.empty() && !skipInside_ ? 0 : lh_ + 2;  // (a nameless line keeps the row for the Skip tab when it sits inside)
  // Up to three lines a page, fewer when a big font would make the box cover more than 40% of the screen.
  int room = (screenH * 2 / 5 - 2 * pad_ - headerH_) / step_;
  linesPerPage_ = std::max(1, std::min(3, room));
  boxH_ = 2 * pad_ + headerH_ + linesPerPage_ * step_;

  // The portrait stands to the left of the box, on a plate of a fixed size that follows the font (the game's "portraitSize", 48 x 48
  // by default, at the default 8 px font, so it scales with the UI scale). The plate never takes more than a third of the width nor
  // more than 60 percent of the height; a smaller plate keeps its proportions.
  psW_ = psH_ = 0;
  if (art.any()) {
    int w = std::max(1, art.plateW * lh_ / 8), h = std::max(1, art.plateH * lh_ / 8);
    const int maxW = std::max(1, screenW / 3), maxH = std::max(1, screenH * 3 / 5);
    if (w > maxW) {  // (whole numbers, so the default square plate is exactly the old min(48 * lh / 8, screenW / 3))
      h = std::max(1, h * maxW / w);
      w = maxW;
    }
    if (h > maxH) {
      w = std::max(1, w * maxH / h);
      h = maxH;
    }
    psW_ = std::max(24, w);
    psH_ = std::max(24, h);
  }
  const int margin = 2;
  portraitX_ = margin;
  boxX_ = margin + (art.any() ? psW_ + margin : 0);
  boxW_ = std::max(40, screenW - boxX_ - margin);
  boxY_ = std::max(0, bottom - boxH_ - margin);
  portraitY_ = boxY_ + boxH_ - psH_;

  skipX_ = boxX_ + boxW_ - skipW_;
  skipY_ = skipInside_ ? boxY_ + 1 : boxY_ - skipH_;
  if (skipInside_) skipX_ -= 2;

  // The text keeps clear of the "more" mark in the bottom-right corner.
  int textW_ = boxW_ - 2 * pad_ - textW(r, "v") - 2;
  pages_ = paginateDialogue(r, text, textW_, linesPerPage_);
}

// Rows of the answers panel: each answer wrapped to the box's width, at most as many lines as the free room above the box
// allows (four long answers on a phone: fewer lines each, the last one cut with ".."), the panel sitting on the box.
void DialogueBox::setAnswers(const Renderer& r, const std::vector<std::string>& answers, int start) {
  answers_ = answers;
  choosing_ = false;
  sel_ = answers_.empty() ? 0 : std::max(0, std::min(start, (int)answers_.size() - 1));  // "cursor" (docs/EVENTS.md "Notices")
  chosen_ = -1;
  markW_ = textW(r, "> ");
  if (shape_ == DialogueFrame::Shape::Notice) {  // the answers go inside the notice window, under the question
    answersInNotice(r);
    return;
  }
  if (shape_ == DialogueFrame::Shape::Bubble) {
    // The usual panel across the view: at its bottom, or at its top when the bubble (with its tail and portrait) is down there.
    panelX_ = areaX_ + 2;
    panelW_ = std::max(40, areaW_ - 4);
    int lo = boxY_, hi = boxY_ + boxH_;
    if (request_.sideW > 0) { lo = std::min(lo, place_.sideY); hi = std::max(hi, place_.sideY + psH_); }
    lo = std::min(lo, place_.tipY);
    hi = std::max(hi, place_.tipY);
    const int below = areaY_ + areaH_ - 2 - (hi + 2), above = (lo - 2) - (areaY_ + 2);
    const int h = wrapAnswers(r, std::max(20, panelW_ - 2 * pad_ - markW_), std::max(lh_ + 2 * pad_, std::max(below, above)));
    panelY_ = h <= below || below >= above ? areaY_ + areaH_ - 2 - h : areaY_ + 2;
    rowsFrom(panelY_ + pad_);
    return;
  }
  panelX_ = boxX_;
  panelW_ = boxW_;
  const int room = std::max(lh_ + 2 * pad_, boxY_ - 2 - 2);  // the free height above the box
  wrapAnswers(r, std::max(20, panelW_ - 2 * pad_ - markW_), room);
  panelY_ = std::max(2, boxY_ - 2 - panelH_);
  rowsFrom(panelY_ + pad_);
}

int DialogueBox::wrapAnswers(const Renderer& r, int wrapW, int room) {
  const int gap = 2;  // between the rows
  std::vector<std::vector<std::string>> wrapped;
  for (const std::string& a : answers_) wrapped.push_back(wrapText(r, a, wrapW));
  int cap = 8;
  auto height = [&](int c) {
    int h = 2 * pad_ - gap;
    for (const std::vector<std::string>& w : wrapped) h += std::min((int)w.size(), c) * step_ + gap + 2;
    return h;
  };
  while (cap > 1 && height(cap) > room) --cap;
  answerLines_.clear();
  for (std::vector<std::string>& w : wrapped) {
    if ((int)w.size() > cap) {  // too long for its share: the last line takes the rest and is cut
      std::string rest = w[(size_t)cap - 1];
      for (size_t i = (size_t)cap; i < w.size(); ++i) rest += " " + w[i];
      w.resize((size_t)cap);
      w.back() = fitText(r, rest, wrapW);
    }
    answerLines_.push_back(w);
  }
  panelH_ = height(cap);
  return panelH_;
}

void DialogueBox::rowsFrom(int y) {
  const int gap = 2;
  rowY_.clear();
  rowH_.clear();
  for (const std::vector<std::string>& w : answerLines_) {
    rowY_.push_back(y);
    rowH_.push_back((int)w.size() * step_ + 2);
    y += (int)w.size() * step_ + gap + 2;
  }
}

std::string DialogueBox::describeAnswers() const {
  std::string s = "choice: " + std::to_string(answers_.size()) + " answers:";
  for (size_t i = 0; i < answers_.size(); ++i)
    s += " " + std::to_string(i) + "@" + std::to_string(panelX_ + panelW_ / 2) + "," + std::to_string(rowY_[i] + rowH_[i] / 2);
  return s;
}

void DialogueBox::startChoosing() {
  page_ = (int)pages_.size() - 1;
  letters_ = (float)countLetters(pages_[(size_t)page_]);
  held_ = false;
  choosing_ = true;
}

int DialogueBox::rowAt(int x, int y) const {
  if (x < panelX_ || x >= panelX_ + panelW_) return -1;
  for (size_t i = 0; i < rowY_.size(); ++i)
    if (y >= rowY_[i] - 1 && y < rowY_[i] + rowH_[i] + 1) return (int)i;
  return -1;
}

DialogueBox::Result DialogueBox::chooseInput(const InputEvent& e) {
  const int n = (int)answers_.size();
  switch (e.type) {
    case InputEvent::ButtonDown:
      if (e.button == Button::Up) sel_ = (sel_ + n - 1) % n;
      else if (e.button == Button::Down) sel_ = (sel_ + 1) % n;
      else if (e.button == Button::Confirm) { chosen_ = sel_; return Chosen; }
      return None;  // Cancel, Menu, Skip: no way out of a question
    case InputEvent::PointerMove: {
      int row = rowAt(e.x, e.y);
      if (row >= 0) sel_ = row;
      return None;
    }
    case InputEvent::PointerDown: {
      int row = rowAt(e.x, e.y);
      if (row < 0) return None;
      if (row == sel_) { chosen_ = sel_; return Chosen; }
      sel_ = row;  // the first tap only highlights
      return None;
    }
    default: return None;
  }
}

DialogueBox::Result DialogueBox::advance() {
  if (!pageDone()) {
    letters_ = (float)countLetters(pages_[(size_t)page_]);
    return None;
  }
  if (page_ + 1 < (int)pages_.size()) {
    ++page_;
    letters_ = 0;
    return None;
  }
  if (!answers_.empty()) {  // the question has been read: now the answers
    startChoosing();
    return None;
  }
  return Finished;
}

DialogueBox::Result DialogueBox::update(double dt) {
  if (!open_) return None;
  time_ += (float)dt;
  if (!pageDone()) {
    const int before = (int)letters_;
    letters_ += (float)dt * kDialogueLettersPerSecond;
    if ((int)letters_ != before) uiCue(Cue::Dialogue);  // typing (never when the page is shown at once); rate-limited by GameAudio
  }
  // "auto" (docs/EVENTS.md "Scene presentation"): a complete page goes on by itself after autoMs_ (a question never does).
  if (autoMs_ > 0 && pageDone() && !choosing_ && answers_.empty()) {
    autoT_ += (float)dt;
    if (autoT_ * 1000.0f >= (float)autoMs_) {
      autoT_ = 0;
      return advance();
    }
  }
  if (held_ && !choosing_) {
    heldFor_ += (float)dt;
    if (heldFor_ >= kDialogueHoldToSkip) {
      held_ = false;
      if (!answers_.empty()) { startChoosing(); return None; }  // a question is never skipped, only read at once
      return SkipAll;
    }
  }
  return None;
}

DialogueBox::Result DialogueBox::onInput(const InputEvent& e) {
  if (!open_) return None;
  if (choosing_) return chooseInput(e);
  switch (e.type) {
    case InputEvent::ButtonDown:
      if (e.button == Button::Confirm) return advance();
      if (e.button == Button::Skip) {
        if (!answers_.empty()) { startChoosing(); return None; }
        return SkipAll;
      }
      if (e.button == Button::Cancel || e.button == Button::Menu) {
        held_ = true;
        heldFor_ = 0;
      }
      return None;
    case InputEvent::ButtonUp:
      if (e.button == Button::Cancel || e.button == Button::Menu) held_ = false;
      return None;
    case InputEvent::PointerDown:
      if (e.x >= skipX_ && e.y >= skipY_ && e.x < skipX_ + skipW_ && e.y < skipY_ + skipH_) {
        if (!answers_.empty()) { startChoosing(); return None; }
        return SkipAll;
      }
      return advance();
    default: return None;
  }
}

void DialogueBox::draw(Renderer& r) const {
  if (!open_) return;
  if (shape_ == DialogueFrame::Shape::Bubble) { drawBubble(r); return; }
  if (shape_ == DialogueFrame::Shape::Notice) { drawNotice(r); return; }
  using namespace chrome;
  // Portrait: a dark plate, the picture fitted inside it keeping the cell's proportions, a frame.
  if (art_.any()) {
    r.fillRect(portraitX_, portraitY_, psW_, psH_, Color(16, 20, 36, 240));
    // Fitted inside the plate keeping the cell's proportions, standing on its bottom edge, centred across.
    int dw = psW_, dh = std::max(1, psW_ * art_.sh / std::max(1, art_.sw));
    if (dh > psH_) {
      dh = psH_;
      dw = std::max(1, psH_ * art_.sw / std::max(1, art_.sh));
    }
    const PortraitArt face = portraitFrame(art_, revealing(), time_, name_);  // talk / blink frames, when the portrait has them
    r.drawTexture(face.tex, face.sx, face.sy, face.sw, face.sh, portraitX_ + (psW_ - dw) / 2, portraitY_ + psH_ - dh,
                  dw, dh);
    r.drawRect(portraitX_, portraitY_, psW_, psH_, Color(200, 205, 230));
  }
  r.fillRect(boxX_ + 2, boxY_ + 2, boxW_, boxH_, Color(0, 0, 0, 110));  // drop shadow
  r.fillRect(boxX_, boxY_, boxW_, boxH_, Color(16, 20, 36, 240));
  r.drawRect(boxX_, boxY_, boxW_, boxH_, Color(200, 205, 230));

  // The Skip button is a tab on the box's top-right corner (a question keeps it until its answers are up: it then
  // only shows them at once).
  drawSkipTab(r);

  int y = boxY_ + pad_;
  if (!name_.empty()) {
    r.drawText(boxX_ + pad_, y, fitText(r, name_, boxW_ - 2 * pad_ - skipW_ - 4).c_str(), kAccent);
    y += headerH_;
  }
  std::vector<std::string> shown = revealLetters(pages_[(size_t)page_], (int)letters_);
  for (const std::string& ln : shown) {
    const int x = centred_ ? boxX_ + std::max(pad_, (boxW_ - textW(r, ln)) / 2) : boxX_ + pad_;
    r.drawText(x, y, ln.c_str(), kTextSel);
    y += step_;
  }
  // "More" mark once the page is fully shown: it blinks, and a last page shows it too (= "continue").
  if (pageDone() && !choosing_ && std::fmod(time_, 0.8f) < 0.5f)
    r.drawText(boxX_ + boxW_ - pad_ - textW(r, "v"), boxY_ + boxH_ - pad_ - lh_ + 2, "v", kAccent);
  if (choosing_) drawAnswers(r, true);
}

void DialogueBox::drawSkipTab(Renderer& r) const {
  using namespace chrome;
  if (choosing_) return;
  r.fillRect(skipX_, skipY_, skipW_, skipH_, Color(16, 20, 36, 240));
  r.drawRect(skipX_, skipY_, skipW_, skipH_, Color(150, 156, 185));
  r.drawText(skipX_ + 4, skipY_ + 2, kDialogueSkipLabel, held_ ? kAccent : kDim);
}

// `framed`: the panel of its own (box, bubble); false = the rows sit inside the notice window, which is drawn already.
void DialogueBox::drawAnswers(Renderer& r, bool framed) const {
  using namespace chrome;
  if (framed) {
    r.fillRect(panelX_ + 2, panelY_ + 2, panelW_, panelH_, Color(0, 0, 0, 110));
    r.fillRect(panelX_, panelY_, panelW_, panelH_, Color(16, 20, 36, 245));
    r.drawRect(panelX_, panelY_, panelW_, panelH_, Color(200, 205, 230));
  }
  for (size_t i = 0; i < answerLines_.size(); ++i) {
    const bool on = (int)i == sel_;
    if (on) r.fillRect(panelX_ + 2, rowY_[i] - 1, panelW_ - 4, rowH_[i] + 1, Color(60, 66, 110, 255));
    int y = rowY_[i] + 1;
    for (size_t k = 0; k < answerLines_[i].size(); ++k) {
      if (k == 0 && on) r.drawText(panelX_ + pad_, y, ">", kAccent);
      r.drawText(panelX_ + pad_ + markW_, y, answerLines_[i][k].c_str(), on ? kTextSel : kText);
      y += step_;
    }
  }
}

}  // namespace ote
