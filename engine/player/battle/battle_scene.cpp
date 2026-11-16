#include "battle/battle_scene.h"

#include <algorithm>
#include <cmath>

#include "battle/economy_text.h"
#include "battle/fog_text.h"
#include "battle/hex_draw.h"
#include "core/ai/ai.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

BattleScene::BattleScene(SceneHost& host)
    : host_(host), session_(*host.session()), battle_(host.session()->battle) {
  autoplay_ = host.args().autoplay;
  store_ = host.saveStore(host.currentGameId());
  OTE_INFO("saves: %s", !store_.enabled() ? "not available in this run"
                        : session_.db.saving == SavingMode::Off ? "off for this game"
                                                                : store_.folder().c_str());
  styleIdx_ = std::max(0, std::min(host.styleIndex(), (int)session_.db.styles.size() - 1));
  const ViewConfig& vc = session_.db.view;
  layout_ = Layout::make(vc.width, vc.height, vc.tileSize, host.renderer());
  proj_.setViewport(layout_.vw, layout_.vh);
  proj_.init(battle_, &session_.db.styles[styleIdx_], layout_.tile);
  if (proj_.view3d()) map3d_.reset(new Map3D(host.renderer3d(), host.show3d(), session_.dir, session_.db.graphics));
  view_.init(battle_, layout_.tile, autoplay_);
  view_.setAudio(&host_.gameAudio());
  initFog();  // fog of war: who the screen is drawn for (battle_fog.cpp)
  view_.push(session_.startEvents);  // first TurnStarted + income (empty when the battle was resumed from a save)
  // A new battle says what it is about under its first banner (nothing when the map has no objective text).
  if (!session_.startEvents.empty() && !battle_.objectiveText().empty()) view_.announceObjective();
  int rx, ry;
  panelResourceAnchor(layout_, (int)session_.db.resources.size(), rx, ry);
  view_.setResourceAnchor(rx, ry);
}

void BattleScene::ensureLayout(const Renderer& r) {
  if (!layout_.stale(r.width(), r.height(), r)) return;
  layout_ = Layout::make(r.width(), r.height(), layout_.tile, r);
  int rx, ry;
  panelResourceAnchor(layout_, (int)session_.db.resources.size(), rx, ry);
  view_.setResourceAnchor(rx, ry);
  cam_.placed = false;
  proj_.setViewport(layout_.vw, layout_.vh);
}

bool BattleScene::humanTurn() const {
  return !battle_.over && !autoplay_ && !battle_.sides[battle_.activeSide].ai;
}

Cell BattleScene::cellAt(int x, int y) const {
  if (x < 0 || y < 0 || x >= layout_.vw || y >= layout_.vh) return kNoCell;
  return proj_.pick(x + cam_.ix(), y + cam_.iy());
}

// Bottom-left corner of the map: 0 = turn left, 1 = turn right.
void BattleScene::turnButtonBox(int which, int& x, int& y, int& w, int& h) const {
  w = h = std::max(14, layout_.lh + 8);
  x = 3 + which * (w + 3) + (which / 2) * 4;  // pairs: turn, tilt, zoom
  y = layout_.vh - h - 3;
}

void BattleScene::cellScreenBox(Cell c, int& x, int& y, int& w, int& h) const {
  proj_.cellBox(battle_.grid.x(c), battle_.grid.y(c), x, y, w, h);
  x -= cam_.ix();
  y -= cam_.iy();
}

void BattleScene::fillCellBox(Renderer& r, int x, int y, int w, int h, Color col) const {
  if (proj_.hex() && !proj_.iso()) fillHex(r, proj_.hexGeo(), x, y, col);  // (isometric: the box over the unit)
  else r.fillRect(x, y, w, h, col);
}

void BattleScene::enterResult() {
  if (chapterBattle()) chapterDecided();  // the playthrough goes on: see battle_campaign.cpp
  else clearContinue();                   // a finished battle has nothing to continue
  mode_ = Mode::Result;
  menu_.close();
  slots_.close();
  closeItems();
  bool humanWon = false, anyHuman = false;
  std::string winners;
  for (const SideState& s : battle_.sides) {
    if (battle_.winnerTeam < 0 || s.team != battle_.winnerTeam) continue;
    if (!s.ai) humanWon = true;
    if (!s.defeated) winners += (winners.empty() ? "" : " & ") + s.name;
  }
  for (const SideState& s : battle_.sides) anyHuman |= !s.ai;
  soundResult(anyHuman && !autoplay_, humanWon);
  resultSub_ = chapterBattle() ? "Confirm: continue"
               : session_.db.campaign ? "Confirm: back to the game page" : "Confirm: back to map select";
  if (battle_.winnerTeam < 0) {
    resultText_ = "DRAW";
    resultColor_ = Color(150, 155, 175);
  } else if (!autoplay_ && anyHuman) {
    resultText_ = humanWon ? "VICTORY" : "DEFEAT";
    resultColor_ = humanWon ? Color(235, 195, 50) : Color(180, 40, 40);
  } else {
    resultText_ = winners + " wins";
    for (const SideState& s : battle_.sides)
      if (s.team == battle_.winnerTeam && !s.defeated) { resultColor_ = Color::rgb(s.color); break; }
  }
  OTE_INFO("battle over: %s", resultText_.c_str());
}

void BattleScene::beginHumanTurn() {
  OTE_INFO("turn: %s, round %d t=%.2f", battle_.sides[(size_t)battle_.activeSide].name.c_str(), battle_.round, time_);
  mode_ = Mode::Idle;
  menu_.close();
  sel_ = -1;
  overlay_ = Overlay();
  hasUndo_ = false;
  for (const Unit& u : battle_.units)
    if (u.alive && u.side == battle_.activeSide && !u.done) {
      cursor_ = u.cell;
      return;
    }
}

// True if the human still has something to do: a unit that has not acted or
// an affordable build. Used only for the "end turn?" hint.
bool BattleScene::humanHasOptions() const {
  for (const Unit& u : battle_.units)
    if (u.alive && u.side == battle_.activeSide && !u.done) return true;
  if (!session_.db.rules.production) return false;
  std::vector<int> opts;
  for (Cell c = 0; c < battle_.grid.cellCount(); ++c) {
    battle_.buildOptions(c, opts);
    for (int t : opts)
      if (battle_.canAfford(battle_.activeSide, t)) return true;
  }
  return false;
}

void BattleScene::followCamera(double dt) {
  float fx, fy;
  if (view_.hasFocus) {
    fx = view_.focusX;
    fy = view_.focusY;
  } else {
    Cell c = battle_.grid.valid(cursor_) ? cursor_ : 0;
    cellCentre(battle_.grid, layout_.tile, c, fx, fy);  // (a hexagon's middle on a hexagon map)
  }
  if (proj_.free3d()) {  // this view has no scrolling: its own camera keeps the spot in the picture
    float gx = fx / layout_.tile, gy = fy / layout_.tile;
    freeCam_.update(dt, proj_.cam3(), gx - proj_.gridW() * 0.5f, proj_.elevationAt(gx, gy) * proj_.stepY(), gy - proj_.gridH() * 0.5f);
    cam_.x = cam_.y = 0;
    cam_.placed = true;
    return;
  }
  float wx, wy;
  proj_.toWorld(fx, fy, wx, wy);
  if (map3d_ && !cam_.placed) cam_.centre(wx, wy, layout_, proj_.worldW(), proj_.worldH());
  else cam_.follow(wx, wy, layout_, proj_.worldW(), proj_.worldH(), dt);
}

void BattleScene::update(double dt) {
  time_ += (float)dt;
  if (covered_) return;  // hot-seat cover: nothing plays (or is decided) until the next player confirms
  view_.update(dt, battle_);
  updateDialogue(dt);
  if (mode_ != Mode::Result && !view_.busy()) {
    updateSaves();  // before the AI moves on: a turn's first idle moment is its starting position
    if (battle_.over) enterResult();
    else if (!humanTurn()) aiStep();
    else if (fogTurnGate()) return;  // the cover went up (hot-seat): the turn begins when it comes down
    else if (turnKey_ != battle_.round * 1000 + battle_.activeSide) {
      turnKey_ = battle_.round * 1000 + battle_.activeSide;
      beginHumanTurn();
    }
  }
  if (turnPos_ != (float)turn_) {  // a quarter turn takes a quarter of a second
    float d = (float)turn_ - turnPos_, step = (float)dt * 4.0f;
    turnPos_ = std::fabs(d) <= step ? (float)turn_ : turnPos_ + (d > 0 ? step : -step);
    if (turnPos_ == (float)turn_) {  // keep the numbers small: whole turns change nothing
      turn_ = ((turn_ % 4) + 4) % 4;
      turnPos_ = (float)turn_;
    }
    // The square under the cursor keeps its place on screen while the map turns about it.
    Cell c = battle_.grid.valid(cursor_) ? cursor_ : 0;
    float fx = (battle_.grid.x(c) + 0.5f) * layout_.tile, fy = (battle_.grid.y(c) + 0.5f) * layout_.tile, ax, ay, bx, by;
    proj_.toWorld(fx, fy, ax, ay);
    proj_.setTurn(turnPos_);
    proj_.toWorld(fx, fy, bx, by);
    cam_.x += bx - ax;
    cam_.y += by - ay;
  }
  if (!view_.notice.empty()) {  // something the view wants said in the hint line (an item sent to storage)
    say(view_.notice);
    view_.notice.clear();
  }
  if (noticeTime_ > 0) noticeTime_ -= (float)dt;
  // A chapter battle watched in --autoplay goes on by itself, so a smoke test plays a whole story.
  if (autoplay_ && chapterBattle() && mode_ == Mode::Result && !view_.busy()) {
    resultTime_ += (float)dt;
    if (resultTime_ >= 1.5f) leaveResult();
  }
  logInspected();
  followCamera(dt);
  if (mode_ == Mode::ActionMenu) anchorActionMenu();
}

std::string BattleScene::hint() const {
  if (noticeTime_ > 0 && !notice_.empty()) return notice_;
  if (view_.dialogue.active) return kDialogueHint;
  if (mode_ == Mode::Result)
    return chapterBattle() ? "Confirm / tap: continue"
           : session_.db.campaign ? "Confirm / tap: back to the game page" : "Confirm / tap: back to map select";
  if (view_.busy()) return "Confirm / Cancel / tap: skip animation";
  if (!humanTurn()) return battle_.sides[battle_.activeSide].name + " is acting...";
  switch (mode_) {
    case Mode::Idle:
      if (!humanHasOptions()) return "All units done.  Menu (M): end turn";
      return proj_.free3d() ? "Arrows  Z:select  ,.:turn  RF:tilt  -=:zoom  M:menu"
             : map3d_ ? "Arrows:cursor  Z:select  Tab/Q:next unit  ,/.:turn view  M:menu"
                    : "Arrows:cursor  Z:select  Tab/Q:next/prev unit  M:menu";
    case Mode::Selected: {
      // A tank too empty for the full move: the blue range is already cut by the core, say why.
      const Unit& su = battle_.units[(size_t)sel_];
      if (battle_.maxFuel(sel_) > 0 && battle_.fuel(sel_) < battle_.stat(su, session_.db.rules.statMove))
        return EconomyWords::kLowFuel + intStr(battle_.fuel(sel_)) + "  Z:move  X:cancel";
      return "Arrows:cursor  Z:move here (own cell = stay)  X:cancel";
    }
    case Mode::ActionMenu:
      return ambushed_ ? FogWords::kHintAmbushed : itemsUsed_ ? "Up/Down + Z:choose" : "Up/Down + Z:choose  X:undo move";
    case Mode::Targeting: return "Arrows/Tab:next target  Z:attack  X:back";
    case Mode::BuildMenu: return "Up/Down + Z:build  X:cancel";
    case Mode::EndMenu: return "Up/Down + Z:choose  X:cancel";
    case Mode::Items: return itemsHint();
    case Mode::Objective: return "Z / X / tap: close";
    case Mode::StructureMenu:
    case Mode::UpgradeMenu:
    case Mode::Research: return economyHint();
    default: return "";
  }
}

void BattleScene::draw(Renderer& r) {
  if (covered_) {  // nothing of the map is drawn under the cover
    cover_.draw(r);
    return;
  }
  ensureLayout(r);
  DrawCtx ctx{battle_, view_, layout_, cam_, time_, &session_.db.styles[styleIdx_], host_.sprites(), styleIdx_,
              &proj_, host_.isoTiles(), map3d_.get()};
  bool human = humanTurn() && mode_ != Mode::Result;
  bool idleView = !view_.busy();
  r.clear(Color(0, 0, 0));
  drawMap(r, ctx, mode_ == Mode::Selected ? overlay_ : supplyOverlay(), cursor_, human);
  drawFloats(r, ctx, false);
  drawStory(r);
  drawEconomy(r);
  if (map3d_) {  // the camera buttons, for pointer and touch
    static const char* const kLabels[6] = {"<", ">", "^", "v", "+", "-"};
    for (int i = 0; i < cameraButtons(); ++i) {
      int x, y, w, h;
      turnButtonBox(i, x, y, w, h);
      r.fillRect(x, y, w, h, Color(10, 12, 24, 190));
      r.drawRect(x, y, w, h, Color(150, 156, 185, 200));
      const char* label = kLabels[i];
      r.drawText(x + (w - r.textWidth(label)) / 2, y + (h - layout_.lh) / 2, label, Color(235, 238, 255));
    }
  }
  bool aiActing = !battle_.over && !humanTurn();  // nobody to inspect for: AI is playing
  drawPanel(r, ctx, aiActing ? kNoCell : cursor_, autoplay_);
  drawHint(r, layout_, hint());
  drawFloats(r, ctx, true);

  if (idleView && human) {
    if (mode_ == Mode::Targeting && targetIdx_ < (int)targets_.size()) {
      const AttackOption& t = targets_[targetIdx_];
      drawForecast(r, ctx, sel_, t.target, t.weapon, battle_.units[t.target].cell);
    }
    if (mode_ == Mode::Items) drawItems(r);
    else if (mode_ == Mode::Research) researchPanel_.draw(r);
    else menu_.draw(r);
    if (mode_ == Mode::Objective) drawObjectiveBox(r);
  }
  if (mode_ == Mode::SlotList) {
    r.fillRect(0, 0, r.width(), r.height(), Color(0, 0, 0, 150));
    slots_.fitScreen(r);
    slots_.draw(r);
  }
  if (!view_.banner.empty()) drawBanner(r, layout_, view_.banner, view_.bannerColor, 2, view_.bannerSub);
  if (view_.dialogue.active) dialogue_.draw(r);
  drawLevelUp(r, layout_, view_.levelUp);
  if (mode_ == Mode::Result) drawBanner(r, layout_, resultText_, resultColor_, 3, resultSub_);
}

}  // namespace ote
