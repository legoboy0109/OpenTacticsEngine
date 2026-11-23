#include "scene_list.h"

#include <algorithm>

#include "sound/cue_sink.h"
#include "core/base/log.h"
#include "ui/chrome.h"
#include "ui/input_hints.h"
#include "ui/text.h"

namespace ote {

ListScene::ListScene(SceneHost& host, std::string title, std::vector<ListEntry> entries, std::string footer)
    : host_(host), title_(std::move(title)), entries_(std::move(entries)), footer_(std::move(footer)) {
  list_.count = (int)entries_.size();
}

// All vertical metrics come from the active font.
void ListScene::layout(const Renderer& r) {
  int lh = r.lineHeight();
  list_.rowH = 2 * lh + 8;
  int top = 8 + r.lineHeight(lh <= 11 ? 2 : 1) + 8 + lh;  // below the title, room for the "^" marker
  int bottom = footerHeight(r) + lh;                        // above the footer, room for "v"
  list_.w = std::min(r.width() - 24, 320);
  list_.x = (r.width() - list_.w) / 2;
  list_.y = top;
  list_.h = std::max(list_.rowH, r.height() - top - bottom);
  list_.count = (int)entries_.size();
}

void ListScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (list_.input(e)) return;  // the wheel, a "^" / "v" marker, a finger's drag (ui/row_list.h)
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) { list_.sel = row; uiCue(Cue::Confirm); onChoose(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return;
    if (e.button == Button::Confirm && !entries_.empty()) { uiCue(Cue::Confirm); onChoose(list_.sel); }
    else if (e.button == Button::Cancel) { uiCue(Cue::Cancel); onCancel(); }
  }
}

void ListScene::draw(Renderer& r) {
  layout(r);
  const int lh = r.lineHeight();
  drawTitle(r, title_);
  if (entries_.empty())
    drawTextCentered(r, r.width() / 2, list_.y + 20, "(nothing here)", chrome::kDim);
  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < (int)entries_.size(); ++i) {
    const ListEntry& it = entries_[first + i];
    int y = list_.rowY(first + i);
    bool sel = first + i == list_.sel;
    drawRowBox(r, list_.x, y, list_.w, list_.rowH - 3, sel);
    int textW_ = list_.w - 16;
    r.drawText(list_.x + 8, y + 3, fitText(r, it.label, textW_).c_str(), sel ? chrome::kTextSel : chrome::kText);
    if (!it.detail.empty())
      r.drawText(list_.x + 8, y + 3 + lh + 1, fitText(r, it.detail, textW_).c_str(), Color(165, 172, 205));
  }
  list_.drawMore(r, chrome::kAccent);
  drawFooter(r, footer_);
}

void ErrorScene::onInput(const InputEvent& e) {
  bool back = e.type == InputEvent::PointerDown ||
              (e.type == InputEvent::ButtonDown &&
               (e.button == Button::Cancel || e.button == Button::Confirm || e.button == Button::Menu));
  if (!back) return;
  uiCue(Cue::Cancel);
  if (back_ == SceneHost::ToMapSelect) host_.showMapSelect();
  else if (back_ == SceneHost::ToGame && !host_.currentGameId().empty()) host_.showGame(host_.currentGameId());
  else host_.showHome();
}

void ErrorScene::draw(Renderer& r) {
  int w = r.width(), h = r.height(), lh = r.lineHeight(), rh = lh + 2;
  int head = lh + 18;
  r.clear(Color(40, 14, 18));
  r.fillRect(0, 0, w, head, Color(150, 32, 40));
  r.drawText(10, (head - lh) / 2, fitText(r, title_, w - 20).c_str(), Color(255, 255, 255));
  int maxLines = std::max(1, (h - head - lh - 16) / rh);
  std::vector<std::string> lines = wrapText(r, message_, w - 20);
  for (int i = 0; i < (int)lines.size() && i < maxLines; ++i)
    r.drawText(10, head + 10 + i * rh, lines[i].c_str(), Color(255, 220, 215));
  if ((int)lines.size() > maxLines)
    r.drawText(10, head + 10 + maxLines * rh, "... (see log for full text)", Color(200, 150, 150));
  drawTextCentered(r, w / 2, h - lh - 4, hintText("{cancelTap} to go back"), Color(190, 140, 140));
}

}  // namespace ote
