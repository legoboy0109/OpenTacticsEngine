#include "battle/promotion_screen.h"

#include <algorithm>
#include <utility>

#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr int kPad = 6;
constexpr int kNarrow = 300;  // a screen narrower than this puts the list above the details
const Color kGain(190, 255, 190), kWarn(255, 175, 140);

using Line = std::pair<std::string, Color>;

// The wrapped text under the picture: changes, level, skills, weapons that can no longer be wielded.
std::vector<Line> detailLines(const Renderer& r, const PromotionPreview& p, int width) {
  std::vector<Line> out;
  auto add = [&](const std::string& text, Color c) {
    for (const std::string& l : wrapText(r, text, width)) out.emplace_back(l, c);
  };
  if (p.changes.empty()) add(joinChanges(p.changes), chrome::kDim);
  else add("Gains: " + joinChanges(p.changes), kGain);
  if (!p.level.empty()) add(p.level, chrome::kDim);
  if (!p.skills.empty()) {
    std::string s;
    for (const std::string& n : p.skills) s += (s.empty() ? "" : ", ") + n;
    add("Skills: " + s, chrome::kText);
  }
  if (!p.cannotWield.empty()) {
    std::string s;
    for (const std::string& n : p.cannotWield) s += (s.empty() ? "" : ", ") + n;
    add("Cannot wield: " + s, kWarn);
  }
  return out;
}

}  // namespace

void PromotionScreen::open(const std::string& title, std::vector<PromotionPreview> options) {
  open_ = true;
  title_ = title;
  options_ = std::move(options);
  list_ = RowList();
  list_.count = (int)options_.size();
}

// Sized for the tallest option so the box does not change size while the highlight moves.
void PromotionScreen::layout(const Renderer& r) {
  const int lh = r.lineHeight(), rowH = lh + 4;
  const bool narrow = aw_ < kNarrow;
  w_ = std::min(aw_ - 8, 420);
  const int inner = w_ - 2 * kPad;
  int widest = 0;
  for (const PromotionPreview& p : options_) widest = std::max(widest, textW(r, p.name));
  const int n = std::max(1, list_.count);
  const int top = kPad + lh + 6;  // under the title
  const int maxH = ah_ - 8;

  int listW = narrow ? inner : std::max(70, std::min(widest + 16, inner / 2));
  dw_ = narrow ? inner : inner - listW - kPad;
  const int pic = std::max(16, std::min(40, dw_ / 3));
  int linesH = 0;
  for (const PromotionPreview& p : options_) linesH = std::max(linesH, (int)detailLines(r, p, dw_).size() * (lh + 1));
  const int detailH = pic + 4 + linesH;

  const int listH = n * rowH;
  h_ = narrow ? top + listH + kPad + detailH + kPad : top + std::max(listH, detailH) + kPad;
  h_ = std::min(h_, maxH);
  x_ = ax_ + (aw_ - w_) / 2;
  y_ = ay_ + (ah_ - h_) / 2;

  list_.rowH = rowH;
  list_.x = x_ + kPad;
  list_.y = y_ + top;
  list_.w = listW;
  // Too many rows for the room: the list scrolls (at least one row; in the narrow layout the details keep their share).
  int room = h_ - top - kPad - (narrow ? kPad + detailH : 0);
  list_.h = std::max(1, std::min(n, room / rowH)) * rowH;
  dx_ = narrow ? x_ + kPad : list_.x + listW + kPad;
  dy_ = narrow ? list_.y + list_.h + kPad : list_.y;
  dh_ = y_ + h_ - kPad - dy_;
}

PromotionScreen::Result PromotionScreen::onInput(const InputEvent& e, const Renderer& r) {
  if (!open_ || options_.empty()) return None;
  layout(r);
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) {
      bool again = row == list_.sel;
      list_.sel = row;
      if (again) uiCue(Cue::Confirm);
      return again ? Chosen : None;  // a first tap only selects (a touch screen never hovers)
    }
    if (e.x < x_ || e.y < y_ || e.x >= x_ + w_ || e.y >= y_ + h_) { uiCue(Cue::Cancel); return Cancelled; }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return None;
    if (e.button == Button::Confirm) { uiCue(Cue::Confirm); return Chosen; }
    if (e.button == Button::Cancel || e.button == Button::Menu) { uiCue(Cue::Cancel); return Cancelled; }
  }
  return None;
}

void PromotionScreen::draw(Renderer& r, const Picture& picture) {
  if (!open_ || options_.empty()) return;
  layout(r);
  const int lh = r.lineHeight();
  r.fillRect(x_ + 2, y_ + 2, w_, h_, Color(0, 0, 0, 110));
  r.fillRect(x_, y_, w_, h_, chrome::kBg);
  r.drawRect(x_, y_, w_, h_, Color(200, 205, 230));
  r.drawText(x_ + kPad, y_ + kPad, fitText(r, title_, w_ - 2 * kPad).c_str(), chrome::kAccent);

  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < list_.count; ++i) {
    int row = first + i;
    bool sel = row == list_.sel;
    drawRowBox(r, list_.x, list_.rowY(row), list_.w, list_.rowH - 1, sel);
    r.drawText(list_.x + 4, list_.rowY(row) + (list_.rowH - 1 - lh) / 2,
               fitText(r, options_[(size_t)row].name, list_.w - 8).c_str(), sel ? chrome::kTextSel : chrome::kText);
  }
  list_.drawMore(r, chrome::kAccent);

  // Details of the highlighted type: picture and name side by side, the text under them.
  const PromotionPreview& p = options_[(size_t)list_.sel];
  const int pic = std::max(16, std::min(40, dw_ / 3));
  r.fillRect(dx_, dy_, pic, pic, Color(30, 34, 54));
  r.drawRect(dx_, dy_, pic, pic, Color(90, 98, 140));
  picture(r, p.to, dx_, dy_, pic, pic);
  r.drawText(dx_ + pic + 6, dy_ + (pic - lh) / 2, fitText(r, p.name, dw_ - pic - 6).c_str(), chrome::kTextSel);
  int y = dy_ + pic + 4;
  for (const Line& l : detailLines(r, p, dw_)) {
    if (y + lh > dy_ + dh_ + 1) break;  // never past the box (a very small screen loses the last lines)
    r.drawText(dx_, y, l.first.c_str(), l.second);
    y += lh + 1;  // a pixel of air between lines
  }
}

}  // namespace ote
