#include "battle/unit_info.h"

#include <algorithm>

#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

UnitInfoPages buildUnitInfoPages(const Battle& b, int unit) {
  UnitInfoPages pages;
  addStatsPage(pages, b, unit);
  addSkillsPage(pages, b, unit);
  addAbilitiesPage(pages, b, unit);
  addSupportsPage(pages, b, unit);
  addItemsPage(pages, b, unit);
  return pages;
}

void UnitInfoView::open(const Battle& b, int unit) {
  openPages(unit, buildUnitInfoPages(b, unit));
  std::string titles;
  for (const UnitInfoPage& p : pages_) titles += (titles.empty() ? "" : ", ") + p.title;
  OTE_INFO("unit details: %s (%s)", b.units[(size_t)unit].name.c_str(), titles.c_str());
}

void UnitInfoView::openPages(int unit, UnitInfoPages pages) {
  open_ = true;
  unit_ = unit;
  page_ = 0;
  pages_ = std::move(pages);
}

void UnitInfoView::turn(int dir) {
  int n = pageCount();
  if (n > 0) page_ = (page_ + dir % n + n) % n;
}

UnitInfoView::Result UnitInfoView::onInput(const InputEvent& e) {
  if (!open_) return None;
  if (e.type == InputEvent::ButtonDown) {
    switch (e.button) {
      case Button::Left: case Button::Prev: turn(-1); break;
      case Button::Right: case Button::Next: turn(1); break;
      case Button::Cancel: open_ = false; return Closed;
      default: break;
    }
  } else if (e.type == InputEvent::PointerDown) {
    bool inside = e.x >= bx_ && e.y >= by_ && e.x < bx_ + bw_ && e.y < by_ + bh_;
    if (!inside) { open_ = false; return Closed; }
    turn(e.x < bx_ + bw_ / 2 ? -1 : 1);
  }
  return None;
}

void UnitInfoView::draw(Renderer& r, const Battle& b, int lineH) const {
  if (!open_ || unit_ < 0 || unit_ >= (int)b.units.size()) return;
  const int pad = 6;
  bw_ = std::min(r.width() - 8, std::max(160, r.width() * 3 / 4));
  bh_ = std::min(r.height() - 8, std::max(120, r.height() * 3 / 4));
  bx_ = (r.width() - bw_) / 2;
  by_ = (r.height() - bh_) / 2;
  r.fillRect(0, 0, r.width(), r.height(), Color(0, 0, 0, 140));
  r.fillRect(bx_, by_, bw_, bh_, Color(18, 22, 40, 240));
  r.drawRect(bx_, by_, bw_, bh_, Color(150, 156, 185));
  r.drawText(bx_ + pad, by_ + pad, b.units[(size_t)unit_].name.c_str(), Color(240, 240, 255));
  // Tabs: the open page is bright.
  int tx = bx_ + pad, ty = by_ + pad + lineH + 2;
  for (size_t i = 0; i < pages_.size(); ++i) {
    bool cur = (int)i == page_;
    std::string t = pages_[i].title;
    r.drawText(tx, ty, t.c_str(), cur ? Color(255, 235, 130) : Color(150, 156, 185));
    tx += textW(r, t) + 10;
  }
  if (page_ >= 0 && page_ < pageCount() && pages_[(size_t)page_].draw) {
    int top = ty + lineH + 4;
    pages_[(size_t)page_].draw(r, bx_ + pad, top, bw_ - 2 * pad, by_ + bh_ - pad - top, lineH);
  }
}

}  // namespace ote
