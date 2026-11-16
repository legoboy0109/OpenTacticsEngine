// The last pass of loading a game (docs/SRPG.md "Load order"): every cross reference between the strategy-RPG
// data becomes an index and is checked, area by area. The order is fixed: skills and abilities first (the others
// refer to them), then promotions, characters, supports. Each area's part lives in its own loader file.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

bool resolveSrpgRefs(const std::string& root, Database& db, std::string& err) {
  if (!resolveSkillRefs(root, db, err) || !resolveAbilityRefs(root, db, err) || !resolvePromotionRefs(root, db, err) ||
      !resolveCharacterRefs(root, db, err) || !resolveSupportRefs(root, db, err))
    return false;
  // The turn flags of "move again" / "act again" are only part of a save in a game that has such a skill.
  db.turnFlagSkills = false;
  for (const SkillDef& s : db.skills)
    for (const SkillBlock& b : s.blocks)
      for (const SkillEffect& e : b.effects)
        if (e.kind == SkillEffect::Kind::MoveAgain || e.kind == SkillEffect::Kind::ActAgain) db.turnFlagSkills = true;
  return true;
}

}  // namespace ote
