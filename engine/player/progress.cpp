#include "progress.h"

#include <ctime>

#include "core/data/campaign.h"
#include "platform/platform.h"

namespace ote {

bool storyCompleteRecorded(const SaveStore& store) {
  if (!store.enabled()) return false;
  std::string text;
  if (!readTextFile(store.progressPath(), text)) return false;
  Json root;
  std::string perr;
  if (!Json::parse(text, root, perr) || !root.isObject()) return false;
  return root["storyComplete"].isBool() && root["storyComplete"].asBool();
}

bool recordStoryComplete(const SaveStore& store, long long at, std::string& err) {
  if (!store.enabled()) { err = "saving is switched off"; return false; }
  if (storyCompleteRecorded(store)) return true;
  char when[32] = "";
  std::time_t t = (std::time_t)at;
  std::tm utc;
#if defined(OTE_NXDK)  // nxdk: no gmtime_s / gmtime_r; one thread, so the plain call is fine
  if (const std::tm* g = std::gmtime(&t)) { utc = *g; std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%SZ", &utc); }
#elif defined(_WIN32)
  if (gmtime_s(&utc, &t) == 0) std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%SZ", &utc);
#else
  if (gmtime_r(&t, &utc)) std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%SZ", &utc);
#endif
  Json j = Json::makeObject();
  j.set("storyComplete", Json::makeBool(true));
  j.set("at", Json::makeString(when));
  return store.writeAt(store.progressPath(), j, err);
}

bool storyUnlocked(const SaveStore& store, const std::string& gameId) {
  if (!store.enabled()) return false;
  if (storyCompleteRecorded(store)) return true;
  for (int slot = 0; slot <= SaveStore::kSlots; ++slot) {
    const SaveInfo info = store.inspect(slot, nullptr, gameId);  // the file alone: a game edited since still counts
    if (info.state == SaveInfo::Ok && info.campaign && info.phase == "finished") return true;
  }
  return false;
}

bool freeBattlesListed(const Database& db, const SaveStore& store, const std::string& gameId) {
  if (!db.campaign || !db.campaign->freeAfterStory) return true;
  return storyUnlocked(store, gameId);
}

}  // namespace ote
