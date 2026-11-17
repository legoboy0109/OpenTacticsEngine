// Playthrough basics: a new one, lookups, and which maps may be played on their own.
#include "core/campaign/playthrough.h"

namespace ote {

bool Playthrough::begin(const Database& db) {
  if (!db.campaign) return false;
  *this = Playthrough();
  switches.assign(db.switches.size(), 0);
  variables.assign(db.variables.size(), 0);
  quotesSaid.assign((size_t)db.quotes.count, 0);  // no quote said yet (empty in a game without quotes)
  beginSupports(db, *this);  // every support pair at its "start" points (docs/SRPG.md); nothing without pairs
  difficulty = db.difficulties.defaultId;  // docs/DIFFICULTY.md: the player's choice replaces it (campaign/difficulty.h); empty without difficulties
  return true;
}

const RosterUnit* Playthrough::findUnit(const std::string& id) const {
  for (const RosterUnit& r : roster)
    if (r.id == id) return &r;
  return nullptr;
}

CarriedSide& Playthrough::sideCarry(const std::string& id, size_t resourceCount) {
  for (CarriedSide& s : sides)
    if (s.id == id) return s;
  CarriedSide s;
  s.id = id;
  s.resources.assign(resourceCount, 0);
  sides.push_back(s);
  return sides.back();
}

const ChapterDef* currentChapter(const Database& db, const Playthrough& pt) {
  if (!db.campaign || !pt.active() || pt.chapter < 0 || pt.chapter >= (int)db.campaign->chapters.size()) return nullptr;
  return &db.campaign->chapters[(size_t)pt.chapter];
}

bool freeBattleAllowed(const Database& db, const std::string& mapId) {
  if (db.findMap(mapId) < 0) return false;
  if (!db.campaign) return true;  // no campaign: every map is a single battle, as before
  switch (db.campaign->freeBattles) {
    case CampaignDef::FreeBattles::All: return true;
    case CampaignDef::FreeBattles::None: return false;
    case CampaignDef::FreeBattles::List:
      for (const std::string& m : db.campaign->freeMaps)
        if (m == mapId) return true;
      return false;
  }
  return false;
}

}  // namespace ote
