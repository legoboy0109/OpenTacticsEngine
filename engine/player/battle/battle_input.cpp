// Human input: the order state machine.
//   Idle -> Selected -> (move) -> ActionMenu -> Targeting -> (attack) -> Idle
// Menus only react once the view has finished playing the last events (see
// onInput), so state can change the moment apply() is called.
#include "battle/battle_prep_ui.h"
#include "battle/battle_scene.h"
#include "battle/condition_text.h"
#include "core/base/log.h"
#include "core/battle/threat_area.h"
#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {

bool BattleScene::submit(const Action& a) {
  events_.clear();
  std::string why;
  if (a.type == ActionType::Attack) noteForecast(battle_, a.unit, a.target, a.weapon);
  if (!battle_.apply(a, events_, &why)) {
    // A rejected action means the UI offered something the rules forbid.
    OTE_WARN("UI bug: action type %d rejected: %s", (int)a.type, why.c_str());
    uiCue(Cue::Error);
    return false;
  }
  view_.push(events_);
  if (battle_.apGame()) OTE_INFO("ap pool: %d (order %d)", battle_.apPool(battle_.activeSide), (int)a.type);  // action points: what is left
  // The orders of an attack, in the order they were given (the weapon-list tests read these: equip first, then the attack).
  if (a.type == ActionType::Equip)
    OTE_INFO("order: equip %s slot %d", battle_.units[(size_t)a.unit].name.c_str(), a.slot);
  else if (a.type == ActionType::Attack) {
    if (battle_.isAreaWeapon(a.weapon))  // aimed at a square (docs/SRPG.md "Area attacks and line fire")
      OTE_INFO("order: attack %s -> square %d,%d (unit %d) with %s", battle_.units[(size_t)a.unit].name.c_str(), battle_.grid.x(a.cell),
               battle_.grid.y(a.cell), a.target, battle_.db->weapons[(size_t)a.weapon].name.c_str());
    else
      OTE_INFO("order: attack %s -> unit %d with %s", battle_.units[(size_t)a.unit].name.c_str(), a.target, battle_.db->weapons[(size_t)a.weapon].name.c_str());
  }
  // A Move is only the first half of an order (it can still be undone); every
  // other action completes one, so the Continue save is due once it has played.
  if (a.type != ActionType::Move) dirty_ = true;
  return true;
}

// Turning the view is allowed at any time, also while something is playing
// or another side is acting: it changes nothing but the picture.
bool BattleScene::turnInput(const InputEvent& e) {
  if (!map3d_ || mode_ == Mode::SlotList) return false;
  if (proj_.free3d()) {
    static const Button kButtons[6] = {Button::TurnLeft, Button::TurnRight, Button::TiltUp, Button::TiltDown, Button::ZoomIn, Button::ZoomOut};
    if (e.type == InputEvent::PointerUp) freeCam_.releaseAll();  // and the tap goes on to whoever else wants it
    if (e.type == InputEvent::PointerDown || e.type == InputEvent::PointerMove) {
      for (int i = 0; i < cameraButtons(); ++i) {
        int x, y, w, h;
        turnButtonBox(i, x, y, w, h);
        if (e.x < x || e.y < y || e.x >= x + w || e.y >= y + h) continue;
        if (e.type == InputEvent::PointerDown) freeCam_.press(kButtons[i]);
        return true;  // over a button the pointer belongs to the button, not to the square behind it
      }
      return false;
    }
    return freeCam_.input(e);
  }
  int dir = 0;
  if (e.type == InputEvent::ButtonDown && e.button == Button::TurnLeft) dir = -1;
  else if (e.type == InputEvent::ButtonDown && e.button == Button::TurnRight) dir = 1;
  else if (e.type == InputEvent::PointerDown || e.type == InputEvent::PointerMove) {
    for (int i = 0; i < 2 && !dir; ++i) {
      int x, y, w, h;
      turnButtonBox(i, x, y, w, h);
      if (e.x >= x && e.y >= y && e.x < x + w && e.y < y + h) dir = i ? 1 : -1;
    }
    // Over a button the pointer belongs to the button, not to the square behind it.
    if (dir && e.type == InputEvent::PointerMove) return true;
  }
  if (!dir) return false;
  turn_ += dir;
  return true;
}

// A list or a menu has the input (not the map, a target pick or the preparation's map screens).
bool BattleScene::listOpen() const {
  switch (mode_) {
    case Mode::ActionMenu: case Mode::BuildMenu: case Mode::EndMenu: case Mode::SlotList: case Mode::Items: case Mode::StructureMenu:
    case Mode::UpgradeMenu: case Mode::Research: case Mode::Promotion: case Mode::DifficultyMenu: case Mode::WeaponPick:
    case Mode::ResearchCancel: return true;
    case Mode::Prep: return !prepOnMapView() && !(prep_ && prep_->screen == PrepUi::Screen::Positions);
    default: return false;
  }
}

void BattleScene::onInput(const InputEvent& e) {
  trackPointer(e);
  if (covered_) {  // the cover takes every input (no camera, no cursor) until the next player confirms
    if (cover_.confirmed(e)) coverConfirmed();
    return;
  }
  // While a line of dialogue is up, the box takes every input: no camera, no cursor, no orders.
  if (view_.dialogue.active) {
    if (dialogue_.isOpen()) dialogueResult(dialogue_.onInput(e));
    return;
  }
  if (fightScene_.active()) {  // the battle scene takes every input: confirm / cancel / tap skip to the result of this fight
    if (e.type == InputEvent::PointerDown || (e.type == InputEvent::ButtonDown && (e.button == Button::Confirm || e.button == Button::Cancel)))
      fightScene_.skip();
    return;
  }
  // The mouse wheel arrives as ZoomIn / ZoomOut: while a list or menu is open it scrolls that list (ui/row_list.h), so the free
  // 3D camera behind it does not zoom; with the map in charge it zooms as ever.
  const bool wheel = (e.type == InputEvent::ButtonDown || e.type == InputEvent::ButtonUp) &&
                     (e.button == Button::ZoomIn || e.button == Button::ZoomOut);
  if (!(wheel && listOpen()) && turnInput(e)) return;
  bool confirmish = e.type == InputEvent::PointerDown ||
                    (e.type == InputEvent::ButtonDown && (e.button == Button::Confirm || e.button == Button::Cancel));
  if (mode_ == Mode::Result) {
    if (confirmish && !view_.busy()) leaveResult();
    else if (confirmish) view_.skip(battle_);
    return;
  }
  // A story scene without a dialogue box up (a fade, a wait, a shake, a walk): the skip control fast-forwards the whole scene (the
  // Skip button, or Cancel / Menu held half a second, as on the dialogue box; a plain tap still ends only the current entry).
  if (view_.busy() && view_.inScene()) {
    if (e.type == InputEvent::ButtonDown && e.button == Button::Skip) { view_.skipDialogues(battle_); return; }
    if (e.type == InputEvent::ButtonDown && (e.button == Button::Cancel || e.button == Button::Menu)) sceneHold_ = 0;
    if (e.type == InputEvent::ButtonUp && (e.button == Button::Cancel || e.button == Button::Menu)) sceneHold_ = -1;
  }
  if (view_.busy()) {  // the player cannot give orders mid-animation
    if (confirmish) view_.skip(battle_);
    return;
  }
  if (!humanTurn()) return;

  if (mode_ == Mode::SlotList) {
    slotInput(e);
    return;
  }
  if (mode_ == Mode::Prep) {  // the preparation screen (battle_prep*.cpp)
    prepInput(e);
    return;
  }
  if (mode_ == Mode::Items) {
    itemsInput(e);
    return;
  }
  if (mode_ == Mode::WeaponPick) {
    weaponPickInput(e);
    return;
  }
  if (mode_ == Mode::Objective && objectiveInput(e)) return;
  if (mode_ == Mode::UnitInfo) {
    unitInfoInput(e);
    return;
  }
  if (mode_ == Mode::Promotion) {
    promotionInput(e);
    return;
  }
  if (mode_ == Mode::Research) {
    researchInput(e);
    return;
  }
  if (mode_ == Mode::ActionMenu || mode_ == Mode::BuildMenu || mode_ == Mode::EndMenu || mode_ == Mode::StructureMenu ||
      mode_ == Mode::UpgradeMenu || mode_ == Mode::DifficultyMenu || mode_ == Mode::ResearchCancel) {
    menuInput(e);
    return;
  }
  if (e.type == InputEvent::ButtonDown) handleButton(e.button);
  else if (e.type == InputEvent::PointerMove) {
    if (mode_ != Mode::Targeting && mode_ != Mode::PickUnit && mode_ != Mode::PickCell) hoverAt(e.x, e.y);
  } else if (e.type == InputEvent::PointerDown) handlePointerDown(e.x, e.y);
}

void BattleScene::handleButton(Button b) {
  switch (mode_) {
    case Mode::Idle: idleButton(b); break;
    case Mode::Selected: selectedButton(b); break;
    case Mode::Targeting: targetingButton(b); break;
    case Mode::Facing: facingButton(b); break;
    case Mode::PickUnit:
    case Mode::PickCell: pickButton(b); break;
    default: break;
  }
}

void BattleScene::handlePointerDown(int x, int y) {
  Cell c = cellAt(x, y);
  if (c == kNoCell) return;  // panel / outside the map
  if (mode_ == Mode::PickUnit || mode_ == Mode::PickCell) {
    pickPointer(x, y);
    return;
  }
  if (mode_ == Mode::Facing) {
    facingPointer(c);
    return;
  }
  if (mode_ == Mode::Targeting) {
    // Tapping a target selects it; tapping the selected one fires.
    for (size_t i = 0; i < targets_.size(); ++i) {
      if (targets_[i].cell != c) continue;  // (the square aimed at: an area weapon may aim at an empty one)
      if ((int)i == targetIdx_ || targets_[targetIdx_].cell == c) {
        targetingButton(Button::Confirm);
      } else {
        targetIdx_ = (int)i;
        cursor_ = c;
      }
      return;
    }
    return;
  }
  cursor_ = c;
  handleButton(Button::Confirm);
}

void BattleScene::moveCursor(int dx, int dy) {
  const Grid& g = battle_.grid;
  // In a turned view the arrows keep their directions on screen (up = up-right, as in the 2D isometric view).
  for (int i = quarterTurns(); i > 0; --i) {
    int t = dx;
    dx = dy;
    dy = -t;
  }
  Cell c = g.at(g.x(cursor_) + dx, g.y(cursor_) + dy);
  if (c != kNoCell && c != cursor_) uiCue(Cue::Cursor);
  if (c != kNoCell) cursor_ = c;
}

void BattleScene::cycleUnit(int dir) {
  int cur = battle_.unitAt(cursor_);
  int best = -1, wrap = -1;
  for (const Unit& u : battle_.units) {
    if (!battle_.mayActNow(u)) continue;
    if (dir > 0) {
      if (u.id > cur && best < 0) best = u.id;
      if (wrap < 0) wrap = u.id;
    } else {
      if (u.id < cur || cur < 0) best = u.id;
      wrap = u.id;
    }
  }
  int pick = best >= 0 ? best : wrap;
  if (pick >= 0 && battle_.units[pick].cell != cursor_) uiCue(Cue::Cursor);
  if (pick >= 0) cursor_ = battle_.units[pick].cell;
}

void BattleScene::idleButton(Button b) {
  switch (b) {
    case Button::Up: moveCursor(0, -1); break;
    case Button::Down: moveCursor(0, 1); break;
    case Button::Left: moveCursor(-1, 0); break;
    case Button::Right: moveCursor(1, 0); break;
    case Button::Next: cycleUnit(1); break;
    case Button::Prev: cycleUnit(-1); break;
    case Button::Menu: openEndMenu(); break;
    case Button::Ranges: toggleAllRanges(); break;
    case Button::Cancel: clearRanges(); break;  // (nothing else to cancel on the plain map)
    case Button::Confirm: {
      int uid = battle_.unitAt(cursor_);
      if (uid >= 0 && battle_.mayActNow(battle_.units[uid])) {
        selectUnit(uid);
        return;
      }
      // A unit of the active side that a condition stops (asleep, or acting on its own): it takes no orders, and tapping it says why
      // (docs/SRPG.md section 9).
      if (uid >= 0 && battle_.units[uid].alive && battle_.units[uid].side == battle_.activeSide) {
        const std::string stopped = unitStoppedText(battle_, battle_.units[uid]);
        if (!stopped.empty()) {
          uiCue(Cue::Error);
          say(stopped);
          OTE_INFO("unit stopped: %s", stopped.c_str());
          break;
        }
      }
      // An enemy in sight: mark it (its reach is drawn, enemy_ranges.h). This used to open the map menu, which Menu / a tap on an empty
      // square still does (and its "Unit" entry shows this unit's details).
      if (uid >= 0 && rangesOn() && markEnemy(uid)) break;
      uiCue(Cue::Confirm);
      if (!openStructureAt(cursor_)) openEndMenu();  // Build / Upgrade / Research of an own structure (battle_economy.cpp)
      break;
    }
    default: break;
  }
}

void BattleScene::selectUnit(int unit) {
  soundCue(Cue::Select, unit);
  sel_ = unit;
  hasUndo_ = false;
  itemsUsed_ = false;
  ambushed_ = false;
  // Moved this turn: only the action is left. (Not for a unit that has acted: it may still move, so it shows its
  // remaining range like a fresh unit; its menu holds Info and Wait only.)
  movingOn_ = false;
  if (battle_.units[unit].moved && !battle_.units[unit].acted && !(battle_.apGame() && battle_.apMayMove(battle_.units[unit]))) {
    openActionMenu();
    return;
  }
  mode_ = Mode::Selected;
  battle_.reachable(unit, reach_);
  computeOverlay();
  const std::string stuck = unitStuckText(battle_, battle_.units[unit]);  // a condition that stops it moving (docs/SRPG.md section 9)
  if (!stuck.empty()) {
    say(stuck);
    OTE_INFO("unit stuck: %s", stuck.c_str());
  }
}

void BattleScene::computeOverlay() {
  const Unit& u = battle_.units[sel_];
  int n = battle_.grid.cellCount();
  overlay_.reach.assign(n, 0);
  overlay_.attack.assign(n, 0);
  for (Cell s : reach_.stops) overlay_.reach[s] = 1;
  // Height rules: one line per painted selection, for the player tests (the squares it can stop on, and the squares only its weapons reach).
  struct HeightLog {
    const BattleScene& scene;
    ~HeightLog() {
      if (!scene.battle_.heightGame()) return;
      int move = 0, attack = 0;
      for (size_t c = 0; c < scene.overlay_.reach.size(); ++c) {
        move += scene.overlay_.reach[c] ? 1 : 0;
        attack += scene.overlay_.attack[c] && !scene.overlay_.reach[c] ? 1 : 0;
      }
      OTE_INFO("overlay: %d move squares, %d attack-only squares", move, attack);
    }
  } heightLog{*this};
  if (u.acted) return;  // it cannot attack any more: no red squares
  if (battle_.apGame() && battle_.apAttacksLeft(u) <= 0) return;  // (action points: the attacks of the turn are made)
  for (Cell s : reach_.stops) {
    std::vector<int> weapons;  // what it could use from there: carried weapons, and the weapon of a structure standing there
    battle_.attackWeaponsAt(u, s, weapons);
    // Action points: the walk to the square and the attack must both be paid.
    const int walk = battle_.apGame() && s != u.cell ? battle_.apMoveCost(u, reach_.cost[(size_t)s]) : 0;
    for (int wi : weapons) {
      const WeaponDef& w = battle_.db->weapons[wi];
      if ((s != u.cell || (battle_.apGame() && u.moved)) && !w.afterMove) continue;
      if (battle_.apGame() && walk + battle_.apAttackCost(sel_, wi) > battle_.apPool(u.side)) continue;
      if (battle_.heightGame()) {  // the height rules raise and limit what a weapon reaches (core/battle/height_rules.cpp): the one shared function says where
        markWeaponReach(battle_, sel_, wi, s, overlay_.attack);
        continue;
      }
      for (Cell c = 0; c < n; ++c) {
        int d = battle_.grid.distance(s, c);
        if (d >= w.minRange && d <= w.maxRange) overlay_.attack[c] = 1;
      }
    }
  }
}

void BattleScene::selectedButton(Button b) {
  switch (b) {
    case Button::Up: moveCursor(0, -1); break;
    case Button::Down: moveCursor(0, 1); break;
    case Button::Left: moveCursor(-1, 0); break;
    case Button::Right: moveCursor(1, 0); break;
    case Button::Ranges: toggleAllRanges(); break;
    case Button::Cancel:
      uiCue(Cue::Cancel);
      cursor_ = battle_.units[sel_].cell;
      if (movingOn_) {  // asked for from the action menu: back to it, nothing changed
        movingOn_ = false;
        overlay_ = Overlay();
        openActionMenu();
        break;
      }
      mode_ = Mode::Idle;
      overlay_ = Overlay();
      break;
    case Button::Confirm: {
      const Unit& u = battle_.units[sel_];
      if (cursor_ == u.cell) {  // stay put
        uiCue(Cue::Confirm);
        openActionMenu();
      } else if (reach_.canStop(cursor_)) {
        undo_ = battle_;  // copy BEFORE the move: Cancel restores it
        undoCell_ = u.cell;
        const Cell wanted = cursor_;
        uiCue(Cue::Confirm);
        if (!submit(Action::move(sel_, cursor_))) return;
        if (battle_.units[sel_].done) {  // fog: an ambush ended its turn; nothing to undo to (that would show what was hidden)
          dirty_ = true;
          finishOrder();
          return;
        }
        // Fog, ambush "canAct": the unit stopped short and may still act. The move stays (undoing it would show what
        // was hidden): Cancel in the menu only closes it, and the unit can be picked again for its action menu.
        ambushed_ = battle_.units[sel_].cell != wanted;
        hasUndo_ = !ambushed_;
        if (ambushed_) OTE_INFO("ambushed: %s may still act from %d,%d", battle_.units[sel_].name.c_str(),
                                battle_.grid.x(battle_.units[sel_].cell), battle_.grid.y(battle_.units[sel_].cell));
        openActionMenu();
      } else {
        int t = battle_.unitAt(cursor_);
        if (u.acted) { uiCue(Cue::Error); break; }  // no attack left
        if (t < 0 || !battle_.isEnemy(u, battle_.units[t])) {  // nowhere to go there
          uiCue(Cue::Error);
          if (const char* steep = battle_.heightMoveRefusal(sel_, cursor_)) { say(steep); OTE_INFO("refused: %s", steep); }  // height rules: say why
          break;
        }
        std::vector<AttackOption> opts;
        battle_.attackOptions(sel_, u.cell, opts);
        bool offered = false;
        for (const AttackOption& o : opts)
          if (o.target == t) { beginAttack(t); offered = true; break; }
        if (!offered) {  // height rules: an enemy it cannot hit because of how high or low it stands
          std::vector<int> weapons;
          battle_.attackWeaponsAt(u, u.cell, weapons);
          for (int w : weapons)
            if (const char* why = battle_.heightAimRefusal(w, u.cell, cursor_)) { say(why); OTE_INFO("refused: %s", why); break; }
        }
      }
      break;
    }
    default: break;
  }
}

void BattleScene::finishOrder() {
  movingOn_ = false;
  mode_ = Mode::Idle;
  menu_.close();
  overlay_ = Overlay();
  hasUndo_ = false;
  itemsUsed_ = false;
  ambushed_ = false;
  if (sel_ >= 0 && battle_.units[sel_].alive) cursor_ = battle_.units[sel_].cell;
  sel_ = -1;
}

void BattleScene::cancelAction() {
  if (hasUndo_) {
    battle_ = undo_;       // Battle is a value: this restores the pre-move state exactly
    view_.resync(battle_);
    cursor_ = undoCell_;
    hasUndo_ = false;
  }
  menu_.close();
  if (battle_.units[sel_].moved && !(battle_.apGame() && battle_.apMayMove(battle_.units[sel_]))) {  // nothing to undo to: back to the map
    finishOrder();
    return;
  }
  mode_ = Mode::Selected;
  battle_.reachable(sel_, reach_);
  computeOverlay();
}

void BattleScene::beginAttack(int preferTarget) {
  if (session_.db.attackWeaponPick) openWeaponPick(preferTarget);  // battle_weapon_pick.cpp
  else beginTargeting(preferTarget);
}

void BattleScene::beginTargeting(int preferTarget) {
  battle_.attackOptions(sel_, battle_.units[sel_].cell, targets_);
  apKeepAffordable(targets_);
  weaponRow_ = -1;
  enterTargeting(preferTarget);
}

void BattleScene::enterTargeting(int preferTarget) {
  if (targets_.empty()) return;
  targetIdx_ = 0;
  for (size_t i = 0; i < targets_.size(); ++i)
    if (targets_[i].target == preferTarget) { targetIdx_ = (int)i; break; }
  menu_.close();
  mode_ = Mode::Targeting;
  overlay_ = Overlay();
  cursor_ = targets_[targetIdx_].cell;
}

void BattleScene::targetingButton(Button b) {
  int n = (int)targets_.size();
  if (b == Button::Right || b == Button::Down || b == Button::Next) { targetIdx_ = (targetIdx_ + 1) % n; uiCue(Cue::Cursor); }
  else if (b == Button::Left || b == Button::Up || b == Button::Prev) { targetIdx_ = (targetIdx_ + n - 1) % n; uiCue(Cue::Cursor); }
  else if (b == Button::Cancel) {
    uiCue(Cue::Cancel);
    cursor_ = battle_.units[sel_].cell;
    if (weaponRow_ >= 0) openWeaponPick(weaponPrefer_, weaponRow_);  // came from the weapon list: back to it
    else openActionMenu();
    return;
  } else if (b == Button::Confirm) {
    uiCue(Cue::Confirm);
    const AttackOption t = targets_[targetIdx_];
    if (weaponRow_ >= 0 && !equipForAttack()) return;  // "pick": the weapon is put in hand first, as the Items menu does
    Action shot = Action::attack(sel_, t.target, t.weapon);
    if (t.area) shot.cell = t.cell;  // an area weapon: the square aimed at (it may be empty)
    if (submit(shot)) finishOrder();
    return;
  }
  cursor_ = targets_[targetIdx_].cell;
}

}  // namespace ote
