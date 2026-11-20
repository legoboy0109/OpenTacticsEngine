// Skills in a battle (docs/SRPG.md "Skills"): where a unit's skills come from, learning, and the story's
// teachSkill / forgetSkill / hasSkill. The effects live in skill_standing.cpp (always), skill_cond.cpp (conditions),
// skill_combat.cpp and skill_timed.cpp (K2).
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

bool contains(const std::vector<int>& list, int v) { return std::find(list.begin(), list.end(), v) != list.end(); }

// A maximum that changed because a skill came or went: what was gained is gained in current HP too (like a level
// up's HP growth), what was lost cuts the current HP down to the new maximum, never below 1.
void followMaxHp(const Battle& b, Unit& u, int before) {
  if (!u.alive) return;
  const int after = b.maxHp(u);
  if (after > before) u.hp += after - before;
  u.hp = std::max(1, std::min(u.hp, after));
}

Event learnedEvent(int unit, int skill) {
  Event e;
  e.type = EventType::SkillLearned;
  e.a = unit;
  e.b = skill;
  return e;
}

}  // namespace

int Battle::restingWeapon(const Unit& u) const {
  if (db->rules.items.carriedWeapons) {
    const int slot = equippedSlot(u);
    return slot < 0 ? -1 : itemOf(u.items[(size_t)slot]).weapon;
  }
  const std::vector<int>& list = typeOf(u).weapons;
  return list.empty() ? -1 : list[0];
}

// The order is the order in which blocks are judged and chances are rolled (docs/SRPG.md): the type's skills, the
// learned ones, the character's, the side's modifiers, accessories in inventory order, the weapon in use. A skill counts once, at its
// first place.
void Battle::skillsOf(const Unit& u, int weapon, std::vector<int>& out) const {
  out.clear();
  if (db->skills.empty()) return;
  auto add = [&](int s) {
    if (s >= 0 && s < (int)db->skills.size() && !contains(out, s)) out.push_back(s);
  };
  for (int s : typeOf(u).skills) add(s);
  for (int s : u.learned) add(s);
  if (u.character >= 0 && (size_t)u.character < db->characters.size())
    for (int s : db->characters[(size_t)u.character].skills) add(s);
  // The side's own modifiers (finished research today, factions later): as if the unit had them, after what is its own.
  if (u.side >= 0 && (size_t)u.side < sides.size())
    for (const SideModifier& m : sides[(size_t)u.side].modifiers)
      if (modifierApplies(m, u)) add(m.skill);
  for (const ItemStack& stack : u.items) {
    const ItemDef& d = itemOf(stack);
    if (d.kind == ItemKind::Accessory)
      for (int s : d.skills) add(s);
  }
  const int w = weapon == -1 ? restingWeapon(u) : weapon;  // -2 = no weapon at all
  if (w >= 0 && (size_t)w < db->weapons.size())
    for (int s : db->weapons[(size_t)w].skills) add(s);
  // Conditions (docs/SRPG.md section 9): the blocks of each condition the unit has, in the order they were applied, last.
  for (const ConditionState& cs : u.conditions) add(db->conditions[(size_t)cs.condition].skill);
}

bool Battle::hasSkill(const Unit& u, int skill) const {
  std::vector<int> list;
  skillsOf(u, -1, list);
  return contains(list, skill);
}

bool Battle::learnSkill(Unit& u, int skill) const {
  if (skill < 0 || contains(u.learned, skill)) return false;
  u.learned.push_back(skill);
  return true;
}

// Every "learns" entry of the unit's type and character at or below its level, oldest level first (the type's
// before the character's at the same level). No event: a new unit, a promotion.
void Battle::learnUpTo(Unit& u) const {
  if (db->skills.empty()) return;
  std::vector<LearnDef> due;
  for (const LearnDef& l : typeOf(u).learns)
    if (l.level <= u.level) due.push_back(l);
  if (u.character >= 0 && (size_t)u.character < db->characters.size())
    for (const LearnDef& l : db->characters[(size_t)u.character].learns)
      if (l.level <= u.level) due.push_back(l);
  std::stable_sort(due.begin(), due.end(), [](const LearnDef& a, const LearnDef& b) { return a.level < b.level; });
  for (const LearnDef& l : due) learnSkill(u, l.skill);
}

void Battle::dropOldClassSkills(Unit& u, int oldType) {
  auto taught = [](const std::vector<LearnDef>& list, int skill) {
    for (const LearnDef& l : list)
      if (l.skill == skill) return true;
    return false;
  };
  const std::vector<LearnDef>& old = db->unitTypes[(size_t)oldType].learns;
  const std::vector<LearnDef>& now = typeOf(u).learns;
  const bool hasCharacter = u.character >= 0 && (size_t)u.character < db->characters.size();
  for (size_t i = 0; i < u.learned.size();) {
    const int s = u.learned[i];
    const bool fromOld = taught(old, s);
    const bool stays = taught(now, s) || (hasCharacter && taught(db->characters[(size_t)u.character].learns, s));
    if (fromOld && !stays) u.learned.erase(u.learned.begin() + (long)i);
    else ++i;
  }
  clampHp(u);  // a skill that raised the maximum may be gone
}

// The entries for exactly the new level.
void Battle::skillsLevelUp(int unit, std::vector<Event>& events) {
  if (db->skills.empty()) return;
  Unit& u = units[(size_t)unit];
  const int before = maxHp(u);
  std::vector<LearnDef> due;
  for (const LearnDef& l : typeOf(u).learns)
    if (l.level == u.level) due.push_back(l);
  if (u.character >= 0 && (size_t)u.character < db->characters.size())
    for (const LearnDef& l : db->characters[(size_t)u.character].learns)
      if (l.level == u.level) due.push_back(l);
  bool any = false;
  for (const LearnDef& l : due)
    if (learnSkill(u, l.skill)) {
      events.push_back(learnedEvent(unit, l.skill));
      any = true;
    }
  if (any) followMaxHp(*this, u, before);
}

// teachSkill: every matching unit on the board learns it for good.
void Battle::storyTeachSkill(const Command& c, std::vector<Event>& events) {
  const int count = (int)units.size();
  for (int i = 0; i < count; ++i) {
    Unit& u = units[(size_t)i];
    if (!u.alive || !unitMatches(c.filter, u)) continue;
    const int before = maxHp(u);
    if (!learnSkill(u, c.index)) continue;
    events.push_back(learnedEvent(i, c.index));
    followMaxHp(*this, u, before);
  }
}

// forgetSkill: removed from the learned list; a skill the unit has from another source stays.
void Battle::storyForgetSkill(const Command& c, std::vector<Event>& events) {
  (void)events;
  for (Unit& u : units) {
    if (!u.alive || !unitMatches(c.filter, u)) continue;
    auto at = std::find(u.learned.begin(), u.learned.end(), c.index);
    if (at == u.learned.end()) continue;
    const int before = maxHp(u);
    u.learned.erase(at);
    followMaxHp(*this, u, before);
  }
}

bool Battle::storyHasSkill(const Condition& c) const {
  for (const Unit& u : units)
    if (u.alive && unitMatches(c.filter, u) && hasSkill(u, c.index)) return true;
  return false;
}

}  // namespace ote
