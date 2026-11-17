// Story events in the battle scene (docs/EVENTS.md "Dialogue (player)"): the dialogue box that the
// view waits on, the Talk entries of the action menu, and the objective box of the battle menu.
// The pause mechanism lives in BattleView (a Dialogue entry stays "current" until skip()); this
// file only shows the box and tells the view when the player is done.
#include <algorithm>

#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "ui/chrome.h"
#include "ui/dialogue_portrait.h"
#include "ui/text.h"

namespace ote {

// ---- Dialogue

PortraitArt BattleScene::portraitFor(const std::string& id) const {
  return portraitArt(session_.db, host_.sprites(), styleIdx_, id);  // a missing portrait is fine: no picture
}

void BattleScene::dialogueResult(DialogueBox::Result res) {
  if (res == DialogueBox::None) return;
  if (res == DialogueBox::Chosen) {
    const int pick = dialogue_.chosen();
    dialogue_.close();
    answerChosen(pick);
    return;
  }
  dialogue_.close();  // the next line, if any, opens in the next update()
  if (res == DialogueBox::SkipAll) view_.skipDialogues(battle_);
  else view_.skip(battle_);
}

// The answer goes to the core, which runs the rest of the action that reached the question; the view then plays the
// entries behind the question. The Continue save comes once everything has played (updateSaves waits for an idle view).
void BattleScene::answerChosen(int pick) {
  events_.clear();
  OTE_INFO("choice: battle answer %d", pick);
  if (!battle_.answerChoice(pick, events_)) OTE_WARN("UI bug: answer %d refused (no question is waiting)", pick);
  view_.push(events_);
  view_.skip(battle_);  // the question's entry ends; the next one starts
  dirty_ = true;
}

void BattleScene::updateDialogue(double dt) {
  const DialogueState& d = view_.dialogue;
  if (d.active && d.serial != dialogueSerial_) {
    dialogueSerial_ = d.serial;
    freeCam_.releaseAll();  // a camera button held down when the line appeared must not keep turning
    Renderer& r = host_.renderer();
    // A speaker standing on the scene stage has no portrait beside the box; a narrator line has none at all.
    dialogue_.open(r, layout_.W, layout_.H, layout_.vh, d.line, d.staged || d.line.narrate ? PortraitArt() : portraitFor(d.line.portrait));
    if (!d.answers.empty()) {
      dialogue_.setAnswers(r, d.answers);
      OTE_INFO("choice: battle question \"%s\": %s", d.line.text.c_str(), dialogue_.describeAnswers().c_str());
    }
  } else if (!d.active && dialogue_.isOpen()) {
    dialogue_.close();  // the view moved on by itself (autoplay)
  }
  if (dialogue_.isOpen()) dialogueResult(dialogue_.update(dt));
}

// ---- Talk

void BattleScene::addTalkEntries(std::vector<MenuItem>& items) {
  battle_.talkOptions(sel_, battle_.units[(size_t)sel_].cell, talk_);
  for (size_t i = 0; i < talk_.size(); ++i) {
    const TalkOption& t = talk_[i];
    std::string label = t.label >= 0 ? battle_.storyText(t.label) : "";
    if (label.empty()) label = "Talk";
    // Several partners (or events) at once: tell them apart by the partner's name.
    if (talk_.size() > 1) label += ": " + battle_.units[(size_t)t.target].name;
    items.push_back({label, "", true, kTalkBase + (int)i});
  }
}

void BattleScene::talkChosen(int index) {
  if (index < 0 || index >= (int)talk_.size()) return;
  const TalkOption t = talk_[(size_t)index];
  if (!submit(Action::talk(sel_, t.target, t.event))) return;
  const Unit& u = battle_.units[(size_t)sel_];
  // Like the item actions: the turn goes on (menu rebuilt, move no longer undoable), unless the event
  // ended it, took the unit away or ended the battle.
  if (battle_.over || !u.alive || u.done) finishOrder();
  else itemActionDone();
}

// ---- Objective

void BattleScene::openObjective() {
  mode_ = Mode::Objective;
  menu_.close();
}

bool BattleScene::objectiveInput(const InputEvent& e) {
  bool close = e.type == InputEvent::PointerDown ||
               (e.type == InputEvent::ButtonDown &&
                (e.button == Button::Confirm || e.button == Button::Cancel || e.button == Button::Menu));
  if (close) mode_ = Mode::Idle;
  return true;
}

void BattleScene::drawObjectiveBox(Renderer& r) {
  const int pad = 6;
  int maxW = std::max(60, std::min(layout_.vw - 16, layout_.W * 3 / 5));
  std::vector<std::string> lines = wrapText(r, battle_.objectiveText(), maxW - 2 * pad);
  int textW_ = 0;
  for (const std::string& l : lines) textW_ = std::max(textW_, textW(r, l));
  int w = std::min(maxW, std::max(textW_, textW(r, "Objective")) + 2 * pad);
  int lh = layout_.lh;
  int h = 2 * pad + lh + 4 + (int)lines.size() * lh;
  int x = (layout_.vw - w) / 2, y = std::max(2, (layout_.vh - h) / 2);
  r.fillRect(x + 2, y + 2, w, h, Color(0, 0, 0, 110));
  r.fillRect(x, y, w, h, Color(24, 28, 44, 245));
  r.drawRect(x, y, w, h, Color(200, 205, 230));
  r.drawText(x + pad, y + pad, "Objective", chrome::kAccent);
  int ty = y + pad + lh + 4;
  for (const std::string& l : lines) {
    r.drawText(x + pad, ty, l.c_str(), chrome::kTextSel);
    ty += lh;
  }
}

// ---- Drawing

// The short flash of a unit that changed sides: a white wash over its square, fading out. A box on
// the square works the same in every kind of view.
void BattleScene::drawStory(Renderer& r) {
  for (const Unit& u : battle_.units) {
    if ((size_t)u.id >= view_.units.size() || !u.alive) continue;
    const UnitView& v = view_.units[(size_t)u.id];
    if (v.flash <= 0 || !v.visible) continue;
    int x, y, w, h;
    cellScreenBox(u.cell, x, y, w, h);
    if (proj_.raised()) {  // the unit stands above the square's middle
      y -= proj_.unitW();
      h += proj_.unitW();
    }
    fillCellBox(r, x, y, w, h, Color(255, 255, 255, (uint8_t)(170 * std::min(1.0f, v.flash))));
  }
}

}  // namespace ote
