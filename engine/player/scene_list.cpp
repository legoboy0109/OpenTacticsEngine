#include "scene_list.h"

#include <algorithm>

#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kTop = 44, kRowH = 24, kBottom = 22;
const Color kBg(18, 20, 32), kAccent(255, 220, 90);
}  // namespace

ListScene::ListScene(SceneHost& host, std::string title, std::vector<ListEntry> entries,
                     std::string footer)
    : host_(host), title_(std::move(title)), entries_(std::move(entries)), footer_(std::move(footer)) {}

int ListScene::visibleRows(int screenH) const { return std::max(1, (screenH - kTop - kBottom) / kRowH); }

int ListScene::firstRow(int screenH) const {
  int vis = visibleRows(screenH), n = (int)entries_.size();
  return std::max(0, std::min(sel_ - vis / 2, n - vis));
}

int ListScene::rowAt(int x, int y) const {
  int row = (y - kTop) / kRowH;
  if (y < kTop || row >= visibleRows(lastH_)) return -1;
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
  r.clear(kBg);
  drawTextCentered(r, lastW_ / 2, 14, fitText(title_, lastW_ / 16 - 1), Color(235, 238, 255), 2);
  int vis = visibleRows(lastH_), first = firstRow(lastH_);
  int boxW = std::min(lastW_ - 24, 320), bx = (lastW_ - boxW) / 2;
  if (entries_.empty())
    drawTextCentered(r, lastW_ / 2, kTop + 20, "(nothing here)", Color(140, 145, 170));
  for (int i = 0; i < vis && first + i < (int)entries_.size(); ++i) {
    const ListEntry& it = entries_[first + i];
    int y = kTop + i * kRowH;
    bool sel = first + i == sel_;
    r.fillRect(bx, y, boxW, kRowH - 3, sel ? Color(60, 78, 130) : Color(30, 34, 54));
    if (sel) r.drawRect(bx, y, boxW, kRowH - 3, kAccent);
    int chars = (boxW - 16) / kGlyph;
    int ly = it.detail.empty() ? y + 6 : y + 3;
    r.drawText(bx + 8, ly, fitText(it.label, chars).c_str(), sel ? Color(255, 255, 255) : Color(215, 218, 235));
    if (!it.detail.empty())
      r.drawText(bx + 8, y + 12, fitText(it.detail, chars).c_str(), Color(165, 172, 205));
  }
  if (first > 0) drawTextCentered(r, lastW_ / 2, kTop - 10, "^", kAccent);
  if (first + vis < (int)entries_.size()) drawTextCentered(r, lastW_ / 2, lastH_ - kBottom + 2, "v", kAccent);
  drawTextCentered(r, lastW_ / 2, lastH_ - 12, fitText(footer_, lastW_ / kGlyph - 2), Color(120, 128, 160));
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
  int w = r.width(), h = r.height();
  r.clear(Color(40, 14, 18));
  r.fillRect(0, 0, w, 26, Color(150, 32, 40));
  r.drawText(10, 9, fitText(title_, w / kGlyph - 2).c_str(), Color(255, 255, 255));
  int maxLines = (h - 26 - 24) / 10;
  std::vector<std::string> lines = wrapText(message_, (w - 20) / kGlyph);
  for (int i = 0; i < (int)lines.size() && i < maxLines; ++i)
    r.drawText(10, 36 + i * 10, lines[i].c_str(), Color(255, 220, 215));
  if ((int)lines.size() > maxLines) r.drawText(10, 36 + maxLines * 10, "... (see log for full text)", Color(200, 150, 150));
  drawTextCentered(r, w / 2, h - 14, "Cancel / tap to go back", Color(190, 140, 140));
}

}  // namespace ote
