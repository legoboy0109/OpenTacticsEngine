#include "scene_faction.h"

#include <algorithm>

#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

FactionScene::FactionScene(SceneHost& host, const std::string& title, std::function<void(int)> onPick, std::function<void()> onCancel)
    : host_(host), list_(host.session()->db.factions), title_(title), onPick_(std::move(onPick)), onCancel_(std::move(onCancel)) {
  rows_.count = (int)list_.size();
  rows_.sel = 0;
  OTE_INFO("faction choice: %s picks from %d factions", title_.c_str(), rows_.count);
}

namespace {
template <class T>
std::string joinNames(const std::vector<int>& idx, const std::vector<T>& defs) {
  std::string out;
  for (int i : idx) out += (out.empty() ? "" : ", ") + defs[(size_t)i].name;
  return out.empty() ? "nothing" : out;
}
}  // namespace

std::vector<std::string> FactionScene::facts(const Database& db, const FactionDef& f) {
  std::vector<std::string> out;
  // The order is the order of importance: on a small screen the last lines are the ones that do not fit.
  if (f.limitUnits) out.push_back("Units: " + joinNames(f.units, db.unitTypes));
  std::string start;
  for (const Amount& a : f.start) start += (start.empty() ? "+" : ", +") + intStr(a.amount) + " " + db.resources[(size_t)a.resource].name;
  if (!start.empty()) out.push_back("Starts with: " + start);
  std::string skills;
  for (const SkillGrant& g : f.modifiers) skills += (skills.empty() ? "" : ", ") + db.skills[(size_t)g.skill].name;
  if (!skills.empty()) out.push_back("Skills: " + skills);
  if (f.limitStructures) out.push_back("Builds: " + joinNames(f.structures, db.structures));
  if (f.limitTechs) out.push_back("Research: " + joinNames(f.techs, db.research));
  return out;
}

void FactionScene::layout(const Renderer& r) {
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

void FactionScene::activate(int row) {
  if (row < 0 || row >= rows_.count) return;
  uiCue(Cue::Confirm);
  OTE_INFO("faction choice: %s picked \"%s\"", title_.c_str(), list_[(size_t)row].id.c_str());
  onPick_(row);
}

void FactionScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
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

void FactionScene::draw(Renderer& r) {
  layout(r);
  const int lh = r.lineHeight(), rh = lh + 2;
  drawTitle(r, title_.empty() ? "Faction" : title_ + " - faction");
  const int first = rows_.first();
  for (int i = 0; i < rows_.visible() && first + i < rows_.count; ++i) {
    const int row = first + i, ry = rows_.rowY(row);
    const bool sel = row == rows_.sel;
    drawRowBox(r, rows_.x, ry, rows_.w, rows_.rowH - 3, sel);
    r.drawText(rows_.x + 8, ry + 3, fitText(r, list_[(size_t)row].name, rows_.w - 16).c_str(), sel ? chrome::kTextSel : chrome::kText);
  }
  rows_.drawMore(r, chrome::kAccent);
  // The selected faction's description and what it brings, under the list.
  const FactionDef& f = list_[(size_t)rows_.sel];
  const int dw = std::min(r.width() - 16, 400), dx = (r.width() - dw) / 2;
  int y = rows_.y + rows_.h + (first + rows_.visible() < rows_.count ? lh : 0) + 6;
  const int bottom = r.height() - footerHeight(r);
  const int maxLines = std::max(0, (bottom - y) / rh);
  std::vector<std::string> lines;
  if (!f.description.empty()) lines = wrapText(r, f.description, dw);
  std::vector<std::string> facts;
  for (const std::string& line : FactionScene::facts(host_.session()->db, f))
    for (const std::string& w : wrapText(r, line, dw)) facts.push_back(w);
  int used = 0;
  // The description gives way to the facts when the room is short (the facts are what differs between factions), but keeps a line.
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
