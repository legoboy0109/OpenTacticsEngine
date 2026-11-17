// ote-sim --campaign: plays a game's campaign from chapter 1 with the AI on every side (the human
// side too), printing scenes and the result of each chapter.
#pragma once
#include <string>

#include "core/data/database.h"

namespace ote {
class Formulas;
}

namespace simtool {

struct CampaignSimOptions {
  unsigned seed = 1;
  int runs = 1;
  int maxRounds = 60;
  bool verbose = false;
  std::string difficulty;  // docs/DIFFICULTY.md: the id the playthrough is on (empty = the game's default)
};

// Returns the process exit code: 0 when every run finished the campaign, 2 when a run ended early
// (campaign over, a chapter stalled, or a chapter lost over and over), 1 for a game without a campaign.
int runCampaignSim(const ote::Database& db, ote::Formulas& formulas, const CampaignSimOptions& opt);

}  // namespace simtool
