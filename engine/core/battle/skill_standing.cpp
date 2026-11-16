// Standing skill effects (docs/SRPG.md "Skills": the "always" blocks): stat bonuses, growth bonuses, the movement
// effects (cap on the cost of a square, passing through enemies), "moveAgain" and "xpPercent". Everything here is
// asked a lot (every stat read), so a game without skills never gets past the first line of each function.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

void Battle::standingEffects(const Unit& u, bool unconditionedOnly, std::vector<const SkillEffect*>& out) const {
  out.clear();
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
      for (const SkillEffect& e : b.effects) out.push_back(&e);
    }
  }
}

// The HP stat only looks at blocks without conditions: a maximum that depended on the current HP (an `hp` condition
// reads the maximum) would ask for itself, and one that flickered would cut the current HP.
int Battle::skillStatBonus(const Unit& u, int stat) const {
  if (db->skills.empty() || stat < 0) return 0;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, stat == db->rules.statHp, fx);
  int sum = 0;
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::Stats && (size_t)stat < e->stats.size()) sum += e->stats[(size_t)stat];
  return sum;
}

// "growths" effects cannot have conditions (the loader refuses them), so only unconditioned blocks are read.
int Battle::skillGrowthBonus(const Unit& u, int stat) const {
  if (db->skills.empty() || stat < 0) return 0;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, true, fx);
  int sum = 0;
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::Growths && (size_t)stat < e->stats.size()) sum += e->stats[(size_t)stat];
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

bool Battle::hasMoveAgain(const Unit& u) const {
  if (db->skills.empty()) return false;
  std::vector<const SkillEffect*> fx;
  standingEffects(u, false, fx);
  for (const SkillEffect* e : fx)
    if (e->kind == SkillEffect::Kind::MoveAgain) return true;
  return false;
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
