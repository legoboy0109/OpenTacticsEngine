// The preparation screen of the battle scene (docs/EVENTS.md "Preparation"): the main menu, who fights, where they start,
// viewing the map, and Begin. The items and the shop are in battle_prep_items.cpp.
//
// The scene is an ordinary BattleScene whose Battle has been set up but not started; Mode::Prep takes the input. The
// Battle shown is rebuilt from the choice after every change (prepRebuild), so what is drawn, what the info panel and the
// unit details say, and what the battle will start with are one and the same. Nothing the screen does touches the rules
// core except through core/campaign/prep.h.
#include "battle/battle_prep_ui.h"
#include "battle/prep_text.h"
#include "core/base/log.h"
#include "core/data/loader.h"
#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {
namespace {

enum PrepMenuId { kmUnits = 1, kmPositions, kmItems, kmShop, kmView, kmSave, kmLeave, kmBegin, kmShopBase = 100 };  // kmShopBase + n: the n-th shop of a `shops` list

const Color kSlotFree(90, 170, 255, 110), kSlotTaken(110, 220, 120, 130), kSlotCarry(255, 220, 90, 170);

}  // namespace

// ---- start and rebuild

void BattleScene::prepStart() {
  PrepUi& p = *prep_;
  p.campaign = session_.campaign != nullptr;
  std::string err;
  bool ok = true;
  if (p.campaign) {
    p.pt = &session_.campaign->pt;
    const ChapterDef* ch = currentChapter(session_.db, *p.pt);
    ok = ch && loadMap(session_.db, session_.db.maps[(size_t)ch->map].id, p.map, err);
  } else {
    p.pt = session_.freePrep.get();
    p.map = session_.map;
    ok = p.pt != nullptr;
  }
  if (!ok || !p.map.prep) {  // cannot happen (the App checked); play without the screen rather than not at all
    OTE_WARN("prep: the preparation screen could not start: %s", err.c_str());
    prep_.reset();
    battle_.start(session_.startEvents);
    view_.push(session_.startEvents);
    return;
  }
  mode_ = Mode::Prep;
  if (!p.map.prep->slots.empty()) {  // the cursor starts on the first slot
    const PrepSlot& s = p.map.prep->slots[0];
    cursor_ = battle_.grid.at(s.x, s.y);
  }
  OTE_INFO("prep: opens on %s, %d slots, limit %d%s", p.map.id.c_str(), (int)p.map.prep->slots.size(), p.map.prep->limit,
           p.campaign ? "" : " (free battle)");
  prepMainMenu();
  if (continueOn()) writeContinue();  // a save made now restores into this screen
}

void BattleScene::prepRebuild() {
  PrepUi& p = *prep_;
  std::string err;
  bool ok;
  if (p.campaign) {
    battle_ = Battle();
    battle_.setAskChoices(session_.askChoices);
    ok = prepareChapterBattle(session_.db, *p.pt, session_.formulas, host_.args().seed, battle_, err);
  } else {
    ok = session_.rebuildFreePrep(host_.args().seed, err);
  }
  if (!ok) OTE_WARN("prep: the battle could not be rebuilt: %s", err.c_str());
  view_.resync(battle_);
  sel_ = -1;
  overlay_ = Overlay();
  p.overlayUnit = -1;
}

void BattleScene::prepChanged() {
  prepRebuild();
  std::string log;
  for (const PrepDeploy& d : prep_->pt->prep.deployed)
    log += (log.empty() ? "" : ", ") + d.id + "@" + intStr(d.slot + 1);
  OTE_INFO("prep: deployed %d/%d: %s", (int)prep_->pt->prep.deployed.size(), prep_->map.prep->limit, log.c_str());
  if (continueOn()) writeContinue();
}

// ---- the main menu

void BattleScene::prepMainMenu() {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::Main;
  mode_ = Mode::Prep;
  itemPanel_.close();
  itemPanel_.setDetailLines(2);
  tradePanel_.close();
  confirm_.close();
  overlay_ = Overlay();
  const PrepDef& def = *p.map.prep;
  std::vector<MenuItem> items;
  items.push_back({PrepWords::kUnits, intStr((int)p.pt->prep.deployed.size()) + "/" + intStr(def.limit), true, kmUnits});
  items.push_back({PrepWords::kPositions, "", true, kmPositions});
  if (!p.pt->roster.empty()) items.push_back({PrepWords::kItems, "", true, kmItems});
  if (def.hasShop) {
    if (def.shops.size() == 1 && def.shops[0].id.empty()) items.push_back({PrepWords::kShop, "", true, kmShop});  // the single unnamed `shop`
    else
      for (size_t i = 0; i < def.shops.size(); ++i) items.push_back({def.shops[i].name, "", true, kmShopBase + (int)i});  // each shop by name
  }
  items.push_back({PrepWords::kView, "", true, kmView});
  if (p.campaign && manualSaves()) items.push_back({PrepWords::kSave, "", true, kmSave});
  items.push_back({p.campaign ? PrepWords::kLeave : PrepWords::kAbandon, "", true, kmLeave});
  items.push_back({PrepWords::kBegin, "", true, kmBegin});
  std::string log;
  for (const MenuItem& it : items) log += (log.empty() ? "" : ", ") + it.label;
  OTE_INFO("prep menu: %s", log.c_str());
  Renderer& r = host_.renderer();
  menu_.open(r, PrepWords::title(p.campaign ? activePartyName(session_.db, *p.pt) : std::string()), items, 0, 0, layout_.vw, layout_.vh);
  int cx, cy, cw, ch;
  cellScreenBox(cursor_, cx, cy, cw, ch);  // on the side of the screen away from the cursor, so the slots stay visible
  menu_.place(cx > layout_.vw / 2 ? 4 : layout_.vw - menu_.w() - 4, 4, layout_.vw, layout_.vh);
  std::string rows;  // where each entry is on screen: the screen tests tap them
  for (size_t i = 0; i < items.size(); ++i) {
    int x, y;
    menu_.rowCentre((int)i, x, y);
    rows += " " + intStr((int)i) + "@" + intStr(x) + "," + intStr(y);
  }
  OTE_INFO("prep menu rows:%s", rows.c_str());
}

void BattleScene::prepMenuChosen(int id) {
  PrepUi& p = *prep_;
  uiCue(Cue::Confirm);
  switch (id) {
    case kmUnits: prepUnitsOpen(); break;
    case kmPositions:
      p.screen = PrepUi::Screen::Positions;
      p.carrySlot = -1;
      menu_.close();
      prepLogSlots();
      break;
    case kmView:
      p.screen = PrepUi::Screen::View;
      menu_.close();
      prepViewLook();
      prepLogSlots();
      break;
    case kmItems: prepItemsWhoOpen(); break;
    case kmShop: p.shopIx = 0; prepShopOpen(); break;
    case kmSave: openSlotList(SlotPanel::Save); break;
    case kmLeave:
      if (continueOn()) writeContinue();
      if (p.campaign) host_.showGame(host_.currentGameId());  // replaces this scene: do nothing after this line
      else host_.showMapSelect();
      return;
    case kmBegin: prepBeginPressed(); break;
    default:
      if (id >= kmShopBase && id - kmShopBase < (int)p.map.prep->shops.size()) {
        p.shopIx = id - kmShopBase;
        prepShopOpen();
      }
      break;
  }
}

// ---- who fights

void BattleScene::prepUnitsOpen(int sel) {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::Units;
  menu_.close();
  const std::vector<PrepEntry> entries = prepEntries(session_.db, *p.pt, p.map);
  std::vector<PanelRow> rows;
  p.ids.clear();
  std::string log;
  for (const PrepEntry& e : entries) {
    PanelRow row;
    row.label = e.name;
    const bool in = e.slot >= 0;
    row.mark = in;
    decorateUnitRow(row, e.type, e.level);
    row.dim = e.lock == PrepLock::Forbidden || e.lock == PrepLock::Fallen;
    if (e.lock == PrepLock::Forbidden) row.extra = "Not allowed";
    else if (e.lock == PrepLock::Fallen) row.extra = "Fallen";
    else if (in) row.extra = std::string(PrepWords::kSlot) + " " + intStr(e.slot + 1) + (e.lock == PrepLock::Required ? " *" : "");
    else row.extra = PrepWords::kOut;
    // The level and class lead the explanation: "Lv2 Soldier. Must fight in this battle."
    row.detail = "Lv" + intStr(e.level) + " " + session_.db.unitTypes[(size_t)e.type].name + ". " +
                 (e.lock != PrepLock::None ? prepLockReason(e.lock) : (in ? PrepWords::kTakeOut : PrepWords::kPutIn));
    rows.push_back(row);
    p.ids.push_back(e.id);
    log += (log.empty() ? "" : ", ") + e.name + " [" + row.extra + "]";
  }
  const std::string head = std::string(PrepWords::kDeployed) + " " + intStr((int)p.pt->prep.deployed.size()) + " / " + intStr(p.map.prep->limit);
  itemPanel_.setDetailLines(3);  // "Lv2 Soldier. Must fight in this battle." needs the room on a narrow screen
  itemPanel_.open(PrepWords::kWhoFights, head, rows, PrepWords::kNobody, sel);
  OTE_INFO("prep units: %s (%s)", log.c_str(), head.c_str());
}

void BattleScene::prepUnitsPicked() {
  PrepUi& p = *prep_;
  const int row = itemPanel_.sel();
  if (row < 0 || row >= (int)p.ids.size()) return;
  const std::string id = p.ids[(size_t)row];
  bool deployed = false;
  for (const PrepDeploy& d : p.pt->prep.deployed) deployed = deployed || d.id == id;
  std::string why;
  const bool ok = deployed ? prepBench(session_.db, *p.pt, p.map, id, why) : prepDeploy(session_.db, *p.pt, p.map, id, -1, why);
  if (!ok) {
    uiCue(Cue::Error);
    say(why);
    OTE_INFO("prep refused: %s", why.c_str());
    return;
  }
  prepChanged();
  prepUnitsOpen(row);
}

// ---- positions and the free look

// Where the slots (and the Back button) are on screen, for the screen tests.
void BattleScene::prepLogSlots() {
  const PrepDef& def = *prep_->map.prep;
  std::string log;
  for (size_t i = 0; i < def.slots.size(); ++i) {
    int x, y, w, h;
    cellScreenBox(battle_.grid.at(def.slots[i].x, def.slots[i].y), x, y, w, h);
    log += " " + intStr((int)i + 1) + "@" + intStr(x + w / 2) + "," + intStr(y + h / 2);
  }
  const int bw = std::max(40, textW(host_.renderer(), "Back") + 16), bh = layout_.lh + 8;
  OTE_INFO("prep slots:%s back@%d,%d", log.c_str(), layout_.vw - bw / 2 - 4, layout_.vh - bh / 2 - 4);
}

namespace {

std::string deployedAt(const Playthrough& pt, int slot) {
  for (const PrepDeploy& d : pt.prep.deployed)
    if (d.slot == slot) return d.id;
  return "";
}

}  // namespace

void BattleScene::prepPositionsButton(Button b) {
  PrepUi& p = *prep_;
  const PrepDef& def = *p.map.prep;
  switch (b) {
    case Button::Up: moveCursor(0, -1); break;
    case Button::Down: moveCursor(0, 1); break;
    case Button::Left: moveCursor(-1, 0); break;
    case Button::Right: moveCursor(1, 0); break;
    case Button::Next:
    case Button::Prev: {  // the next / previous slot
      const int here = prepSlotAt(p.map, battle_.grid.x(cursor_), battle_.grid.y(cursor_));
      const int n = (int)def.slots.size(), to = ((here < 0 ? (b == Button::Next ? -1 : 0) : here) + (b == Button::Next ? 1 : -1) + n) % n;
      cursor_ = battle_.grid.at(def.slots[(size_t)to].x, def.slots[(size_t)to].y);
      uiCue(Cue::Cursor);
      break;
    }
    case Button::Cancel:
      uiCue(Cue::Cancel);
      if (p.carrySlot >= 0) p.carrySlot = -1;
      else prepMainMenu();
      break;
    case Button::Confirm: {
      const int slot = prepSlotAt(p.map, battle_.grid.x(cursor_), battle_.grid.y(cursor_));
      if (slot < 0) {
        uiCue(Cue::Error);
        say(PrepWords::kNotASlot);
        break;
      }
      const std::string here = deployedAt(*p.pt, slot);
      if (p.carrySlot < 0) {
        if (here.empty()) {
          uiCue(Cue::Error);
          say(PrepWords::kNobodyThere);
          break;
        }
        uiCue(Cue::Select);
        p.carrySlot = slot;  // picked up
        break;
      }
      if (slot == p.carrySlot) {  // put back
        uiCue(Cue::Cancel);
        p.carrySlot = -1;
        break;
      }
      std::string why;
      if (!prepMove(session_.db, *p.pt, p.map, deployedAt(*p.pt, p.carrySlot), slot, why)) {
        uiCue(Cue::Error);
        say(why);
        OTE_INFO("prep refused: %s", why.c_str());
        break;
      }
      uiCue(Cue::Confirm);
      p.carrySlot = -1;
      prepChanged();
      break;
    }
    default: break;
  }
}

// The reach of an enemy the player may see and has the cursor on (the same blue and red squares as for an own unit).
void BattleScene::prepViewLook() {
  PrepUi& p = *prep_;
  overlay_ = Overlay();
  p.overlayUnit = -1;
  const int uid = battle_.unitAt(cursor_);
  if (uid < 0 || (size_t)uid >= view_.units.size() || !view_.units[(size_t)uid].visible) return;
  const Unit& u = battle_.units[(size_t)uid];
  if (!u.alive || !battle_.sides[(size_t)u.side].ai) return;
  sel_ = uid;  // computeOverlay works on sel_
  battle_.reachable(uid, reach_);
  computeOverlay();
  sel_ = -1;
  p.overlayUnit = uid;
  int moves = 0, hits = 0;
  for (uint8_t v : overlay_.reach) moves += v;
  for (uint8_t v : overlay_.attack) hits += v;
  OTE_INFO("prep view: %s can move to %d squares and strike %d", u.name.c_str(), moves, hits);
}

void BattleScene::prepViewButton(Button b) {
  switch (b) {
    case Button::Up: moveCursor(0, -1); break;
    case Button::Down: moveCursor(0, 1); break;
    case Button::Left: moveCursor(-1, 0); break;
    case Button::Right: moveCursor(1, 0); break;
    case Button::Next:
    case Button::Prev: {  // the next visible unit of the other side
      const int n = (int)battle_.units.size(), cur = battle_.unitAt(cursor_), dir = b == Button::Next ? 1 : -1;
      for (int i = 1; i <= n; ++i) {
        const int uid = ((cur < 0 ? (dir > 0 ? -1 : 0) : cur) + dir * i + n * 2) % n;
        const Unit& u = battle_.units[(size_t)uid];
        if (u.alive && battle_.sides[(size_t)u.side].ai && (size_t)uid < view_.units.size() && view_.units[(size_t)uid].visible) {
          cursor_ = u.cell;
          uiCue(Cue::Cursor);
          break;
        }
      }
      break;
    }
    case Button::Cancel:
      uiCue(Cue::Cancel);
      overlay_ = Overlay();
      prepMainMenu();
      return;
    case Button::Confirm: {
      const int uid = battle_.unitAt(cursor_);
      if (uid >= 0 && battle_.units[(size_t)uid].alive && (size_t)uid < view_.units.size() && view_.units[(size_t)uid].visible) {
        uiCue(Cue::Confirm);
        openUnitInfo(uid);
        return;
      }
      break;
    }
    default: break;
  }
  prepViewLook();
}

// ---- Begin

void BattleScene::prepBeginPressed() {
  PrepUi& p = *prep_;
  int available = 0;
  for (const PrepEntry& e : prepEntries(session_.db, *p.pt, p.map)) available += e.lock == PrepLock::None || e.lock == PrepLock::Required;
  const int deployed = (int)p.pt->prep.deployed.size(), wanted = std::min(p.map.prep->limit, available);
  if (available > 0 && deployed == 0) {
    uiCue(Cue::Error);
    say("Deploy at least one character.");
    return;
  }
  if (deployed < wanted) {  // room for more, and more to send: ask first
    prepAskBegin(deployed, wanted);
    return;
  }
  std::string why;
  if (!prepBegin(session_.db, *p.pt, p.map, why)) {
    uiCue(Cue::Error);
    say(why);
    return;
  }
  OTE_INFO("prep: begin with %d", deployed);
  host_.prepareDone();  // replaces this scene: do nothing after this line
}

void BattleScene::prepAskBegin(int deployed, int wanted) {
  prep_->screen = PrepUi::Screen::Begin;
  menu_.close();
  confirm_.open(std::string(PrepWords::kBeginAsk) + intStr(deployed) + PrepWords::kOf + intStr(wanted) + "?", PrepWords::kBegin, PrepWords::kBack);
  OTE_INFO("prep: asks \"%s%d%s%d?\"", PrepWords::kBeginAsk, deployed, PrepWords::kOf, wanted);
}

// ---- input

void BattleScene::prepListInput(const InputEvent& e) {
  PrepUi& p = *prep_;
  Renderer& r = host_.renderer();
  itemPanel_.setArea(0, 0, layout_.vw, layout_.vh);
  tradePanel_.setArea(0, 0, layout_.vw, layout_.vh);
  switch (p.screen) {
    case PrepUi::Screen::Main: {
      Menu::Result res = menu_.onInput(e);
      if (res == Menu::Chosen) prepMenuChosen(menu_.chosenId());
      break;
    }
    case PrepUi::Screen::Units: {
      ItemPanel::Result res = itemPanel_.onInput(e, r);
      if (res == ItemPanel::Back) prepMainMenu();
      else if (res == ItemPanel::Picked) prepUnitsPicked();
      break;
    }
    case PrepUi::Screen::Begin: {
      ConfirmBox::Result res = confirm_.onInput(e, r);
      if (res == ConfirmBox::None) break;
      confirm_.close();
      if (res == ConfirmBox::Yes) {
        std::string why;
        if (!prepBegin(session_.db, *p.pt, p.map, why)) {
          say(why);
          prepMainMenu();
          break;
        }
        OTE_INFO("prep: begin with %d", (int)p.pt->prep.deployed.size());
        host_.prepareDone();  // replaces this scene
      } else {
        prepMainMenu();
      }
      break;
    }
    default: prepItemsInput(e); break;
  }
}

// The two screens that use the map itself: a cursor over it, a tap on a square, and a small "Back" button for pointers.
void BattleScene::prepCellInput(const InputEvent& e) {
  PrepUi& p = *prep_;
  const bool positions = p.screen == PrepUi::Screen::Positions;
  auto handle = [&](Button b) { positions ? prepPositionsButton(b) : prepViewButton(b); };
  if (e.type == InputEvent::ButtonDown) {
    handle(e.button);
  } else if (e.type == InputEvent::PointerMove) {
    const Cell c = cellAt(e.x, e.y);
    if (c != kNoCell && c != cursor_) {
      cursor_ = c;
      if (!positions) prepViewLook();
    }
  } else if (e.type == InputEvent::PointerDown) {
    const int bw = std::max(40, textW(host_.renderer(), "Back") + 16), bh = layout_.lh + 8;
    if (e.x >= layout_.vw - bw - 4 && e.x < layout_.vw - 4 && e.y >= layout_.vh - bh - 4 && e.y < layout_.vh - 4) {  // the Back button
      handle(Button::Cancel);
      return;
    }
    const Cell c = cellAt(e.x, e.y);
    if (c == kNoCell) return;
    cursor_ = c;
    handle(Button::Confirm);
  }
}

void BattleScene::prepInput(const InputEvent& e) {
  switch (prep_->screen) {
    case PrepUi::Screen::Positions:
    case PrepUi::Screen::View: prepCellInput(e); break;
    case PrepUi::Screen::Main:
    case PrepUi::Screen::Units:
    case PrepUi::Screen::Begin: prepListInput(e); break;
    default: prepListInput(e); break;
  }
}

// ---- drawing

void BattleScene::prepDrawSlots(Renderer& r) {
  PrepUi& p = *prep_;
  const PrepDef& def = *p.map.prep;
  for (size_t i = 0; i < def.slots.size(); ++i) {
    const Cell c = battle_.grid.at(def.slots[i].x, def.slots[i].y);
    if (c == kNoCell) continue;
    int x, y, w, h;
    cellScreenBox(c, x, y, w, h);
    const bool taken = !deployedAt(*p.pt, (int)i).empty();
    const bool carried = p.carrySlot == (int)i;
    fillCellBox(r, x, y, w, h, carried ? kSlotCarry : taken ? kSlotTaken : kSlotFree);
    const std::string num = intStr((int)i + 1);
    r.drawText(x + 2, y + 1, num.c_str(), Color(0, 0, 0, 200));  // the number in the corner, over a dark edge so it reads on any art
    r.drawText(x + 1, y, num.c_str(), Color(255, 255, 255));
  }
}

// The panel's "Turn 1 / You" row says "Preparation / <deployed>/<limit>" instead.
void BattleScene::prepDrawPanelHead(Renderer& r) {
  const int x = layout_.vw + 4, w = layout_.panelW - 8, y = layout_.panelTop();
  r.fillRect(x, y, w, layout_.lh + 2, Color(20, 23, 38));
  const std::string left = PrepWords::title(prep_->campaign ? activePartyName(session_.db, *prep_->pt) : std::string()), right = intStr((int)prep_->pt->prep.deployed.size()) + "/" + intStr(prep_->map.prep->limit);
  r.drawText(x, y, fitText(r, left, w - textW(r, right) - 6).c_str(), Color(150, 158, 190));
  r.drawText(x + w - textW(r, right), y, right.c_str(), Color(228, 230, 240));
}

void BattleScene::prepDraw(Renderer& r) {
  PrepUi& p = *prep_;
  itemPanel_.setArea(0, 0, layout_.vw, layout_.vh);
  tradePanel_.setArea(0, 0, layout_.vw, layout_.vh);
  itemPanel_.draw(r);
  tradePanel_.draw(r);
  menu_.draw(r);
  confirm_.draw(r);
  if (p.screen == PrepUi::Screen::Positions || p.screen == PrepUi::Screen::View) {  // the Back button for pointers
    const int bw = std::max(40, textW(r, "Back") + 16), bh = layout_.lh + 8;
    const int x = layout_.vw - bw - 4, y = layout_.vh - bh - 4;
    r.fillRect(x, y, bw, bh, Color(10, 12, 24, 190));
    r.drawRect(x, y, bw, bh, Color(150, 156, 185, 200));
    r.drawText(x + (bw - textW(r, "Back")) / 2, y + (bh - layout_.lh) / 2, "Back", Color(235, 238, 255));
  }
}

std::string BattleScene::prepHint() const {
  using S = PrepUi::Screen;
  switch (prep_->screen) {
    case S::Main: return PrepWords::kHintMain;
    case S::Units: return PrepWords::kHintUnits;
    case S::Positions: return prep_->carrySlot >= 0 ? PrepWords::kHintPlace : PrepWords::kHintPick;
    case S::View: return PrepWords::kHintView;
    case S::Trade: return session_.db.rules.items.carriedWeapons ? PrepWords::kHintTradeEquip : PrepWords::kHintTrade;
    case S::Shop: return prep_->map.prep->shops[(size_t)prep_->shopIx].buys ? PrepWords::kHintBuyTabs : PrepWords::kHintBuy;
    case S::SellWho: return PrepWords::kHintSellWho;
    case S::SellItems: return PrepWords::kHintSell;
    case S::Begin: return PrepWords::kHintAsk;
    default: return PrepWords::kHintList;
  }
}

}  // namespace ote
