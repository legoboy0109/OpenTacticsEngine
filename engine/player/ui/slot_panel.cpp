#include "ui/slot_panel.h"

#include <algorithm>

#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kPad = 8;
const Color kGreyed(140, 143, 165), kReason(225, 150, 130);
}  // namespace

void SlotPanel::open(Mode mode, const std::vector<SaveInfo>& slots, const std::string& loadWarning) {
  open_ = true;
  mode_ = mode;
  slots_ = slots;
  slots_.resize(SaveStore::kSlots);
  loadWarning_ = loadWarning;
  list_ = RowList();
  list_.count = SaveStore::kSlots;
  confirm_.close();
  pending_ = -1;
  picked_ = 0;
  // Start on the first slot that makes sense: an empty one to save into, a used one to load.
  for (int i = 0; i < SaveStore::kSlots; ++i)
    if (mode == Save ? slots_[(size_t)i].state == SaveInfo::Empty : slots_[(size_t)i].state == SaveInfo::Ok) {
      list_.sel = i;
      break;
    }
}

void SlotPanel::fitScreen(const Renderer& r) {
  int w = std::min(r.width() - 12, 380), lh = r.lineHeight();
  // Exactly what layout() needs for five rows (when the screen is shorter the rows scroll).
  int wanted = kPad + lh + kPad + SaveStore::kSlots * (2 * lh + 10) + 4 + lh + kPad / 2 + 2;
  int h = std::min(r.height() - 12, wanted);
  setArea((r.width() - w) / 2, (r.height() - h) / 2, w, h);
}

void SlotPanel::showMessage(const std::string& text) {
  pending_ = -1;
  confirm_.open(text, "OK", "");
}

// All sizes come from the active font; rows scroll when five do not fit.
void SlotPanel::layout(const Renderer& r) {
  int lh = r.lineHeight();
  titleY_ = ay_ + kPad;
  footY_ = ay_ + ah_ - lh - kPad / 2 - 1;
  innerX_ = ax_ + kPad;
  innerW_ = std::max(40, aw_ - 2 * kPad);
  int top = titleY_ + lh + kPad, bottom = footY_ - 4;
  int rowH = 2 * lh + 10;
  int avail = bottom - top;
  if (SaveStore::kSlots * rowH > avail) {  // scrolling: leave room for the "^" and "v" markers
    top += lh + 1;
    avail -= 2 * (lh + 1);
  }
  list_.rowH = rowH;
  list_.x = innerX_;
  list_.w = innerW_;
  list_.y = top;
  list_.h = std::max(rowH, std::min(SaveStore::kSlots * rowH, avail));
  list_.count = SaveStore::kSlots;
}

SlotPanel::Result SlotPanel::activate(int row) {
  const SaveInfo& s = slots_[(size_t)row];
  int n = row + 1;
  if (mode_ == Save) {
    picked_ = n;
    if (s.state == SaveInfo::Empty) return Picked;
    pending_ = row;
    confirm_.open("Replace the save in slot " + std::to_string(n) + "?", "Replace", "Cancel");
    return None;
  }
  if (s.state != SaveInfo::Ok) return None;  // greyed out: the reason is on screen
  picked_ = n;
  if (loadWarning_.empty()) return Picked;
  pending_ = row;
  confirm_.open("Load slot " + std::to_string(n) + "? " + loadWarning_, "Load", "Cancel");
  return None;
}

SlotPanel::Result SlotPanel::onInput(const InputEvent& e, const Renderer& r) {
  if (!open_) return None;
  if (confirm_.isOpen()) {
    ConfirmBox::Result a = confirm_.onInput(e, r);
    if (a == ConfirmBox::None) return None;
    confirm_.close();
    bool wasQuestion = pending_ >= 0;
    pending_ = -1;
    return a == ConfirmBox::Yes && wasQuestion ? Picked : None;
  }
  layout(r);
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) { list_.sel = row; return activate(row); }
    if (e.x < ax_ || e.y < ay_ || e.x >= ax_ + aw_ || e.y >= ay_ + ah_) return Back;
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return None;
    if (e.button == Button::Confirm) return activate(list_.sel);
    if (e.button == Button::Cancel || e.button == Button::Menu) return Back;
  }
  return None;
}

void SlotPanel::draw(Renderer& r) {
  if (!open_) return;
  layout(r);
  const int lh = r.lineHeight();
  r.fillRect(ax_, ay_, aw_, ah_, chrome::kBg);
  r.drawRect(ax_, ay_, aw_, ah_, Color(200, 205, 230));
  drawTextCentered(r, ax_ + aw_ / 2, titleY_, mode_ == Save ? "Save game" : "Load game", chrome::kAccent);

  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < list_.count; ++i) {
    int row = first + i, y = list_.rowY(row);
    const SaveInfo& s = slots_[(size_t)row];
    bool sel = row == list_.sel;
    drawRowBox(r, list_.x, y, list_.w, list_.rowH - 3, sel);
    // A slot cannot be loaded when it is empty or unusable; in the Save list only
    // the empty ones look dim, because a used slot can still be written over.
    bool greyed = s.state == SaveInfo::Unusable || (mode_ == Load && s.state == SaveInfo::Empty);
    std::string label = std::to_string(row + 1) + ".  ";
    std::string detail;
    Color detailColor = chrome::kDim;
    if (s.state == SaveInfo::Empty) {
      label += "Empty";
    } else {
      label += s.mapName.empty() ? "Unusable save" : s.mapName;
      if (s.state == SaveInfo::Ok) {
        detail = "Turn " + std::to_string(s.round);
        std::string when = formatSavedAt(s.savedAt);
        if (!when.empty()) detail += " - " + when;
      } else {
        detail = s.reason;
        detailColor = kReason;
      }
    }
    int textW_ = list_.w - 16;
    int ly = detail.empty() ? y + (list_.rowH - 3 - lh) / 2 : y + 4;  // a lone line sits in the middle
    r.drawText(list_.x + 8, ly, fitText(r, label, textW_).c_str(),
               greyed ? kGreyed : (sel ? chrome::kTextSel : chrome::kText));
    if (!detail.empty()) r.drawText(list_.x + 8, y + 4 + lh + 2, fitText(r, detail, textW_).c_str(), detailColor);
  }
  list_.drawMore(r, chrome::kAccent);

  // Bottom line: why the selected save cannot be loaded, else the keys.
  const SaveInfo& cur = slots_[(size_t)list_.sel];
  if (cur.state == SaveInfo::Unusable) {
    drawTextCentered(r, ax_ + aw_ / 2, footY_, fitText(r, cur.reason, aw_ - 2 * kPad), kReason);
  } else {
    const char* hint = mode_ == Save ? "Confirm: save here   Cancel: back" : "Confirm: load   Cancel: back";
    drawTextCentered(r, ax_ + aw_ / 2, footY_, fitText(r, hint, aw_ - 2 * kPad), chrome::kHint);
  }
  confirm_.draw(r);
}

}  // namespace ote
