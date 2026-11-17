// Campaign parties (docs/EVENTS.md "Parties"): several rosters / storages / funds under one story. The ACTIVE party's
// data lives in the Playthrough's own roster, sides, lastDeployed and prep; the others are parked. A switch swaps them,
// so every rule that reads or writes the roster, the carried storage or resources already works on the active party.
#include <algorithm>

#include "core/campaign/playthrough.h"

namespace ote {

namespace {

constexpr int kMaxResource = 1000000000;  // the same ceiling a save accepts (savejson::kMaxNumber)

ParkedParty* findParked(Playthrough& pt, const std::string& id) {
  for (ParkedParty& p : pt.parked)
    if (p.id == id) return &p;
  return nullptr;
}

bool isMerged(const Playthrough& pt, const std::string& id) {
  return std::find(pt.merged.begin(), pt.merged.end(), id) != pt.merged.end();
}

// The active party's data goes into a ParkedParty and the playthrough's own fields are left empty.
ParkedParty takeActive(Playthrough& pt) {
  ParkedParty p;
  p.id = pt.party;
  p.roster.swap(pt.roster);
  p.sides.swap(pt.sides);
  p.lastDeployed.swap(pt.lastDeployed);
  p.prep = pt.prep;
  pt.prep = PrepState();
  return p;
}

// Takes a parked party out of the list (nothing if it was never used).
bool takeParked(Playthrough& pt, const std::string& id, ParkedParty& out) {
  for (size_t i = 0; i < pt.parked.size(); ++i)
    if (pt.parked[i].id == id) {
      out = pt.parked[i];
      pt.parked.erase(pt.parked.begin() + (long)i);
      return true;
    }
  return false;
}

// Combines what `from` kept into the active party's data (see mergeParties).
void absorb(Playthrough& pt, ParkedParty& from, size_t resourceCount) {
  // Units. A character the active party has already stays as it is; the other copy's items go to the storage of the side it fought on.
  std::vector<RosterUnit> duplicates;
  for (RosterUnit& r : from.roster) {
    if (pt.findUnit(r.id)) duplicates.push_back(r);
    else pt.roster.push_back(r);
  }
  // Carried sides: resources add (never above the save ceiling), storage follows, finished research is united.
  for (CarriedSide& s : from.sides) {
    CarriedSide& t = pt.sideCarry(s.id, resourceCount);
    for (size_t i = 0; i < t.resources.size() && i < s.resources.size(); ++i)
      t.resources[i] = (int)std::min<long long>((long long)t.resources[i] + s.resources[i], kMaxResource);
    t.storage.insert(t.storage.end(), s.storage.begin(), s.storage.end());
    t.research.insert(t.research.end(), s.research.begin(), s.research.end());
    std::sort(t.research.begin(), t.research.end());
    t.research.erase(std::unique(t.research.begin(), t.research.end()), t.research.end());
  }
  for (const RosterUnit& d : duplicates) {
    if (d.items.empty()) continue;
    CarriedSide& t = pt.sideCarry(d.side, resourceCount);
    for (ItemGrant g : d.items) {
      g.drop = false;
      t.storage.push_back(g);
    }
  }
  // The default deployment pick: the active party's own order, then the others' ids that are not in it yet.
  for (const std::string& id : from.lastDeployed)
    if (std::find(pt.lastDeployed.begin(), pt.lastDeployed.end(), id) == pt.lastDeployed.end()) pt.lastDeployed.push_back(id);
}

}  // namespace

void beginParties(const Database& db, Playthrough& pt) {
  if (db.campaign && !db.campaign->parties.empty()) pt.party = db.campaign->parties[0].id;
}

bool switchParty(const Database& db, Playthrough& pt, const std::string& id) {
  if (!db.campaign || db.campaign->findParty(id) < 0 || isMerged(pt, id)) return false;
  if (pt.party == id) return true;
  ParkedParty next;  // a party used for the first time stays empty
  next.id = id;
  takeParked(pt, id, next);
  pt.parked.push_back(takeActive(pt));
  pt.roster = next.roster;
  pt.sides = next.sides;
  pt.lastDeployed = next.lastDeployed;
  pt.prep = next.prep;
  pt.party = id;
  return true;
}

void mergeParties(const Database& db, Playthrough& pt, const std::vector<std::string>& from, const std::string& into) {
  if (!switchParty(db, pt, into)) return;  // the target does not exist (any more): nothing to merge into
  for (const std::string& id : from) {
    if (id == into || db.campaign->findParty(id) < 0 || isMerged(pt, id)) continue;
    ParkedParty taken;  // a party that was never used has nothing to give, but it still ceases to exist
    if (takeParked(pt, id, taken)) absorb(pt, taken, db.resources.size());
    pt.merged.push_back(id);
  }
}

std::string activePartyName(const Database& db, const Playthrough& pt) {
  if (!db.campaign || pt.party.empty()) return std::string();
  const int i = db.campaign->findParty(pt.party);
  if (i < 0) return std::string();
  const PartyDef& d = db.campaign->parties[(size_t)i];
  return d.name.empty() ? d.id : d.name;
}

}  // namespace ote
