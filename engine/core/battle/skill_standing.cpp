// Standing skill effects (docs/SRPG.md "Skills": the "always" blocks): stat bonuses, growth bonuses, the movement
// effects (cap on the cost of a square, passing through enemies), "moveAgain" and "xpPercent". Everything here is
// asked a lot (every stat read), so a game without skills never gets past the first line of each function.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

// A fading condition ("scale": "turnsLeft"): its blocks' plain amounts count as many times as it has turns left (0 on its last turn).
int Battle::conditionScale(const Unit& u, int skill) const {
  if (!db->scaledConditions || !db->skills[(size_t)skill].hidden) return 1;
  for (const ConditionState& cs : u.conditions) {
    const ConditionDef& d = db->conditions[(size_t)cs.condition];
    if (d.skill == skill && d.scale) return std::max(0, cs.left);
  }
  return 1;
}

void Battle::standingEffects(const Unit& u, bool unconditionedOnly, std::vector<const SkillEffect*>& out, std::vector<int>* scales) const {
  out.clear();
  if (scales) scales->clear();
  if (db->skills.empty()) return;
  std::vector<int> list;
  skillsOf(u, -1, list);
  // A block with conditions is judged through the unit in `units`; a unit that is not in there yet (being made or
  // read from a save) only has its unconditioned blocks looked at.
  const bool judgeable = u.id >= 0 && u.id < (int)units.size();
  SkillCtx ctx;
  ctx.unit = u.id;
  ctx.weapon = restingWeapon(u);
  for (int s : list) {
    const int scale = scales && db->scaledConditions ? conditionScale(u, s) : 1;
    for (const SkillBlock& b : db->skills[(size_t)s].blocks) {
      if (b.when != SkillWhen::Always) continue;
      bool holds = true;
      if (!b.conds.empty()) {
        if (unconditionedOnly || !judgeable) continue;
        for (const SkillCond& c : b.conds)
          if (!skillCondHolds(c, ctx)) {
            holds = false;
            break;
          }
      }
      if (!holds) continue;
      for (const SkillEffect& e : b.effects) {
        out.push_back(&e);
        if (scales && db->scaledConditions) scales->push_back(scale);  // left empty in a game with no fading condition
      }
    }
  }
}

// The HP stat only looks at blocks without conditions: a maximum that depended on the current HP (an `hp` condition
// reads the maximum) would ask for itself, and one that flickered would cut the current HP.
int Battle::skillStatBonus(const Unit& u, int stat) const {
  if (db->skills.empty() || stat < 0) return 0;
  std::vector<const SkillEffect*> fx;
  std::vector<int> scale;
  standingEffects(u, stat == db->rules.statHp, fx, &scale);
  int sum = 0;
  for (size_t i = 0; i < fx.size(); ++i)
    if (fx[i]->kind == SkillEffect::Kind::Stats && (size_t)stat < fx[i]->stats.size()) sum += fx[i]->stats[(size_t)stat] * (scale.empty() ? 1 : scale[i]);
  return sum;
}

// "growths" effects cannot have conditions (the loader refuses them), so only unconditioned blocks are read.
int Battle::skillGrowthBonus(const Unit& u, int stat) const {
  if (db->skills.empty() || stat < 0) return 0;
  std::vector<const SkillEffect*> fx;
  std::vector<int> scale;
  standingEffects(u, true, fx, &scale);
  int sum = 0;
  for (size_t i = 0; i < fx.size(); ++i)
    if (fx[i]->kind == SkillEffect::Kind::Growths && (size_t)stat < fx[i]->stats.size()) sum += fx[i]->stats[(size_t)stat] * (scale.empty() ? 1 : scale[i]);
  return sum;
}

int Battle::carryBonusOf(const Unit& u) const {
  if (!db->carryBonusSkills) return 0;
  std::vector<const SkillEffect*> fx;
  std::vector<int> scale;
  standingEffects(u, false, fx, &scale);
  int sum = 0;
  for (size_t i = 0; i < fx.size(); ++i)
    if (fx[i]->kind == SkillEffect::Kind::CarryBonus) sum += fx[i]->value * (scale.empty() ? 1 : scale[i]);
  return sum;
}

int Battle::moveCostCap(const Unit& u) const {
  if (db->skills.empty()) return 0;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  int cap = 0;  // the lowest wins
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::MaxMoveCost && (cap == 0 || e->value < cap)) cap = e->value;
  return cap;
}

bool Battle::passesEnemies(const Unit& u) const {
  if (db->skills.empty()) return false;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::Pass) return true;
  return false;
}

// `afterAttack`: a { "notAfterAttack": true } effect (value 1) does not count when the action was a fight.
bool Battle::hasMoveAgain(const Unit& u, bool afterAttack) const {
  if (db->skills.empty()) return false;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::MoveAgain && !(afterAttack && e->value == 1)) return true;
  return false;
}

bool Battle::hasStanding(const Unit& u, SkillEffect::Kind kind) const {
  if (db->skills.empty()) return false;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  for (const SkillEffect* e : fx)
    if (e->kind == kind) return true;
  return false;
}

// Weapon experience gained is multiplied like XP: percents one after another with whole-number division.
int Battle::skillWeaponXp(const Unit& u, int amount) const {
  if (db->skills.empty()) return amount;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::WeaponXpPercent) amount = (int)((long long)amount * e->value / 100);
  return amount;
}

// Percents multiply one after another in skill-list order with whole-number division.
int Battle::skillXp(const Unit& u, int amount) const {
  if (db->skills.empty()) return amount;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::XpPercent) amount = (int)((long long)amount * e->value / 100);
  return amount;
}

}  // namespace ote
