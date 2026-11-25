// The dialogue box's two other shapes (docs/EVENTS.md "Speech bubbles", "Notices"): a speech bubble pointing at the speaking
// unit, and the notice window in the middle of the screen. Paging, typing, input and questions are the box's own
// (dialogue_box.cpp); this file only lays them out and draws them. The look is a placeholder (owner decision).
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ui/chrome.h"
#include "ui/dialogue_box.h"
#include "ui/dialogue_portrait.h"
#include "ui/text.h"

namespace ote {

namespace {
// The bubble's colours: a light card with dark ink, so it reads as speech and not as a menu.
const Color kBubbleFill(250, 247, 235, 250), kBubbleInk(40, 40, 56), kBubbleText(28, 28, 40), kBubbleName(40, 70, 150);
// The notice window: the menus' dark blue with a double frame.
const Color kNoticeFill(20, 26, 64, 245), kNoticeOuter(220, 224, 240), kNoticeInner(110, 120, 180);
constexpr int kBubbleTextLetters = 22;  // the bubble's text is at least this many letters wide when the view allows (a wide view: 3/4 of it)

int widest(const Renderer& r, const std::vector<std::vector<std::string>>& pages) {
  int w = 0;
  for (const std::vector<std::string>& p : pages)
    for (const std::string& l : p) w = std::max(w, textW(r, l));
  return w;
}
int tallest(const std::vector<std::vector<std::string>>& pages) {
  size_t n = 1;
  for (const std::vector<std::string>& p : pages) n = std::max(n, p.size());
  return (int)n;
}

// A rectangle with its corners cut by one pixel, filled, with a 1 px outline.
void roundRect(Renderer& r, int x, int y, int w, int h, Color fill, Color ink) {
  r.fillRect(x + 2, y, w - 4, h, fill);
  r.fillRect(x + 1, y + 1, w - 2, h - 2, fill);
  r.fillRect(x, y + 2, w, h - 4, fill);
  r.fillRect(x + 2, y, w - 4, 1, ink);
  r.fillRect(x + 2, y + h - 1, w - 4, 1, ink);
  r.fillRect(x, y + 2, 1, h - 4, ink);
  r.fillRect(x + w - 1, y + 2, 1, h - 4, ink);
  for (int cx : {x + 1, x + w - 2})
    for (int cy : {y + 1, y + h - 2}) r.fillRect(cx, cy, 1, 1, ink);
}
}  // namespace

// ---- the speech bubble

void DialogueBox::layoutBubble(const Renderer& r, int screenW, int bottom, const std::string& text) {
  (void)screenW;
  (void)bottom;
  const int moreW = textW(r, "v") + 2;
  headerH_ = frame_.names && !name_.empty() ? lh_ + 2 : 0;
  const int room = (areaH_ * 2 / 5 - 2 * pad_ - headerH_) / step_;  // up to three lines a page, fewer with a big font
  linesPerPage_ = std::max(1, std::min(3, room));
  const bool inside = art_.any() && frame_.portrait == 1, beside = art_.any() && frame_.portrait == 2;
  psW_ = psH_ = 0;
  if (beside) {  // the box's plate (the game's "portraitSize" at the 8 px font), at most a quarter of the view across, a third down
    int w = std::max(1, art_.plateW * lh_ / 8), h = std::max(1, art_.plateH * lh_ / 8);
    const int maxW = std::max(24, areaW_ / 4), maxH = std::max(24, areaH_ / 3);
    if (w > maxW) { h = std::max(1, h * maxW / w); w = maxW; }
    if (h > maxH) { w = std::max(1, w * maxH / h); h = maxH; }
    psW_ = std::max(24, w);
    psH_ = std::max(24, h);
  }
  if (inside) {  // as tall as three lines of text (and the name row), keeping the plate's proportions
    psH_ = headerH_ + 3 * step_;
    psW_ = std::max(16, psH_ * art_.plateW / std::max(1, art_.plateH));
  }
  const int sideW = beside ? psW_ + 2 : 0, inW = inside ? psW_ + pad_ : 0;
  // At most 3/4 of the view across (all of it, less the margins, on a narrow phone view), at least kBubbleTextLetters letters.
  int maxW = std::max(lh_ * kBubbleTextLetters, areaW_ * 3 / 4);
  if (areaW_ < areaH_) maxW = areaW_;
  maxW = std::max(40, std::min(maxW, areaW_ - 4 - sideW));
  pages_ = paginateDialogue(r, text, std::max(20, maxW - 2 * pad_ - moreW - inW), linesPerPage_);
  int content = widest(r, pages_);
  if (headerH_ > 0) content = std::max(content, textW(r, name_));
  content = std::max(content, lh_ * 4);
  boxW_ = std::min(maxW, content + 2 * pad_ + moreW + inW);
  const int rows = std::min(linesPerPage_, tallest(pages_));
  boxH_ = 2 * pad_ + std::max(headerH_ + rows * step_, inside ? psH_ : 0);
  request_ = BubbleRequest();
  request_.areaX = areaX_;
  request_.areaY = areaY_;
  request_.areaW = areaW_;
  request_.areaH = areaH_;
  request_.bubbleW = boxW_;
  request_.bubbleH = boxH_;
  request_.sideW = beside ? psW_ : 0;
  request_.sideH = beside ? psH_ : 0;
  request_.tail = std::max(4, lh_ * 3 / 4);
  placeBubbleNow();
}

void DialogueBox::placeBubbleNow() {
  place_ = placeBubble(request_, frame_.anchor);
  boxX_ = place_.x;
  boxY_ = place_.y;
  if (request_.sideW > 0) {
    portraitX_ = place_.sideX;
    portraitY_ = place_.sideY;
  } else {
    portraitX_ = boxX_ + pad_;
    portraitY_ = boxY_ + pad_;
  }
  placeSkipTab();
}

// While a bubble is up the Skip tab is a button in a corner of the view (bottom right, or top right when the bubble is there):
// the bubble moves with its speaker, a button should not.
void DialogueBox::placeSkipTab() {
  skipX_ = areaX_ + areaW_ - skipW_ - 2;
  skipY_ = areaY_ + areaH_ - skipH_ - 2;
  const auto hits = [&](int x, int y, int w, int h) { return skipX_ < x + w && x < skipX_ + skipW_ && skipY_ < y + h && y < skipY_ + skipH_; };
  if (hits(boxX_, boxY_, boxW_, boxH_) || (request_.sideW > 0 && hits(portraitX_, portraitY_, psW_, psH_))) skipY_ = areaY_ + 2;
}

void DialogueBox::reanchor(const BubbleAnchor& a) {
  if (!open_ || shape_ != DialogueFrame::Shape::Bubble) return;
  if (a.x == frame_.anchor.x && a.y == frame_.anchor.y && a.w == frame_.anchor.w && a.h == frame_.anchor.h) return;
  frame_.anchor = a;
  const int oldY = boxY_, oldH = boxH_;
  placeBubbleNow();
  if (!answers_.empty() && (boxY_ != oldY || boxH_ != oldH)) {  // the answer panel keeps clear of the bubble
    const int ph = panelH_;
    int lo = std::min(boxY_, place_.tipY), hi = std::max(boxY_ + boxH_, place_.tipY);
    if (request_.sideW > 0) { lo = std::min(lo, place_.sideY); hi = std::max(hi, place_.sideY + psH_); }
    const int below = areaY_ + areaH_ - 2 - (hi + 2), above = (lo - 2) - (areaY_ + 2);
    panelY_ = ph <= below || below >= above ? areaY_ + areaH_ - 2 - ph : areaY_ + 2;
    rowsFrom(panelY_ + pad_);
  }
}

std::string DialogueBox::describeBubble() const {
  char buf[160];
  std::snprintf(buf, sizeof buf, "bubble: %s %d,%d %dx%d %s tail %d,%d%s", name_.empty() ? "(no name)" : name_.c_str(), boxX_, boxY_, boxW_,
                boxH_, place_.below ? "below" : "above", place_.tipX, place_.tipY, place_.covers ? " (covers the speaker)" : "");
  return buf;
}

void DialogueBox::drawPortrait(Renderer& r) const {
  if (!art_.any() || psW_ <= 0) return;
  const bool beside = request_.sideW > 0;
  if (beside) r.fillRect(portraitX_, portraitY_, psW_, psH_, Color(16, 20, 36, 240));
  int dw = psW_, dh = std::max(1, psW_ * art_.sh / std::max(1, art_.sw));
  if (dh > psH_) {
    dh = psH_;
    dw = std::max(1, psH_ * art_.sw / std::max(1, art_.sh));
  }
  const PortraitArt face = portraitFrame(art_, revealing(), time_, name_);
  r.drawTexture(face.tex, face.sx, face.sy, face.sw, face.sh, portraitX_ + (psW_ - dw) / 2, portraitY_ + psH_ - dh, dw, dh);
  r.drawRect(portraitX_, portraitY_, psW_, psH_, beside ? Color(200, 205, 230) : kBubbleInk);
}

void DialogueBox::drawPage(Renderer& r, int x, int y, int w, bool centred, Color text) const {
  for (const std::string& ln : revealLetters(pages_[(size_t)page_], (int)letters_)) {
    r.drawText(centred ? x + std::max(0, (w - textW(r, ln)) / 2) : x, y, ln.c_str(), text);
    y += step_;
  }
}

void DialogueBox::drawBubble(Renderer& r) const {
  // The tail first (from the bubble's edge to the speaker), then the bubble over its base, so the two read as one shape.
  const int x = boxX_, y = boxY_, w = boxW_, h = boxH_;
  r.fillRect(x + 2, y + 2, w, h, Color(0, 0, 0, 90));  // drop shadow
  if (place_.hasTail()) {
    const int len = place_.below ? place_.baseY - place_.tipY : place_.tipY - place_.baseY;
    const int half = std::max(3, lh_ / 3);
    for (int k = 0; k < len; ++k) {
      const float t = (float)k / (float)len;
      const int cx = place_.baseX + (int)std::lround((place_.tipX - place_.baseX) * t);
      const int hw = (int)std::lround(half * (1.0f - t));
      const int row = place_.below ? place_.baseY - 1 - k : place_.baseY + k;
      r.fillRect(cx - hw, row, 2 * hw + 1, 1, kBubbleFill);
      r.fillRect(cx - hw - 1, row, 1, 1, kBubbleInk);
      r.fillRect(cx + hw + 1, row, 1, 1, kBubbleInk);
    }
  }
  roundRect(r, x, y, w, h, kBubbleFill, kBubbleInk);
  if (place_.hasTail()) {  // open the outline where the tail leaves the bubble
    const int half = std::max(3, lh_ / 3);
    r.fillRect(place_.baseX - half, place_.below ? y : y + h - 1, 2 * half + 1, 1, kBubbleFill);
  }
  drawPortrait(r);
  const bool inside = art_.any() && request_.sideW == 0 && psW_ > 0;
  const int tx = x + pad_ + (inside ? psW_ + pad_ : 0);
  int ty = y + pad_;
  if (headerH_ > 0) {
    r.drawText(tx, ty, fitText(r, name_, x + w - pad_ - tx).c_str(), kBubbleName);
    ty += headerH_;
  }
  drawPage(r, tx, ty, 0, false, kBubbleText);
  if (pageDone() && !choosing_ && std::fmod(time_, 0.8f) < 0.5f)
    r.drawText(x + w - pad_ - textW(r, "v"), y + h - pad_ - lh_ + 2, "v", kBubbleName);
  drawSkipTab(r);
  if (choosing_) drawAnswers(r, true);
}

// ---- the notice window

void DialogueBox::layoutNotice(const Renderer& r, int screenW, int bottom, const std::string& text) {
  (void)screenW;
  (void)bottom;
  const int moreW = textW(r, "v") + 2, frame = 3;  // the double frame takes 3 px each side
  headerH_ = 0;
  psW_ = psH_ = 0;
  art_ = PortraitArt();  // a notice has no speaker
  const int room = (areaH_ * 2 / 5 - 2 * pad_) / step_;
  linesPerPage_ = std::max(1, std::min(3, room));
  noticeMaxW_ = std::max(40, std::min(areaW_ - 16, std::max(lh_ * 16, areaW_ * 2 / 3)));
  pages_ = paginateDialogue(r, text, std::max(20, noticeMaxW_ - 2 * (pad_ + frame) - 2 * moreW), linesPerPage_);
  boxW_ = std::min(noticeMaxW_, std::max(lh_ * 12, widest(r, pages_) + 2 * (pad_ + frame) + 2 * moreW));
  noticeTextH_ = 2 * (pad_ + frame) + std::min(linesPerPage_, tallest(pages_)) * step_ - 2;
  boxH_ = noticeTextH_;
  boxX_ = areaX_ + (areaW_ - boxW_) / 2;
  boxY_ = areaY_ + std::max(2, (areaH_ - boxH_) / 2);
  skipX_ = boxX_ + boxW_ - skipW_;  // the tab on the window's top edge, as on the box
  skipY_ = std::max(0, boxY_ - skipH_);
}

// The answers inside the window under the question: the window widens for long answers (up to its most) and grows down, and stays
// in the middle of the view.
void DialogueBox::answersInNotice(const Renderer& r) {
  const int frame = 3;
  int want = boxW_;
  for (const std::string& a : answers_) want = std::max(want, textW(r, a) + markW_ + 2 * (pad_ + frame));
  boxW_ = std::min(noticeMaxW_, want);
  boxX_ = areaX_ + (areaW_ - boxW_) / 2;
  panelX_ = boxX_ + frame;
  panelW_ = boxW_ - 2 * frame;
  const int h = wrapAnswers(r, std::max(20, panelW_ - 2 * pad_ - markW_), std::max(lh_ + 2 * pad_, areaH_ - noticeTextH_ - 8));
  boxH_ = noticeTextH_ + h;
  boxY_ = areaY_ + std::max(2, (areaH_ - boxH_) / 2);
  panelY_ = boxY_ + noticeTextH_ - pad_;
  panelH_ = h;
  rowsFrom(panelY_ + pad_);
  skipX_ = boxX_ + boxW_ - skipW_;
  skipY_ = std::max(0, boxY_ - skipH_);
}

void DialogueBox::drawNotice(Renderer& r) const {
  r.fillRect(boxX_ + 2, boxY_ + 2, boxW_, boxH_, Color(0, 0, 0, 110));
  r.fillRect(boxX_, boxY_, boxW_, boxH_, kNoticeFill);
  r.drawRect(boxX_, boxY_, boxW_, boxH_, kNoticeOuter);
  r.drawRect(boxX_ + 2, boxY_ + 2, boxW_ - 4, boxH_ - 4, kNoticeInner);
  const int frame = 3;
  drawPage(r, boxX_ + frame + pad_, boxY_ + frame + pad_, boxW_ - 2 * (frame + pad_), true, chrome::kTextSel);
  if (pageDone() && !choosing_ && std::fmod(time_, 0.8f) < 0.5f)
    r.drawText(boxX_ + boxW_ - frame - pad_ - textW(r, "v"), boxY_ + boxH_ - frame - pad_ - lh_ + 2, "v", chrome::kAccent);
  drawSkipTab(r);
  if (choosing_) drawAnswers(r, false);
}

}  // namespace ote
