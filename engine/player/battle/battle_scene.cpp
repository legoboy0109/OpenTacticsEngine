#include "battle/battle_scene.h"

#include "core/ai/ai.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

BattleScene::BattleScene(SceneHost& host)
    : host_(host), session_(*host.session()), battle_(host.session()->battle) {
  autoplay_ = host.args().autoplay;
  const ViewConfig& vc = session_.db.view;
  layout_ = Layout::make(vc.width, vc.height, vc.tileSize, host.renderer());
  view_.init(battle_, layout_.tile, autoplay_);
  view_.push(session_.startEvents);  // first TurnStarted + income
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
}

bool BattleScene::humanTurn() const {
  return !battle_.over && !autoplay_ && !battle_.sides[battle_.activeSide].ai;
}

Cell BattleScene::cellAt(int x, int y) const {
  if (x < 0 || y < 0 || x >= layout_.vw || y >= layout_.vh) return kNoCell;
  int mx = x + cam_.ix(), my = y + cam_.iy();
  if (mx < 0 || my < 0) return kNoCell;
  return battle_.grid.at(mx / layout_.tile, my / layout_.tile);
}

void BattleScene::enterResult() {
  mode_ = Mode::Result;
  menu_.close();
  bool humanWon = false, anyHuman = false;
  std::string winners;
  for (const SideState& s : battle_.sides) {
    if (battle_.winnerTeam < 0 || s.team != battle_.winnerTeam) continue;
    if (!s.ai) humanWon = true;
    if (!s.defeated) winners += (winners.empty() ? "" : " & ") + s.name;
  }
  for (const SideState& s : battle_.sides) anyHuman |= !s.ai;
  resultSub_ = "Confirm: back to map select";
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

void BattleScene::aiStep() {
  events_.clear();
  aiTakeStep(battle_, events_);
  view_.push(events_);
}

void BattleScene::beginHumanTurn() {
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
    fx = (battle_.grid.x(c) + 0.5f) * layout_.tile;
    fy = (battle_.grid.y(c) + 0.5f) * layout_.tile;
  }
  cam_.follow(fx, fy, layout_, battle_.grid.width() * layout_.tile, battle_.grid.height() * layout_.tile, dt);
}

void BattleScene::update(double dt) {
  time_ += (float)dt;
  view_.update(dt, battle_);
  if (mode_ != Mode::Result && !view_.busy()) {
    if (battle_.over) enterResult();
    else if (!humanTurn()) aiStep();
    else if (turnKey_ != battle_.round * 1000 + battle_.activeSide) {
      turnKey_ = battle_.round * 1000 + battle_.activeSide;
      beginHumanTurn();
    }
  }
  followCamera(dt);
  if (mode_ == Mode::ActionMenu) anchorActionMenu();
}

std::string BattleScene::hint() const {
  if (mode_ == Mode::Result) return "Confirm / tap: back to map select";
  if (view_.busy()) return "Confirm / Cancel / tap: skip animation";
  if (!humanTurn()) return battle_.sides[battle_.activeSide].name + " is acting...";
  switch (mode_) {
    case Mode::Idle:
      if (!humanHasOptions()) return "All units done.  Menu (M): end turn";
      return "Arrows:cursor  Z:select  Tab/Q:next/prev unit  M:menu";
    case Mode::Selected: return "Arrows:cursor  Z:move here (own cell = stay)  X:cancel";
    case Mode::ActionMenu: return "Up/Down + Z:choose  X:undo move";
    case Mode::Targeting: return "Arrows/Tab:next target  Z:attack  X:back";
    case Mode::BuildMenu: return "Up/Down + Z:build  X:cancel";
    case Mode::EndMenu: return "Up/Down + Z:choose  X:cancel";
    default: return "";
  }
}

void BattleScene::draw(Renderer& r) {
  ensureLayout(r);
  DrawCtx ctx{battle_, view_, layout_, cam_, time_};
  bool human = humanTurn() && mode_ != Mode::Result;
  bool idleView = !view_.busy();
  r.clear(Color(0, 0, 0));
  drawMap(r, ctx, mode_ == Mode::Selected ? overlay_ : Overlay(), cursor_, human);
  drawFloats(r, ctx, false);
  bool aiActing = !battle_.over && !humanTurn();  // nobody to inspect for: AI is playing
  drawPanel(r, ctx, aiActing ? kNoCell : cursor_, autoplay_);
  drawHint(r, layout_, hint());
  drawFloats(r, ctx, true);

  if (idleView && human) {
    if (mode_ == Mode::Targeting && targetIdx_ < (int)targets_.size()) {
      const AttackOption& t = targets_[targetIdx_];
      drawForecast(r, ctx, sel_, t.target, t.weapon, battle_.units[t.target].cell);
    }
    menu_.draw(r);
  }
  if (!view_.banner.empty()) drawBanner(r, layout_, view_.banner, view_.bannerColor, 2, "");
  drawLevelUp(r, layout_, view_.levelUp);
  if (mode_ == Mode::Result) drawBanner(r, layout_, resultText_, resultColor_, 3, resultSub_);
}

}  // namespace ote
