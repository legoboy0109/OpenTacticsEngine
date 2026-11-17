// Step D1 of docs/SRPG.md: what games/demo-srpg and editor/templates/strategy-rpg carry of the strategy-RPG parts. The
// numbers are pinned on purpose: changing the content means changing them here.
#include "save_helpers.h"

using namespace ote;

namespace {
bool load(Database& db, const std::string& rel) {
  std::string err;
  const bool ok = loadGame(ote_test::sourceDir() + "/" + rel, db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  return ok;
}
}  // namespace

// The skills the maker wrote: a condition's blocks (and a weapon's "inflict") are kept as hidden skills (docs/SRPG.md section 9).
static size_t visibleSkills(const Database& db) {
  size_t n = 0;
  for (const SkillDef& s : db.skills) n += !s.hidden;
  return n;
}

TEST(demo_srpg_has_the_strategy_rpg_content_of_the_contract) {
  Database db;
  CHECK(load(db, "games/demo-srpg"));
  CHECK_EQ(db.characters.size(), size_t(7));  // the company, Sera, and the two bosses
  CHECK_EQ(visibleSkills(db), size_t(33));    // the starter set (30) and Armor Breaker, Woodcraft, Sword Dancer
  CHECK_EQ(db.conditions.size(), size_t(6));  // the starter conditions (docs/SRPG.md section 9)
  CHECK(db.findItem("antidote") >= 0 && db.items[(size_t)db.findItem("antidote")].cures.size() == 1);
  CHECK_EQ(db.abilities.size(), size_t(5));   // Shove, Swap, Pull Back, Rally, Call Hound
  CHECK_EQ(db.supports.pairs.size(), size_t(4));
  CHECK_EQ(db.supports.rules.maxLevels, 3);
  CHECK(db.rules.rescue.mode == RescueMode::Carry);
  CHECK(!db.rules.promotion.keepLevel);
  CHECK(db.findCharacter("sera") >= 0 && db.findUnitType("sky_rider") >= 0 && db.findItem("salve") >= 0);
  CHECK(db.findUnitType("pegasus") < 0 && db.findItem("vulnerary") < 0);  // renamed to neutral ids
  // Promotions: captain (story), lancer and outrider (the writ), marksman (offered at 4), brute (automatic at 5).
  const UnitTypeDef& cav = db.unitTypes[(size_t)db.findUnitType("cavalier")];
  CHECK_EQ(cav.promotions.size(), size_t(2));
  const UnitTypeDef& arch = db.unitTypes[(size_t)db.findUnitType("archer")];
  CHECK_EQ(arch.promotions.size(), size_t(1));
  CHECK_EQ(arch.promotions[0].fromLevel, 4);
  CHECK(db.findItem("officers_writ") >= 0 && db.items[(size_t)db.findItem("officers_writ")].promotes);
  CHECK(db.findSkill("armor_breaker") >= 0 && db.findAbility("call_hound") >= 0);
}

TEST(template_strategy_rpg_has_pair_up_a_promotion_one_support_and_shove) {
  Database db;
  CHECK(load(db, "editor/templates/strategy-rpg"));
  CHECK_EQ(visibleSkills(db), size_t(30));
  CHECK_EQ(db.conditions.size(), size_t(6));  // poison, sleep, silence, berserk, stun, curse
  CHECK_EQ(db.characters.size(), size_t(5));
  CHECK(db.rules.rescue.mode == RescueMode::PairUp);
  CHECK_EQ(db.abilities.size(), size_t(1));
  CHECK_EQ(db.supports.pairs.size(), size_t(1));
  CHECK_EQ(db.supports.pairs[0].levels.size(), size_t(1));
  const UnitTypeDef& fighter = db.unitTypes[(size_t)db.findUnitType("fighter")];
  CHECK_EQ(fighter.abilities.size(), size_t(1));
  CHECK_EQ(db.unitTypes[(size_t)db.findUnitType("cavalier")].promotions.size(), size_t(1));
  int withSkill = 0;
  for (const UnitTypeDef& t : db.unitTypes) withSkill += !t.skills.empty();
  CHECK_EQ(withSkill, 2);
}
