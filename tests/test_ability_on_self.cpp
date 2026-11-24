// An ability on its user (docs/SRPG.md "An ability on its user"): a heal, cure or condition ability with "target" "ally" or "unit" whose "range" starts
// at 0 may be used on the user's own square as well as on others; with a range from 1 the user is refused as before. The loader's refusals, the
// computer opponent healing itself. Game: tests/ability_conditions_fixture.h.
#include <algorithm>

#include "ability_conditions_fixture.h"

using namespace ote;
using namespace acf;

namespace {

const char* kSelfAbilities =
    "{\"id\":\"mend\",\"name\":\"Mend\",\"target\":\"ally\",\"range\":[0,2],\"effect\":{\"heal\":\"10\"}},"
    "{\"id\":\"cure\",\"name\":\"Cure\",\"target\":\"unit\",\"range\":[1,2],\"effect\":{\"heal\":\"10\"}},"
    "{\"id\":\"quicken\",\"name\":\"Quicken\",\"target\":\"unit\",\"range\":[0,3],\"inflict\":{\"condition\":\"haste\"}},"
    "{\"id\":\"purge\",\"name\":\"Purge\",\"target\":\"ally\",\"range\":[0,1],\"effect\":{\"cure\":\"harmful\"}}";

Opts selfOpts() {
  Opts o;
  o.abilities = kSelfAbilities;
  o.mageAbilities = "[\"mend\",\"cure\",\"quicken\",\"purge\"]";
  return o;
}

std::string withAbility(const char* name, const std::string& ability) {
  Opts o = selfOpts();
  o.abilities += "," + ability;
  return error(make(name, o));
}

}  // namespace

TEST(self_ability_heals_its_user) {
  World w(make("self_heal", selfOpts()));
  const int hero = w.id("hero");
  w.unit("hero").hp = 10;
  w.unit("pal").hp = 10;
  std::vector<int> units;
  std::vector<Cell> cells;
  w.b().abilityTargets(hero, w.ability("mend"), w.unit("hero").cell, units, cells);
  CHECK(std::find(units.begin(), units.end(), hero) != units.end());     // the user ...
  CHECK(std::find(units.begin(), units.end(), w.id("pal")) != units.end());  // ... and the others in range
  CHECK(w.run(Action::useAbility(hero, w.ability("mend"), hero)));
  CHECK_EQ(w.unit("hero").hp, 20);
  // A condition and a cure on itself.
  World v(make("self_cond", selfOpts()));
  CHECK(v.run(Action::useAbility(v.id("hero"), v.ability("quicken"), v.id("hero"))));
  CHECK(v.has("hero", "haste"));
}

TEST(self_ability_with_a_range_from_1_still_refuses_the_user) {
  World w(make("self_refuse", selfOpts()));
  const int hero = w.id("hero");
  w.unit("hero").hp = 10;
  CHECK(says(w.why(Action::useAbility(hero, w.ability("cure"), hero)), "a unit cannot use that on itself"));
  std::vector<int> units;
  std::vector<Cell> cells;
  w.b().abilityTargets(hero, w.ability("cure"), w.unit("hero").cell, units, cells);
  CHECK(std::find(units.begin(), units.end(), hero) == units.end());
}

TEST(self_ability_the_computer_heals_itself) {
  Opts o = selfOpts();
  o.blueAi = true;
  o.units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1,\"behaviour\":\"hold\"},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":8,\"y\":2}";
  o.mageAbilities = "[\"mend\"]";
  World w(make("self_ai", o));
  w.unit("hero").hp = 5;
  int guard = 0;
  while (!w.b().over && w.b().activeSide == 0 && aiTakeStep(w.b(), w.events) && ++guard < 50) {
  }
  CHECK(w.unit("hero").hp > 5);
}

TEST(self_ability_loader_refusals) {
  CHECK_STR(error(make("self_load", selfOpts())), "");
  const char* onlyThese = "\"range\" may start at 0 (the user itself) only on a heal, cure or condition ability with \"target\" \"ally\" or \"unit\"";
  CHECK(says(withAbility("self_r1", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,2],\"inflict\":{\"condition\":\"sleep\"}}"), onlyThese));
  CHECK(says(withAbility("self_r2", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,2],\"effect\":{\"heal\":\"5\"}}"), "\""));
  CHECK(says(withAbility("self_r3", "{\"id\":\"x\",\"target\":\"unit\",\"range\":[-1,2],\"effect\":{\"heal\":\"5\"}}"),
             "\"range\" must be [min, max]: whole numbers from 0 to 20 (0 = the user's own square)"));
  CHECK(says(withAbility("self_r4", "{\"id\":\"x\",\"target\":\"unit\",\"range\":[2,1],\"effect\":{\"heal\":\"5\"}}"), "min not above max"));
  // With an area, a range from 0 is the area centred on the user (as before).
  CHECK_STR(withAbility("self_ok", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"sleep\"},"
                                   "\"area\":{\"shape\":\"radius\",\"radius\":1}}"),
            "");
}
