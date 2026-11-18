// Which items give an ability (docs/SRPG.md "Who has an ability"): an accessory's skills, or a weapon item's weapon's skills,
// where a skill is an "always" block with an "ability" effect. Used by the loader's check of "spendsItem" and by the battle.
#pragma once
#include "core/data/database.h"

namespace ote {

// Does the skill (index into db.skills) give the ability (index into db.abilities)?
bool skillGivesAbility(const Database& db, int skill, int ability);
// Does the item (index into db.items) give it: an accessory through its skills, a weapon item through its weapon's skills.
bool itemGivesAbility(const Database& db, int item, int ability);

}  // namespace ote
