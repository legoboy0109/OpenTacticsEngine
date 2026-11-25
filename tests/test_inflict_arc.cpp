// `arc` in an ability's "inflict" "chance" formula (docs/SRPG.md "Ability conditions", "Facing"): 0 front, 1 side, 2 back of the target as seen from
// the square the ability is used from; 0 on the user itself and in a game without facing. Game: tests/ability_conditions_fixture.h.
#include "ability_conditions_fixture.h"

using namespace ote;
using namespace acf;

namespace {

Opts arcOpts(bool facing) {
  Opts o;
  o.abilities = "{\"id\":\"stab\",\"name\":\"Stab\",\"target\":\"enemy\",\"range\":[1,3],\"inflict\":{\"condition\":\"sleep\",\"chance\":\"arc * 40 + 10\"}},"
                "{\"id\":\"calm\",\"name\":\"Calm\",\"target\":\"unit\",\"range\":[0,3],\"inflict\":{\"condition\":\"haste\",\"chance\":\"arc * 40 + 10\"}}";
  o.mageAbilities = "[\"stab\",\"calm\"]";
  if (facing) o.rules = "\"facing\":{},";
  // The bandit at 4,1 faces west: from 3,1 is its front, from 4,0 its side, from 5,1 its back.
  o.units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":4,\"y\":1" +
            std::string(facing ? ",\"facing\":\"west\"" : "") + "}";
  return o;
}

}  // namespace

TEST(inflict_arc_reads_the_targets_arc_from_the_users_square) {
  World w(make("arc_on", arcOpts(true)));
  const int hero = w.id("hero"), bandit = w.id("bandit"), stab = w.ability("stab");
  const Grid& g = w.b().grid;
  CHECK_EQ(w.b().abilityInflictChance(hero, stab, bandit, g.at(3, 1)), 10);  // front
  CHECK_EQ(w.b().abilityInflictChance(hero, stab, bandit, g.at(4, 0)), 50);  // side
  CHECK_EQ(w.b().abilityInflictChance(hero, stab, bandit, g.at(5, 1)), 90);  // back
  // On the user itself: the front.
  CHECK_EQ(w.b().abilityInflictChance(hero, w.ability("calm"), hero, w.unit("hero").cell), 10);
}

TEST(inflict_arc_is_0_without_facing) {
  World w(make("arc_off", arcOpts(false)));
  const Grid& g = w.b().grid;
  CHECK_EQ(w.b().abilityInflictChance(w.id("hero"), w.ability("stab"), w.id("bandit"), g.at(5, 1)), 10);
}

TEST(inflict_arc_in_the_battle_and_twice_the_same) {
  // From behind (90 %), twice with the same seed: the same rolls and the same result.
  auto play = [](const char* name) {
    Opts o = arcOpts(true);
    o.units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":5,\"y\":1},"
              "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":4,\"y\":1,\"facing\":\"west\"}";
    World w(make(name, o), 7);
    CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("stab"), w.id("bandit"))));
    return std::to_string(w.has("bandit", "sleep")) + "/" + std::to_string(w.b().rng.state) + "/" + std::to_string(w.events.size());
  };
  CHECK_STR(play("arc_twice_a"), play("arc_twice_b"));
}
