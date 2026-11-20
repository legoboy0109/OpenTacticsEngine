// Branching research and side-wide modifiers (docs/ECONOMY.md "Branching research", docs/SRPG.md "Side-wide modifiers"). Fixture
// tests/fixtures/research-branches: blue (side 0, 500 gold, a lab) and red (side 1, a lab), a neutral forge; technologies
// tactics (50, 1 turn) -> shield_wall / skirmish (100, 2 turns, one doctrine: exclusive), trade (60, +30 gold a turn),
// smelting (needs trade and a forge). Soldiers are tagged "infantry", archers "ranged". The rule refunds 50 % on cancel.
#include "battle/unit_info_rows.h"
#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "core/battle/save_research.h"
#include "json_edit.h"
using namespace jsonedit;
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::saysIn;
using skilltest::World;

namespace {

const char* kFix = "research-branches";

std::string fixtureDir() { return ote_test::sourceDir() + "/tests/fixtures/research-branches"; }

int tech(World& w, const char* id) { return w.db().findTechnology(id); }

// One full round: the active side ends its turn, then the other does. The research of the first counts down.
void round(World& w) {
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));
}

bool start(World& w, const char* id) { return w.apply(Action::research(tech(w, id))); }

// Researches and finishes a technology for blue (waiting out its turns).
void finish(World& w, const char* id) {
  CHECK(start(w, id));
  for (int i = 0; i < 4 && !w.b().hasTechnology(0, tech(w, id)); ++i) round(w);
  CHECK(w.b().hasTechnology(0, tech(w, id)));
}

std::string whyNot(World& w, const char* id) {
  std::vector<ResearchOption> opts;
  w.b().researchOptions(w.b().activeSide, opts);
  for (const ResearchOption& o : opts)
    if (o.tech == tech(w, id)) return o.ok ? "" : o.why;
  return "(not listed)";
}

}  // namespace

// ---- loader ----

TEST(research_branches_fixture_loads_with_every_key) {
  Copy c("rb_load", kFix);
  CHECK_STR(c.gameError(), "");
  World w("field", fixtureDir());
  const Database& db = w.db();
  const TechnologyDef& wall = db.research[(size_t)tech(w, "shield_wall")];
  CHECK_STR(wall.tree, "Military");
  CHECK_EQ(wall.tier, 2);
  CHECK_STR(wall.exclusiveGroup, "doctrine");
  CHECK_EQ((int)wall.grants.size(), 1);
  CHECK_EQ(wall.grants[0].skill, w.skill("bulwark"));
  CHECK_STR(wall.grants[0].filterTag, "infantry");
  CHECK_EQ(db.research[(size_t)tech(w, "skirmish")].grants[0].filterType, db.findUnitType("archer"));
  const TechnologyDef& smelt = db.research[(size_t)tech(w, "smelting")];
  CHECK_EQ((int)smelt.requiresStructures.size(), 1);
  CHECK_EQ(smelt.requiresStructures[0], db.findStructure("forge"));
  CHECK_EQ(smelt.grants[0].filterType, -1);  // a plain skill id: every unit of the side
  CHECK_EQ((int)db.research[(size_t)tech(w, "trade")].bonusIncome.size(), 1);
  CHECK_EQ(db.rules.researchCancelRefundPercent, 50);
}

TEST(research_branches_loader_refuses_bad_data_naming_file_entry_and_key) {
  auto refuses = [](const char* name, const char* file, const char* from, const char* to, const char* want) {
    Copy c(name, kFix);
    c.swap(file, from, to);
    CHECK(saysIn(c.gameError(), want));
  };
  refuses("rb_skill", "data/research.json", "\"skill\": \"bulwark\", \"filter\": { \"tag\"", "\"skill\": \"nope\", \"filter\": { \"tag\"",
          "technology \"shield_wall\": \"grants\".\"skills\": names unknown skill \"nope\"");
  refuses("rb_struct", "data/research.json", "[\"forge\"]", "[\"castle\"]",
          "research.json: technology \"smelting\": \"requiresStructures\" names unknown structure \"castle\"");
  refuses("rb_tier", "data/research.json", "\"tier\": 1, \"cost\": { \"gold\": 50 }", "\"tier\": 1.5, \"cost\": { \"gold\": 50 }",
          "technology \"tactics\": \"tier\" must be a whole number from 0 to 99");
  refuses("rb_res", "data/research.json", "\"income\": { \"gold\": 30 }", "\"income\": { \"mana\": 30 }",
          "technology \"trade\": \"bonus\": \"income\" names unknown resource \"mana\"");
  refuses("rb_ftype", "data/research.json", "\"type\": \"archer\"", "\"type\": \"mage\"", "\"filter\".\"type\" names unknown unit type \"mage\"");
  refuses("rb_ftag", "data/research.json", "\"tag\": \"infantry\"", "\"tag\": \"mounted\"", "\"filter\".\"tag\" \"mounted\" is not a tag of any unit type");
  refuses("rb_fkey", "data/research.json", "\"type\": \"archer\"", "\"side\": \"blue\"", "\"filter\" has unknown key \"side\"");
  refuses("rb_gkey", "data/research.json", "{ \"skills\": [ { \"skill\": \"fleet\"", "{ \"skillz\": [ { \"skill\": \"fleet\"",
          "\"grants\": unknown key \"skillz\"");
  refuses("rb_tree", "data/research.json", "\"tree\": \"Economy\", \"tier\": 1", "\"tree\": \"\", \"tier\": 1", "\"tree\" must not be empty text");
  refuses("rb_eco", "game.json", "\"economy\": { \"enabled\": true },", "", "\"bonus\".\"income\" needs \"rules.economy\"");
  refuses("rb_pct", "game.json", "\"cancelRefundPercent\": 50", "\"cancelRefundPercent\": 150",
          "rules.research: \"cancelRefundPercent\" must be a whole number from 0 to 100");
  refuses("rb_rkey", "game.json", "\"cancelRefundPercent\": 50", "\"cancelRefund\": 50", "rules.research: unknown key \"cancelRefund\"");
  // A group of one is fine (the only entry of its group closes nothing).
  Copy one("rb_group_one", kFix);
  one.swap("data/research.json", "\"exclusiveGroup\": \"doctrine\",\n    \"cost\": { \"gold\": 100 }, \"turns\": 2, \"grants\": { \"skills\": [ { \"skill\": \"fleet\"",
           "\"exclusiveGroup\": \"alone\",\n    \"cost\": { \"gold\": 100 }, \"turns\": 2, \"grants\": { \"skills\": [ { \"skill\": \"fleet\"");
  CHECK_STR(one.gameError(), "");
}

TEST(research_without_the_new_keys_is_a_plain_technology) {
  Copy c("rb_plain", kFix);
  c.put("data/research.json", "{ \"technologies\": [ { \"id\": \"a\", \"cost\": { \"gold\": 10 }, \"turns\": 1 } ] }");
  CHECK_STR(c.gameError(), "");
  World w("field", c.dir);
  CHECK(w.db().research[0].tree.empty() && w.db().research[0].grants.empty() && w.db().research[0].bonusIncome.empty());
  Json j;
  saveBattleState(w.b(), j);
  CHECK(!j["sides"][size_t(0)].has("modifiers") && !j["sides"][size_t(0)].has("research"));
  finish(w, "a");
  saveBattleState(w.b(), j);
  CHECK(!j["sides"][size_t(0)].has("modifiers"));  // nothing granted: nothing written
}

// ---- exclusive branches, structures, prerequisites ----

TEST(exclusive_group_closes_the_other_branch_for_that_side_only) {
  World w("field", fixtureDir());
  finish(w, "tactics");
  CHECK_STR(whyNot(w, "shield_wall"), "");
  CHECK_STR(whyNot(w, "skirmish"), "");
  CHECK(start(w, "shield_wall"));
  // Started is enough to close the sibling.
  round(w);  // (blue's turn again, shield_wall under way)
  CHECK(saysIn(whyNot(w, "skirmish"), "closed: you chose Shield Wall"));
  CHECK(saysIn(w.refusal(Action::research(tech(w, "skirmish"))), "closed"));
  CHECK(saysIn(w.refusal(Action::research(tech(w, "trade"))), "already researching"));  // one at a time as well
  // Red never chose: red's own turn could still take either (it lacks tactics, so only that reason shows).
  CHECK(w.apply(Action::endTurn()));
  CHECK(saysIn(whyNot(w, "skirmish"), "needs Tactics"));
  CHECK(w.apply(Action::endTurn()));
  while (!w.b().hasTechnology(0, tech(w, "shield_wall"))) round(w);
  CHECK(saysIn(whyNot(w, "skirmish"), "closed: you chose Shield Wall"));  // done keeps it closed
  CHECK(saysIn(w.refusal(Action::research(tech(w, "skirmish"))), "closed"));
}

TEST(a_cancelled_branch_choice_opens_the_group_again) {
  World w("field", fixtureDir());
  finish(w, "tactics");
  CHECK(start(w, "shield_wall"));
  CHECK(w.apply(Action::cancelResearch()));
  CHECK_STR(whyNot(w, "skirmish"), "");
  CHECK_STR(whyNot(w, "shield_wall"), "");
  CHECK(start(w, "skirmish"));
}

TEST(required_structures_must_be_owned_by_the_side) {
  World w("field", fixtureDir());
  finish(w, "trade");
  CHECK(saysIn(whyNot(w, "smelting"), "needs a Forge"));
  const int forge = w.db().findStructure("forge");
  for (StructureState& s : w.b().structures)
    if (s.type == forge) s.owner = 1;  // the enemy owns it: still not ours
  CHECK(saysIn(whyNot(w, "smelting"), "needs a Forge"));
  for (StructureState& s : w.b().structures)
    if (s.type == forge) s.owner = 0;
  CHECK_STR(whyNot(w, "smelting"), "");
  CHECK(start(w, "smelting"));
}

// ---- cancel ----

TEST(cancel_returns_the_share_and_forgets_the_progress) {
  World w("field", fixtureDir());
  const int before = w.b().sides[0].resources[0];
  CHECK_STR(w.refusal(Action::cancelResearch()), "no research is under way");
  CHECK(start(w, "shield_wall") == false);  // needs tactics
  finish(w, "tactics");
  const int gold = w.b().sides[0].resources[0];
  CHECK_EQ(gold, before - 50);
  CHECK(start(w, "shield_wall"));
  CHECK_EQ(w.b().sides[0].resources[0], gold - 100);
  std::vector<Amount> back;
  w.b().cancelRefund(0, back);
  CHECK(back.size() == 1 && back[0].amount == 50);
  CHECK(w.apply(Action::cancelResearch()));
  CHECK_EQ(w.count(EventType::ResearchCancelled), 1);
  CHECK_EQ(w.b().researching(0), -1);
  CHECK_EQ(w.b().sides[0].resources[0], gold - 50);  // 50 % of 100 back
  CHECK(!w.b().hasTechnology(0, tech(w, "shield_wall")));
  CHECK(saysIn(w.refusal(Action::cancelResearch()), "no research is under way"));
}

TEST(cancel_refund_rounds_down_and_defaults_to_nothing) {
  Copy c("rb_refund", kFix);
  c.swap("game.json", "\"cancelRefundPercent\": 50", "\"cancelRefundPercent\": 33");
  World w("field", c.dir);
  CHECK(start(w, "tactics"));  // 50 gold: 33 % = 16.5 -> 16
  const int gold = w.b().sides[0].resources[0];
  CHECK(w.apply(Action::cancelResearch()));
  CHECK_EQ(w.b().sides[0].resources[0], gold + 16);
  Copy none("rb_norefund", kFix);
  none.swap("game.json", ",\n             \"research\": { \"cancelRefundPercent\": 50 }", "");
  World w2("field", none.dir);
  CHECK_EQ(w2.db().rules.researchCancelRefundPercent, 0);
  CHECK(start(w2, "tactics"));
  const int g2 = w2.b().sides[0].resources[0];
  CHECK(w2.apply(Action::cancelResearch()));
  CHECK_EQ(w2.b().sides[0].resources[0], g2);
  CHECK_EQ(w2.count(EventType::ResourceChanged), 0);
}

TEST(only_the_active_side_cancels_and_the_battle_must_be_running) {
  World w("field", fixtureDir());
  CHECK(start(w, "tactics"));
  CHECK(w.apply(Action::endTurn()));  // red's turn: red has nothing under way
  CHECK(saysIn(w.refusal(Action::cancelResearch()), "no research is under way"));
  CHECK_EQ(w.b().researching(0), tech(w, "tactics"));
}

// ---- income ----

TEST(finished_research_adds_to_the_per_turn_income) {
  World w("field", fixtureDir());
  finish(w, "trade");  // finished as blue's turn started (income is counted before research finishes)
  const int gold = w.b().sides[0].resources[0];
  round(w);
  CHECK_EQ(w.b().sides[0].resources[0], gold + 30);
  CHECK_EQ(w.b().sides[1].resources[0], 500);  // red has no such technology
  std::vector<int> inc(1, 0);
  w.b().researchIncome(0, inc);
  CHECK_EQ(inc[0], 30);
}

// ---- side-wide modifiers ----

TEST(granted_skills_apply_to_matching_units_of_the_side_only) {
  World w("field", fixtureDir());
  finish(w, "tactics");
  const int def0 = w.stat(w.unit("sol"), "def");
  CHECK_STR(w.skillIds(w.unit("sol")), "");
  finish(w, "shield_wall");
  CHECK_STR(w.skillIds(w.unit("sol")), "bulwark");   // an infantry unit of blue
  CHECK_STR(w.skillIds(w.unit("arc")), "");          // the archer is no infantry
  CHECK_STR(w.skillIds(w.unit("foe")), "");          // red did not research it
  CHECK_EQ(w.stat(w.unit("sol"), "def"), def0 + 2);  // and it works like the unit's own skill
  CHECK(w.b().hasSkill(w.unit("sol"), w.skill("bulwark")));
}

TEST(a_type_filter_and_an_unfiltered_grant) {
  World w("field", fixtureDir());
  finish(w, "tactics");
  const int mov0 = w.stat(w.unit("arc"), "mov");
  finish(w, "skirmish");
  CHECK_STR(w.skillIds(w.unit("arc")), "fleet");
  CHECK_STR(w.skillIds(w.unit("sol")), "");
  CHECK_EQ(w.stat(w.unit("arc"), "mov"), mov0 + 1);
  finish(w, "trade");
  for (StructureState& s : w.b().structures)
    if (s.type == w.db().findStructure("forge")) s.owner = 0;
  finish(w, "smelting");  // no filter: every unit of the side
  CHECK_STR(w.skillIds(w.unit("arc")), "fleet bulwark");
  CHECK_STR(w.skillIds(w.unit("sol")), "bulwark");
}

TEST(a_unit_that_has_the_skill_anyway_counts_it_once_and_other_sources_keep_their_place) {
  World w("field", fixtureDir());
  Unit& sol = w.unit("sol");
  sol.learned.push_back(w.skill("fleet"));
  finish(w, "tactics");
  finish(w, "shield_wall");
  CHECK_STR(w.skillIds(sol), "fleet bulwark");  // learned first, the side's modifier after
  sol.learned.push_back(w.skill("bulwark"));
  CHECK_STR(w.skillIds(sol), "fleet bulwark");  // listed once
}

TEST(unit_details_mark_the_side_as_the_origin) {
  World w("field", fixtureDir());
  finish(w, "tactics");
  finish(w, "shield_wall");
  std::vector<SkillRow> rows = skillRows(w.b(), w.unit("sol"));
  CHECK_EQ((int)rows.size(), 1);
  CHECK_STR(skillSourceName(rows[0].source), "Side");
  CHECK_STR(rows[0].name, "Bulwark");
}

TEST(a_modifier_source_added_later_goes_through_the_same_door) {
  World w("field", fixtureDir());
  SideModifier m;  // what a later "factions" feature would do: add its own modifier to the side
  m.skill = w.skill("fleet");
  w.b().addSideModifier(0, m);
  w.b().addSideModifier(0, m);  // the same one again: no duplicate
  CHECK_EQ((int)w.b().sides[0].modifiers.size(), 1);
  CHECK_STR(w.skillIds(w.unit("arc")), "fleet");
  CHECK_STR(w.skillIds(w.unit("foe")), "");
}

TEST(campaign_carry_over_gives_the_same_modifiers) {
  World v("field", fixtureDir(), false);
  v.b().setStartingResearch(0, {tech(v, "shield_wall")});  // what a campaign hands to the next chapter
  v.b().start(v.events);
  CHECK_STR(v.skillIds(v.unit("sol")), "bulwark");
}

// ---- saves ----

TEST(modifiers_are_saved_only_when_there_are_some_and_round_trip) {
  World w("field", fixtureDir());
  Json j;
  saveBattleState(w.b(), j);
  CHECK(!j["sides"][size_t(0)].has("modifiers"));
  finish(w, "tactics");
  finish(w, "shield_wall");
  saveBattleState(w.b(), j);
  CHECK(j["sides"][size_t(0)].has("modifiers"));
  CHECK(!j["sides"][size_t(1)].has("modifiers"));
  CHECK_EQ(j["sides"][size_t(0)]["modifiers"].size(), size_t(1));
  Json text;
  std::string err;
  CHECK(Json::parse(j.dump(0), text, err));
  Battle r;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err));
  CHECK(restoreBattleState(r, text, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK_EQ((int)r.sides[0].modifiers.size(), 1);
  CHECK(r.hasSkill(r.units[(size_t)w.id("sol")], w.skill("bulwark")));
  CHECK(!r.hasSkill(r.units[(size_t)w.id("arc")], w.skill("bulwark")));
}

TEST(damaged_modifier_saves_are_refused) {
  World w("field", fixtureDir());
  finish(w, "tactics");
  finish(w, "shield_wall");
  Json good;
  saveBattleState(w.b(), good);
  auto refusal = [&](const Json& bad) {
    Battle r;
    std::string err;
    CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err));
    return restoreBattleState(r, bad, err) ? std::string() : err;
  };
  CHECK_STR(refusal(good), "");
  using namespace jsonedit;
  const std::vector<std::string> mods = {"sides", "#0", "modifiers"};
  auto field = [&](const char* key) { return std::vector<std::string>{"sides", "#0", "modifiers", "#0", key}; };
  // The grant is lost: a finished technology must have its modifiers.
  CHECK(saysIn(refusal(withoutAt(good, mods)), "is finished but its granted skills are missing"));
  // A modifier of a technology that is not finished.
  CHECK(saysIn(refusal(withValue(good, field("tech"), num((double)tech(w, "skirmish")))), "is not finished"));
  // Another skill than the grant names.
  CHECK(saysIn(refusal(withValue(good, field("skill"), num((double)w.skill("fleet")))), "is another skill"));
  CHECK(saysIn(refusal(withValue(good, field("grant"), num(5))), "grant"));
}

// ---- the computer opponent ----

TEST(the_computer_values_income_and_skills_and_never_picks_a_closed_entry) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::endTurn()));  // red's turn
  CHECK_EQ(w.b().activeSide, 1);
  int pick = -1;
  CHECK(ai::chooseResearch(w.b(), pick));
  // Tactics costs 50; trade costs 60 but is worth 5 turns of +30: the income bonus wins.
  CHECK_EQ(pick, tech(w, "trade"));
  // With tactics done the two doctrines tie on price (100 - 10 for the skill): the lower index, shield wall, wins; and once
  // red has it, the other doctrine is never offered.
  w.b().sides[1].researched[(size_t)tech(w, "tactics")] = 1;
  w.b().sides[1].researched[(size_t)tech(w, "trade")] = 1;
  CHECK(ai::chooseResearch(w.b(), pick));
  CHECK_EQ(pick, tech(w, "shield_wall"));
  w.b().sides[1].researched[(size_t)tech(w, "shield_wall")] = 1;
  CHECK(!ai::chooseResearch(w.b(), pick));  // skirmish is closed, smelting needs a forge red does not own
}

TEST(the_computer_does_not_stall_when_everything_left_is_closed) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::endTurn()));
  SideState& red = w.b().sides[1];
  for (const char* id : {"tactics", "trade", "shield_wall"}) red.researched[(size_t)tech(w, id)] = 1;
  int pick = -1;
  // Left: skirmish (closed) and smelting (no forge). Nothing is picked; the turn goes on without research.
  CHECK(!ai::chooseResearch(w.b(), pick));
  CHECK(w.apply(Action::endTurn()));  // and the game still goes on
}

TEST(the_computer_plays_whole_games_with_branches_and_picks_only_legal_research) {
  World w("field", fixtureDir(), false);
  w.b().sides[0].ai = true;  // both sides computer-run
  std::vector<Event> ev;
  w.b().start(ev);
  for (int i = 0; i < 60 && !w.b().over; ++i) savetest::playAi(w.b(), 40, ev);
  // Whatever it did, a closed branch stayed closed: no side has both doctrines.
  for (int s = 0; s < 2; ++s) {
    const SideState& side = w.b().sides[(size_t)s];
    CHECK(!(side.researched[(size_t)tech(w, "shield_wall")] && side.researched[(size_t)tech(w, "skirmish")]));
  }
}
