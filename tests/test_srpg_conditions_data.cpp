// Status conditions, the data (docs/SRPG.md section 9): the loader and its refusals, the data hash, and saves in the middle of a
// condition. Game: tests/fixtures/srpg-conditions.
#include "json_edit.h"
#include "srpg_conditions_fixture.h"

using namespace ote;
using namespace condtest;
using namespace jsonedit;

namespace {
// The load message when `from` (a piece of one file of the fixture) is replaced by `to`.
std::string fileError(const char* name, const char* file, const std::string& from, const std::string& to) {
  Copy c(name, "srpg-conditions");
  c.swap(file, from, to);
  return c.gameError();
}
std::string condError(const char* name, const std::string& from, const std::string& to) {
  return fileError(name, "data/conditions.json", from, to);
}
const std::string kC = "data/conditions.json: condition \"";

std::string restoreError(Field& w, const Json& saved) {
  Battle r;
  std::string err;
  if (!r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err)) return "init failed: " + err;
  return restoreBattleState(r, saved, err) ? "" : err;
}
Json saveOf(Field& w) {
  Json j;
  saveBattleState(w.b(), j);
  return j;
}
}  // namespace

TEST(conditions_the_fixture_loads_and_a_conditions_blocks_are_hidden_skills) {
  Copy c("c_load", "srpg-conditions");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK_EQ((int)db.conditions.size(), 19);
  const ConditionDef& poison = db.conditions[(size_t)db.findCondition("poison")];
  CHECK(poison.until == ConditionUntil::Turns && poison.turns == 3 && poison.tag == "Po");
  CHECK(db.conditions[(size_t)db.findCondition("mark")].until == ConditionUntil::Cured);
  CHECK(db.conditions[(size_t)db.findCondition("scar")].until == ConditionUntil::MapEnd);
  CHECK_EQ(db.conditions[(size_t)db.findCondition("fleeting")].wearOff, 99);
  CHECK(db.conditions[(size_t)db.findCondition("sleep")].endsWhenStruck);
  CHECK_EQ(db.conditions[(size_t)db.findCondition("chill")].replaces[0], db.findCondition("ember"));
  // The tag defaults to the first two letters of the name.
  CHECK_STR(db.conditions[(size_t)db.findCondition("deadly")].tag, "De");
  // The hidden skills: one per condition, one per weapon with "inflict"; the maker's four skills are the only visible ones.
  int hidden = 0;
  for (const SkillDef& s : db.skills) hidden += s.hidden;
  CHECK_EQ(hidden, 19 + 2);
  CHECK_STR(db.skills[(size_t)db.conditions[(size_t)db.findCondition("poison")].skill].id, "condition:poison");
  // A condition cannot be found as a skill by any id (no story command or learn entry can teach it).
  CHECK(db.findSkill("poison") < 0 && db.findSkill("condition:poison") < 0 && db.findSkill("weapon:venom_blade") < 0);
  const StaffDef& staff = db.staves[(size_t)db.items[(size_t)db.findItem("sleep_staff")].staff];
  CHECK(staff.effects[0].kind == StaffEffect::Kind::Inflict && staff.effects[0].turns == 2 && staff.area == StaffArea::Radius);
  const StaffDef& cure = db.staves[(size_t)db.items[(size_t)db.findItem("purify_staff")].staff];
  CHECK(cure.effects[0].kind == StaffEffect::Kind::Cure && cure.effects[0].cureAll);
  const ItemDef& antidote = db.items[(size_t)db.findItem("antidote")];
  CHECK(antidote.cures.size() == 1 && antidote.inflict < 0);
  const ItemDef& brew = db.items[(size_t)db.findItem("bad_brew")];
  CHECK(brew.inflict == db.findCondition("curse") && brew.inflictTurns == 2);
}

TEST(conditions_the_loader_refuses_a_bad_conditions_file_in_plain_words) {
  CHECK(saysIn(condError("c_l1", "\"id\": \"poison\", \"name\"", "\"id\": \"poison\", \"nom\": \"x\", \"name\""), kC + "poison\": unknown key \"nom\""));
  CHECK(saysIn(condError("c_l2", "\"id\": \"sleep\"", "\"id\": \"poison\""), kC + "poison\": duplicate id"));
  CHECK(saysIn(condError("c_l3", "\"tag\": \"Br\", \"turns\": 3,", "\"tag\": \"Br\","), "needs a duration"));
  CHECK(saysIn(condError("c_l4", "\"tag\": \"Br\", \"turns\": 3,", "\"tag\": \"Br\", \"turns\": 3, \"until\": \"cured\","), "has both \"turns\" and \"until\""));
  CHECK(saysIn(condError("c_l5", "\"tag\": \"Br\", \"turns\": 3,", "\"tag\": \"Br\", \"turns\": 0,"), "\"turns\" must be a whole number from 1 to 99"));
  CHECK(saysIn(condError("c_l6", "\"tag\": \"Br\", \"turns\": 3,", "\"tag\": \"Br\", \"turns\": 100,"), "\"turns\" must be a whole number from 1 to 99"));
  CHECK(saysIn(condError("c_l7", "\"until\": \"cured\"", "\"until\": \"forever\""), "\"until\" must be \"cured\" or \"mapEnd\""));
  CHECK(saysIn(condError("c_l8", "\"wearOff\": 99", "\"wearOff\": 100"), "\"wearOff\""));
  CHECK(saysIn(condError("c_l9", "\"wearOff\": 99", "\"wearOff\": 0"), "\"wearOff\""));
  CHECK(saysIn(condError("c_l10", "\"replaces\": [\"ember\"]", "\"replaces\": [\"nothing\"]"), "\"replaces\" names unknown condition \"nothing\""));
  CHECK(saysIn(condError("c_l11", "\"replaces\": [\"ember\"]", "\"replaces\": [\"chill\"]"), "\"replaces\" lists the condition itself"));
  CHECK(saysIn(condError("c_l12", "{ \"berserk\": true } ] }, { \"when\": \"always\", \"do\": [ { \"stats\": { \"str\": 3 } } ] } ] }",
                         "{ \"berserk\": true }, { \"cannotAct\": true } ] } ] }"), "\"cannotAct\" and \"berserk\" contradict each other"));
  CHECK(saysIn(condError("c_l13", "\"do\": [ { \"cannotMove\": true } ] } ] },\n  { \"id\": \"curse\"", "\"do\": [ { \"explode\": true } ] } ] },\n  { \"id\": \"curse\""), "is not an effect"));
  CHECK(saysIn(condError("c_l14", "{ \"stats\": { \"str\": -2, \"def\": -2 } }", "{ \"stats\": { \"hp\": -2 } }"), "a condition cannot change the hit points stat"));
  CHECK(saysIn(condError("c_l15", "{ \"hurt\": { \"percent\": 10 }, \"to\": \"self\" }", "{ \"hurt\": { \"percent\": 0 }, \"to\": \"self\" }"), "\"percent\" must be a whole number from 1 to 100"));
  CHECK(saysIn(condError("c_l16", "{ \"hurt\": { \"percent\": 10 }, \"to\": \"self\" }", "{ \"hurt\": { \"percent\": 10, \"canKill\": 1 }, \"to\": \"self\" }"), "\"canKill\" must be true or false"));
  CHECK(saysIn(condError("c_l17", "\"conditions\": [", "\"conditionz\": ["), "unknown key \"conditionz\""));
  CHECK(saysIn(condError("c_l18", "\"tag\": \"Po\"", "\"tag\": \"Poisoned\""), "\"tag\" is the short text shown on the map"));
  CHECK(saysIn(condError("c_l19", "{ \"when\": \"turnStart\", \"do\": [ { \"hurt\": { \"percent\": 10 }, \"to\": \"self\" } ] }", "{ \"when\": \"combat\", \"do\": [ { \"hurt\": 3, \"to\": \"self\" } ] }"), "\"hurt\" cannot be used in a \"combat\" block"));
  CHECK(saysIn(condError("c_l20", "{ \"forbid\": { \"kinds\": [\"staff\"], \"groups\": [\"tome\"] } }", "{ \"forbid\": { \"groups\": [\"wand\"] } }"), "\"forbid\" names \"wand\", which is no weapon or staff group of this game"));
  CHECK(saysIn(condError("c_l21", "{ \"forbid\": { \"kinds\": [\"staff\"], \"groups\": [\"tome\"] } }", "{ \"forbid\": { \"kinds\": [\"gem\"] } }"), "\"forbid\".\"kinds\" has \"gem\""));
  CHECK(saysIn(condError("c_l22", "{ \"forbid\": { \"kinds\": [\"staff\"], \"groups\": [\"tome\"] } }", "{ \"forbid\": {} }"), "\"forbid\" must name at least one"));
}

TEST(conditions_the_loader_refuses_conditions_named_wrongly_elsewhere) {
  const std::string w = "data/weapons.json: weapon \"";
  CHECK(saysIn(fileError("c_n1", "data/weapons.json", "\"condition\": \"poison\", \"chance\": 100", "\"condition\": \"plague\", \"chance\": 100"), w + "venom_blade\": \"inflict\" names unknown condition \"plague\""));
  CHECK(saysIn(fileError("c_n2", "data/weapons.json", "\"chance\": 100", "\"chance\": 0"), "\"inflict\": \"chance\" must be a percent from 1 to 100 or a formula"));
  CHECK(saysIn(fileError("c_n3", "data/weapons.json", "\"chance\": 100", "\"chance\": true"), "\"inflict\": \"chance\" must be a percent from 1 to 100 or a formula"));
  CHECK(saysIn(fileError("c_n4", "data/weapons.json", "\"inflict\": { \"condition\": \"poison\", \"chance\": 100, \"turns\": 2 }", "\"inflict\": { \"condition\": \"mark\", \"turns\": 2 }"), "does not count turns"));
  CHECK(saysIn(fileError("c_n5", "data/weapons.json", "\"chance\": 100, \"turns\": 2", "\"chance\": 100, \"rounds\": 2"), "unknown key \"rounds\""));
  const std::string i = "data/items.json: item \"";
  CHECK(saysIn(fileError("c_n6", "data/items.json", "\"cure\": [\"poison\"] },\n  { \"id\": \"tonic\"", "\"cure\": [\"plague\"] },\n  { \"id\": \"tonic\""), i + "antidote\": \"cure\" names unknown condition \"plague\""));
  CHECK(saysIn(fileError("c_n7", "data/items.json", "\"cure\": [\"poison\"] },\n  { \"id\": \"tonic\"", "\"cure\": \"everything\" },\n  { \"id\": \"tonic\""), "\"cure\" must be a list of condition ids, \"all\", \"harmful\" or \"helpful\""));
  CHECK(saysIn(fileError("c_n8", "data/items.json", "\"cure\": [\"poison\"] },\n  { \"id\": \"tonic\"", "\"cure\": [] },\n  { \"id\": \"tonic\""), "\"cure\" must be a list of condition ids"));
  CHECK(saysIn(fileError("c_n9", "data/items.json", "\"inflict\": { \"condition\": \"curse\", \"turns\": 2 }", "\"inflict\": { \"turns\": 2 }"), "\"inflict\" must be { \"condition\": \"poison\" }"));
  CHECK(saysIn(fileError("c_n10", "data/items.json", "{ \"id\": \"short_sword\", \"name\": \"Short Sword\", \"kind\": \"weapon\"", "{ \"id\": \"short_sword\", \"name\": \"Short Sword\", \"cure\": \"all\", \"kind\": \"weapon\""), "\"cure\" belongs to items of kind \"usable\""));
  CHECK(saysIn(fileError("c_n11", "data/items.json", "\"effects\": [ { \"inflict\": { \"condition\": \"curse\" } } ] }", "\"effects\": [ { \"inflict\": { \"condition\": \"curze\" } } ] }"), i + "hex_staff\": \"effects\" entry 1: \"inflict\" names unknown condition \"curze\""));
  CHECK(saysIn(fileError("c_n12", "data/items.json", "\"effects\": [ { \"inflict\": { \"condition\": \"curse\" } } ] }", "\"effects\": [ { \"inflict\": { \"condition\": \"curse\" } }, { \"inflict\": { \"condition\": \"poison\" } } ] }"), "only one \"inflict\""));
  CHECK(saysIn(fileError("c_n13", "data/items.json", "\"effects\": [ { \"cure\": [\"poison\"] } ] }", "\"effects\": [ { \"cure\": [\"poison\"] }, { \"cure\": \"all\" } ] }"), "only one \"cure\""));
  CHECK(saysIn(fileError("c_n14", "data/items.json", "\"uses\": 3, \"target\": \"self\",", "\"uses\": 3, \"target\": \"self\", \"area\": \"all\","), "\"area\" has no meaning with \"target\": \"self\""));
  CHECK(saysIn(fileError("c_n16", "data/skills.json", "{ \"immune\": [\"poison\", \"sleep\"] }", "{ \"immune\": [\"poison\", \"swoon\"] }"), "\"immune\" names unknown condition \"swoon\""));
  CHECK(saysIn(fileError("c_n17", "data/skills.json", "{ \"immune\": [\"poison\", \"sleep\"] }", "{ \"immune\": [] }"), "\"immune\" must name at least one condition"));
  CHECK(saysIn(fileError("c_n18", "data/skills.json", "{ \"inflict\": { \"condition\": \"curse\" } }", "{ \"inflict\": { \"condition\": \"curze\" } }"), "\"inflict\" names unknown condition \"curze\""));
  CHECK(saysIn(fileError("c_n19", "data/skills.json", "{ \"condition\": \"poison\" }", "{ \"condition\": \"plague\" }"), "\"condition\" names unknown condition \"plague\""));
  CHECK(saysIn(fileError("c_n20", "data/skills.json", "{ \"immune\": [\"poison\", \"sleep\"] }", "{ \"cannotAct\": true }"), "\"cannotAct\" belongs to the blocks of a condition"));
  CHECK(saysIn(fileError("c_n21", "data/skills.json", "\"when\": \"struck\", \"do\": [ { \"inflict\"", "\"when\": \"turnStart\", \"do\": [ { \"inflict\""), "\"inflict\" cannot be used in a \"turnStart\" block (it belongs to: strike, struck)"));
  CHECK(saysIn(fileError("c_n22", "data/skills.json", "\"when\": \"always\", \"do\": [ { \"immune\"", "\"when\": \"strike\", \"do\": [ { \"immune\""), "\"immune\" cannot be used in a \"strike\" block"));
}

TEST(conditions_a_game_without_conditions_loads_the_same_and_a_conditions_file_with_nothing_in_it_changes_no_hash) {
  Copy plain("c_hash_a", "srpg-staves2");
  Copy empty("c_hash_b", "srpg-staves2");
  empty.put("data/conditions.json", "{ \"conditions\": [] }");
  Database da, db;
  std::string err, ha, hb;
  CHECK(loadGame(plain.dir, da, err) && loadGame(empty.dir, db, err));
  CHECK(da.conditions.empty() && db.conditions.empty());
  CHECK(computeDataHash(da, "yard", ha, err) && computeDataHash(db, "yard", hb, err));
  CHECK_STR(ha, hb);
}

TEST(conditions_a_conditions_name_description_and_tag_are_cosmetic_but_its_rules_are_not) {
  auto hashOf = [](const char* name, const std::string& from, const std::string& to) {
    Copy c(name, "srpg-conditions");
    if (!from.empty()) c.swap("data/conditions.json", from, to);
    Database db;
    std::string err, h;
    CHECK(loadGame(c.dir, db, err));
    CHECK(computeDataHash(db, "field", h, err));
    return h;
  };
  const std::string base = hashOf("c_hash_c", "", "");
  CHECK_STR(hashOf("c_hash_d", "\"name\": \"Poison\"", "\"name\": \"Venom\""), base);
  CHECK_STR(hashOf("c_hash_e", "\"description\": \"Loses a little HP at the start of each of its turns.\"", "\"description\": \"x\""), base);
  CHECK_STR(hashOf("c_hash_f", "\"tag\": \"Po\"", "\"tag\": \"Pz\""), base);
  CHECK(hashOf("c_hash_g", "\"tag\": \"Po\", \"turns\": 3", "\"tag\": \"Po\", \"turns\": 4") != base);
}

// ---- saves ----

TEST(conditions_a_save_in_the_middle_of_a_condition_keeps_it_and_its_count_and_an_old_save_loads_with_none) {
  Field w("c_save");
  CHECK(!saveOf(w).has("units") || !saveOf(w)["units"][(size_t)0].has("conditions"));  // nothing afflicted: the save is what it was
  w.give("kay", "poison");
  w.give("kay", "mark");
  w.give("r1", "sleep", 2);
  w.endTurn();
  const Json saved = saveOf(w);
  const Json& kay = saved["units"][(size_t)w.id("kay")];
  CHECK(kay.has("conditions") && kay["conditions"].size() == 2);
  CHECK_EQ(kay["conditions"][(size_t)0]["condition"].asInt(), w.cond("poison"));
  CHECK_EQ(kay["conditions"][(size_t)0]["left"].asInt(), 3);
  CHECK(!kay["conditions"][(size_t)1].has("left"));  // until cured: no count
  Json reread;
  std::string err;
  CHECK(Json::parse(saved.dump(0), reread, err));
  Battle r;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restoreBattleState(r, reread, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK(r.units[(size_t)w.id("r1")].conditions[0].left == 1);
  // The count carries on after the reload exactly as in the original.
  std::vector<Event> ev;
  CHECK(r.apply(Action::endTurn(), ev));
  CHECK(w.run(Action::endTurn()));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  // An old save has no key: no conditions.
  Battle r2;
  CHECK(r2.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  Json old = saved;
  for (size_t i = 0; i < saved["units"].size(); ++i)
    if (saved["units"][i].has("conditions")) old = withoutAt(old, {"units", "#" + std::to_string(i), "conditions"});
  CHECK(restoreBattleState(r2, old, err));
  CHECK(r2.units[(size_t)w.id("kay")].conditions.empty());
}

TEST(conditions_a_damaged_conditions_list_in_a_save_is_refused_with_a_path) {
  Field w("c_save_damaged");
  w.give("kay", "poison");
  const Json good = saveOf(w);
  auto with = [&](const char* json) {
    Json bad;
    std::string err;
    CHECK(Json::parse(json, bad, err));
    return restoreError(w, withValue(good, {"units", "#" + std::to_string(w.id("kay")), "conditions"}, bad));
  };
  CHECK_STR(with("[ { \"condition\": 99, \"left\": 2 } ]"), "units[" + std::to_string(w.id("kay")) + "].conditions[0].condition: value 99 is outside 0..18");
  CHECK(saysIn(with("[ { \"condition\": 0, \"left\": 2 }, { \"condition\": 0, \"left\": 2 } ]"), "is listed twice"));
  CHECK(saysIn(with("[ { \"condition\": 0 } ]"), ".left: missing or not a number"));
  CHECK(saysIn(with("[ { \"condition\": 0, \"left\": 100 } ]"), ".left: value 100 is outside 0..99"));
  CHECK(saysIn(with("[ { \"condition\": 14, \"left\": 2 } ]"), ".left: this condition does not count turns"));  // mark: until cured
  CHECK(saysIn(with("[ 3 ]"), "not an object"));
  CHECK(saysIn(with("5"), "missing or not a list"));
}

TEST(conditions_a_save_of_a_game_without_conditions_refuses_the_key) {
  savetest::Rig rig;
  CHECK(rig.load(ote_test::sourceDir() + "/tests/fixtures/srpg-staves2", "yard"));
  rig.start();
  Json j;
  saveBattleState(rig.battle, j);
  Json bad;
  std::string err;
  CHECK(Json::parse("[ { \"condition\": 0, \"left\": 2 } ]", bad, err));
  Battle r;
  CHECK(r.init(rig.db, rig.map, rig.formulas, 1, err));
  CHECK(!restoreBattleState(r, withValue(j, {"units", "#0", "conditions"}, bad), err));
  CHECK(saysIn(err, "units[0].conditions: this game has no conditions"));
}
