// Human input: the order state machine.
//   Idle -> Selected -> (move) -> ActionMenu -> Targeting -> (attack) -> Idle
// Menus only react once the view has finished playing the last events (see
// onInput), so state can change the moment apply() is called.
#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {

bool BattleScene::submit(const Action& a) {
  events_.clear();
  std::string why;
  if (!battle_.apply(a, events_, &why)) {
    // A rejected action means the UI offered something the rules forbid.
    OTE_WARN("UI bug: action type %d rejected: %s", (int)a.type, why.c_str());
    uiCue(Cue::Error);
    return false;
  }
  view_.push(events_);
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

void BattleScene::onInput(const InputEvent& e) {
  if (covered_) {  // the cover takes every input (no camera, no cursor) until the next player confirms
    if (cover_.confirmed(e)) coverConfirmed();
    return;
  }
  // While a line of dialogue is up, the box takes every input: no camera, no cursor, no orders.
  if (view_.dialogue.active) {
    if (dialogue_.isOpen()) dialogueResult(dialogue_.onInput(e));
    return;
  }
  if (turnInput(e)) return;
  bool confirmish = e.type == InputEvent::PointerDown ||
                    (e.type == InputEvent::ButtonDown && (e.button == Button::Confirm || e.button == Button::Cancel));
  if (mode_ == Mode::Result) {
    if (confirmish && !view_.busy()) leaveResult();
    else if (confirmish) view_.skip(battle_);
    return;
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
  if (mode_ == Mode::Items) {
    itemsInput(e);
    return;
  }
  if (mode_ == Mode::Objective && objectiveInput(e)) return;
  if (mode_ == Mode::UnitInfo) {
    unitInfoInput(e);
    return;
  }
  if (mode_ == Mode::Research) {
    researchInput(e);
    return;
  }
  if (mode_ == Mode::ActionMenu || mode_ == Mode::BuildMenu || mode_ == Mode::EndMenu || mode_ == Mode::StructureMenu ||
      mode_ == Mode::UpgradeMenu) {
    menuInput(e);
    return;
  }
  if (e.type == InputEvent::ButtonDown) handleButton(e.button);
  else if (e.type == InputEvent::PointerMove) {
    Cell c = cellAt(e.x, e.y);
    if (c != kNoCell && mode_ != Mode::Targeting && mode_ != Mode::PickUnit && mode_ != Mode::PickCell) cursor_ = c;
  } else if (e.type == InputEvent::PointerDown) handlePointerDown(e.x, e.y);
}

void BattleScene::handleButton(Button b) {
  switch (mode_) {
    case Mode::Idle: idleButton(b); break;
    case Mode::Selected: selectedButton(b); break;
    case Mode::Targeting: targetingButton(b); break;
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
  if (mode_ == Mode::Targeting) {
    // Tapping a target selects it; tapping the selected one fires.
    for (size_t i = 0; i < targets_.size(); ++i) {
      if (battle_.units[targets_[i].target].cell != c) continue;
      if ((int)i == targetIdx_ || targets_[targetIdx_].target == targets_[i].target) {
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
    if (!u.alive || u.side != battle_.activeSide || u.done) continue;
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
    case Button::Confirm: {
      int uid = battle_.unitAt(cursor_);
      if (uid >= 0 && battle_.canAct(uid) && !battle_.units[uid].done && battle_.units[uid].side == battle_.activeSide) {
        selectUnit(uid);
        return;
      }
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
  if (battle_.units[unit].moved) {  // already moved this turn: only the action is left
    openActionMenu();
    return;
  }
  mode_ = Mode::Selected;
  battle_.reachable(unit, reach_);
  computeOverlay();
}

void BattleScene::computeOverlay() {
  const Unit& u = battle_.units[sel_];
  int n = battle_.grid.cellCount();
  overlay_.reach.assign(n, 0);
  overlay_.attack.assign(n, 0);
  for (Cell s : reach_.stops) overlay_.reach[s] = 1;
  std::vector<int> weapons;  // what it can use now (carried weapons: the ones in its inventory)
  battle_.weaponsOf(u, weapons);
  for (Cell s : reach_.stops) {
    for (int wi : weapons) {
      const WeaponDef& w = battle_.db->weapons[wi];
      if (s != u.cell && !w.afterMove) continue;
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
    case Button::Cancel:
      uiCue(Cue::Cancel);
      cursor_ = battle_.units[sel_].cell;
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
        if (t < 0 || !battle_.isEnemy(u, battle_.units[t])) { uiCue(Cue::Error); break; }  // nowhere to go there
        std::vector<AttackOption> opts;
        battle_.attackOptions(sel_, u.cell, opts);
        for (const AttackOption& o : opts)
          if (o.target == t) { beginTargeting(t); break; }
      }
      break;
    }
    default: break;
  }
}

void BattleScene::finishOrder() {
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
  if (battle_.units[sel_].moved) {  // nothing to undo to: back to the map
    finishOrder();
    return;
  }
  mode_ = Mode::Selected;
  battle_.reachable(sel_, reach_);
  computeOverlay();
}

void BattleScene::beginTargeting(int preferTarget) {
  battle_.attackOptions(sel_, battle_.units[sel_].cell, targets_);
  if (targets_.empty()) return;
  targetIdx_ = 0;
  for (size_t i = 0; i < targets_.size(); ++i)
    if (targets_[i].target == preferTarget) { targetIdx_ = (int)i; break; }
  menu_.close();
  mode_ = Mode::Targeting;
  overlay_ = Overlay();
  cursor_ = battle_.units[targets_[targetIdx_].target].cell;
}

void BattleScene::targetingButton(Button b) {
  int n = (int)targets_.size();
  if (b == Button::Right || b == Button::Down || b == Button::Next) { targetIdx_ = (targetIdx_ + 1) % n; uiCue(Cue::Cursor); }
  else if (b == Button::Left || b == Button::Up || b == Button::Prev) { targetIdx_ = (targetIdx_ + n - 1) % n; uiCue(Cue::Cursor); }
  else if (b == Button::Cancel) {
    uiCue(Cue::Cancel);
    cursor_ = battle_.units[sel_].cell;
    openActionMenu();
    return;
  } else if (b == Button::Confirm) {
    uiCue(Cue::Confirm);
    const AttackOption& t = targets_[targetIdx_];
    if (submit(Action::attack(sel_, t.target, t.weapon))) finishOrder();
    return;
  }
  cursor_ = battle_.units[targets_[targetIdx_].target].cell;
}

}  // namespace ote
