// The speed turn order on screen (docs/TURN_ORDER.md "Player"): a strip of name tags along the top edge of the map showing the next entries of the
// order (Battle::turnPreview), the unit that has the turn first with a bright frame, a charged action on its way as an orange-framed tag with the
// action's name, and, while the player aims a charged action, a dashed tag showing where it would land. And the markers on the map: the squares of
// the units that are charging (teal) and of what their actions will hit (magenta), painted through the overlay tones, so all four views have them.
// Nothing here does anything in a game that plays side by side.
#include <algorithm>

#include "battle/battle_scene.h"
#include "battle/speed_text.h"
#include "core/grid/area.h"
#include "ui/text.h"

namespace ote {

void BattleScene::refreshChargeMarks() {
  if (!battle_.speedGame() || battle_.pending.empty() || battle_.over) {
    chargeFromMask_.clear();
    chargeAimMask_.clear();
    return;
  }
  const size_t n = (size_t)battle_.grid.cellCount();
  chargeFromMask_.assign(n, 0);
  chargeAimMask_.assign(n, 0);
  const int team = viewTeam();
  for (const PendingAction& p : battle_.pending) {
    if (p.unit < 0 || (size_t)p.unit >= battle_.units.size()) continue;
    const Unit& c = battle_.units[(size_t)p.unit];
    if (!c.alive || c.cell == kNoCell || !battle_.unitVisibleTo(team, c)) continue;  // (fog: only what the viewer sees)
    chargeFromMask_[(size_t)c.cell] = 1;
    const Cell aim = battle_.chargeAimCell(p);
    if (!battle_.grid.valid(aim) || !battle_.visibleTo(team, aim)) continue;
    if (p.weapon && battle_.isAreaWeapon(p.index)) {  // the whole blast
      std::vector<Cell> cells;
      areaCells(battle_.grid, c.cell, aim, battle_.db->weapons[(size_t)p.index].area, cells);
      for (Cell x : cells) chargeAimMask_[(size_t)x] = 1;
    } else {
      chargeAimMask_[(size_t)aim] = 1;
    }
  }
}

void BattleScene::addChargeTint(Overlay& ov) const {
  if (!battle_.speedGame() || chargeFromMask_.empty() || view_.busy() || prep_) return;  // (the picture catches up with the rules when the view is idle)
  ov.chargeFrom = &chargeFromMask_;
  ov.chargeAim = &chargeAimMask_;
}

void BattleScene::drawTurnStrip(Renderer& r) const {
  if (!battle_.speedGame() || battle_.over || prep_ || covered_ || mode_ == Mode::Result || other_) return;
  const SpeedRules& sp = battle_.db->rules.speed;
  // While a charged action is being aimed: where it would land in the order.
  int aimedSpeed = 0;
  if (mode_ == Mode::Targeting && targetIdx_ >= 0 && targetIdx_ < (int)targets_.size() && battle_.weaponCharges(targets_[(size_t)targetIdx_].weapon))
    aimedSpeed = battle_.db->weapons[(size_t)targets_[(size_t)targetIdx_].weapon].charge.speed;
  else if ((mode_ == Mode::PickUnit || mode_ == Mode::PickCell) && aimedChargeSpeed_ > 0)
    aimedSpeed = aimedChargeSpeed_;
  PendingAction ghost;
  const PendingAction* extra = nullptr;
  const std::string ghostName = mode_ == Mode::Targeting && aimedSpeed > 0 ? chargeActionName(battle_, true, targets_[(size_t)targetIdx_].weapon) : "Cast";
  if (aimedSpeed > 0 && battle_.activeUnit >= 0) {
    ghost.unit = battle_.activeUnit;
    ghost.speed = aimedSpeed;
    extra = &ghost;
  }
  std::vector<TurnEntry> order;
  battle_.turnPreview(sp.preview, order, extra);
  if (order.empty()) return;

  const int lh = layout_.lh, h = lh + 4, pad = 3, gap = 2, y = 1;
  const int team = viewTeam();
  const int under = battle_.unitAt(cursor_);  // the unit under the cursor shows up in the strip
  struct Tag {
    std::string text;
    int w;
    Color fill, frame, ink;
    bool first, charged, ghost, under;
  };
  std::vector<Tag> tags;
  // Names as wide as the width allows; on a narrow screen (a big UI scale) too few entries fit that way, so the names are cut shorter to show more.
  const int wanted = std::min((int)order.size(), 5);
  for (int mode = 0; mode < 3; ++mode) {  // 0: the name as far as it fits; 1: three letters; 2: two letters
    const int maxName = std::max(textW(r, "WWWW"), layout_.vw / 12);
    tags.clear();
    int total = 2;
    for (size_t i = 0; i < order.size(); ++i) {
      const TurnEntry& e = order[i];
      const Unit& u = battle_.units[(size_t)e.unit];
      Tag t;
      t.first = i == 0 && !e.charged;
      t.charged = e.charged;
      t.ghost = e.forecast;
      t.under = !e.charged && e.unit == under;
      const bool shown = !battle_.isEnemy(u, battle_.units[(size_t)std::max(0, battle_.activeUnit)]) || battle_.unitVisibleTo(team, u);
      Color side = Color::rgb(battle_.sides[(size_t)u.side].color);
      if (e.charged) {
        const PendingAction* p = e.pending >= 0 ? &battle_.pending[(size_t)e.pending] : nullptr;
        t.text = e.forecast ? "(" + ghostName + ")" : shown ? chargeActionName(battle_, p && p->weapon, p ? p->index : -1) : SpeedWords::kHidden;
        t.fill = Color(34, 28, 20);
        t.frame = Color(255, 170, 40);
        t.ink = Color(255, 200, 110);
      } else {
        t.text = shown ? u.name : SpeedWords::kHidden;
        t.fill = shown ? mix(side, Color(14, 16, 28), 0.45f) : Color(50, 52, 66);
        t.frame = t.first ? Color(255, 255, 255) : mix(side, Color(255, 255, 255), 0.25f);
        t.ink = readableOn(t.fill);
      }
      t.text = mode == 0 ? fitText(r, t.text, maxName) : t.text.substr(0, mode == 1 ? 3 : 2);
      t.w = textW(r, t.text) + 2 * pad;
      if (total + t.w > layout_.vw - 2 && !tags.empty()) break;
      total += t.w + gap;
      tags.push_back(t);
    }
    if ((int)tags.size() >= wanted) break;
  }
  r.fillRect(0, 0, layout_.vw, h + y + 1, Color(8, 10, 20, 170));
  int x = 2;
  for (const Tag& t : tags) {
    r.fillRect(x, y, t.w, h, t.fill);
    if (t.ghost) {  // dashed: it is not there yet
      for (int dx = 0; dx < t.w; dx += 3) {
        r.fillRect(x + dx, y, std::min(2, t.w - dx), 1, t.frame);
        r.fillRect(x + dx, y + h - 1, std::min(2, t.w - dx), 1, t.frame);
      }
      for (int dy = 0; dy < h; dy += 3) {
        r.fillRect(x, y + dy, 1, std::min(2, h - dy), t.frame);
        r.fillRect(x + t.w - 1, y + dy, 1, std::min(2, h - dy), t.frame);
      }
    } else {
      r.drawRect(x, y, t.w, h, t.frame);
      if (t.first) r.drawRect(x + 1, y + 1, t.w - 2, h - 2, t.frame);
    }
    r.drawText(x + pad, y + (h - lh) / 2, t.text.c_str(), t.ink);
    if (t.under) r.fillRect(x + 1, y + h - 2, t.w - 2, 2, Color(255, 235, 110));
    x += t.w + gap;
  }
}

}  // namespace ote
