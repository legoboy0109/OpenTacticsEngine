// Action menu: Staff (docs/SRPG.md section 8 "Player"). One entry, "Staff", after the abilities when the unit carries any
// staff; greyed with the reason of the first staff when none can be used now. It opens a list of the staves carried (the
// menu again, ids kStaffBase + slot): name and uses left, or the plain reason on the right for one that cannot be used.
// Choosing one shows its range (every square in reach, blue) and picks the unit like Attack does; the highlighted unit's area
// is marked red and the forecast box (staff_forecast.cpp) says what each unit would get. A staff that picks nobody (area
// "all", target "self") highlights the user and Confirm uses it. Cancel goes back: pick -> list -> action menu.
#include "battle/battle_scene.h"
#include "battle/item_text.h"
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
  const Unit& u = battle_.units[(size_t)sel_];
  const int user = sel_;
  const int item = u.items[(size_t)slot].item;
  const ItemDef& d = battle_.db->items[(size_t)item];
  const bool pick = battle_.db->staves[(size_t)d.staff].needsPick();
  std::vector<int> targets;
  battle_.staffTargets(user, slot, u.cell, targets);
  if (targets.empty()) {
    say(StaffWords::kNothingToTarget);
    return;
  }
  int minR = 1, maxR = 1;
  battle_.staffRange(u, item, minR, maxR);
  const Cell from = u.cell;
  const std::string effect = staffEffectText(*battle_.db, d);
  // The order is given; a staff ends the unit's turn, so the order is finished (a unit with "move again" gets its menu back).
  auto use = [this, user, slot, pick](int target) {
    staffFcOn_ = false;
    if (!submit(Action::useStaff(user, slot, pick ? target : -1))) return;
    const Unit& me = battle_.units[(size_t)user];
    if (battle_.over || !me.alive || me.done) finishOrder();
    else itemActionDone();
  };
  auto mark = [this, user, slot, minR, maxR, from, pick](int cand, Overlay& ov) {
    if (pick) {  // the reach, blue: every square the staff could be used on from here
      std::vector<Cell> near;
      battle_.grid.within(from, maxR, near);
      for (Cell c : near)
        if (battle_.grid.distance(from, c) >= minR) ov.reach[(size_t)c] = 1;
    }
    StaffForecast f;
    staffFcOn_ = battle_.staffForecast(user, slot, cand, from, f);
    if (!staffFcOn_) return;
    staffFc_ = f;
    for (const StaffHit& h : f.hits) {  // red: the units it would reach (blue loses on a shared square on hexagon maps)
      const Cell c = h.unit == user ? from : battle_.units[(size_t)h.unit].cell;
      ov.reach[(size_t)c] = 0;
      ov.attack[(size_t)c] = 1;
    }
  };
  staffList_ = false;
  beginPickUnit(targets, use, mark);
  OTE_INFO("staff chosen: %s by %s (%s, reach %d-%d, %d candidates)", d.name.c_str(), u.name.c_str(), effect.c_str(), minR, maxR,
           (int)targets.size());
}

}  // namespace ote
