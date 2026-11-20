// The commander picks of a free battle (docs/COMMANDERS.md "Player"): like the faction picks, a person's side whose map says "commander": "choose" picks on
// a screen before the battle (after its faction, whose "commanders" list limits the choice); a computer side is settled by Battle::init from the seed.
#include "core/base/log.h"
#include "game_session.h"

namespace ote {

namespace {
// The faction a side will have: the map's own, or the person's pick when the map leaves it to "choose" (-1 = none, or not picked yet).
int factionOfSide(const GameSession& s, const MapDef& m, size_t i) {
  if (m.sides[i].faction >= 0) return m.sides[i].faction;
  if (m.sides[i].factionChoose && i < s.factionPicks.size()) return s.factionPicks[i];
  return -1;
}
}  // namespace

int GameSession::nextCommanderPick(const MapDef& m) const {
  for (size_t i = 0; i < m.sides.size(); ++i)
    if (m.sides[i].commanderChoose && !m.sides[i].ai && (i >= commanderPicks.size() || commanderPicks[i] < 0)) return (int)i;
  return -1;
}

std::vector<int> GameSession::commanderChoices(const MapDef& m, int side) const {
  std::vector<int> out;
  const int faction = side >= 0 && (size_t)side < m.sides.size() ? factionOfSide(*this, m, (size_t)side) : -1;
  if (faction >= 0 && (size_t)faction < db.factions.size() && db.factions[(size_t)faction].limitCommanders)
    return db.factions[(size_t)faction].commanders;
  for (size_t i = 0; i < db.commanders.size(); ++i) out.push_back((int)i);
  return out;
}

void GameSession::applyCommanderPicks(MapDef& m) const {
  for (size_t i = 0; i < m.sides.size() && i < commanderPicks.size(); ++i) {
    if (!m.sides[i].commanderChoose || m.sides[i].ai || commanderPicks[i] < 0 || (size_t)commanderPicks[i] >= db.commanders.size()) continue;
    m.sides[i].commander = commanderPicks[i];
    m.sides[i].commanderChoose = false;
    OTE_INFO("commander: side \"%s\" plays %s", m.sides[i].id.c_str(), db.commanders[(size_t)commanderPicks[i]].id.c_str());
  }
}

}  // namespace ote
