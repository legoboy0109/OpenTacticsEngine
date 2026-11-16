// Step K2 of docs/SRPG.md: whole battles with every kind of skill, played by the AI on 20 seeds. The AI must never
// hang on the new turn-flag skills (actAgain, activations), and the same seed must play the same battle.
#include "core/ai/ai.h"
#include "srpg_skills_duel.h"

using namespace ote;
using namespace duel;

namespace {

// Every effect and every `when` the combat and timed part of K2 handles, spread over both sides.
std::string allSkills() {
  return skill("vigor", block("turnStart", "{ \"heal\": { \"percent\": 10 } }")) + "," +
         skill("sting", block("turnStart", "{ \"hurt\": 2, \"to\": \"enemies\", \"within\": 1 }", "", "40")) + "," +
         skill("brave", block("combat", "{ \"damage\": 2 }, { \"hit\": -5, \"to\": \"foe\" }", "{ \"initiating\": true }")) + "," +
         skill("grit", block("combat", "{ \"damageTaken\": -1 }, { \"strikeFirst\": true }",
                             "{ \"hp\": { \"op\": \"<=\", \"percent\": 50 } }")) + "," +
         skill("twin", block("combat", "{ \"multiStrike\": 2 }", "{ \"initiating\": true }")) + "," +
         skill("rally", "{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"within\": 2 }, \"do\": [ { \"damage\": 1 } ] }") + "," +
         skill("heavy", block("strike", "{ \"damagePercent\": 200 }", "", "\"a.skl\"")) + "," +
         skill("leech", block("strike", "{ \"drain\": 50 }", "", "30")) + "," +
         skill("again", block("strike", "{ \"strikeAgain\": true }", "", "25")) + "," +
         skill("hang", block("struck", "{ \"survive\": true }", "", "50")) + "," +
         skill("ward", block("struck", "{ \"damageTakenPercent\": 50 }", "", "30")) + "," +
         skill("charge", block("defeat", "{ \"actAgain\": true }")) + "," +
         skill("catch", block("defeat", "{ \"heal\": 5 }")) + "," +
         skill("jab", block("afterCombat", "{ \"hurt\": 3 }"));
}

struct Result {
  bool over = false;
  bool hung = false;
  std::string trace;
};

Result play(uint32_t seed) {
  Options o;
  o.seed = seed;
  o.aSkills = "\"vigor\", \"brave\", \"rally\", \"heavy\", \"again\", \"charge\", \"jab\", \"ward\"";
  o.dSkills = "\"sting\", \"grit\", \"twin\", \"leech\", \"hang\", \"catch\", \"jab\", \"rally\"";
  o.extraUnits = ", { \"type\": \"ua\", \"side\": \"blue\", \"x\": 0, \"y\": 0 }, { \"type\": \"ua\", \"side\": \"blue\", \"x\": 0, \"y\": 2 }"
                 ", { \"type\": \"ub\", \"side\": \"red\", \"x\": 7, \"y\": 0 }, { \"type\": \"ub\", \"side\": \"red\", \"x\": 7, \"y\": 2 }";
  o.gap = 6;
  Duel d(allSkills(), o);
  Battle& b = d.b();
  std::vector<Event> events;
  Result r;
  const long limit = 10L * (long)b.units.size() + 100;
  while (!b.over && b.round <= 60) {
    long steps = 0;
    while (aiTakeStep(b, events))
      if (++steps > limit) {
        r.hung = true;  // a turn must always end
        return r;
      }
  }
  for (const Event& e : events)
    r.trace += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," +
               std::to_string(e.c) + "," + std::to_string(e.d) + ";";
  r.trace += savetest::stateText(b);
  r.over = b.over;
  return r;
}

}  // namespace

TEST(skills_sim_battles_end_and_repeat_on_20_seeds) {
  int withSkillEvents = 0;
  for (uint32_t seed = 1; seed <= 20; ++seed) {
    const Result a = play(seed), b = play(seed);
    CHECK(!a.hung);
    CHECK(a.over);  // nobody stalls
    CHECK(a.trace == b.trace);
    withSkillEvents += a.trace.find(std::to_string((int)EventType::SkillActivated) + ":") != std::string::npos;
  }
  CHECK(withSkillEvents > 15);  // the skills really took part
}
