#include "scene_difficulty.h"

#include <algorithm>

#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

namespace {
const char* const kDefaultTag = "default";
}

DifficultyScene::DifficultyScene(SceneHost& host, const std::string& title, std::function<void(const std::string&)> onPick,
                                 std::function<void()> onCancel)
    : host_(host), rules_(host.session()->db.difficulties), title_(title), onPick_(std::move(onPick)), onCancel_(std::move(onCancel)) {
  list_.count = (int)rules_.list.size();
  list_.sel = rules_.defaultIndex;  // the game's default is preselected
  OTE_INFO("difficulty choice: %d difficulties, default \"%s\"", list_.count, rules_.defaultId.c_str());
}

std::string DifficultyScene::effects(const Database& db, const DifficultyDef& d) {
  std::vector<std::string> parts;
  const DifficultyEnemy& e = d.enemy;
  std::string foes;
  if (e.levels != 0) foes += (e.levels > 0 ? "+" : "") + intStr(e.levels) + (std::abs(e.levels) == 1 ? " level" : " levels");
  for (size_t s = 0; s < e.stats.size() && s < db.stats.size(); ++s)
    if (e.stats[s] != 0) foes += (foes.empty() ? "" : ", ") + std::string(e.stats[s] > 0 ? "+" : "") + intStr(e.stats[s]) + " " + db.stats[s].name;
  if (e.statsPercent != 100) foes += (foes.empty() ? "" : ", ") + std::string("stats ") + intStr(e.statsPercent) + "%";
  if (!foes.empty()) parts.push_back("Enemies: " + foes);
  if (d.xpPercent != 100) parts.push_back("Experience " + intStr(d.xpPercent) + "%");
  if (d.resourcePercent != 100) parts.push_back("Resources " + intStr(d.resourcePercent) + "%");
  if (parts.empty()) return "Enemies, experience and resources as the game made them.";
  std::string out;
  for (size_t i = 0; i < parts.size(); ++i) out += (i ? ". " : "") + parts[i];
  return out + ".";
}

void DifficultyScene::layout(const Renderer& r) {
  const int lh = r.lineHeight(), rh = lh + 2;
  const int top = 8 + r.lineHeight(lh <= 11 ? 2 : 1) + 8 + lh;  // title block + room for "^"
  list_.rowH = lh + 8;
  list_.w = std::min(r.width() - 24, 320);
  list_.x = (r.width() - list_.w) / 2;
  list_.y = top;
  // Keep room for about five lines of description under the list; a long list scrolls.
  const int room = r.height() - top - footerHeight(r) - lh - 5 * rh;
  list_.h = std::max(list_.rowH, std::min(list_.count * list_.rowH, room));
}

void DifficultyScene::activate(int row) {
  if (row < 0 || row >= list_.count) return;
  uiCue(Cue::Confirm);
  OTE_INFO("difficulty choice: picked \"%s\"", rules_.list[(size_t)row].id.c_str());
  onPick_(rules_.list[(size_t)row].id);
}

void DifficultyScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (e.type == InputEvent::PointerMove) {
    const int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    const int row = list_.rowAt(e.x, e.y);
    if (row >= 0) { list_.sel = row; activate(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return;
    if (e.button == Button::Confirm) activate(list_.sel);
    else if (e.button == Button::Cancel) { uiCue(Cue::Cancel); onCancel_(); }
  }
}

void DifficultyScene::draw(Renderer& r) {
  layout(r);
  const int lh = r.lineHeight(), rh = lh + 2;
  drawTitle(r, title_.empty() ? "Difficulty" : title_ + " - difficulty");
  const int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < list_.count; ++i) {
    const int row = first + i, ry = list_.rowY(row);
    const bool sel = row == list_.sel;
    drawRowBox(r, list_.x, ry, list_.w, list_.rowH - 3, sel);
    const DifficultyDef& d = rules_.list[(size_t)row];
    const int tagW = row == rules_.defaultIndex ? textW(r, kDefaultTag) + 8 : 0;
    r.drawText(list_.x + 8, ry + 3, fitText(r, d.name, list_.w - 16 - tagW).c_str(), sel ? chrome::kTextSel : chrome::kText);
    if (tagW) drawTextRight(r, list_.x + list_.w - 8, ry + 3, kDefaultTag, chrome::kDim);
  }
  list_.drawMore(r, chrome::kAccent);
  // The selected difficulty's description and what it changes, under the list.
  const DifficultyDef& d = rules_.list[(size_t)list_.sel];
  const int dw = std::min(r.width() - 16, 400), dx = (r.width() - dw) / 2;
  int y = list_.y + list_.h + (first + list_.visible() < list_.count ? lh : 0) + 6;
  const int bottom = r.height() - footerHeight(r);
  const int maxLines = std::max(0, (bottom - y) / rh);
  std::vector<std::string> lines;
  if (!d.description.empty()) lines = wrapText(r, d.description, dw);
  std::vector<std::string> fx = wrapText(r, effects(host_.session()->db, d), dw);
  int used = 0;
  for (const std::string& l : lines) {
    if (used >= maxLines - (int)std::min<size_t>(fx.size(), 1)) break;
    r.drawText(dx, y + used * rh, l.c_str(), chrome::kText);
    ++used;
  }
  for (const std::string& l : fx) {
    if (used >= maxLines) break;
    r.drawText(dx, y + used * rh, l.c_str(), chrome::kDim);
    ++used;
  }
  drawFooter(r, "Up/Down  Confirm  Cancel: back");
}

}  // namespace ote
