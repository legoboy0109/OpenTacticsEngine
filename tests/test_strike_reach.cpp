// A strike ability whose reach follows the user's weapon (docs/SRPG.md "Abilities that strike"): "effect": { "strike": w, "reach": "equipped" }
// strikes with w's damage but from the range of the weapon in the user's hand (w's own range for a unit with none). Options, the refusal, the
// computer opponent, the loader. Game: tests/ability_conditions_fixture.h (the iron sword reaches 1; fire_spell 1..3).
#include "ability_conditions_fixture.h"

using namespace ote;
using namespace acf;

namespace {

Opts reachOpts(const char* heroItems = nullptr) {
  Opts o;
  o.abilities = "{\"id\":\"scorch\",\"name\":\"Scorch\",\"effect\":{\"strike\":\"fire_spell\"}},"
                "{\"id\":\"rend\",\"name\":\"Rend\",\"effect\":{\"strike\":\"fire_spell\",\"reach\":\"equipped\"}}";
  o.mageAbilities = "[\"scorch\",\"rend\"]";
  o.units = std::string("{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1") + (heroItems ? heroItems : "") +
            "},{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1}";
  return o;
}

bool reaches(World& w, const char* ability, int target) {
  std::vector<AttackOption> opts;
  w.b().abilityStrikeOptions(w.id("hero"), w.ability(ability), w.unit("hero").cell, opts);
  for (const AttackOption& a : opts)
    if (a.target == target) return true;
  return false;
}

std::string withAbility(const char* name, const std::string& ability) {
  Opts o = reachOpts();
  o.abilities += "," + ability;
  return error(make(name, o));
}

}  // namespace

TEST(strike_reach_follows_the_weapon_in_hand) {
  World w(make("reach_sword", reachOpts()));
  const int bandit = w.id("bandit");
  CHECK(reaches(w, "scorch", bandit));  // fire_spell's own 1..3 reaches 2 squares
  CHECK(!reaches(w, "rend", bandit));   // the sword in hand reaches 1
  CHECK(!w.why(Action::useAbility(w.id("hero"), w.ability("rend"), bandit)).empty());
  // Next to it, it strikes with fire_spell's damage (mag 5 - res 0).
  World v(make("reach_near", [] {
    Opts o = reachOpts();
    o.units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":2,\"y\":1}";
    return o;
  }()));
  CHECK(reaches(v, "rend", v.id("bandit")));
  const int hp = v.unit("bandit").hp;
  CHECK(v.run(Action::useAbility(v.id("hero"), v.ability("rend"), v.id("bandit"))));
  CHECK_EQ(v.unit("bandit").hp, hp - 5);
}

TEST(strike_reach_without_a_weapon_is_the_strike_weapons_own) {
  World w(make("reach_bare", reachOpts(",\"items\":[]")));
  CHECK(reaches(w, "rend", w.id("bandit")));
}

TEST(strike_reach_leaves_the_weapon_alone_after) {
  World w(make("reach_after", reachOpts()));
  std::vector<AttackOption> opts;
  w.b().abilityStrikeOptions(w.id("hero"), w.ability("rend"), w.unit("hero").cell, opts);
  // After the scope, fire_spell reaches as far as ever: the plain strike still gets the bandit 2 squares off.
  CHECK(reaches(w, "scorch", w.id("bandit")));
}

TEST(strike_reach_the_computer_uses_it_only_in_reach_and_the_same_twice) {
  auto play = [](const char* name) {
    Opts o = reachOpts();
    o.mageAbilities = "[\"rend\"]";
    o.blueAi = true;
    World w(make(name, o), 5);
    int guard = 0;
    while (!w.b().over && ++guard < 400)
      if (!aiTakeStep(w.b(), w.events)) break;
    return std::to_string(w.events.size()) + "/" + std::to_string(w.b().rng.state) + "/" + std::to_string(w.b().winnerTeam);
  };
  CHECK_STR(play("reach_ai_a"), play("reach_ai_b"));
}

TEST(strike_reach_loader_refusals) {
  CHECK_STR(error(make("reach_load", reachOpts())), "");
  CHECK(says(withAbility("reach_r1", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[1,2],\"effect\":{\"heal\":\"5\",\"reach\":\"equipped\"}}"),
             "\"effect\": \"reach\" goes with \"strike\""));
  CHECK(says(withAbility("reach_r2", "{\"id\":\"x\",\"effect\":{\"strike\":\"fire_spell\",\"reach\":\"far\"}}"),
             "\"effect\": \"reach\" must be \"equipped\""));
}
