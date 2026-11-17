// Status conditions in the player's words and rows (docs/SRPG.md section 9 "Player"): the text helpers, the unit details pages and the
// rows that must not show a condition's hidden skill. No window: these are the pieces the scene and the view call.
#include "battle/condition_text.h"
#include "battle/staff_text.h"
#include "battle/unit_info.h"
#include "battle/unit_info_rows.h"
#include "srpg_conditions_fixture.h"

using namespace ote;
using namespace condtest;

TEST(conditions_player_how_long_it_lasts_in_full_and_in_the_narrow_panels_words) {
  Field w("cp_duration");
  w.give("kay", "poison", 3);
  w.give("kay", "mark");
  w.give("kay", "scar");
  const Database& db = w.rig.db;
  const std::vector<ConditionState>& cs = w.unit("kay").conditions;
  CHECK_STR(conditionDurationText(db, cs[0]), "3 turns left");
  CHECK_STR(conditionDurationShort(db, cs[0]), "3");
  CHECK_STR(conditionDurationText(db, cs[1]), "Until cured");
  CHECK_STR(conditionDurationShort(db, cs[1]), "cure");
  CHECK_STR(conditionDurationText(db, cs[2]), "Until the map ends");
  CHECK_STR(conditionDurationShort(db, cs[2]), "map");
  ConditionState one = cs[0];
  one.left = 1;
  CHECK_STR(conditionDurationText(db, one), "1 turn left");
  one.left = 0;
  CHECK_STR(conditionDurationText(db, one), "last turn");
  CHECK_STR(conditionDurationShort(db, one), "ends");
}

TEST(conditions_player_floats_tags_and_the_reasons_a_unit_takes_no_orders) {
  Field w("cp_text");
  const Database& db = w.rig.db;
  CHECK_STR(conditionAppliedText(db, w.cond("poison")), "Poison!");
  CHECK_STR(conditionCuredText(db, w.cond("poison")), "Cured Poison");
  CHECK_STR(conditionEndedText(db, w.cond("poison"), kConditionRanOut), "Poison ended");
  CHECK_STR(conditionEndedText(db, w.cond("poison"), kConditionCured), "Cured Poison");
  CHECK_STR(conditionEndedText(db, w.cond("poison"), kConditionReplaced), "");
  CHECK_STR(conditionTickText(db, w.cond("berserk"), kTickActsAlone), "Berserk!");
  CHECK_STR(conditionTickText(db, w.cond("sleep"), kTickCannotAct), "Sleep");
  CHECK_STR(unitStoppedText(w.b(), w.unit("kay")), "");
  w.give("kay", "sleep");
  CHECK_STR(unitStoppedText(w.b(), w.unit("kay")), "Sleep: cannot act");
  CHECK_STR(unitStuckText(w.b(), w.unit("kay")), "Sleep: cannot move");
  w.unit("kay").conditions.clear();
  w.give("kay", "berserk");
  CHECK_STR(unitStoppedText(w.b(), w.unit("kay")), "Berserk: acts on its own");
  w.unit("kay").conditions.clear();
  w.give("kay", "stun");
  CHECK_STR(unitStoppedText(w.b(), w.unit("kay")), "");
  CHECK_STR(unitStuckText(w.b(), w.unit("kay")), "Stun: cannot move");
  w.give("kay", "poison");
  const std::vector<std::string> tags = conditionTags(w.b(), w.unit("kay"));
  CHECK(tags.size() == 2 && tags[0] == "St" && tags[1] == "Po");
}

TEST(conditions_player_staff_words_cover_the_new_effects_and_reasons) {
  Field w("cp_staff");
  const Database& db = w.rig.db;
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("hex_staff")]), "Inflicts Curse");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("cure_staff")]), "Cures Poison");
  CHECK_STR(staffEffectText(db, db.items[(size_t)w.item("purify_staff")]), "Cures every condition");
  CHECK_STR(staffReasonText("nobody in reach has anything it cures", 0), "Nobody in reach to cure");
  CHECK_STR(staffReasonText("nobody in reach has anything it cures", 1), "Nobody to cure");
  CHECK_STR(staffReasonText("nobody in reach has anything it cures", 2), "No one");
  CHECK_STR(staffReasonText("nobody in reach can be affected", 1), "Nobody to affect");
  CHECK_STR(staffReasonText("Silence stops that", 0), "Silence stops that");  // a condition's own words pass through
  CHECK_STR(staffWhyText("that unit is immune to it"), "Immune");
  CHECK_STR(staffWhyText("that unit has it already"), "Already affected");
  CHECK_STR(staffWhyText("that unit has nothing it cures"), "Nothing to cure");
  // The forecast lines.
  w.place("r1", 3, 2);
  w.give("kay", "poison");
  w.give("kay", "curse");
  std::vector<std::string> a = staffConditionLines(w.b(), w.forecast("mira", "hex_staff", "r1").hits[0]);
  CHECK(a.size() == 1 && a[0] == "Rook  Curse 3 turns");
  std::vector<std::string> c = staffConditionLines(w.b(), w.forecast("nell", "cure_staff", "kay").hits[0]);
  CHECK(c.size() == 1 && c[0] == "Kay  Cures Poison");
}

TEST(conditions_player_hidden_skills_stay_out_of_the_skill_rows_and_the_status_page_shows_only_for_an_afflicted_unit) {
  Field w("cp_pages");
  w.unit("kay").learned.push_back(w.skill("wary"));
  w.give("kay", "poison");
  w.give("kay", "curse");
  std::vector<int> all;
  w.b().skillsOf(w.unit("kay"), -1, all);
  CHECK(all.size() >= 3);  // wary and the two conditions' hidden skills
  const std::vector<SkillRow> rows = skillRows(w.b(), w.unit("kay"));
  CHECK_EQ((int)rows.size(), 1);
  CHECK(rows.size() == 1 && rows[0].name == "Wary");
  UnitInfoPages pages = buildUnitInfoPages(w.b(), w.id("kay"));
  std::string titles;
  for (const UnitInfoPage& p : pages) titles += p.title + ",";
  CHECK(titles.find("Status") != std::string::npos && titles.find("Skills") != std::string::npos);
  UnitInfoPages healthy = buildUnitInfoPages(w.b(), w.id("mira"));
  titles.clear();
  for (const UnitInfoPage& p : healthy) titles += p.title + ",";
  CHECK(titles.find("Status") == std::string::npos);
}
