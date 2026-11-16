// The rules of supports that need no battle (docs/SRPG.md "Supports"): points, which level is ready, which gets
// gained at once. A Battle and a campaign's camp both keep a SupportState and use these, so the two cannot drift.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "core/battle/srpg_state.h"
#include "core/data/database.h"

namespace ote {

// Every pair at its "start" points. Levels without a conversation that the start points already reach are gained
// (without an event); levels with one wait to be played. Empty in a game without pairs.
SupportState startingSupports(const Database& db);

// Does the state fit the game: one entry per pair, points 0..kMaxSupportPoints, each level within the pair's levels
// and not above what its points allow. `why` (optional) says which entry is wrong.
bool supportStateFits(const Database& db, const SupportState& state, std::string* why = nullptr);

// Sum of the levels a character holds over all its pairs (for rules.maxLevels).
int supportLevelsHeld(const Database& db, const SupportState& state, int character);

// The next level of the pair can be gained now: it exists, its points are reached and neither character would go
// past rules.maxLevels.
bool supportReady(const Database& db, const SupportState& state, int pair);
// Ready, and the next level has a conversation (so it is gained when that is played, not at once).
bool supportTalkPending(const Database& db, const SupportState& state, int pair);

// Adds `by` (may be negative) to the pair's points, kept within 0..kMaxSupportPoints. A level whose points are no
// longer reached is lost (the level never stands above what the points allow). Returns false when nothing changed.
bool supportAddPoints(const Database& db, SupportState& state, int pair, int by);

// Gains every ready level that has no conversation, in pair order (a pair may gain several in a row). The
// (pair, new level) of each is appended to `gained`.
void supportSettle(const Database& db, SupportState& state, std::vector<std::pair<int, int>>* gained);

// What the level is called ("1", "2", ... unless rules.levelNames says otherwise); level 0 = "".
std::string supportLevelName(const SupportRules& rules, int level);

}  // namespace ote
