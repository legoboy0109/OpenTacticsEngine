#include "scene_list.h"

#include <algorithm>

#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {
const Color kBg(18, 20, 32), kAccent(255, 220, 90);
}  // namespace

ListScene::ListScene(SceneHost& host, std::string title, std::vector<ListEntry> entries,
                     std::string footer)
    : host_(host), title_(std::move(title)), entries_(std::move(entries)), footer_(std::move(footer)) {
  measure(host.renderer());
}

// All vertical metrics come from the active font (at the classic 8px font they
// are the original 44 / 24 / 22).
void ListScene::measure(const Renderer& r) {
  int lh = r.lineHeight();
  rowH_ = 2 * lh + 8;
  top_ = 14 + r.lineHeight(2) + 14;
  bottom_ = lh + 14;
}

int ListScene::visibleRows(int screenH) const { return std::max(1, (screenH - top_ - bottom_) / rowH_); }

int ListScene::firstRow(int screenH) const {
  int vis = visibleRows(screenH), n = (int)entries_.size();
  return std::max(0, std::min(sel_ - vis / 2, n - vis));
}

int ListScene::rowAt(int x, int y) const {
  int row = (y - top_) / rowH_;
  if (y < top_ || row >= visibleRows(lastH_)) return -1;
  row += firstRow(lastH_);
  return row < (int)entries_.size() ? row : -1;
}

void ListScene::onInput(const InputEvent& e) {
  int n = (int)entries_.size();
  if (e.type == InputEvent::PointerMove) {
    int row = rowAt(e.x, e.y);
    if (row >= 0) sel_ = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = rowAt(e.x, e.y);
    if (row >= 0) { sel_ = row; onChoose(row); }
  } else if (e.type == InputEvent::ButtonDown && n > 0) {
    if (e.button == Button::Up || e.button == Button::Prev) sel_ = (sel_ + n - 1) % n;
    else if (e.button == Button::Down || e.button == Button::Next) sel_ = (sel_ + 1) % n;
    else if (e.button == Button::Confirm) onChoose(sel_);
    else if (e.button == Button::Cancel) onCancel();
  } else if (e.type == InputEvent::ButtonDown && e.button == Button::Cancel) {
    onCancel();
  }
}

void ListScene::draw(Renderer& r) {
  lastW_ = r.width();
  lastH_ = r.height();
  measure(r);
  const int lh = r.lineHeight();
  r.clear(kBg);
  int tsc = fitScale(r, title_, lastW_ - 16, 2);
  drawTextCentered(r, lastW_ / 2, 14, fitText(r, title_, lastW_ - 16, tsc), Color(235, 238, 255), tsc);
  int vis = visibleRows(lastH_), first = firstRow(lastH_);
  int boxW = std::min(lastW_ - 24, 320), bx = (lastW_ - boxW) / 2;
  if (entries_.empty())
    drawTextCentered(r, lastW_ / 2, top_ + 20, "(nothing here)", Color(140, 145, 170));
  for (int i = 0; i < vis && first + i < (int)entries_.size(); ++i) {
    const ListEntry& it = entries_[first + i];
    int y = top_ + i * rowH_;
    bool sel = first + i == sel_;
    r.fillRect(bx, y, boxW, rowH_ - 3, sel ? Color(60, 78, 130) : Color(30, 34, 54));
    if (sel) r.drawRect(bx, y, boxW, rowH_ - 3, kAccent);
    int textW_ = boxW - 16;
    int ly = it.detail.empty() ? y + (rowH_ - 3 - lh) / 2 : y + 3;
    r.drawText(bx + 8, ly, fitText(r, it.label, textW_).c_str(), sel ? Color(255, 255, 255) : Color(215, 218, 235));
    if (!it.detail.empty())
      r.drawText(bx + 8, y + 3 + lh + 1, fitText(r, it.detail, textW_).c_str(), Color(165, 172, 205));
  }
  if (first > 0) drawTextCentered(r, lastW_ / 2, top_ - lh - 2, "^", kAccent);
  if (first + vis < (int)entries_.size()) drawTextCentered(r, lastW_ / 2, lastH_ - bottom_ + 2, "v", kAccent);
  drawTextCentered(r, lastW_ / 2, lastH_ - lh - 4, fitText(r, footer_, lastW_ - 16), Color(120, 128, 160));
}

void ErrorScene::onInput(const InputEvent& e) {
  bool back = e.type == InputEvent::PointerDown ||
              (e.type == InputEvent::ButtonDown &&
               (e.button == Button::Cancel || e.button == Button::Confirm || e.button == Button::Menu));
  if (!back) return;
  if (back_ == SceneHost::ToMapSelect) host_.showMapSelect();
  else host_.showGameSelect();
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
  drawTextCentered(r, w / 2, h - lh - 4, "Cancel / tap to go back", Color(190, 140, 140));
}

}  // namespace ote
