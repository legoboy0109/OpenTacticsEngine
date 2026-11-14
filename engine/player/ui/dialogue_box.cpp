#include "ui/dialogue_box.h"

#include <algorithm>
#include <cmath>

#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr const char* kSkipLabel = "Skip";
}

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

  // Everything is measured from the active font, like the other panels.
  lh_ = r.lineHeight();
  pad_ = std::max(4, lh_ / 2);
  headerH_ = name_.empty() ? 0 : lh_ + 2;
  // Up to three lines a page, fewer when a big font would make the box cover more than 40% of the screen.
  step_ = lh_ + 2;  // line pitch (a little air between lines, like the other panels)
  int room = (screenH * 2 / 5 - 2 * pad_ - headerH_) / step_;
  linesPerPage_ = std::max(1, std::min(3, room));
  boxH_ = 2 * pad_ + headerH_ + linesPerPage_ * step_;

  // The portrait stands to the left of the box, at a fixed size that follows the font (48 px at the
  // default 8 px font, so it scales with the UI scale), and never takes more than a third of the width.
  ps_ = art.any() ? std::max(24, std::min(48 * lh_ / 8, screenW / 3)) : 0;
  const int margin = 2;
  portraitX_ = margin;
  boxX_ = margin + (art.any() ? ps_ + margin : 0);
  boxW_ = std::max(40, screenW - boxX_ - margin);
  boxY_ = std::max(0, bottom - boxH_ - margin);
  portraitY_ = boxY_ + boxH_ - ps_;

  skipW_ = textW(r, kSkipLabel) + 8;
  skipH_ = lh_ + 4;
  skipX_ = boxX_ + boxW_ - skipW_;
  skipY_ = boxY_ - skipH_;

  // The text keeps clear of the "more" mark in the bottom-right corner.
  int textW_ = boxW_ - 2 * pad_ - textW(r, "v") - 2;
  pages_ = paginateDialogue(r, line.text, textW_, linesPerPage_);
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
  return Finished;
}

DialogueBox::Result DialogueBox::update(double dt) {
  if (!open_) return None;
  time_ += (float)dt;
  if (!pageDone()) letters_ += (float)dt * kDialogueLettersPerSecond;
  if (held_) {
    heldFor_ += (float)dt;
    if (heldFor_ >= kDialogueHoldToSkip) {
      held_ = false;
      return SkipAll;
    }
  }
  return None;
}

DialogueBox::Result DialogueBox::onInput(const InputEvent& e) {
  if (!open_) return None;
  switch (e.type) {
    case InputEvent::ButtonDown:
      if (e.button == Button::Confirm) return advance();
      if (e.button == Button::Skip) return SkipAll;
      if (e.button == Button::Cancel || e.button == Button::Menu) {
        held_ = true;
        heldFor_ = 0;
      }
      return None;
    case InputEvent::ButtonUp:
      if (e.button == Button::Cancel || e.button == Button::Menu) held_ = false;
      return None;
    case InputEvent::PointerDown:
      if (e.x >= skipX_ && e.y >= skipY_ && e.x < skipX_ + skipW_ && e.y < skipY_ + skipH_) return SkipAll;
      return advance();
    default: return None;
  }
}

void DialogueBox::draw(Renderer& r) const {
  if (!open_) return;
  using namespace chrome;
  // Portrait: a dark plate, the picture fitted inside it keeping the cell's proportions, a frame.
  if (art_.any()) {
    r.fillRect(portraitX_, portraitY_, ps_, ps_, Color(16, 20, 36, 240));
    int dw = ps_, dh = ps_;
    if (art_.sw >= art_.sh) dh = std::max(1, ps_ * art_.sh / art_.sw);
    else dw = std::max(1, ps_ * art_.sw / art_.sh);
    r.drawTexture(art_.tex, art_.sx, art_.sy, art_.sw, art_.sh, portraitX_ + (ps_ - dw) / 2, portraitY_ + ps_ - dh,
                  dw, dh);
    r.drawRect(portraitX_, portraitY_, ps_, ps_, Color(200, 205, 230));
  }
  r.fillRect(boxX_ + 2, boxY_ + 2, boxW_, boxH_, Color(0, 0, 0, 110));  // drop shadow
  r.fillRect(boxX_, boxY_, boxW_, boxH_, Color(16, 20, 36, 240));
  r.drawRect(boxX_, boxY_, boxW_, boxH_, Color(200, 205, 230));

  // The Skip button is a tab on the box's top-right corner.
  r.fillRect(skipX_, skipY_, skipW_, skipH_, Color(16, 20, 36, 240));
  r.drawRect(skipX_, skipY_, skipW_, skipH_, Color(150, 156, 185));
  r.drawText(skipX_ + 4, skipY_ + 2, kSkipLabel, held_ ? kAccent : kDim);

  int y = boxY_ + pad_;
  if (!name_.empty()) {
    r.drawText(boxX_ + pad_, y, fitText(r, name_, boxW_ - 2 * pad_ - skipW_ - 4).c_str(), kAccent);
    y += headerH_;
  }
  std::vector<std::string> shown = revealLetters(pages_[(size_t)page_], (int)letters_);
  for (const std::string& ln : shown) {
    r.drawText(boxX_ + pad_, y, ln.c_str(), kTextSel);
    y += step_;
  }
  // "More" mark once the page is fully shown: it blinks, and a last page shows it too (= "continue").
  if (pageDone() && std::fmod(time_, 0.8f) < 0.5f)
    r.drawText(boxX_ + boxW_ - pad_ - textW(r, "v"), boxY_ + boxH_ - pad_ - lh_ + 2, "v", kAccent);
}

}  // namespace ote
