// Factions (docs/ECONOMY.md "Factions"): what the faction a side took lets it produce, build and research, and the side-wide
// skills it gives. Everything here is a no-op for a side without a faction (a game without data/factions.json has none), so
// such a game plays, saves and simulates exactly as before. Units and structures already on the map are never touched.
#include "core/battle/battle.h"

namespace ote {

const FactionDef* Battle::factionOf(int side) const {
  if (side < 0 || side >= (int)sides.size()) return nullptr;
  const int f = sides[(size_t)side].faction;
  return f >= 0 && (size_t)f < db->factions.size() ? &db->factions[(size_t)f] : nullptr;
}

namespace {
std::string notAvailable(const FactionDef& f) { return "not available to " + f.name; }
}  // namespace

std::string Battle::factionBlocksUnit(int side, int unitType) const {
  const FactionDef* f = factionOf(side);
  return f && !FactionDef::allows(f->limitUnits, f->units, unitType) ? notAvailable(*f) : "";
}

std::string Battle::factionBlocksStructure(int side, int structureKind) const {
  const FactionDef* f = factionOf(side);
  return f && !FactionDef::allows(f->limitStructures, f->structures, structureKind) ? notAvailable(*f) : "";
}

std::string Battle::factionBlocksTech(int side, int tech) const {
  const FactionDef* f = factionOf(side);
  return f && !FactionDef::allows(f->limitTechs, f->techs, tech) ? notAvailable(*f) : "";
}

const AiWeightSet* Battle::factionWeights(int side) const {
  const FactionDef* f = factionOf(side);
  return f && f->hasWeights ? &f->weights : nullptr;
}

int Battle::defaultFactionChoice(const Database& db, uint32_t seed, int side, bool ai) {
  if (db.factions.empty()) return -1;
  if (!ai) return 0;
  // A small mixing function of the seed and the side (not the battle's own random numbers: a "choose" must not shift the stream).
  uint64_t x = (uint64_t)seed * 0x9E3779B97F4A7C15ull + (uint64_t)(side + 1) * 0xBF58476D1CE4E5B9ull;
  x ^= x >> 30;
  x *= 0xBF58476D1CE4E5B9ull;
  x ^= x >> 27;
  x *= 0x94D049BB133111EBull;
  x ^= x >> 31;
  return (int)(x % db.factions.size());
}

void factionSideModifiers(const Database& db, int faction, std::vector<SideModifier>& out) {
  out.clear();
  if (faction < 0 || (size_t)faction >= db.factions.size()) return;
  const std::vector<SkillGrant>& list = db.factions[(size_t)faction].modifiers;
  for (size_t g = 0; g < list.size(); ++g) {
    SideModifier m;
    m.skill = list[g].skill;
    m.filterType = list[g].filterType;
    m.filterTag = list[g].filterTag;
    m.faction = faction;
    m.grant = (int)g;
    out.push_back(m);
  }
}

void Battle::takeFaction(int side, int f) {
  if (side < 0 || side >= (int)sides.size() || f < 0 || (size_t)f >= db->factions.size()) return;
  SideState& s = sides[(size_t)side];
  s.faction = f;
  std::vector<SideModifier> mods;
  factionSideModifiers(*db, f, mods);
  for (const SideModifier& m : mods) addSideModifier(side, m);
}

}  // namespace ote
