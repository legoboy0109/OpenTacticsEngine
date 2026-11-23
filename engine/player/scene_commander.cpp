#include "scene_commander.h"

#include <algorithm>

#include "battle/commander_text.h"
#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

CommanderScene::CommanderScene(SceneHost& host, const std::string& title, std::vector<int> choices, std::function<void(int)> onPick,
                               std::function<void()> onCancel)
    : host_(host), list_(host.session()->db.commanders), choices_(std::move(choices)), title_(title), onPick_(std::move(onPick)),
      onCancel_(std::move(onCancel)) {
  rows_.count = (int)choices_.size();
  rows_.sel = 0;
  OTE_INFO("commander choice: %s picks from %d commanders", title_.c_str(), rows_.count);
}

void CommanderScene::layout(const Renderer& r) {
  const int lh = r.lineHeight(), rh = lh + 2;
  const int top = 8 + r.lineHeight(lh <= 11 ? 2 : 1) + 8 + lh;  // title block + room for "^"
  rows_.rowH = lh + 8;
  rows_.w = std::min(r.width() - 24, 320);
  rows_.x = (r.width() - rows_.w) / 2;
  rows_.y = top;
  // Room for about five lines of text under the list; a long list scrolls.
  const int room = r.height() - top - footerHeight(r) - lh - 5 * rh;
  rows_.h = std::max(rows_.rowH, std::min(rows_.count * rows_.rowH, room));
}

void CommanderScene::activate(int row) {
  if (row < 0 || row >= rows_.count) return;
  uiCue(Cue::Confirm);
  OTE_INFO("commander choice: %s picked \"%s\"", title_.c_str(), list_[(size_t)choices_[(size_t)row]].id.c_str());
  onPick_(choices_[(size_t)row]);
}

void CommanderScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (rows_.input(e)) return;  // the wheel, a "^" / "v" marker, a finger's drag (ui/row_list.h)
  if (e.type == InputEvent::PointerMove) {
    const int row = rows_.rowAt(e.x, e.y);
    if (row >= 0) rows_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    const int row = rows_.rowAt(e.x, e.y);
    if (row >= 0) { rows_.sel = row; activate(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (rows_.step(e.button)) return;
    if (e.button == Button::Confirm) activate(rows_.sel);
    else if (e.button == Button::Cancel) { uiCue(Cue::Cancel); onCancel_(); }
  }
}

void CommanderScene::draw(Renderer& r) {
  layout(r);
  const int lh = r.lineHeight(), rh = lh + 2;
  drawTitle(r, title_.empty() ? "Commander" : title_ + CommanderWords::kPickTitleSuffix);
  const int first = rows_.first();
  for (int i = 0; i < rows_.visible() && first + i < rows_.count; ++i) {
    const int row = first + i, ry = rows_.rowY(row);
    const bool sel = row == rows_.sel;
    drawRowBox(r, rows_.x, ry, rows_.w, rows_.rowH - 3, sel);
    r.drawText(rows_.x + 8, ry + 3, fitText(r, list_[(size_t)choices_[(size_t)row]].name, rows_.w - 16).c_str(), sel ? chrome::kTextSel : chrome::kText);
  }
  rows_.drawMore(r, chrome::kAccent);
  // The selected commander under the list: the description, then the passive skills, the powers and the meter's rule.
  const CommanderDef& d = list_[(size_t)choices_[(size_t)rows_.sel]];
  const int dw = std::min(r.width() - 16, 400), dx = (r.width() - dw) / 2;
  int y = rows_.y + rows_.h + (first + rows_.visible() < rows_.count ? lh : 0) + 6;
  const int bottom = r.height() - footerHeight(r);
  const int maxLines = std::max(0, (bottom - y) / rh);
  std::vector<std::string> lines;
  if (!d.description.empty()) lines = wrapText(r, d.description, dw);
  std::vector<std::string> facts;
  for (const std::string& line : commanderLines(host_.session()->db, d))
    for (const std::string& w : wrapText(r, line, dw)) facts.push_back(w);
  int used = 0;
  // As on the faction screen the description gives way to the facts when the room is short, but keeps a line.
  const int descLines = std::max(1, std::min((int)lines.size(), maxLines - (int)facts.size()));
  for (int i = 0; i < descLines && i < (int)lines.size() && used < maxLines; ++i) {
    const bool cut = i == descLines - 1 && descLines < (int)lines.size();
    r.drawText(dx, y + used * rh, (cut ? fitText(r, lines[(size_t)i] + " ...", dw) : lines[(size_t)i]).c_str(), chrome::kText);
    ++used;
  }
  for (const std::string& l : facts) {
    if (used >= maxLines) break;
    r.drawText(dx, y + used * rh, l.c_str(), chrome::kDim);
    ++used;
  }
  drawFooter(r, "{updown}  {confirm}  {cancelWord}: back");
}

}  // namespace ote
