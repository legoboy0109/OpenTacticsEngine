#include "campaign/campaign_run.h"

namespace ote {

bool writeCampaignSave(const SaveStore& store, int slot, const Database& db, const std::string& gameId,
                       const Playthrough& pt, const Battle* battleOrNull, std::string& err) {
  Json file;
  return buildCampaignSave(db, gameId, pt, battleOrNull, nowSeconds(), file, err) && store.write(slot, file, err);
}

}  // namespace ote
