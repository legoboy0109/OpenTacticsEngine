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
  return meetsRankFor(u, db->weapons[(size_t)weapon], why);
}

// The test itself on a weapon-like record: a weapon's data, or a staff's `gear` (docs/SRPG.md section 8).
bool Battle::meetsRankFor(const Unit& u, const WeaponDef& w, std::string* why) const {
  if (!db->ranks.on()) return true;
  if (w.rankNeed < 0 || weaponRank(u, w.rankGroup) >= w.rankNeed) return true;
  if (why) *why = "Needs rank " + db->ranks.ladder[(size_t)w.rankNeed].name;
  return false;
}

// A condition that forbids this weapon's group (or weapons at all): "silence" (docs/SRPG.md section 9).
bool Battle::weaponForbidden(const Unit& u, int weapon, std::string* why) const {
  if (u.conditions.empty() || weapon < 0 || (size_t)weapon >= db->weapons.size()) return false;
  int c = conditionForbidsGroup(u, db->weapons[(size_t)weapon].group);
  if (c < 0) c = conditionForbidsKind(u, ItemKind::Weapon);
  if (c < 0) return false;
  if (why) *why = db->conditions[(size_t)c].name + " stops that";
  return true;
}

bool Battle::canWieldNow(const Unit& u, int weapon, std::string* why) const {
  if (!db->canWield(u.type, weapon, u.character)) {
    if (why) *why = "the unit cannot wield that";
    return false;
  }
  if (weaponForbidden(u, weapon, why)) return false;
  if (!meetsCharacterLock(u, weapon, why)) return false;
  return meetsRank(u, weapon, why);
}

bool Battle::meetsCharacterLock(const Unit& u, int weapon, std::string* why) const {
  if (weapon < 0 || (size_t)weapon >= db->weapons.size()) return true;
  return meetsLockFor(u, db->weapons[(size_t)weapon], why);
}

// The weapon's lock (docs/SRPG.md "Character-locked weapons"): "wieldedBy" characters and "wieldedByTypes" unit types. No
// list at all = open to everyone; otherwise the unit's character OR its unit type must be on a list. A generic unit
// (character -1) is never on the character list. The words are a placeholder: one or two names (characters first, then
// types), then "and others".
bool Battle::meetsLockFor(const Unit& u, const WeaponDef& w, std::string* why) const {
  if (w.wielders.empty() && w.wielderTypes.empty()) return true;
  for (int ch : w.wielders)
    if (ch == u.character) return true;
  for (int t : w.wielderTypes)
    if (t == u.type) return true;
  if (why) {
    std::vector<std::string> names;
    for (int ch : w.wielders) names.push_back(db->characters[(size_t)ch].name);
    for (int t : w.wielderTypes) names.push_back(db->unitTypes[(size_t)t].name);
    *why = "Only " + names[0];
    if (names.size() == 2) *why += " and " + names[1];
    else if (names.size() > 2) *why += ", " + names[1] + " and others";
    *why += " can wield this";
  }
  return false;
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

void Battle::addRankXp(Unit& u, int group, int amount, int& before, int& after) const {
  u.weaponXp.resize(db->ranks.groups.size(), 0);
  int& xp = u.weaponXp[(size_t)group];
  before = db->ranks.rankAt(xp);
  xp = std::min(xp + amount, std::max(xp, db->ranks.topXp()));
  after = db->ranks.rankAt(xp);
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
    int before = 0, after = 0;
    addRankXp(units[(size_t)unit], w.rankGroup, w.rankXp, before, after);
    if (after > before) ups.push_back(rankUpEvent(unit, w.rankGroup, after, before));
  }
  events.insert(events.end(), ups.begin(), ups.end());
}

}  // namespace ote
