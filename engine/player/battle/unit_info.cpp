#include "battle/unit_info.h"

#include <algorithm>

#include "battle/unit_info_rows.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

UnitInfoPages buildUnitInfoPages(const Battle& b, int unit) {
  UnitInfoPages pages;
  addStatsPage(pages, b, unit);
  addSkillsPage(pages, b, unit);
  addConditionsPage(pages, b, unit);
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
  // The identity rows of the Stats page (class, job, level, rank with its counter), for the player tests; only when the unit has a job or a rank.
  std::string ident;
  for (const InfoRow& row : statRows(b, b.units[(size_t)unit]))
    if (row.label == "Class" || row.label == "Job" || row.label == "Level" || row.label == "Rank")
      ident += (ident.empty() ? "" : " | ") + row.label + ": " + row.value + (row.note.empty() ? "" : " " + row.note);
  if (b.db->jobsOn() || b.db->rules.unitRanks.on()) OTE_INFO("unit identity: %s: %s", b.units[(size_t)unit].name.c_str(), ident.c_str());
  // The non-zero damage-type resistances the Stats page lists, for the player tests (only in a game that declares damage types).
  std::string res;
  for (size_t t = 0; t < b.db->damageTypes.list.size(); ++t) {
    const int rs = b.resistOf(b.units[(size_t)unit], (int)t);
    if (rs != 0) res += (res.empty() ? "" : ", ") + b.db->damageTypes.list[t].name + " res " + (rs > 0 ? "+" : "") + std::to_string(rs);
  }
  if (b.db->damageTypes.on()) OTE_INFO("unit resist: %s: %s", b.units[(size_t)unit].name.c_str(), res.c_str());
  // Skills the unit has from its side (research grants, docs/SRPG.md "Side-wide modifiers"), for the player tests; only when there are any.
  std::string side;
  for (const SkillRow& row : skillRows(b, b.units[(size_t)unit]))
    if (row.source == SkillSource::Side) side += (side.empty() ? "" : ", ") + row.name + " (" + skillSourceName(row.source) + ")";
  if (!side.empty()) OTE_INFO("unit side skills: %s: %s", b.units[(size_t)unit].name.c_str(), side.c_str());
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
