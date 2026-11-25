#include "ui/name_entry.h"

#include <algorithm>
#include <cstdio>

#include "core/data/name_tokens.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

namespace {
constexpr int kCols = 10;
// The grid: four rows of ten single keys, then the four wide ones. "Spc" types a space.
const char* const kRows[4] = {"ABCDEFGHIJ", "KLMNOPQRST", "UVWXYZ_-'.", "0123456789"};
struct Wide { const char* label; int span; };
const Wide kWide[4] = {{"Case", 3}, {"Del", 2}, {"Reset", 2}, {"OK", 3}};

// The number of letters (UTF-8 code points) in `s`.
int countUtf8(const std::string& s) {
  int n = 0;
  for (unsigned char c : s)
    if ((c & 0xC0) != 0x80) ++n;
  return n;
}
void popUtf8(std::string& s) {
  while (!s.empty()) {
    const unsigned char c = (unsigned char)s.back();
    s.pop_back();
    if ((c & 0xC0) != 0x80) return;
  }
}
std::string trimmed(const std::string& s) {
  size_t a = s.find_first_not_of(' '), b = s.find_last_not_of(' ');
  return a == std::string::npos ? std::string() : s.substr(a, b - a + 1);
}
}  // namespace

int NameEntry::letters() const { return countUtf8(name_); }

void NameEntry::open(const Renderer& r, int screenW, int bottom, const std::string& who, const std::string& prompt,
                     const std::string& start, int maxLetters) {
  open_ = true;
  who_ = who;
  prompt_ = prompt;
  max_ = std::max(1, std::min(kMaxNameLetters, maxLetters));
  start_ = start;
  while (countUtf8(start_) > max_) popUtf8(start_);  // (a default longer than the limit is cut)
  name_ = start_;
  upper_ = true;
  lh_ = r.lineHeight();
  keys_.clear();
  row_.clear();
  col_.clear();
  span_.clear();
  for (int row = 0; row < 4; ++row)
    for (int c = 0; c < kCols; ++c) {
      keys_.push_back(Key{std::string(1, kRows[row][c]), 0, 0, 0, 0});
      row_.push_back(row);
      col_.push_back(c);
      span_.push_back(1);
    }
  int c = 0;
  for (const Wide& w : kWide) {
    keys_.push_back(Key{w.label, 0, 0, 0, 0});
    row_.push_back(4);
    col_.push_back(c);
    span_.push_back(w.span);
    c += w.span;
  }
  relabel();
  // The panel: the heading (wrapped), the name being typed, then the grid; centred in the area, as wide as the keys need.
  const int pad = std::max(4, lh_ / 2), gap = 2;
  const int keyH = lh_ + 6;
  int keyW = std::max(textW(r, "W") + 6, textW(r, "Reset") / 2 + 4);
  keyW = std::max(keyW, std::min((screenW - 8 - 2 * pad) / kCols - gap, lh_ * 2));
  panelW_ = std::min(screenW - 4, kCols * (keyW + gap) - gap + 2 * pad);
  keyW = (panelW_ - 2 * pad + gap) / kCols - gap;
  std::string heading = prompt_.empty() ? "Name: " + who_ : prompt_;
  promptLines_ = wrapText(r, heading, panelW_ - 2 * pad);
  if (promptLines_.size() > 3) promptLines_.resize(3);
  const int headH = (int)promptLines_.size() * (lh_ + 2);
  panelH_ = pad + headH + 4 + (lh_ + 6) + 6 + 5 * (keyH + gap) - gap + pad;
  panelX_ = (screenW - panelW_) / 2;
  panelY_ = std::max(2, (bottom - panelH_) / 2);
  fieldY_ = panelY_ + pad + headH + 4;
  const int gridY = fieldY_ + lh_ + 6 + 6;
  for (size_t k = 0; k < keys_.size(); ++k) {
    Key& key = keys_[k];
    key.x = panelX_ + pad + col_[k] * (keyW + gap);
    key.y = gridY + row_[k] * (keyH + gap);
    key.w = span_[k] * (keyW + gap) - gap;
    key.h = keyH;
  }
  sel_ = 0;
}

// The letter keys show the case they type; "_" stands for the space.
void NameEntry::relabel() {
  for (size_t k = 0; k < 40 && k < keys_.size(); ++k) {
    char ch = kRows[k / kCols][k % kCols];
    if (ch >= 'A' && ch <= 'Z' && !upper_) ch = (char)(ch - 'A' + 'a');
    keys_[k].label = ch == '_' ? "Spc" : std::string(1, ch);
  }
}

int NameEntry::keyIndex(const std::string& label) const {
  for (size_t k = 0; k < keys_.size(); ++k)
    if (keys_[k].label == label) return (int)k;
  return -1;
}

int NameEntry::keyAt(int x, int y) const {
  for (size_t k = 0; k < keys_.size(); ++k) {
    const Key& key = keys_[k];
    if (x >= key.x && x < key.x + key.w && y >= key.y && y < key.y + key.h) return (int)k;
  }
  return -1;
}

void NameEntry::move(int dx, int dy) {
  const int k = sel_;
  if (dx != 0) {  // along the row, wrapping
    std::vector<int> row;
    for (size_t i = 0; i < keys_.size(); ++i)
      if (row_[i] == row_[(size_t)k]) row.push_back((int)i);
    const int at = (int)(std::find(row.begin(), row.end(), k) - row.begin());
    sel_ = row[(size_t)((at + dx + (int)row.size()) % (int)row.size())];
    return;
  }
  // Up / down: the key of the next row under the middle of this one (wrapping from the bottom row to the top).
  const int mid = col_[(size_t)k] * 2 + span_[(size_t)k] - 1;  // (in half columns)
  const int nextRow = (row_[(size_t)k] + dy + 5) % 5;
  for (size_t i = 0; i < keys_.size(); ++i)
    if (row_[i] == nextRow && col_[i] * 2 <= mid && mid < (col_[i] + span_[i]) * 2) {
      sel_ = (int)i;
      return;
    }
}

void NameEntry::press(int k) {
  if (k < 0 || k >= (int)keys_.size()) return;
  sel_ = k;
  const std::string& label = keys_[(size_t)k].label;
  if (label == "Case") {
    upper_ = !upper_;
    relabel();
  } else if (label == "Del") {
    popUtf8(name_);
  } else if (label == "Reset") {
    name_ = start_;
  } else if (label == "OK") {
    const std::string t = trimmed(name_);
    if (!validEnteredName(t)) return;  // nothing typed (or only spaces): OK waits
    name_ = t;
    open_ = false;
  } else if (letters() < max_) {
    name_ += label == "Spc" ? " " : label;
  }
}

NameEntry::Result NameEntry::onInput(const InputEvent& e) {
  if (!open_) return None;
  if (e.type == InputEvent::PointerMove) {
    const int k = keyAt(e.x, e.y);
    if (k >= 0) sel_ = k;
    return None;
  }
  if (e.type == InputEvent::PointerDown) {
    press(keyAt(e.x, e.y));
    return open_ ? None : Done;
  }
  if (e.type != InputEvent::ButtonDown) return None;
  switch (e.button) {
    case Button::Left: move(-1, 0); break;
    case Button::Right: move(1, 0); break;
    case Button::Up: move(0, -1); break;
    case Button::Down: move(0, 1); break;
    case Button::Confirm: press(sel_); break;
    case Button::Cancel: popUtf8(name_); break;
    case Button::Menu: sel_ = keyIndex("OK"); break;
    case Button::Skip: press(keyIndex("OK")); break;  // the "skip everything" script token: take the name as it stands
    default: break;
  }
  return open_ ? None : Done;
}

std::string NameEntry::describeKeys() const {
  std::string out = "name entry: keys";
  for (const char* l : {"OK", "Del", "Case", "Reset", "A", "E", "I", "R", "K", "a"}) {
    const int k = keyIndex(l);
    if (k < 0) continue;
    const Key& key = keys_[(size_t)k];
    char buf[48];
    std::snprintf(buf, sizeof buf, " %s@%d,%d", l, key.x + key.w / 2, key.y + key.h / 2);
    out += buf;
  }
  return out;
}

void NameEntry::draw(Renderer& r) const {
  if (!open_) return;
  using namespace chrome;
  const int pad = std::max(4, lh_ / 2);
  r.fillRect(panelX_ + 2, panelY_ + 2, panelW_, panelH_, Color(0, 0, 0, 110));
  r.fillRect(panelX_, panelY_, panelW_, panelH_, Color(20, 26, 64, 245));
  r.drawRect(panelX_, panelY_, panelW_, panelH_, Color(220, 224, 240));
  r.drawRect(panelX_ + 2, panelY_ + 2, panelW_ - 4, panelH_ - 4, Color(110, 120, 180));
  int y = panelY_ + pad;
  for (const std::string& l : promptLines_) {
    drawTextCentered(r, panelX_ + panelW_ / 2, y, l, kTitle);
    y += lh_ + 2;
  }
  // The name, with a line for every letter still free, and "n/max" at the right.
  const int fx = panelX_ + pad, fw = panelW_ - 2 * pad;
  r.fillRect(fx, fieldY_, fw, lh_ + 6, Color(8, 10, 24, 255));
  r.drawRect(fx, fieldY_, fw, lh_ + 6, Color(150, 156, 185));
  std::string shown = name_;
  if (letters() < max_) shown += "_";
  r.drawText(fx + 4, fieldY_ + 3, fitText(r, shown, fw - 8 - textW(r, "00/00 ")).c_str(), kTextSel);
  const std::string count = std::to_string(letters()) + "/" + std::to_string(max_);
  r.drawText(fx + fw - 4 - textW(r, count), fieldY_ + 3, count.c_str(), kDim);
  const bool okReady = validEnteredName(trimmed(name_));
  for (size_t k = 0; k < keys_.size(); ++k) {
    const Key& key = keys_[k];
    const bool hi = (int)k == sel_;
    const bool off = key.label == "OK" && !okReady;
    r.fillRect(key.x, key.y, key.w, key.h, hi ? Color(70, 90, 170, 255) : Color(30, 36, 80, 255));
    r.drawRect(key.x, key.y, key.w, key.h, hi ? kAccent : Color(90, 98, 140));
    drawTextCentered(r, key.x + key.w / 2, key.y + 3, key.label, off ? kDim : hi ? kTextSel : kText);
  }
}

}  // namespace ote
