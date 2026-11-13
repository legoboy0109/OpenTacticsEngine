#include "scene_game.h"

#include <algorithm>

#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

GameScene::GameScene(SceneHost& host, const GameEntry& game) : host_(host), game_(game) {
  list_.count = kRows;
}

void GameScene::layout(const Renderer& r) {
  int lh = r.lineHeight();
  int top = 8 + r.lineHeight(lh <= 11 ? 2 : 1) + 8 + lh;  // title + room for "^"
  list_.rowH = lh + 8;
  list_.w = std::min(r.width() - 24, 260);
  list_.x = (r.width() - list_.w) / 2;
  list_.y = top;
  int room = r.height() - top - footerHeight(r) - lh;
  list_.h = std::max(list_.rowH, std::min(kRows * list_.rowH, room));
}

void GameScene::activate(int row) {
  if (row == kPlay) host_.loadGame(game_.id);
  else if (row == kOptions) host_.showOptions(game_.id);
  else if (row == kBack) host_.showHome();
}

void GameScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) { list_.sel = row; activate(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return;
    if (e.button == Button::Confirm) activate(list_.sel);
    else if (e.button == Button::Cancel) host_.showHome();
  }
}

void GameScene::draw(Renderer& r) {
  layout(r);
  drawTitle(r, game_.name);
  static const char* const kLabels[kRows] = {"Play", "Game options", "Back"};
  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < kRows; ++i) {
    int row = first + i, ry = list_.rowY(row);
    bool sel = row == list_.sel;
    drawRowBox(r, list_.x, ry, list_.w, list_.rowH - 3, sel);
    r.drawText(list_.x + 8, ry + 3, fitText(r, kLabels[row], list_.w - 16).c_str(),
               sel ? chrome::kTextSel : chrome::kText);
  }
  list_.drawMore(r, chrome::kAccent);

  // Description, author and map count in the space left under the rows.
  int lh = r.lineHeight(), rh = lh + 2;
  int dy = list_.y + list_.h + (list_.first() + list_.visible() < kRows ? lh : 0) + 6;
  int dw = std::min(r.width() - 16, 400), dx = (r.width() - dw) / 2;
  int maxLines = (r.height() - footerHeight(r) - dy) / rh;
  std::vector<std::string> lines =
      wrapText(r, game_.info.description.empty() ? "(no description)" : game_.info.description, dw);
  std::vector<std::string> meta;
  if (!game_.info.author.empty()) meta.push_back("by " + game_.info.author);
  meta.push_back(intStr(game_.info.mapCount) + (game_.info.mapCount == 1 ? " map" : " maps"));
  int n = std::min((int)lines.size(), std::max(0, maxLines - (int)meta.size()));
  for (int i = 0; i < n; ++i) {
    std::string t = lines[i];
    if (i == n - 1 && n < (int)lines.size()) t = fitText(r, t + "..", dw);
    r.drawText(dx, dy + i * rh, t.c_str(), chrome::kText);
  }
  for (size_t i = 0; i < meta.size(); ++i) {
    int line = n + (int)i;
    if (line >= maxLines) break;
    r.drawText(dx, dy + line * rh, fitText(r, meta[i], dw).c_str(), chrome::kDim);
  }
  drawFooter(r, "Up/Down: choose   Confirm: open   Cancel: back");
}

}  // namespace ote
