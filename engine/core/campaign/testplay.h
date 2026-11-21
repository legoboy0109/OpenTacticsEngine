// Test playthroughs (docs/SAVES.md "Test playthroughs", docs/DIFFICULTY.md "Test level"): a maker's way into a late chapter. A new
// playthrough is started AT a chapter with an approximated roster, and the roster can be lifted to a test level. Plain functions on
// a Playthrough, deterministic and free of input, like the rest of the campaign module.
#pragma once
#include <string>

#include "core/campaign/playthrough.h"

namespace ote {

class Formulas;

// A new playthrough that starts at chapter `chapterId` (phase Before: its scene, its preparation, its battle, then the story goes on as
// normal). The roster is APPROXIMATED by walking the campaign's main path from chapter 1 up to that chapter without playing it: each
// chapter's "before" scene runs (the first offered answer of every question, so parties switch and merge as the story says, switches
// and variables start unset and only scenes change them), every named unit that the chapter's map places on a human side (or that a
// map event adds with addUnit there) joins the active party's roster with the level, stats and items written on its first
// placement, and the "after" scene runs and gives the next chapter (its "next", a "goto", or the following one). Funds and the
// storage start empty; nothing a battle would have changed (experience, items used, units that fell, map-event switches) is known.
// `note` gets one line about an approximation worth telling (a chapter that is not on the main path: the chapters in the list
// before it are walked instead). False with `err` for an unknown chapter or a game without a campaign.
// `difficulty` (an id, may be empty) is set before the walk, so scene conditions on the difficulty read it.
bool beginPlaythroughAt(const Database& db, Playthrough& pt, const std::string& chapterId, const std::string& difficulty, std::string& note,
                        std::string& err);

// Raises every roster unit of every party (not the gone ones) that is below `level` to it, by level-ups at the average growth
// (Battle::levelUnitTo: caps and maxLevel respected, no promotion, no dice); each unit's weapon ranks gain the experience of the levels it
// gained (Battle::raiseTestRanks), so a unit already at the level gets nothing more. Returns how many units were raised.
int levelRoster(const Database& db, Playthrough& pt, int level);

// The test level for the chapter battle about to be built (phase Battle): `option` 0 = off, N > 0 = N, -1 = automatic, the rounded
// average level of the hostile computer units of this chapter's map at its start. The roster is raised to it (levelRoster) and
// the number is returned for Battle::setTestLevel (0 = nothing: off, or automatic with no hostile units). Upward only, so
// calling it again for the next chapter with `automatic` never lowers anything.
int applyChapterTestLevel(const Database& db, Playthrough& pt, Formulas& formulas, uint32_t seed, int option, std::string& err);

}  // namespace ote
