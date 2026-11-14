#include "ui/confirm.h"

#include <algorithm>

#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kPad = 10, kGap = 8;
}

void ConfirmBox::open(const std::string& message, const std::string& yes, const std::string& no) {
  open_ = true;
  message_ = message;
  choices_.clear();
  if (!no.empty()) {
    Choice b;
    b.label = no;
    choices_.push_back(b);
  }
  Choice y;
  y.label = yes;
  y.yes = true;
  choices_.push_back(y);
  sel_ = 0;  // the safe answer (or the only button)
}

// Computed from the active font each time: the font can change between frames.
void ConfirmBox::layout(const Renderer& r) {
  int lh = r.lineHeight();
  rowH_ = lh + 2;
  int wrapW = std::max(60, std::min(r.width() - 2 * kPad - 16, 300));
  lines_ = wrapText(r, message_, wrapW);
  int widest = 0;
  for (const std::string& l : lines_) widest = std::max(widest, textW(r, l));
  int btnH = lh + 8, btnsW = -kGap;
  for (Choice& b : choices_) {
    b.w = textW(r, b.label) + 16;
    b.h = btnH;
    btnsW += b.w + kGap;
  }
  w_ = std::min(r.width() - 4, std::max(widest, btnsW) + 2 * kPad);
  h_ = kPad + (int)lines_.size() * rowH_ + kPad / 2 + btnH + kPad;
  x_ = (r.width() - w_) / 2;
  y_ = std::max(2, (r.height() - h_) / 2);
  textY_ = y_ + kPad;
  int bx = x_ + (w_ - btnsW) / 2, by = y_ + h_ - kPad - btnH;
  for (Choice& b : choices_) {
    b.x = bx;
    b.y = by;
    bx += b.w + kGap;
  }
}

int ConfirmBox::buttonAt(int px, int py) const {
  for (size_t i = 0; i < choices_.size(); ++i) {
    const Choice& b = choices_[i];
    if (px >= b.x && py >= b.y && px < b.x + b.w && py < b.y + b.h) return (int)i;
  }
  return -1;
}

ConfirmBox::Result ConfirmBox::onInput(const InputEvent& e, const Renderer& r) {
  if (!open_) return None;
  layout(r);
  int n = (int)choices_.size();
  const Choice& dismiss = choices_.front();  // "No", or the single "OK"
  if (e.type == InputEvent::PointerMove) {
    int i = buttonAt(e.x, e.y);
    if (i >= 0) sel_ = i;
  } else if (e.type == InputEvent::PointerDown) {
    int i = buttonAt(e.x, e.y);
    if (i >= 0) return answer(choices_[(size_t)i]);
    if (e.x < x_ || e.y < y_ || e.x >= x_ + w_ || e.y >= y_ + h_) return answer(dismiss);
  } else if (e.type == InputEvent::ButtonDown) {
    if (e.button == Button::Left || e.button == Button::Up || e.button == Button::Prev) sel_ = (sel_ + n - 1) % n;
    else if (e.button == Button::Right || e.button == Button::Down || e.button == Button::Next) sel_ = (sel_ + 1) % n;
    else if (e.button == Button::Confirm) return answer(choices_[(size_t)sel_]);
    else if (e.button == Button::Cancel || e.button == Button::Menu) return answer(dismiss);
  }
  return None;
}

void ConfirmBox::draw(Renderer& r) {
  if (!open_) return;
  layout(r);
  r.fillRect(0, 0, r.width(), r.height(), Color(0, 0, 0, 150));
  r.fillRect(x_ + 2, y_ + 2, w_, h_, Color(0, 0, 0, 110));
  r.fillRect(x_, y_, w_, h_, chrome::kPanel);
  r.drawRect(x_, y_, w_, h_, Color(200, 205, 230));
  for (size_t i = 0; i < lines_.size(); ++i)
    drawTextCentered(r, x_ + w_ / 2, textY_ + (int)i * rowH_, lines_[i], chrome::kText);
  for (size_t i = 0; i < choices_.size(); ++i) {
    const Choice& b = choices_[i];
    bool sel = (int)i == sel_;
    drawRowBox(r, b.x, b.y, b.w, b.h, sel);
    drawTextCentered(r, b.x + b.w / 2, b.y + 4, b.label, sel ? chrome::kTextSel : chrome::kText);
  }
}

}  // namespace ote
