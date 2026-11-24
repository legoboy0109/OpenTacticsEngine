// Small shared pieces of the K2 skill code (docs/SRPG.md "Skills"): the context a fighter's blocks are judged with,
// conditions and chances. Nothing here rolls or changes the battle.
#include <algorithm>
#include <cmath>

#include "core/battle/battle.h"
#include "core/script/formulas.h"

namespace ote {

SkillCtx Battle::skillCombatCtx(const CombatSetup& setup, int unit) const {
  const bool atk = unit == setup.attacker;
  SkillCtx ctx;
  ctx.unit = unit;
  ctx.cell = atk ? setup.from : kNoCell;  // kNoCell = where it really stands
  ctx.weapon = atk ? setup.weaponA : setup.weaponD;
  ctx.foe = atk ? setup.defender : setup.attacker;
  ctx.foeCell = atk ? kNoCell : setup.from;
  ctx.foeWeapon = atk ? setup.weaponD : setup.weaponA;
  ctx.combat = true;
  ctx.initiating = atk;
  return ctx;
}

// The one place a fighter's skills for a fight are gathered (docs/SRPG.md "Switch off the foe's skills"): its own list for the weapon it
// fights with, minus what the foe's "nullifySkills" switches off. Forecast, strikes, auras and notes all come through here.
void Battle::skillCombatList(const CombatSetup& setup, int unit, std::vector<int>& out) const {
  skillsOfFighter(setup, unit, out);
  if (!db->nullifySkills || out.empty()) return;
  const int foe = unit == setup.attacker ? setup.defender : setup.attacker;
  std::vector<int> foeList;
  skillsOfFighter(setup, foe, foeList);
  const SkillCtx foeCtx = skillCombatCtx(setup, foe);
  // What the foe's "combat" blocks (whose conditions hold) switch off. A skill that itself has such an effect is never switched off:
  // when both fighters have one, each loses everything but the nullifying skill.
  std::vector<const SkillEffect*> fx;
  for (int s : foeList)
    for (const SkillBlock& b : db->skills[(size_t)s].blocks) {
      if (b.when != SkillWhen::Combat || b.aura) continue;
      bool any = false;
      for (const SkillEffect& e : b.effects) any = any || e.kind == SkillEffect::Kind::NullifySkills;
      if (!any || !skillBlockHolds(b, foeCtx)) continue;
      for (const SkillEffect& e : b.effects)
        if (e.kind == SkillEffect::Kind::NullifySkills) fx.push_back(&e);
    }
  if (fx.empty()) return;
  for (size_t i = 0; i < out.size();) {
    const int s = out[i];
    const SkillDef& def = db->skills[(size_t)s];
    bool exempt = def.hidden;  // a condition's blocks (section 9) are never switched off
    for (const SkillBlock& b : def.blocks)
      for (const SkillEffect& e : b.effects) exempt = exempt || e.kind == SkillEffect::Kind::NullifySkills;
    bool off = false;
    if (!exempt)
      for (const SkillEffect* e : fx) {
        if (e->value == 0) off = true;
        else if (e->value == 1) off = off || std::find(e->skillIds.begin(), e->skillIds.end(), s) != e->skillIds.end();
        else off = off || std::find(def.tags.begin(), def.tags.end(), e->names[0]) != def.tags.end();
      }
    if (off) out.erase(out.begin() + (long)i);
    else ++i;
  }
}

// A fighter's own skills for this fight (before anything is switched off): the weapon it fights with; a defender with nothing that can
// answer has no weapon skills.
void Battle::skillsOfFighter(const CombatSetup& setup, int unit, std::vector<int>& out) const {
  const bool atk = unit == setup.attacker;
  const int w = atk ? setup.weaponA : setup.weaponD;
  skillsOf(units[(size_t)unit], w >= 0 ? w : -2, out);
}

bool Battle::skillBlockHolds(const SkillBlock& block, const SkillCtx& ctx) const {
  for (const SkillCond& c : block.conds)
    if (!skillCondHolds(c, ctx)) return false;
  return true;
}

// a = the owner, b = the foe (nil without one), the usual variables (docs/SRPG.md "Formulas"), `turn` = the round number.
double Battle::skillExpr(int expr, const SkillCtx& ctx, double def) const {
  if (!formulas || ctx.unit < 0 || ctx.unit >= (int)units.size()) return def;
  const Unit& a = units[(size_t)ctx.unit];
  const Unit* b = ctx.foe >= 0 && ctx.foe < (int)units.size() ? &units[(size_t)ctx.foe] : nullptr;
  const Cell ca = ctx.cell == kNoCell ? a.cell : ctx.cell;
  const Cell cb = !b ? kNoCell : (ctx.foeCell == kNoCell ? b->cell : ctx.foeCell);
  Props scratchA, scratchB;
  FormulaCtx fc;
  fc.sides = &sides;
  fc.battle = this;
  fc.a = &a;
  fc.b = b;
  if (ctx.weapon >= 0) fc.w = &db->weapons[(size_t)ctx.weapon];
  if (ctx.foeWeapon >= 0) fc.fw = &db->weapons[(size_t)ctx.foeWeapon];
  if (ca != kNoCell) { fc.ta = &numbersAt(ca, scratchA); fc.ha = heightAt(ca); }
  if (cb != kNoCell) { fc.tb = &numbersAt(cb, scratchB); fc.hb = heightAt(cb); }
  if (ca != kNoCell && cb != kNoCell) fc.dist = grid.distance(ca, cb);
  if (b && ctx.weapon >= 0) {
    double vs = 1;
    if (versusValue(ctx.weapon, *b, vs)) fc.vs = vs;
  }
  fc.strikeNo = ctx.strikeNo;
  fc.healed = ctx.healed;
  fc.dealt = ctx.dealt;
  fc.item = ctx.item;
  fc.itemUses = ctx.itemUses;
  fc.arc = ctx.arc;
  return formulas->evalExpr(expr, fc, def);
}

int Battle::skillEffectNumber(const SkillEffect& e, const SkillCtx& ctx, std::vector<int>& stats) const {
  using K = SkillEffect::Kind;
  auto whole = [](double v, int lo, int hi) {
    if (v != v) return 0;
    v = std::floor(v);
    return v < lo ? lo : v > hi ? hi : (int)v;
  };
  if (e.kind == K::Stats) {
    stats = e.stats;
    for (size_t i = 0; i < e.statExprs.size() && i < stats.size(); ++i)
      if (e.statExprs[i] >= 0) stats[i] = whole(skillExpr(e.statExprs[i], ctx, 0), -1000, 1000);
    return 0;
  }
  if (e.expr < 0) return e.value;
  const bool percent = e.kind == K::DamagePercent || e.kind == K::DamageTakenPercent;
  return percent ? whole(skillExpr(e.expr, ctx, 100), 0, 1000) : whole(skillExpr(e.expr, ctx, 0), -1000, 1000);
}

bool Battle::unitHasKey(const Unit& u, const std::string& key) const {
  if (unitTypeMatchesKey(typeOf(u), key)) return true;
  if (u.character >= 0 && (size_t)u.character < db->characters.size()) {
    const std::vector<std::string>& tags = db->characters[(size_t)u.character].tags;
    for (const std::string& t : tags)
      if (t == key) return true;
  }
  return false;
}

int Battle::skillChance(const SkillBlock& block, const SkillCtx& ctx) const {
  double v = block.chance;
  if (block.chanceExpr >= 0) {
    if (!formulas || ctx.unit < 0 || ctx.unit >= (int)units.size()) return 0;
    v = skillExpr(block.chanceExpr, ctx, 0);
  }
  if (v != v) return 0;  // not a number
  v = std::floor(v);
  return v < 0 ? 0 : v > 100 ? 100 : (int)v;
}

}  // namespace ote
