// A game's progress record and the "story complete" unlock rule (docs/SAVES.md "Progress record").
// The record is progress.json next to the game's saves: written once when a playthrough reaches the story's end
// (Phase::Finished, never "Story ended" by defeat), and independent of the Continue save and the slots.
#pragma once
#include <string>

#include "core/data/database.h"
#include "saves.h"

namespace ote {

// True when progress.json exists in the game's saves folder and says {"storyComplete": true}.
bool storyCompleteRecorded(const SaveStore& store);

// Writes progress.json ({"storyComplete": true, "at": "<UTC time>"}) unless it is already recorded (the first date stays).
// False with `err` when the file cannot be written or the store is disabled (nothing written).
bool recordStoryComplete(const SaveStore& store, long long at, std::string& err);

// Has this player completed the story? The record, or (for players who finished before the record existed) any save in
// the Continue file or a slot that holds a finished story (label "Story complete"). A save of an ended story
// ("Story ended", lost with onDefeat "end") does not count. Saves are judged by their own file only, so a game
// updated since still counts. A disabled store (no saves folder) is never complete.
bool storyUnlocked(const SaveStore& store, const std::string& gameId);

// Does the game page list this campaign's free battles now? Always, unless the campaign says freeBattles.when is
// "storyComplete" and the story is not complete yet.
bool freeBattlesListed(const Database& db, const SaveStore& store, const std::string& gameId);

}  // namespace ote
