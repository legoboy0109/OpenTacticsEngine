// The last pass of loading a game (docs/SRPG.md "Load order"): every cross reference between the strategy-RPG
// data becomes an index and is checked, area by area. The order is fixed: skills and abilities first (the others
// refer to them), then promotions, characters, supports. Each area's part lives in its own loader file.
#include "core/data/loader_conditions.h"
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_staves.h"
#include "core/data/loader_util.h"
#include "core/data/loader_weapon_locks.h"

namespace ote {

bool resolveSrpgRefs(const std::string& root, Database& db, std::string& err) {
  if (!resolveWeaponWielders(root, db, err) || !resolveConditionRefs(root, db, err) || !resolveSkillRefs(root, db, err) || !resolveAbilityRefs(root, db, err) || !resolvePromotionRefs(root, db, err) ||
      !resolveCharacterRefs(root, db, err) || !resolveSupportRefs(root, db, err) || !resolveStaffRefs(root, db, err))
    return false;
  // The turn flags of "move again" / "act again" are only part of a save in a game that has such a skill.
  db.turnFlagSkills = false;
  for (const SkillDef& s : db.skills)
    for (const SkillBlock& b : s.blocks)
      for (const SkillEffect& e : b.effects)
        if (e.kind == SkillEffect::Kind::MoveAgain || e.kind == SkillEffect::Kind::ActAgain) db.turnFlagSkills = true;
  db.nullifySkills = db.staffRangeSkills = db.carryBonusSkills = db.cancelEffectiveSkills = db.visionSkills = db.marksUsed = false;
  for (const SkillDef& s : db.skills)
    for (const SkillBlock& b : s.blocks) {
      for (const SkillCond& c : b.conds) db.marksUsed = db.marksUsed || c.kind == SkillCond::Kind::Marked;
      for (const SkillEffect& e : b.effects) {
        db.marksUsed = db.marksUsed || e.kind == SkillEffect::Kind::Mark;
        db.nullifySkills = db.nullifySkills || e.kind == SkillEffect::Kind::NullifySkills;
        db.staffRangeSkills = db.staffRangeSkills || e.kind == SkillEffect::Kind::StaffRange;
        db.carryBonusSkills = db.carryBonusSkills || e.kind == SkillEffect::Kind::CarryBonus;
        db.cancelEffectiveSkills = db.cancelEffectiveSkills || e.kind == SkillEffect::Kind::CancelEffective;
        db.visionSkills = db.visionSkills || e.kind == SkillEffect::Kind::Vision;
      }
    }
  return true;
}

}  // namespace ote
