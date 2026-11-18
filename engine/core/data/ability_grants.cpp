#include "core/data/ability_grants.h"

namespace ote {

bool skillGivesAbility(const Database& db, int skill, int ability) {
  if (skill < 0 || (size_t)skill >= db.skills.size()) return false;
  for (const SkillBlock& block : db.skills[(size_t)skill].blocks) {
    if (block.when != SkillWhen::Always) continue;
    for (const SkillEffect& fx : block.effects)
      if (fx.kind == SkillEffect::Kind::Ability && fx.ability == ability) return true;
  }
  return false;
}

bool itemGivesAbility(const Database& db, int item, int ability) {
  if (item < 0 || (size_t)item >= db.items.size()) return false;
  const ItemDef& d = db.items[(size_t)item];
  if (d.kind == ItemKind::Accessory) {
    for (int s : d.skills)
      if (skillGivesAbility(db, s, ability)) return true;
  } else if (d.kind == ItemKind::Weapon && d.weapon >= 0 && (size_t)d.weapon < db.weapons.size()) {
    for (int s : db.weapons[(size_t)d.weapon].skills)
      if (skillGivesAbility(db, s, ability)) return true;
  }
  return false;
}

}  // namespace ote
