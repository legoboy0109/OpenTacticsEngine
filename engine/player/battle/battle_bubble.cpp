// How a battle line is shown (docs/EVENTS.md "Speech bubbles", "Notices"): the classic box, a speech bubble pointing at the
// speaking unit, or the notice window. Bubbles are the game's choice (game.json "dialogue": {"bubbles": true}); a line still
// takes the box whenever a bubble would point at nothing: a free name or a narrator, a speaker standing on the scene stage, a
// story background or the side-view fight covering the map, another map on screen, or a speaker that is hidden by fog,
// off the view or gone. Games without "dialogue" never reach the bubble.
#include "battle/battle_scene.h"

namespace ote {

bool BattleScene::speakerAnchor(int unit, BubbleAnchor& a) const {
  if (unit < 0 || (size_t)unit >= battle_.units.size() || (size_t)unit >= view_.units.size()) return false;
  const Unit& u = battle_.units[(size_t)unit];
  if (!u.alive || !view_.units[(size_t)unit].visible || !battle_.grid.valid(u.cell) || view_.dimmed(u.cell)) return false;
  int x, y, w, h;
  cellScreenBox(u.cell, x, y, w, h);
  if (proj_.raised()) {  // the unit stands above the square's middle (as drawStory's flash)
    y -= proj_.unitW();
    h += proj_.unitW();
  }
  // Its middle must be on the map's part of the screen (left of the panel, above the hint line).
  const int cx = x + w / 2, cy = y + h / 2;
  if (cx < 0 || cx >= layout_.vw || cy < 0 || cy >= layout_.vh) return false;
  a.x = x;
  a.y = y;
  a.w = w;
  a.h = h;
  return true;
}

DialogueFrame BattleScene::dialogueFrame(const DialogueState& d) const {
  DialogueFrame f;
  const Database::DialogueLook& look = session_.db.dialogue;
  f.portrait = look.portrait;
  f.names = look.names;
  const bool mapCovered = view_.stage.backgroundShown() || view_.fight.talking || onOtherMap();
  if (d.line.notice) {  // the middle of the map's part of the screen, or of the whole screen when something covers the map
    f.shape = DialogueFrame::Shape::Notice;
    if (!mapCovered) {
      f.areaW = layout_.vw;
      f.areaH = layout_.vh;
    }
    return f;
  }
  if (!look.bubbles || d.line.narrate || d.staged || mapCovered) return f;
  if (!speakerAnchor(d.line.unit, f.anchor)) return f;
  f.shape = DialogueFrame::Shape::Bubble;
  f.areaW = layout_.vw;
  f.areaH = layout_.vh;
  return f;
}

}  // namespace ote
