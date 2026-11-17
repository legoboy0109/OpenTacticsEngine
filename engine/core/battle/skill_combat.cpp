// Skills in a combat (docs/SRPG.md "Skills" and "Combat pipeline"): the "combat" blocks of the two fighters and the
// auras of other units. Both run for every forecast and before every strike, so they only read and never roll.
// The strike-level blocks are in skill_strike.cpp.
#include "core/battle/combat_mods.h"

namespace ote {

namespace {

// One effect of a "combat" block (or an aura) into the bag of whoever receives it.
// `v` is the effect's number (a formula's result already worked out by Battle::skillEffectNumber), `stats` the per-stat numbers.
void applyCombatEffect(const SkillEffect& e, int v, const std::vector<int>& stats, CombatMods& m) {
  using K = SkillEffect::Kind;
  switch (e.kind) {
    case K::Stats:
      for (size_t i = 0; i < stats.size(); ++i)
        if (stats[i] != 0) m.addStat((int)i, stats[i]);
      break;
    case K::Hit: m.hit += v; break;
    case K::Avoid: m.avoid += v; break;
    case K::Crit: m.crit += v; break;
    case K::CritAvoid: m.critAvoid += v; break;
    case K::Damage: m.damage += v; break;
    case K::DamageTaken: m.damageTaken += v; break;
    case K::DamagePercent: m.damagePercent.push_back(v); break;
    case K::DamageTakenPercent: m.damageTakenPercent.push_back(v); break;
    case K::Strikes: m.strikes += e.value; break;
    case K::MultiStrike: m.raiseMultiStrike(e.value); break;
    case K::StrikeFirst: m.strikeFirst = true; break;
    case K::CounterAnyRange: m.counterAnyRange = true; break;
    case K::NoCounter: m.noCounter = true; break;
    default: break;  // the loader keeps other effects out of "combat" blocks
  }
}

}  // namespace

// The attacker's own "combat" blocks, then the defender's; `to: foe` goes into the other fighter's bag.
void Battle::skillCombatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const {
  if (db->skills.empty()) return;
  std::vector<int> list, scratch;
  for (int side = 0; side < 2; ++side) {
    const int owner = side == 0 ? setup.attacker : setup.defender;
    CombatMods& own = side == 0 ? attacker : defender;
    CombatMods& foe = side == 0 ? defender : attacker;
    skillCombatList(setup, owner, list);
    const SkillCtx ctx = skillCombatCtx(setup, owner);
    for (int s : list)
      for (const SkillBlock& b : db->skills[(size_t)s].blocks) {
        if (b.when != SkillWhen::Combat || b.aura || !skillBlockHolds(b, ctx)) continue;
        for (const SkillEffect& e : b.effects) {
          const int v = skillEffectNumber(e, ctx, scratch);
          applyCombatEffect(e, v, e.kind == SkillEffect::Kind::Stats ? scratch : e.stats, e.to == SkillEffect::To::Foe ? foe : own);
        }
      }
  }
}

// Auras of units on the board, in unit id order: the effects go to each fighter (never the owner) within reach.
// Conditions are judged for the owner alone (no foe), with the weapon it fights with if it is a fighter itself.
void Battle::auraCombatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const {
  if (db->skills.empty()) return;
  const int fighters[2] = {setup.attacker, setup.defender};
  CombatMods* bags[2] = {&attacker, &defender};
  const Cell cells[2] = {setup.from, units[(size_t)setup.defender].cell};
  std::vector<int> list, scratch;
  for (const Unit& o : units) {
    if (!o.alive || o.cell == kNoCell) continue;
    const bool isFighter = o.id == setup.attacker || o.id == setup.defender;
    const Cell ownerCell = o.id == setup.attacker ? setup.from : o.cell;
    // Nothing reaches further than 10 steps: skip owners far from both fighters before looking at their skills.
    if (grid.distance(ownerCell, cells[0]) > 10 && grid.distance(ownerCell, cells[1]) > 10) continue;
    if (isFighter) skillCombatList(setup, o.id, list);
    else skillsOf(o, -1, list);
    SkillCtx ctx;
    ctx.unit = o.id;
    ctx.cell = ownerCell;
    ctx.weapon = isFighter ? (o.id == setup.attacker ? setup.weaponA : setup.weaponD) : restingWeapon(o);
    for (int s : list)
      for (const SkillBlock& b : db->skills[(size_t)s].blocks) {
        if (b.when != SkillWhen::Combat || !b.aura || !skillBlockHolds(b, ctx)) continue;
        for (int f = 0; f < 2; ++f) {
          const Unit& target = units[(size_t)fighters[f]];
          if (target.id == o.id || grid.distance(ownerCell, cells[f]) > b.auraWithin) continue;
          // "enemies" reaches a fighter only if the owner's team sees it.
          if (b.auraEnemies ? !(isEnemy(o, target) && unitVisibleTo(teamOf(o), target)) : teamOf(o) != teamOf(target))
            continue;
          if (!b.auraMatch.empty() && !unitHasKey(target, b.auraMatch)) continue;  // "match": only that kind of unit
          // A formula amount sees the owner as `a` and the fighter that receives it as `b`.
          SkillCtx fx = ctx;
          fx.foe = target.id;
          fx.foeCell = cells[f];
          fx.foeWeapon = f == 0 ? setup.weaponA : setup.weaponD;
          for (const SkillEffect& e : b.effects) {
            const int v = skillEffectNumber(e, fx, scratch);
            applyCombatEffect(e, v, e.kind == SkillEffect::Kind::Stats ? scratch : e.stats, *bags[f]);
          }
        }
      }
  }
}

}  // namespace ote
