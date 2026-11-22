// Learning from equipment in a battle (docs/JOBS.md "Learning from equipment (ability points)"): which taught skills and abilities a unit may use now,
// and the points it earns. The rules over plain data are core/jobs/mastery.cpp; everything here is a no-op in a game where no item teaches.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/jobs/mastery.h"

namespace ote {

namespace {

void addOnce(std::vector<int>& out, int v) {
  if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
}

}  // namespace

void Battle::teachFromItems(const Unit& u, std::vector<int>& out, int weapon) const {
  out.clear();
  if (!db->teachingOn()) return;
  auto consider = [&](const ItemDef& d) {
    for (int t : d.teaches) {
      const TeachDef& def = db->teachEntries[(size_t)t];
      // An entry a job teaches too belongs to that job: only usable (and filled) while the unit is in it (docs/JOBS.md "With jobs").
      if (def.jobEntry >= 0 && (!u.jobs.on() || db->jobEntries[(size_t)def.jobEntry].job != u.jobs.job)) continue;
      if (teachMastered(*db, u.jobs, u.mastery, t)) continue;  // mastered: it is the unit's own now, not the item's
      addOnce(out, t);
    }
  };
  int slot = -1;  // the weapon item asked about
  if (weapon == -1) {
    slot = equippedSlot(u);  // the weapon in hand
  } else if (weapon >= 0) {
    for (size_t i = 0; i < u.items.size() && slot < 0; ++i)
      if (itemOf(u.items[i]).kind == ItemKind::Weapon && itemOf(u.items[i]).weapon == weapon) slot = (int)i;
  }
  if (slot >= 0) consider(itemOf(u.items[(size_t)slot]));
  for (const ItemStack& s : u.items)  // accessories work while carried
    if (itemOf(s).kind == ItemKind::Accessory) consider(itemOf(s));
}

void Battle::teachMasteredOf(const Unit& u, std::vector<int>& out) const {
  out.clear();
  for (const MasteryPoints& p : u.mastery) {
    if (p.entry < 0 || (size_t)p.entry >= db->teachEntries.size()) continue;
    const TeachDef& t = db->teachEntries[(size_t)p.entry];
    if (t.jobEntry < 0 && p.points >= t.points) out.push_back(p.entry);
  }
}

int Battle::teachEntryFromItem(const Unit& u, bool ability, int ref) const {
  std::vector<int> list;
  teachFromItems(u, list);
  for (int t : list)
    if (db->teachEntries[(size_t)t].ability == ability && db->teachEntries[(size_t)t].ref == ref) return t;
  return -1;
}

void Battle::awardPoints(int unit, int amount, std::vector<Event>& events) {
  if (!db->teachingOn() || amount <= 0 || unit < 0 || unit >= (int)units.size()) return;
  if (sides[(size_t)units[(size_t)unit].side].ai) return;  // only units of human sides learn (as with JP)
  std::vector<int> entries;
  teachFromItems(units[(size_t)unit], entries);
  for (int t : entries) {
    Unit& u = units[(size_t)unit];
    if (!u.alive) return;
    bool mastered = false;
    const int now = addTeachPoints(*db, u.jobs, u.mastery, t, amount, mastered);
    Event e;
    e.type = EventType::PointsGained;
    e.a = unit;
    e.b = amount;
    e.c = t;
    e.d = now;
    if (mastered) e.flags |= kMastered;
    events.push_back(e);
  }
}

void Battle::awardBattlePoints(int team, std::vector<Event>& events) {
  if (!db->teachingOn() || team < 0 || db->rules.abilityPoints.perBattle <= 0) return;
  for (size_t i = 0; i < units.size(); ++i) {
    const Unit& u = units[i];
    if (!u.alive || u.cell == kNoCell || teamOf(u) != team) continue;
    awardPoints((int)i, db->rules.abilityPoints.perBattle, events);
  }
}

}  // namespace ote
