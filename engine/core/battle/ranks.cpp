// Weapon ranks (docs/SRPG.md section 7): the equip test every weapon question goes through, experience from use, and
// the rank a class or promotion gives. Nothing here runs in a game without data/ranks.json.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {
Event rankUpEvent(int unit, int group, int to, int from) {
  Event e;
  e.type = EventType::RankUp;
  e.a = unit;
  e.b = group;
  e.c = to;
  e.d = from;
  return e;
}
}  // namespace

int Battle::weaponRank(const Unit& u, int group) const {
  if (group < 0 || (size_t)group >= u.weaponXp.size()) return 0;
  return db->ranks.rankAt(u.weaponXp[(size_t)group]);
}

bool Battle::meetsRank(const Unit& u, int weapon, std::string* why) const {
  if (!db->ranks.on() || weapon < 0 || (size_t)weapon >= db->weapons.size()) return true;
  const WeaponDef& w = db->weapons[(size_t)weapon];
  if (w.rankNeed < 0 || weaponRank(u, w.rankGroup) >= w.rankNeed) return true;
  if (why) *why = "Needs rank " + db->ranks.ladder[(size_t)w.rankNeed].name;
  return false;
}

bool Battle::canWieldNow(const Unit& u, int weapon, std::string* why) const {
  if (!db->canWield(u.type, weapon)) {
    if (why) *why = "the unit cannot wield that";
    return false;
  }
  return meetsRank(u, weapon, why);
}

// A class's and a character's value per group: the higher wins; a unit is raised to it, never lowered (so this is safe
// to call twice, and a roster unit that earned more keeps it). `events` (optional) receives a RankUp per group whose
// rank rose (a promotion; a new unit is dressed silently).
void Battle::rankRaiseToClass(Unit& u, int unit, std::vector<Event>* events) const {
  if (!db->ranks.on()) return;
  const size_t n = db->ranks.groups.size();
  u.weaponXp.resize(n, 0);
  const std::vector<int>& cls = typeOf(u).rankStart;
  const std::vector<int>* own = u.character >= 0 ? &db->characters[(size_t)u.character].rankStart : nullptr;
  for (size_t g = 0; g < n; ++g) {
    int want = g < cls.size() ? cls[g] : 0;
    if (own && g < own->size()) want = std::max(want, (*own)[g]);
    if (want <= u.weaponXp[g]) continue;
    const int before = db->ranks.rankAt(u.weaponXp[g]);
    u.weaponXp[g] = want;
    const int after = db->ranks.rankAt(want);
    if (events && after > before) events->push_back(rankUpEvent(unit, (int)g, after, before));
  }
}

// A map unit's own "ranks" replace the groups they name (makeUnit, after the class's values).
void Battle::rankOverride(Unit& u, const std::vector<int>& values) const {
  if (!db->ranks.on()) return;
  u.weaponXp.resize(db->ranks.groups.size(), 0);
  for (size_t g = 0; g < values.size() && g < u.weaponXp.size(); ++g)
    if (values[g] >= 0) u.weaponXp[g] = values[g];
}

// After a combat (growth "use"): each fighter that struck at least once with a weapon of a group gains the weapon's
// experience. The strikes are read off the events of this combat, so a weapon that broke or a strike that missed is
// counted exactly as it happened. A fighter that did not survive gains nothing.
void Battle::ranksAfterCombat(std::vector<Event>& events, size_t from, int attacker, int defender) {
  if (!db->ranks.on() || !db->ranks.growByUse) return;
  std::vector<Event> ups;
  for (int unit : {attacker, defender}) {
    int weapon = -1;
    for (size_t i = from; i < events.size() && weapon < 0; ++i)
      if (events[i].type == EventType::Attack && events[i].a == unit) weapon = events[i].c;
    if (weapon < 0 || (size_t)unit >= units.size() || !units[(size_t)unit].alive) continue;
    const WeaponDef& w = db->weapons[(size_t)weapon];
    if (w.rankGroup < 0) continue;
    Unit& u = units[(size_t)unit];
    u.weaponXp.resize(db->ranks.groups.size(), 0);
    int& xp = u.weaponXp[(size_t)w.rankGroup];
    const int before = db->ranks.rankAt(xp);
    xp = std::min(xp + w.rankXp, std::max(xp, db->ranks.topXp()));
    const int after = db->ranks.rankAt(xp);
    if (after > before) ups.push_back(rankUpEvent(unit, w.rankGroup, after, before));
  }
  events.insert(events.end(), ups.begin(), ups.end());
}

}  // namespace ote
