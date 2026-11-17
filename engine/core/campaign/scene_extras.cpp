// A campaign scene's storeItems (docs/EVENTS.md "Sending a unit's items to storage"): the roster character's items go to the
// campaign's shared storage, the one the preparation screen shows.
#include <algorithm>

#include "core/campaign/playthrough.h"
#include "core/campaign/prep.h"
#include "core/data/loader.h"

namespace ote {

// Which side's carried storage: the current chapter's map names it, as the preparation screen does (the prep block's side), else the
// map's first human side. A chapter whose map cannot be read, or has no human side, stores nothing (logged by the loader long before).
void sceneStoreItems(const Database& db, Playthrough& pt, const SceneCommand& c) {
  if (!db.campaign || db.rules.items.storage == ItemStorage::Off) return;
  if (pt.chapter < 0 || pt.chapter >= (int)db.campaign->chapters.size()) return;
  const ChapterDef& ch = db.campaign->chapters[(size_t)pt.chapter];
  MapDef map;
  std::string err;
  if (ch.map < 0 || ch.map >= (int)db.maps.size() || !loadMap(db, db.maps[(size_t)ch.map].id, map, err)) return;
  int side = map.prep ? map.prep->side : -1;
  for (size_t i = 0; side < 0 && i < map.sides.size(); ++i)
    if (!map.sides[i].ai) side = (int)i;
  if (side < 0) return;
  for (RosterUnit& r : pt.roster) {
    if (r.id != c.unit || r.gone) continue;
    CarriedSide& carried = pt.sideCarry(map.sides[(size_t)side].id, db.resources.size());
    for (size_t s = 0; s < r.items.size();) {
      if (std::find(c.keep.begin(), c.keep.end(), r.items[s].item) != c.keep.end()) { ++s; continue; }
      ItemGrant g = r.items[s];
      g.drop = false;
      r.items.erase(r.items.begin() + (long)s);
      carried.storage.push_back(g);
    }
  }
}

}  // namespace ote
