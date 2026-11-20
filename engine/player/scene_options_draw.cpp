// Layout and drawing of the options screen. Everything is measured from the
// active font (which the screen itself re-applies live when an option
// changes), rows scroll, and the preview shrinks before anything overflows.
#include <algorithm>

#include "scene_options.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr int kMargin = 8, kGap = 6, kPad = 4;
const char* const kPangram = "The quick brown fox jumps over the lazy dog 0123456789";
const char* const kStatLine = "HP 24/30   Atk 12   Def 7   Mov 5";

// One block of the sample area; the preview shows as many as fit.
struct Item {
  enum Kind { Text, Stat, Swatches } kind;
  std::string text;
  int h;
};

}  // namespace

std::string OptionsScene::title() const {
  return perGame() && entry_ ? entry_->name + " options" : "Options";
}

// Blocks of the sample area, top to bottom.
static std::vector<Item> previewItems(const Renderer& r, int inner, bool swatches) {
  std::vector<Item> out;
  int lh = r.lineHeight(), rh = lh + 2;
  std::vector<std::string> lines = wrapText(r, kPangram, inner);
  for (size_t i = 0; i < lines.size() && i < 2; ++i) {
    std::string t = lines[i];
    if (i == 1 && lines.size() > 2) t = fitText(r, t + "..", inner);
    out.push_back({Item::Text, t, rh});
  }
  out.push_back({Item::Stat, fitText(r, kStatLine, inner), rh});
  if (swatches) out.push_back({Item::Swatches, "", lh + 2});
  return out;
}

void OptionsScene::layout(const Renderer& r) {
  int lh = r.lineHeight(), W = r.width(), H = r.height();
  list_.rowH = lh + 6;
  rowW_ = std::min(W - 2 * kMargin, 440);
  rowX_ = (W - rowW_) / 2;
  int top = 8 + r.lineHeight(lh <= 11 ? 2 : 1) + 8 + lh;  // title + room for the "^" marker
  int avail = H - top - footerHeight(r);                  // includes room for a "v" marker
  int rowsAll = list_.count * list_.rowH;

  // Sample area: gets what is left after at least three rows.
  std::vector<Item> items = previewItems(r, rowW_ - 2 * kPad, perGame() && dbLoaded_);
  int budget = avail - std::min(rowsAll, 3 * list_.rowH + lh) - kGap - 2 * kPad;
  int used = 0;
  previewH_ = 0;
  for (const Item& it : items) {
    if (used + it.h > budget) break;
    used += it.h;
    previewH_ = used + 2 * kPad;
  }
  int rowsRoom = avail - (previewH_ ? previewH_ + kGap : 0);
  bool scrolls = rowsAll > rowsRoom;
  list_.y = top;
  list_.x = rowX_;
  list_.w = rowW_;
  list_.h = scrolls ? std::max(1, (rowsRoom - lh) / list_.rowH) * list_.rowH : rowsAll;  // whole rows only
  previewY_ = list_.y + list_.h + (scrolls ? lh : 0) + kGap;

  int widest = 0;
  for (const OptionDef& d : optionDefs()) widest = std::max(widest, textW(r, d.label));
  int labelW = std::min(widest, rowW_ * 40 / 100);
  valueX_ = rowX_ + 8 + labelW + 8;
  arrowW_ = textW(r, "<") + 10;
}

void OptionsScene::drawRow(Renderer& r, int row) {
  int lh = r.lineHeight(), y = list_.rowY(row), h = list_.rowH - 2;
  bool sel = row == list_.sel;
  drawRowBox(r, rowX_, y, rowW_, h, sel);
  Color text = sel ? chrome::kTextSel : chrome::kText;
  int ty = y + (h - lh) / 2;
  if (row >= defCount()) {  // Reset / Back
    const char* name = row == resetRow() ? "Reset to defaults" : "Back";
    drawTextCentered(r, rowX_ + rowW_ / 2, ty, fitText(r, name, rowW_ - 16), sel ? chrome::kAccent : chrome::kDim);
    return;
  }
  bool over = overridden(row);
  if (over) r.fillRect(rowX_ + 1, y + 1, 3, h - 2, chrome::kAccent);  // "this game only" marker
  int labelW = valueX_ - 8 - (rowX_ + 8);
  r.drawText(rowX_ + 8, ty, fitText(r, optionDefs()[row].label, labelW).c_str(), text);

  int vx = valueX_, vw = rowX_ + rowW_ - 6 - valueX_;
  int idx = currentChoice(row);
  Color vc = over ? chrome::kAccent : (storedValue(row).isNull() ? chrome::kDim : text);
  Color arrows = sel ? chrome::kAccent : chrome::kDim;
  r.drawText(vx + 3, ty, "<", arrows);
  drawTextRight(r, vx + vw - 3, ty, ">", arrows);
  int inner = vw - 2 * arrowW_;
  std::string v = fitText(r, choiceLabel(row, idx), inner);
  drawTextCentered(r, vx + vw / 2, ty, v, vc);
}

void OptionsScene::drawPreview(Renderer& r) {
  if (previewH_ <= 0) return;
  int inner = rowW_ - 2 * kPad;
  r.fillRect(rowX_, previewY_, rowW_, previewH_, chrome::kPanel);
  r.drawRect(rowX_, previewY_, rowW_, previewH_, Color(52, 58, 88));
  int y = previewY_ + kPad, lh = r.lineHeight();
  for (const Item& it : previewItems(r, inner, perGame() && dbLoaded_)) {
    if (y + it.h > previewY_ + previewH_ - kPad + 1) break;
    if (it.kind == Item::Text) r.drawText(rowX_ + kPad, y, it.text.c_str(), chrome::kText);
    else if (it.kind == Item::Stat) r.drawText(rowX_ + kPad, y, it.text.c_str(), Color(150, 225, 160));
    else if (it.kind == Item::Swatches && !db_.styles.empty()) {
      // A few terrain colours of the style this game currently uses.
      int idx = std::min(host_.styleIndex(), (int)db_.styles.size() - 1);
      const StyleDef& st = db_.styles[idx];
      int sq = lh, x = rowX_ + kPad;
      if (st.hasBackground) {
        r.fillRect(x, y, sq, sq, Color::rgb(st.background));
        r.drawRect(x, y, sq, sq, Color(200, 205, 230));
        x += sq + kGap;
      }
      for (size_t t = 0; t < db_.terrains.size() && x + sq <= rowX_ + rowW_ - kPad; ++t) {
        int32_t c = t < st.terrainColors.size() ? st.terrainColors[t] : -1;
        r.fillRect(x, y, sq, sq, Color::rgb(c >= 0 ? (uint32_t)c : db_.terrains[t].color));
        x += sq + 2;
      }
    }
    y += it.h;
  }
}

void OptionsScene::draw(Renderer& r) {
  layout(r);
  drawTitle(r, title());
  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < list_.count; ++i) drawRow(r, first + i);
  list_.drawMore(r, chrome::kAccent);
  drawPreview(r);

  std::string hint = "{leftright}: change   {cancelWord}: back";
  if (list_.sel == resetRow())
    hint = perGame() ? "{confirm}: clear this game's overrides" : "{confirm}: clear all options";
  else if (list_.sel < defCount() && perGame()) hint += "   yellow = overridden";
  drawFooter(r, hint);
}

}  // namespace ote
