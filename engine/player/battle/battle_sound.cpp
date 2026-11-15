// The battle scene's part of sound (docs/SOUND.md): what the music director is told about this battle, the
// victory / defeat jingle, and the cue with a unit's type for the lookup. The sounds of the events themselves are
// in battle_view_sound.cpp; the menu cues come from the widgets (ui/) through uiCue().
#include "battle/battle_scene.h"

namespace ote {

// The music of a battle: its map (map and turn-phase music), whether the story had already asked for a track (a
// resumed battle plays it again), and whose turn it is (a resumed battle may be mid-way through the AI's turn).
SceneAudio BattleScene::audioContext() const {
  SceneAudio a;
  a.kind = SceneAudio::Battle;
  // A chapter battle is not started through session_.map: its map is the chapter's.
  const ChapterDef* chapter = chapterBattle() ? currentChapter(session_.db, session_.campaign->pt) : nullptr;
  a.map = chapter ? chapter->map : session_.db.findMap(session_.map.id);
  // Only a battle that was loaded from a save starts with what the story had asked and whose turn it is. A new
  // battle has already run its start events in the rules, but its view has not played them yet: those entries
  // (MusicChanged, TurnStarted) tell the director themselves, in order, with their fades.
  const bool resumed = session_.startEvents.empty();
  a.storyAsked = resumed && battle_.storyState.musicAsked;
  a.storyMusic = battle_.storyState.music;
  a.enemyTurn = resumed && !battle_.over && battle_.activeSide >= 0 && battle_.activeSide < (int)battle_.sides.size() &&
                battle_.sides[(size_t)battle_.activeSide].ai;
  return a;
}

void BattleScene::soundCue(Cue cue, int unit) {
  CueCtx c;
  if (unit >= 0 && unit < (int)battle_.units.size()) c.unitType = battle_.units[(size_t)unit].type;
  host_.gameAudio().cue(cue, c);
}

// A decided battle: "victory" when a human side won, "defeat" when the humans lost; nothing for a draw or a
// battle nobody human plays (autoplay).
void BattleScene::soundResult(bool anyHuman, bool humanWon) {
  if (!anyHuman || battle_.winnerTeam < 0) return;
  host_.gameAudio().music().jingle(humanWon ? MusicDirector::Jingle::Victory : MusicDirector::Jingle::Defeat);
}

}  // namespace ote
