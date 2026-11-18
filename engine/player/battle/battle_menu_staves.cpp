// Action menu: Staff (docs/SRPG.md section 8 "Player"). One entry, "Staff", after the abilities when the unit carries any
// staff; greyed with the reason of the first staff when none can be used now. It opens a list of the staves carried (the
// menu again, ids kStaffBase + slot): name and uses left, or the plain reason on the right for one that cannot be used.
// Choosing one shows its range (every square in reach, blue) and picks the unit like Attack does; the highlighted unit's area
// is marked red and the forecast box (staff_forecast.cpp) says what each unit would get. A staff that picks nobody (area
// "all", target "self") highlights the user and Confirm uses it. Cancel goes back: pick -> list -> action menu.
//
// Stage 2 (docs/SRPG.md section 8) adds picks AFTER the unit, one stage at a time, and Cancel steps back one stage: a warp asks
// for the square (the pick of squares), a repair for the item ("Repair which?" menu), a light picks only a square. A staff that
// picks no unit (target "self") starts at its second stage and Cancel there goes back to the action menu.
#include <functional>

#include "battle/battle_scene.h"
#include "battle/menu_hide.h"
#include "battle/item_text.h"
#include "battle/place_text.h"
#include "battle/staff_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

namespace {
// A reason must not push the menu off a small screen (the phone): the longest form that fits the room, else none (the entry
// stays greyed without words; the Items screen and the unit details say why in full).
std::string fitReason(Renderer& r, const std::string& why, int room) {
  for (int form = 0; form <= 2; ++form) {
    const std::string text = staffReasonText(why, form);
    if (textW(r, text) <= room) return text;
  }
  return "";
}
// "5/6" when there is room, else just "5", else nothing.
std::string fitUses(Renderer& r, const std::string& full, int uses, int room) {
  if (textW(r, full) <= room) return full;
  const std::string n = uses >= 0 ? intStr(uses) : "";
  return textW(r, n) <= room ? n : "";
}
// What a row of a menu has left after its label: the menu is at most the screen's map area wide, less its own padding, arrow
// and border (Menu::open: 2 * 4 + arrow + 2).
int roomAfter(Renderer& r, int screenW, const std::string& label) {
  return screenW - 4 - (2 * 4 + textW(r, ">") + 4 + 2) - textW(r, "  ") - textW(r, label);
}
}  // namespace

void BattleScene::addStaffEntries(std::vector<MenuItem>& items) {
  if (sel_ < 0) return;
  std::vector<StaffOption> opts;
  battle_.staffOptions(sel_, battle_.units[(size_t)sel_].cell, opts);
  if (opts.empty()) return;
  if (staffEntryHidden(host_.menuHide(), opts)) {  // "hide": every staff carried has nobody to use it on (menu_hide.h)
    OTE_INFO("staff entry of %s: hidden (nobody in reach)", battle_.units[(size_t)sel_].name.c_str());
    return;
  }
  bool any = false;
  for (const StaffOption& o : opts) any = any || o.ok;
  Renderer& r = host_.renderer();
  items.push_back({StaffWords::kMenu, any ? "" : fitReason(r, opts[0].why, roomAfter(r, layout_.vw, StaffWords::kMenu)), any, kStaffEntry});
  OTE_INFO("staff entry of %s: %s", battle_.units[(size_t)sel_].name.c_str(), any ? "usable" : ("greyed: " + staffReasonText(opts[0].why, false)).c_str());
}

bool BattleScene::staffChosen(int id) {
  if (sel_ < 0) return false;
  if (id == kStaffEntry) {
    openStaffList();
    return true;
  }
  if (id >= kStaffItemBase) {  // a repair's "Repair which?" menu: the item picked
    if (staffItemUser_ < 0) return false;
    staffUse(staffItemUser_, staffItemSlot_, staffItemTarget_, kNoCell, id - kStaffItemBase);
    return true;
  }
  const int slot = id - kStaffBase;
  std::vector<StaffOption> opts;
  battle_.staffOptions(sel_, battle_.units[(size_t)sel_].cell, opts);
  for (const StaffOption& o : opts)
    if (o.slot == slot && o.ok) {
      staffListChosen(slot);
      return true;
    }
  return false;
}

void BattleScene::openStaffList() {
  const Unit& u = battle_.units[(size_t)sel_];
  std::vector<StaffOption> opts;
  battle_.staffOptions(sel_, u.cell, opts);
  Renderer& r = host_.renderer();
  std::vector<MenuItem> items;
  std::string log;
  for (const StaffOption& o : opts) {
    const ItemDef& d = battle_.db->items[(size_t)o.item];
    const int room = roomAfter(r, layout_.vw, d.name);
    const ItemStack& stack = u.items[(size_t)o.slot];
    const std::string extra = o.ok ? fitUses(r, usesText(battle_, stack), stack.uses, room) : fitReason(r, o.why, room);
    items.push_back({d.name, extra, o.ok, kStaffBase + o.slot});
    const std::string uses = usesText(battle_, stack);  // the log says it all, whatever fits on this screen
    log += (log.empty() ? "" : ", ") + d.name + (o.ok ? (uses.empty() ? "" : " " + uses) : " [" + staffReasonText(o.why, 0) + "]");
  }
  OTE_INFO("staff list: %s", log.c_str());
  staffList_ = true;  // Mode::ActionMenu stays; Cancel (battle_menus.cpp) comes back to the action menu
  menu_.open(r, StaffWords::kMenu, items, 0, 0, layout_.vw, layout_.vh);
  anchorActionMenu();
}

void BattleScene::staffListChosen(int slot) {
  pickBack_ = nullptr;
  pickWhy_ = nullptr;
  staffBack_ = nullptr;
  const Unit& u = battle_.units[(size_t)sel_];
  const int user = sel_;
  const int item = u.items[(size_t)slot].item;
  const ItemDef& d = battle_.db->items[(size_t)item];
  const StaffDef& sd = battle_.db->staves[(size_t)d.staff];
  const std::string effect = staffEffectText(*battle_.db, d);
  staffList_ = false;
  if (sd.has(StaffEffect::Kind::Unlock)) {  // docs/SRPG.md section 10: pick the door or chest
    staffUnlockPick(user, slot);
    OTE_INFO("staff chosen: %s by %s (%s)", d.name.c_str(), u.name.c_str(), effect.c_str());
    return;
  }
  if (sd.picksSquare()) {  // a light: pick the square (stage 2)
    staffLightPick(user, slot);
    OTE_INFO("staff chosen: %s by %s (%s)", d.name.c_str(), u.name.c_str(), effect.c_str());
    return;
  }
  std::vector<int> targets;
  battle_.staffTargets(user, slot, u.cell, targets);
  if (targets.empty()) {
    staffList_ = true;
    say(StaffWords::kNothingToTarget);
    return;
  }
  if (!sd.needsPick()) {  // nobody to pick (the user itself, or everyone in reach): on to the second pick, or use it
    OTE_INFO("staff chosen: %s by %s (%s, nothing to pick)", d.name.c_str(), u.name.c_str(), effect.c_str());
    staffAfterTarget(user, slot, sd.target == StaffTarget::Self ? user : -1, [this] { openActionMenu(); });
    return;
  }
  int minR = 1, maxR = 1;
  battle_.staffRange(u, item, minR, maxR);
  const Cell from = u.cell;
  auto mark = [this, user, slot, minR, maxR, from](int cand, Overlay& ov) {
    // the reach, blue: every square the staff could be used on from here
    std::vector<Cell> near;
    battle_.grid.within(from, maxR, near);
    for (Cell c : near)
      if (battle_.grid.distance(from, c) >= minR) ov.reach[(size_t)c] = 1;
    StaffForecast f;
    staffFcOn_ = battle_.staffForecast(user, slot, cand, from, f);
    if (!staffFcOn_) return;
    staffFc_ = f;
    auto red = [&ov](Cell c) {  // (blue loses on a shared square on hexagon maps)
      if (c == kNoCell) return;
      ov.reach[(size_t)c] = 0;
      ov.attack[(size_t)c] = 1;
    };
    for (const StaffHit& h : f.hits) {  // red: the units it would reach, and where a pull or swap would put them
      red(h.unit == user ? from : battle_.units[(size_t)h.unit].cell);
      red(h.moveTo);
      red(h.userTo);
    }
  };
  // Stage one picks the unit; what comes next (a square, an item) steps back to this pick on Cancel.
  auto chosen = [this, user, slot](int target) {
    staffFcOn_ = false;
    staffAfterTarget(user, slot, target, [this, slot] { staffListChosen(slot); });
  };
  beginPickUnit(targets, chosen, mark);
  pickWhy_ = [this, user, slot, from](int unit) {
    return unit < 0 ? std::string() : staffWhyText(battle_.staffTargetWhy(user, slot, unit, from));
  };
  OTE_INFO("staff chosen: %s by %s (%s, reach %d-%d, %d candidates)", d.name.c_str(), u.name.c_str(), effect.c_str(), minR, maxR,
           (int)targets.size());
}

namespace {
const StaffDef& staffOf(const Battle& b, int user, int slot) {
  return b.db->staves[(size_t)b.db->items[(size_t)b.units[(size_t)user].items[(size_t)slot].item].staff];
}
}  // namespace

// The unit is picked (or there was nobody to pick): the second pick the staff needs, else the use itself.
void BattleScene::staffAfterTarget(int user, int slot, int target, std::function<void()> back) {
  const StaffDef& sd = staffOf(battle_, user, slot);
  if (sd.has(StaffEffect::Kind::Warp)) {
    staffWarpPick(user, slot, target, std::move(back));
    return;
  }
  if (sd.has(StaffEffect::Kind::Repair) && target >= 0) {
    std::vector<int> mendable;
    battle_.staffRepairSlots(user, slot, target, mendable);
    if (!mendable.empty()) {
      staffRepairMenu(user, slot, target, std::move(back));
      return;
    }
  }
  staffUse(user, slot, target, kNoCell, -1);
}

// A warp: the square to send the unit to, among those the core lists (free, standable, in reach of the user, in sight).
void BattleScene::staffWarpPick(int user, int slot, int target, std::function<void()> back) {
  const Cell from = battle_.units[(size_t)user].cell;
  std::vector<Cell> cells;
  battle_.staffWarpCells(user, slot, target, from, cells);
  if (cells.empty()) {  // only a staff that does something else too gets here: no square, the rest still works
    staffUse(user, slot, target, kNoCell, -1);
    return;
  }
  const Cell targetCell = battle_.units[(size_t)target].cell;
  auto mark = [this, user, slot, target, from, targetCell](int cand, Overlay& ov) {
    StaffForecast f;
    staffFcOn_ = battle_.staffForecast(user, slot, target, from, f, (Cell)cand);
    if (staffFcOn_) staffFc_ = f;
    ov.reach[(size_t)cand] = 0;  // red: who goes, and where to
    ov.attack[(size_t)cand] = 1;
    ov.attack[(size_t)targetCell] = 1;
  };
  beginPickCell(cells, [this, user, slot, target](Cell c) {
    staffFcOn_ = false;
    staffUse(user, slot, target, c, -1);
  }, mark);
  pickBack_ = std::move(back);
  int minR = 1, maxR = 0;
  battle_.staffWarpRange(battle_.units[(size_t)user], staffOf(battle_, user, slot), *staffOf(battle_, user, slot).find(StaffEffect::Kind::Warp), minR, maxR);
  const int team = battle_.teamOf(battle_.units[(size_t)user]);
  pickWhy_ = [this, from, minR, maxR, team](int c) {
    const int dist = battle_.grid.distance(from, (Cell)c);
    if (dist < minR || dist > maxR) return std::string(StaffWords::kOutOfRange);
    if (battle_.occupant[(size_t)c] >= 0) return std::string(StaffWords::kOccupied);
    if (!battle_.visibleTo(team, (Cell)c)) return std::string(StaffWords::kNotInSight);
    return std::string(StaffWords::kCannotStandThere);
  };
  say(StaffWords::kPickSquare);
  OTE_INFO("staff hint: %s", StaffWords::kPickSquare);
  OTE_INFO("staff warp: %d squares to choose from for %s", (int)cells.size(), battle_.units[(size_t)target].name.c_str());
}

// A repair: which of the unit's items (a menu: name, uses before > after). Cancel steps back to the unit pick.
void BattleScene::staffRepairMenu(int user, int slot, int target, std::function<void()> back) {
  std::vector<int> mendable;
  battle_.staffRepairSlots(user, slot, target, mendable);
  const Unit& t = battle_.units[(size_t)target];
  const StaffEffect& fx = *staffOf(battle_, user, slot).find(StaffEffect::Kind::Repair);
  Renderer& r = host_.renderer();
  std::vector<MenuItem> items;
  std::string log;
  for (int s : mendable) {
    const ItemStack& st = t.items[(size_t)s];
    const ItemDef& def = battle_.db->items[(size_t)st.item];
    const int after = fx.amount == StaffEffect::Amount::Full ? def.uses : std::min(def.uses, st.uses + fx.value);
    const std::string extra = fitUses(r, intStr(st.uses) + " > " + intStr(after), after, roomAfter(r, layout_.vw, def.name));
    items.push_back({def.name, extra, true, kStaffItemBase + s});
    log += (log.empty() ? "" : ", ") + def.name + " " + intStr(st.uses) + ">" + intStr(after);
  }
  OTE_INFO("staff repair choices for %s: %s", t.name.c_str(), log.c_str());
  mode_ = Mode::ActionMenu;
  overlay_ = Overlay();
  staffFcOn_ = false;
  staffBack_ = std::move(back);
  staffItemUser_ = user;
  staffItemSlot_ = slot;
  staffItemTarget_ = target;
  menu_.open(r, StaffWords::kRepairWhich, items, 0, 0, layout_.vw, layout_.vh);
  anchorActionMenu();
  say(StaffWords::kRepairWhich);
  OTE_INFO("staff hint: %s", StaffWords::kRepairWhich);
}

// A light: the square to light, among those within reach where something would come into sight; the area it would light is red.
void BattleScene::staffLightPick(int user, int slot) {
  const Cell from = battle_.units[(size_t)user].cell;
  std::vector<Cell> cells;
  battle_.staffLightCells(user, slot, from, cells);
  if (cells.empty()) {
    staffList_ = true;
    say(StaffWords::kNothingToTarget);
    return;
  }
  auto mark = [this, user, slot, from](int cand, Overlay& ov) {
    StaffForecast f;
    staffFcOn_ = battle_.staffForecast(user, slot, cand, from, f);
    if (!staffFcOn_) return;
    staffFc_ = f;
    for (Cell c : f.area) {
      ov.reach[(size_t)c] = 0;
      ov.attack[(size_t)c] = 1;
    }
  };
  beginPickCell(cells, [this, user, slot](Cell c) {
    staffFcOn_ = false;
    staffUse(user, slot, -1, c, -1);
  }, mark);
  int minR = 1, maxR = 1;
  battle_.staffRange(battle_.units[(size_t)user], battle_.units[(size_t)user].items[(size_t)slot].item, minR, maxR);
  pickWhy_ = [this, from, minR, maxR](int c) {
    const int dist = battle_.grid.distance(from, (Cell)c);
    return std::string(dist < minR || dist > maxR ? StaffWords::kOutOfRange : StaffWords::kNothingHidden);
  };
  say(StaffWords::kPickLight);
  OTE_INFO("staff hint: %s", StaffWords::kPickLight);
  OTE_INFO("staff light: %d squares to choose from", (int)cells.size());
}

// An unlock staff: a square of a door or chest in reach (all squares of such a structure are offered, the structure opens); the
// structure is marked red and the forecast box says what opens and the chance.
void BattleScene::staffUnlockPick(int user, int slot) {
  const Cell from = battle_.units[(size_t)user].cell;
  std::vector<Cell> cells;
  battle_.staffUnlockCells(user, slot, from, cells);
  if (cells.empty()) {
    staffList_ = true;
    say(PlaceWords::kNothingToUnlock);
    return;
  }
  auto mark = [this, user, slot, from](int cand, Overlay& ov) {
    StaffForecast f;
    staffFcOn_ = battle_.staffForecast(user, slot, cand, from, f);
    if (!staffFcOn_) return;
    staffFc_ = f;
    std::vector<Cell> fp;
    battle_.structureCells(f.unlockStructure, fp);
    for (Cell c : fp) {
      ov.reach[(size_t)c] = 0;
      ov.attack[(size_t)c] = 1;
    }
  };
  beginPickCell(cells, [this, user, slot](Cell c) {
    staffFcOn_ = false;
    staffUse(user, slot, -1, c, -1);
  }, mark);
  int minR = 1, maxR = 1;
  battle_.staffRange(battle_.units[(size_t)user], battle_.units[(size_t)user].items[(size_t)slot].item, minR, maxR);
  pickWhy_ = [this, from, minR, maxR](int c) {
    const int dist = battle_.grid.distance(from, (Cell)c);
    return std::string(dist < minR || dist > maxR ? StaffWords::kOutOfRange : PlaceWords::kNothingToUnlock);
  };
  say(PlaceWords::kPickUnlock);
  OTE_INFO("staff hint: %s", PlaceWords::kPickUnlock);
  OTE_INFO("staff unlock: %d squares to choose from", (int)cells.size());
}

void BattleScene::clearStaffHint() {
  if (notice_ == PlaceWords::kPickUnlock || notice_.rfind("Pick ", 0) == 0) noticeTime_ = 0;
  if (notice_ == StaffWords::kPickSquare || notice_ == StaffWords::kPickLight || notice_ == StaffWords::kRepairWhich) noticeTime_ = 0;
}

// The order is given; a staff ends the unit's turn, so the order is finished (a unit with "move again" gets its menu back).
void BattleScene::staffUse(int user, int slot, int target, Cell dest, int itemSlot) {
  clearStaffHint();
  staffFcOn_ = false;
  staffBack_ = nullptr;
  const bool pick = staffOf(battle_, user, slot).needsPick();
  if (!submit(Action::useStaff(user, slot, pick ? target : -1, dest, itemSlot))) {
    openActionMenu();  // refused (the log says why): back to the menu rather than a dead screen
    return;
  }
  const Unit& me = battle_.units[(size_t)user];
  if (battle_.over || !me.alive || me.done) finishOrder();
  else itemActionDone();
}

}  // namespace ote
