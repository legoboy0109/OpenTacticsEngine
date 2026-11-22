// Dressing named units from a campaign's roster (see carried.h).
#include "core/battle/battle.h"

namespace ote {

int Battle::applyRoster(Unit& u) const {
  if (!roster || u.storyId.empty() || u.side < 0 || u.side >= (int)sides.size() || sides[(size_t)u.side].ai) return -1;
  for (const RosterUnit& r : *roster) {
    if (r.id != u.storyId) continue;
    if (r.gone) return 0;
    u.type = r.type;
    u.name = r.name;
    u.level = r.level;
    u.xp = r.xp;
    u.merit = r.merit;
    u.stats = r.stats;
    u.stats.resize(db->stats.size(), 0);
    u.items.clear();
    for (const ItemGrant& g : r.items) {
      ItemStack s;
      s.item = g.item;
      s.uses = g.uses;
      s.drop = g.drop;
      u.items.push_back(s);
    }
    // The skills it learned in earlier chapters, then what its (possibly new) type and level add (docs/SRPG.md).
    u.learned = r.skills;
    u.jobs = r.jobs;  // jobs: what it uses, learned and equipped in earlier chapters (docs/JOBS.md); a roster entry of a game without jobs has none
    u.mastery = r.mastery;  // points in what items teach (docs/JOBS.md "Learning from equipment")
    u.weaponXp = r.weaponXp;  // dressUnit raises it to what the (possibly new) class gives, never lowers it
    dressUnit(u);
    u.hp = maxHp(u);  // a chapter always begins at full strength
    stockUnit(u);     // ... and with full fuel and ammunition (the roster does not carry them)
    return 1;
  }
  return -1;
}

void Battle::setStartingRoster(std::shared_ptr<const Roster> carried) {
  roster = carried;
  if (!roster) return;
  // Units the roster calls gone are removed outright (not "defeated": no event may see them),
  // so the list is rebuilt and the ids renumbered. Nothing has happened yet, so nothing refers to
  // the old numbers.
  std::vector<Unit> kept;
  for (Unit& u : units) {
    Cell cell = u.cell;
    int r = applyRoster(u);
    if (r == 0) {
      occupant[(size_t)cell] = -1;
      continue;
    }
    kept.push_back(u);
  }
  units = kept;
  for (size_t i = 0; i < units.size(); ++i) {
    units[i].id = (int)i;
    occupant[(size_t)units[i].cell] = (int)i;
  }
  applyTestLevel();  // the roster's version of a unit may be below the test level (docs/DIFFICULTY.md "Test level")
}

}  // namespace ote
