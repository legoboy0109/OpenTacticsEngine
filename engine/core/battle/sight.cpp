// Fog of war: what each team sees (docs/ECONOMY.md "Fog of war").
//
// The battle always holds the truth; sight is derived data. For every team there is a grid of "the square is in
// sight" and a second one of "a unit of another team standing here would be seen" (they differ on squares that
// hide). Both are recomputed from scratch when something that matters changed (markSight() drops them; the next
// query rebuilds them): about (units x area) work, deterministic, no randomness, no iteration over hash maps.
//
// The ONLY memory fog adds is `seenOwners_` and `seenKinds_` (the last owner and the last kind of each structure
// that each team saw); both are refreshed by the same pass and are what saves keep.
#include "core/battle/battle.h"

namespace ote {

int Battle::visionOf(const Unit& u) const {
  // Vision is a property of the unit's type plus the square it stands on. It is deliberately not a stat, so
  // research and accessories do not change it.
  int bonus = terrainAt(u.cell).visionBonus;
  if (const StructureDef* s = structureDefAt(u.cell)) bonus += s->visionBonus;
  return typeOf(u).vision + bonus;
}

void Battle::computeTeamSight(int team) const {
  const size_t n = (size_t)grid.cellCount();
  if ((int)sight_.seen.size() != teamCount) {
    sight_.seen.assign((size_t)teamCount, std::vector<uint8_t>());
    sight_.unitSeen.assign((size_t)teamCount, std::vector<uint8_t>());
  }
  std::vector<uint8_t>& seen = sight_.seen[(size_t)team];
  std::vector<uint8_t>& unitSeen = sight_.unitSeen[(size_t)team];
  seen.assign(n, 0);

  std::vector<Cell> area, mine;
  for (const Unit& u : units) {
    if (!u.alive || teamOf(u) != team) continue;
    grid.within(u.cell, visionOf(u), area);
    for (Cell c : area) seen[(size_t)c] = 1;
    mine.push_back(u.cell);
  }
  // A structure of the team sees one square around every square of its footprint (the footprint included).
  std::vector<Cell> footprint;
  for (size_t s = 0; s < structures.size(); ++s) {
    int owner = structures[s].owner;
    if (owner < 0 || sides[(size_t)owner].team != team) continue;
    structureCells((int)s, footprint);
    for (Cell f : footprint) {
      grid.within(f, 1, area);
      for (Cell c : area) seen[(size_t)c] = 1;
    }
  }

  // A unit standing on a square that hides is seen only by units right next to it.
  auto hides = [&](Cell c) {
    if (terrainAt(c).hides) return true;
    const StructureDef* sd = structureDefAt(c);
    return sd && sd->hides;
  };
  unitSeen = seen;
  for (size_t c = 0; c < n; ++c)
    if (seen[c] && hides((Cell)c)) unitSeen[c] = 0;
  for (Cell m : mine) {
    Cell nb[Grid::kMaxNeighbors];
    int count = grid.neighbors(m, nb);
    for (int i = 0; i < count; ++i)
      if (hides(nb[i])) seen[(size_t)nb[i]] = unitSeen[(size_t)nb[i]] = 1;  // even a unit with vision 0 sees its neighbours' hideouts
  }

  // Remember the owner and the kind of every structure the team can see now.
  if (seenOwners_.size() != (size_t)teamCount) seenOwners_.resize((size_t)teamCount);
  if (seenKinds_.size() != (size_t)teamCount) seenKinds_.resize((size_t)teamCount);
  std::vector<int>& known = seenOwners_[(size_t)team];
  std::vector<int>& knownKind = seenKinds_[(size_t)team];
  if (known.size() != structures.size()) known.assign(structures.size(), -1);
  if (knownKind.size() != structures.size()) {
    knownKind.resize(structures.size());
    for (size_t s = 0; s < structures.size(); ++s) knownKind[s] = structures[s].mapType;
  }
  for (size_t s = 0; s < structures.size(); ++s) {
    structureCells((int)s, footprint);
    for (Cell f : footprint)
      if (seen[(size_t)f]) {
        known[s] = structures[s].owner;
        knownKind[s] = structures[s].type;
        break;
      }
  }
}

void Battle::ensureSight() const {
  if (!fog || sight_.valid) return;
  if ((int)sight_.seen.size() != teamCount) {
    sight_.seen.assign((size_t)teamCount, std::vector<uint8_t>());
    sight_.unitSeen.assign((size_t)teamCount, std::vector<uint8_t>());
  }
  for (int t = 0; t < teamCount; ++t) computeTeamSight(t);
  sight_.valid = true;
}

void Battle::markSight() {
  if (!fog) return;
  stampEvents();  // what was pushed so far describes the world as it still is
  sight_.valid = false;
}

bool Battle::visibleTo(int team, Cell cell) const {
  if (!fog) return true;
  if (team < 0 || team >= teamCount || !grid.valid(cell)) return false;
  ensureSight();
  return sight_.seen[(size_t)team][(size_t)cell] != 0;
}

bool Battle::unitVisibleTo(int team, const Unit& u) const {
  if (!u.alive || u.cell == kNoCell) return false;
  if (!fog) return true;
  if (team < 0 || team >= teamCount) return false;
  if (teamOf(u) == team) return true;
  ensureSight();
  return sight_.unitSeen[(size_t)team][(size_t)u.cell] != 0;
}

bool Battle::unitVisibleTo(int team, int unit) const {
  if (unit < 0 || unit >= (int)units.size()) return false;
  return unitVisibleTo(team, units[(size_t)unit]);
}

bool Battle::structureVisibleTo(int team, int structure) const {
  if (structure < 0 || structure >= (int)structures.size()) return false;
  if (!fog) return true;
  std::vector<Cell> footprint;
  structureCells(structure, footprint);
  for (Cell c : footprint)
    if (visibleTo(team, c)) return true;
  return false;
}

int Battle::seenOwner(int team, int structure) const {
  if (structure < 0 || structure >= (int)structures.size()) return -1;
  if (!fog) return structures[(size_t)structure].owner;
  if (team < 0 || team >= teamCount) return -1;
  ensureSight();  // also brings the memory up to date
  return seenOwners_[(size_t)team][(size_t)structure];
}

int Battle::seenKind(int team, int structure) const {
  if (structure < 0 || structure >= (int)structures.size()) return -1;
  if (!fog) return structures[(size_t)structure].type;
  if (team < 0 || team >= teamCount) return structures[(size_t)structure].mapType;
  ensureSight();  // also brings the memory up to date
  return seenKinds_[(size_t)team][(size_t)structure];
}

void Battle::learnOwner(int structure, uint32_t teams) {
  if (!fog) return;
  for (int t = 0; t < teamCount && t < kMaxFogTeams; ++t)
    if ((teams >> t) & 1u) {
      if (seenOwners_.size() != (size_t)teamCount) seenOwners_.resize((size_t)teamCount);
      if (seenOwners_[(size_t)t].size() != structures.size()) seenOwners_[(size_t)t].assign(structures.size(), -1);
      seenOwners_[(size_t)t][(size_t)structure] = structures[(size_t)structure].owner;
      // The kind is remembered together with the owner (a capture shows both).
      if (seenKinds_.size() != (size_t)teamCount) seenKinds_.resize((size_t)teamCount);
      if (seenKinds_[(size_t)t].size() != structures.size()) {
        seenKinds_[(size_t)t].resize(structures.size());
        for (size_t s = 0; s < structures.size(); ++s) seenKinds_[(size_t)t][s] = structures[s].mapType;
      }
      seenKinds_[(size_t)t][(size_t)structure] = structures[(size_t)structure].type;
    }
}

}  // namespace ote
