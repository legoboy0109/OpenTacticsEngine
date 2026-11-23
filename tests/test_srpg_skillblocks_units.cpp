// Docs/SRPG.md "More skill blocks", third part: cure from a skill, a map unit with skills of its own, class skills that go away on
// promotion. The fixture tests/fixtures/srpg-skillblocks (scout promotes to ranger; conditions poison and curse).
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks";

std::string skill(const std::string& id, const std::string& when, const std::string& dos) {
  return "{ \"id\": \"" + id + "\", \"blocks\": [ { \"when\": \"" + when + "\", \"do\": [" + dos + "] } ] }";
}

}  // namespace

TEST(skillblocks_cure_at_the_start_of_the_turn) {
  Copy c("sb_cure_run", kFix);
  c.skills(skill("clean", "turnStart", "{ \"cure\": [\"poison\"] }") + "," + skill("clean_all", "turnStart", "{ \"cure\": \"all\" }"));
  World w("field", c.dir);
  Battle& b = w.b();
  const int poison = w.db().findCondition("poison"), curse = w.db().findCondition("curse");
  ConditionState ps, cs;
  ps.condition = poison; ps.left = 3;
  cs.condition = curse; cs.left = 3;
  b.units[0].conditions.push_back(ps);
  b.units[0].conditions.push_back(cs);
  b.units[0].learned.push_back(w.skill("clean"));
  CHECK(w.apply(Action::endTurn()));  // red's turn, then blue's again: its start cures the poison
  CHECK(w.apply(Action::endTurn()));
  CHECK(!b.hasCondition(b.units[0], poison));
  CHECK(b.hasCondition(b.units[0], curse));  // only the listed condition
  b.units[0].learned.clear();
  b.units[0].learned.push_back(w.skill("clean_all"));
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));
  CHECK(!b.hasCondition(b.units[0], curse));
}

TEST(skillblocks_cure_refusals) {
  Copy c("sb_cure", kFix);
  c.skills(skill("x", "turnStart", "{ \"cure\": [\"nothing\"] }"));
  CHECK(saysIn(c.gameError(), "\"cure\" names unknown condition \"nothing\""));
  c.skills(skill("x", "combat", "{ \"cure\": \"all\" }"));
  CHECK(saysIn(c.gameError(), "cannot be used in a \"combat\" block"));
  c.skills(skill("x", "turnStart", "{ \"cure\": [] }"));
  CHECK(saysIn(c.gameError(), "\"cure\" must be \"all\", \"harmful\", \"helpful\" or a list of condition ids"));
}

TEST(skillblocks_map_units_and_addunit_take_skills_of_their_own) {
  Copy c("sb_unitskills", kFix);
  c.swap("maps/field.json", "{ \"type\": \"guard\", \"side\": \"red\", \"x\": 1, \"y\": 1 }",
         "{ \"type\": \"guard\", \"side\": \"red\", \"x\": 1, \"y\": 1, \"skills\": [\"rank_skill\"] }");
  {
    World w("field", c.dir);
    CHECK_STR(w.skillIds(w.b().units[1]), "rank_skill");
    CHECK_STR(w.skillIds(w.b().units[0]), "");
    CHECK_EQ(w.b().units[1].learned.size(), (size_t)1);  // kept like a learned skill: saved with the unit
  }
  c.swap("maps/field.json", "\"skills\": [\"rank_skill\"]", "\"skills\": [\"no_such_skill\"]");
  CHECK(saysIn(c.mapError("field"), "\"skills\" names unknown skill \"no_such_skill\""));
}

TEST(skillblocks_class_skills_can_go_away_on_promotion) {
  auto run = [](bool replace) {
    Copy c("sb_classskills", kFix);
    if (replace) c.swap("game.json", "\"rescue\"", "\"promotion\": { \"classSkills\": \"replace\" }, \"rescue\"");
    World w("field", c.dir);
    Unit& u = w.b().units[0];
    u.level = 2;
    u.learned.push_back(w.skill("plain_skill"));  // what the scout class taught at level 2
    CHECK(w.apply(Action::promote(0)));
    return w.learnedIds(w.b().units[0]);
  };
  CHECK_STR(run(false), "plain_skill rank_skill");  // today's behaviour
  CHECK_STR(run(true), "rank_skill");               // the old class's skill is gone, the new class's is there
  Copy c("sb_classskills_bad", kFix);
  c.swap("game.json", "\"rescue\"", "\"promotion\": { \"classSkills\": \"both\" }, \"rescue\"");
  CHECK(saysIn(c.gameError(), "\"classSkills\" must be \"keep\""));
}
