// The small decisions of the campaign flow and the words of the save lists, kept free of windows and
// scenes so they can be tested (tests/test_campaign_player.cpp). All wording here is placeholder.
#pragma once
#include <string>

#include "core/campaign/playthrough.h"
#include "saves.h"

namespace ote {

// What the player does next in a playthrough.
enum class Next {
  None,          // nothing to do (the battle is not decided)
  BeforeScene,   // the chapter's title card + before scene
  StartBattle,   // a new battle of the chapter (no before scene)
  ResumeBattle,  // a loaded, half-played battle
  AfterScene,    // the chapter's after scene
  RetryChoice,   // a lost battle: "Try again" / "Back to title"
  EndFinished,   // "The End"
  EndOver,       // "The story ends here."
};

// Where a playthrough stands (after a scene, after loading a save). `battleInHand` = a loaded save
// brought a battle that was under way.
Next nextForPhase(Phase phase, bool battleInHand);
// What follows a decided chapter battle.
Next nextAfterBattle(ChapterResult result);

// Is the Continue file written for this battle? Plain games: whenever saving is on. A campaign game:
// only for the playthrough's own battles; a free battle never touches it, so it cannot destroy a
// playthrough (docs/SAVES.md "Campaign saves in the player").
bool continueIsWritten(bool savingOn, bool campaignGame, bool campaignBattle);

// "Chapter 2" for the chapter at `index` (position in the list).
std::string chapterHeading(int index);

// Rows of the Continue line and the slot lists. Title = the map's name, or a campaign save's chapter
// name; detail = "Turn N" mid-battle, else a word for the phase.
std::string saveTitle(const SaveInfo& info);
std::string saveDetail(const SaveInfo& info);
// A campaign save's phase in words ("Ready to fight", "Battle won", ...).
std::string phaseWords(const std::string& phase);

}  // namespace ote
