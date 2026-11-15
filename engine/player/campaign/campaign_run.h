// A campaign being played in the player: the playthrough plus the few things the flow needs that are
// not part of it, and the save writes (docs/SAVES.md "Campaign saves"). Lives in the GameSession
// while a story is on; null there for a free battle or a game without a campaign.
#pragma once
#include <string>

#include "core/campaign/campaign_save.h"
#include "core/campaign/playthrough.h"
#include "saves.h"

namespace ote {

struct CampaignRun {
  Playthrough pt;
  bool battleInHand = false;  // a loaded save brought a battle under way (consumed by the next step)
  int battlesStarted = 0;     // every start of a chapter battle (also a retry) gets its own seed

  uint32_t nextSeed(uint32_t base) { return base + (uint32_t)battlesStarted++; }
};

// Writes the playthrough (with its battle when given; only legal in phase Battle) to `slot`
// (0 = Continue). False with the reason in `err`.
bool writeCampaignSave(const SaveStore& store, int slot, const Database& db, const std::string& gameId,
                       const Playthrough& pt, const Battle* battleOrNull, std::string& err);

}  // namespace ote
