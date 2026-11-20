// The words and small numbers of the support screens, free of any window so tests/test_srpg_player_supports.cpp can
// check them: the menu entry, the banner, the Supports page rows and the camp's list rows. All wording is
// placeholder (docs/SRPG.md "U5 as built" lists it).
#pragma once
#include <string>
#include <vector>

#include "core/battle/srpg_state.h"
#include "core/data/database.h"

namespace ote {

// "Eirik and Lyra" for a pair (character names, in the pair's order).
std::string supportPairName(const Database& db, int pair);
// The action menu entry: "Support: Lyra".
std::string supportMenuLabel(const std::string& partner);
// The banner after a level was gained: "Eirik and Lyra: support level 1" (the level's own name).
std::string supportBannerText(const Database& db, int pair, int level);
// The small label over both units when a conversation became ready.
const char* supportReadyLabel();

// One row of the Supports page: one partner of the unit.
struct SupportRow {
  std::string partner;     // the other character's name
  std::string levelName;   // "" = no level yet
  int level = 0, levels = 0;
  int points = 0;          // closeness now
  int from = 0, to = 0;    // the bar runs from `from` (this level's points) to `to` (the next level's); to == from at the top
  bool ready = false;      // "Conversation ready"
  bool top = false;        // all levels gained
};
// The rows of a character (index into Database::characters) over every pair it is in, in pair order. Empty for
// a character without pairs or a state that does not fit the game.
std::vector<SupportRow> supportRows(const Database& db, const SupportState& state, int character);
// 0..1 of the bar (1 at the top level).
float supportBarFraction(const SupportRow& row);
// "Level B" / "No support yet": the level column of a row.
std::string supportLevelText(const SupportRow& row);

// One row of the camp's list: the pair and the level its conversation unlocks.
struct CampRow {
  int pair = -1;
  std::string label;  // "Eirik and Lyra"
  std::string extra;  // "Level 2"
  bool jobs = false;  // the camp's "Jobs" row (docs/JOBS.md): no pair, `label` is "Jobs"
};
std::vector<CampRow> campRows(const Database& db, const SupportState& state, const std::vector<int>& pending);

}  // namespace ote
