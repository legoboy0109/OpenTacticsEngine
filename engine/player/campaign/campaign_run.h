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
  // The camp between chapters (campaign/camp_scene.h): the pair chosen to talk (-1 = none) and "Continue the
  // story" chosen. The flow runs doCampaignNext again after each, so these carry the choice; campDone is cleared
  // when the chapter's before scene starts.
  int campPlay = -1;
  bool campDone = false;
  // The chapter's preparation screen (docs/EVENTS.md "Preparation") was answered with Begin: the battle starts now. Cleared when
  // a chapter's before scene starts and when a lost battle is tried again (the screen comes back, with the same choice).
  bool prepDone = false;
  int battlesStarted = 0;     // every start of a chapter battle (also a retry) gets its own seed

  uint32_t nextSeed(uint32_t base) { return base + (uint32_t)battlesStarted++; }
};

// Writes the playthrough (with its battle when given; only legal in phase Battle) to `slot`
// (0 = Continue). False with the reason in `err`.
bool writeCampaignSave(const SaveStore& store, int slot, const Database& db, const std::string& gameId,
                       const Playthrough& pt, const Battle* battleOrNull, std::string& err);

}  // namespace ote
