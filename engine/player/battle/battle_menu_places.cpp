// Action menu: doors, chests and visiting (docs/SRPG.md section 10 "Player"). In the menu mode of "rules.places" the menu gets one
// entry per thing the unit could do to a door, chest or place within reach: "Open" (with the structure's name added when there
// are several), "Visit", "Destroy"; one that cannot be done now stays greyed with the short reason ("Needs a key", "Pack full").
// Several keys that fit: a "Which key?" menu (the first is the default, so Z twice opens with it). "Break door" picks a door
// that can be attacked (the best weapon is used; the forecast box says damage, hit points and "Breaks"). In the automatic mode
// there are no Open / Visit entries (the move does it); Break door, abilities and staves stay.
// Ids: kPlaceBase + n (the n-th option of Battle::placeOptions), kPlaceKeyBase + slot, kPlaceBreakEntry.
#include "battle/battle_scene.h"
#include "battle/place_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

namespace {
std::string structureName(const Battle& b, int s) { return b.defOf(b.structures[(size_t)s]).name; }
}  // namespace

void BattleScene::addPlaceEntries(std::vector<MenuItem>& items) {
  if (sel_ < 0) return;
  const Unit& u = battle_.units[(size_t)sel_];
  placeOpts_.clear();
  if (!battle_.db->rules.places.automatic) battle_.placeOptions(sel_, u.cell, placeOpts_);
  int opens = 0;
  for (const PlaceOption& o : placeOpts_) opens += o.verb == PlaceOption::Verb::Open;
  Renderer& r = host_.renderer();
  std::string log;
  for (size_t i = 0; i < placeOpts_.size() && i < 90; ++i) {
    const PlaceOption& o = placeOpts_[i];
    std::string label = o.verb == PlaceOption::Verb::Open ? PlaceWords::kOpen
                        : o.verb == PlaceOption::Verb::Visit ? PlaceWords::kVisit : PlaceWords::kDestroy;
    const bool several = o.verb == PlaceOption::Verb::Open ? opens > 1 : placeOpts_.size() > 1;
    if (several) label += " " + structureName(battle_, o.structure);
    std::string extra = o.ok ? "" : placeReasonText(o.why);
    if (textW(r, label) + textW(r, "  " + extra) > layout_.vw - 40) extra = "";  // the phone: greyed without words
    items.push_back({label, extra, o.ok, kPlaceBase + (int)i});
    log += (log.empty() ? "" : ", ") + label + (o.ok ? "" : " [" + placeReasonText(o.why) + "]");
  }
  placeAtk_.clear();
  battle_.placeAttackOptions(sel_, u.cell, placeAtk_);
  if (!placeAtk_.empty()) {
    items.push_back({PlaceWords::kBreak, "", true, kPlaceBreakEntry});
    log += (log.empty() ? "" : ", ") + std::string(PlaceWords::kBreak);
  }
  if (!log.empty()) OTE_INFO("place entries of %s: %s", u.name.c_str(), log.c_str());
}

// The order is given: like an ability or a staff, it ends the turn (unless a "move again" skill keeps the menu).
void BattleScene::placeSubmit(const Action& a) {
  const int user = sel_;
  if (!submit(a)) {
    openActionMenu();
    return;
  }
  const Unit& me = battle_.units[(size_t)user];
  if (battle_.over || !me.alive || me.done) finishOrder();
  else itemActionDone();
}

bool BattleScene::placeChosen(int id) {
  if (sel_ < 0) return false;
  if (id == kPlaceBreakEntry) {
    placeBreakPick();
    return true;
  }
  if (id >= kPlaceKeyBase) {  // the key menu: the slot picked
    if (placeKeyStructure_ < 0) return false;
    const int s = placeKeyStructure_;
    placeKeyStructure_ = -1;
    staffBack_ = nullptr;
    placeSubmit(Action::openPlace(sel_, s, id - kPlaceKeyBase));
    return true;
  }
  const size_t n = (size_t)(id - kPlaceBase);
  if (n >= placeOpts_.size() || !placeOpts_[n].ok) return false;
  const PlaceOption o = placeOpts_[n];
  if (o.verb != PlaceOption::Verb::Open) {
    placeSubmit(Action::visitPlace(sel_, o.structure));
    return true;
  }
  if (o.keySlots.size() <= 1) {
    placeSubmit(Action::openPlace(sel_, o.structure, o.keySlots.empty() ? -1 : o.keySlots[0]));
    return true;
  }
  // Several keys fit: which one (the first is highlighted).
  const Unit& u = battle_.units[(size_t)sel_];
  std::vector<MenuItem> items;
  std::string log;
  for (int slot : o.keySlots) {
    const ItemStack& st = u.items[(size_t)slot];
    const ItemDef& d = battle_.db->items[(size_t)st.item];
    items.push_back({d.name, st.uses >= 0 ? "x" + intStr(st.uses) : "", true, kPlaceKeyBase + slot});
    log += (log.empty() ? "" : ", ") + d.name;
  }
  OTE_INFO("key choices for %s: %s", structureName(battle_, o.structure).c_str(), log.c_str());
  placeKeyStructure_ = o.structure;
  staffBack_ = [this] { placeKeyStructure_ = -1; openActionMenu(); };  // Cancel: back to the action menu
  menu_.open(host_.renderer(), PlaceWords::kWhichKey, items, 0, 0, layout_.vw, layout_.vh);
  anchorActionMenu();
  return true;
}

// Break door: pick a square of a door that can be attacked. The best weapon for that door is used.
void BattleScene::placeBreakPick() {
  const int user = sel_;
  const Cell from = battle_.units[(size_t)user].cell;
  std::vector<PlaceAttack> atk;
  battle_.placeAttackOptions(user, from, atk);
  std::vector<Cell> cells;
  for (const PlaceAttack& a : atk) {
    std::vector<Cell> fp;
    battle_.structureCells(a.structure, fp);
    for (Cell c : fp)
      if (std::find(cells.begin(), cells.end(), c) == cells.end()) cells.push_back(c);
  }
  if (cells.empty()) {
    say(PlaceWords::kNothingToUnlock);
    return;
  }
  std::sort(cells.begin(), cells.end());
  // The weapon with the most damage for the door that owns the square.
  auto bestFor = [this, user, from](int s, PlaceForecast& out) {
    std::vector<PlaceAttack> opts;
    battle_.placeAttackOptions(user, from, opts);
    bool any = false;
    for (const PlaceAttack& a : opts) {
      PlaceForecast f;
      if (a.structure != s || !battle_.placeForecast(user, s, a.weapon, from, f)) continue;
      if (!any || f.total > out.total) out = f;
      any = true;
    }
    return any;
  };
  auto mark = [this, bestFor](int cand, Overlay& ov) {
    const int s = battle_.structureIndexAt((Cell)cand);
    PlaceForecast f;
    placeFcOn_ = s >= 0 && bestFor(s, f);
    if (!placeFcOn_) return;
    placeFc_ = f;
    std::vector<Cell> fp;
    battle_.structureCells(s, fp);
    for (Cell c : fp) {
      ov.reach[(size_t)c] = 0;
      ov.attack[(size_t)c] = 1;
    }
  };
  beginPickCell(cells, [this, user, bestFor](Cell c) {
    placeFcOn_ = false;
    const int s = battle_.structureIndexAt(c);
    PlaceForecast f;
    if (s < 0 || !bestFor(s, f)) {
      openActionMenu();
      return;
    }
    placeSubmit(Action::breakPlace(user, s, f.weapon));
  }, mark);
  pickWhy_ = [](int) { return std::string(PlaceWords::kNotBreakable); };
  say(PlaceWords::kPickDoor);
  OTE_INFO("place hint: %s", PlaceWords::kPickDoor);
  OTE_INFO("door break: %d squares to choose from", (int)cells.size());
}

}  // namespace ote
