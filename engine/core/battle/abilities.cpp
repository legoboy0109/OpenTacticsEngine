// Menu abilities in a battle (docs/SRPG.md "Menu abilities and summoning"): who has which ability, the menu and its
// targets, validation, and the per-turn bookkeeping. The rest: ability_target.cpp (what a target needs),
// ability_apply.cpp (carrying one out), ability_moves.cpp (push, pull, swap, moveBehind), summons.cpp.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

bool refuse(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

void addOnce(std::vector<int>& out, int ability) {
  if (std::find(out.begin(), out.end(), ability) == out.end()) out.push_back(ability);
}

}  // namespace

// The type's abilities, the character's, then the skills' (an "ability" effect of an unconditioned "always" block).
void Battle::abilitiesOf(const Unit& u, std::vector<int>& out) const {
  out.clear();
  if (db->abilities.empty()) return;
  for (int a : typeOf(u).abilities) addOnce(out, a);
  if (u.character >= 0 && (size_t)u.character < db->characters.size())
    for (int a : db->characters[(size_t)u.character].abilities) addOnce(out, a);
  if (u.jobs.on()) {  // the abilities its job taught it and the equipped ones (docs/JOBS.md)
    std::vector<int> entries;
    jobEntriesOf(u, entries);
    for (int flat : entries)
      if (db->jobEntries[(size_t)flat].ability) addOnce(out, db->jobEntries[(size_t)flat].ref);
  }
  if (db->teachingOn()) {  // what equipped items teach, and what it mastered from them (docs/JOBS.md "Learning from equipment")
    std::vector<int> taught, kept;
    teachFromItems(u, taught);
    teachMasteredOf(u, kept);
    taught.insert(taught.end(), kept.begin(), kept.end());
    for (int t : taught)
      if (db->teachEntries[(size_t)t].ability) addOnce(out, db->teachEntries[(size_t)t].ref);
  }
  if (db->skills.empty()) return;
  std::vector<int> skills;
  skillsOf(u, -1, skills);
  for (int s : skills)
    for (const SkillBlock& block : db->skills[(size_t)s].blocks) {
      if (block.when != SkillWhen::Always) continue;
      for (const SkillEffect& fx : block.effects)
        if (fx.kind == SkillEffect::Kind::Ability && fx.ability >= 0) addOnce(out, fx.ability);
    }
  // An ability that spends its item is offered only while an item gives it (ability_item.cpp).
  out.erase(std::remove_if(out.begin(), out.end(),
                           [&](int a) { return db->abilities[(size_t)a].spendsItem && abilityItemSlot(u, a) < 0; }),
            out.end());
}

const AbilityUse* Battle::abilityUseOf(const Unit& u, int ability) const {
  for (const AbilityUse& use : u.abilityUses)
    if (use.ability == ability) return &use;
  return nullptr;
}

void Battle::abilityTargets(int unit, int ability, Cell from, std::vector<int>& units_, std::vector<Cell>& cells_) const {
  units_.clear();
  cells_.clear();
  if (unit < 0 || unit >= (int)units.size() || ability < 0 || ability >= (int)db->abilities.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive) return;
  std::vector<int> owned;
  abilitiesOf(u, owned);
  if (std::find(owned.begin(), owned.end(), ability) == owned.end()) return;
  const AbilityDef& d = db->abilities[(size_t)ability];
  if (d.target == AbilityTarget::Self) {
    if (abilityTargetOk(u, ability, from, unit, kNoCell, nullptr)) units_.push_back(unit);
  } else if (d.target == AbilityTarget::Cell) {
    std::vector<Cell> near;
    // A strike: the squares its weapon can be aimed at (the height rules may add reach), as abilityStrikeOptions offers them; else the range.
    if (d.effect == AbilityEffect::Strike) aimCells(d.strike, from, unit, near);
    else grid.within(from, d.maxRange, near);  // ascending cell index
    for (Cell c : near)
      if (abilityTargetOk(u, ability, from, -1, c, nullptr)) cells_.push_back(c);
  } else {
    for (const Unit& t : units)  // ascending unit id
      if (t.alive && abilityTargetOk(u, ability, from, t.id, kNoCell, nullptr)) units_.push_back(t.id);
  }
}

void Battle::abilityOptions(int unit, Cell from, std::vector<AbilityOption>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !grid.valid(from)) return;
  const Unit& u = units[(size_t)unit];
  std::vector<int> owned;
  abilitiesOf(u, owned);
  for (int a : owned) {
    AbilityOption o;
    o.ability = a;
    o.ok = abilityStateOk(u, a, from, &o.why);
    if (db->abilities[(size_t)a].spendsItem) {
      const int slot = abilityItemSlot(u, a);
      if (slot >= 0) o.itemUses = u.items[(size_t)slot].uses;
    }
    if (o.ok) {
      std::vector<int> us;
      std::vector<Cell> cs;
      abilityTargets(unit, a, from, us, cs);
      if (us.empty() && cs.empty()) {
        o.ok = false;
        o.why = "nothing to target";
        o.noTarget = true;
        if (db->abilities[(size_t)a].effect == AbilityEffect::Unlock) {
          // A door or chest close by that the trait cannot open from here only because of where the user stands ("from": "on" / "next"):
          // say so, instead of "nothing to target".
          const int reach = std::max(1, db->abilities[(size_t)a].maxRange);
          for (size_t s = 0; s < structures.size() && o.why == "nothing to target"; ++s) {
            if (placeDistance((int)s, from) > reach) continue;
            std::vector<Cell> sc;
            structureCells((int)s, sc);
            std::string why;
            if (!sc.empty() && !unlockTargetOk(u, a, from, sc[0], &why) && (why.rfind("stand on", 0) == 0 || why.rfind("step off", 0) == 0)) o.why = why;
          }
        }
      }
    }
    out.push_back(o);
  }
}

bool Battle::validateAbility(const Action& action, std::string* why) const {
  if (db->abilities.empty()) return refuse(why, "this game has no abilities");
  const Unit& u = units[(size_t)action.unit];  // validate() has checked the unit
  if (action.ability < 0 || action.ability >= (int)db->abilities.size()) return refuse(why, "there is no such ability");
  std::vector<int> owned;
  abilitiesOf(u, owned);
  if (std::find(owned.begin(), owned.end(), action.ability) == owned.end())
    return refuse(why, "that unit does not have that ability");
  if (!abilityStateOk(u, action.ability, u.cell, why) ||
      !abilityTargetOk(u, action.ability, u.cell, action.target, action.cell, why))
    return false;
  if (db->abilities[(size_t)action.ability].effect == AbilityEffect::Steal) {  // which item: `slot` of the target's inventory
    std::vector<StealOption> opts;
    stealOptions(u.id, action.ability, u.cell, action.target, opts);
    if (action.slot < 0 || action.slot >= (int)opts.size()) return refuse(why, "choose an item to steal");
    if (!opts[(size_t)action.slot].ok) return refuse(why, "that item " + opts[(size_t)action.slot].why);
  }
  return true;
}

void Battle::noteAbilityUse(int unit, int ability) {
  Unit& u = units[(size_t)unit];
  const AbilityDef& d = db->abilities[(size_t)ability];
  AbilityUse* use = nullptr;
  for (AbilityUse& e : u.abilityUses)
    if (e.ability == ability) use = &e;
  if (!use) {
    u.abilityUses.push_back(AbilityUse());
    use = &u.abilityUses.back();
    use->ability = ability;
  }
  use->used = std::min(use->used + 1, 999);  // the save keeps it to 999
  use->wait = d.cooldown;
  use->turn = d.free;  // only a free ability is held to once per turn; an action ends the turn anyway
}

// Start of the active side's turn: cooldowns drop, "used this turn" clears, and summons whose time is up leave.
void Battle::abilitiesTurnStart(std::vector<Event>& events) {
  if (db->abilities.empty()) return;
  for (Unit& u : units) {
    if (!ownsTurn(u) || !exists(u)) continue;
    for (AbilityUse& use : u.abilityUses) {
      if (use.wait > 0) --use.wait;
      use.turn = false;
    }
  }
  // "lifetime: n": it stays for n of its side's turns after the one it was made in and leaves when the next begins.
  // removeSummon can remove others (its own summons), so every unit is looked at as it stands then.
  for (size_t i = 0; i < units.size(); ++i) {
    Unit& u = units[i];
    if (!ownsTurn(u) || u.summoner < 0 || u.summonLeft < 0 || !exists(u)) continue;
    if (u.summonLeft == 0) removeSummon((int)i, events);
    else --u.summonLeft;
  }
}

}  // namespace ote
