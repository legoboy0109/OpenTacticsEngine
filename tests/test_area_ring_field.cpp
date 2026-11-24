// The area shapes "ring" (the squares around the aimed one, not the aimed one) and "field" (the whole map), on ability areas and on weapon areas
// (docs/SRPG.md "Area attacks and line fire", "Areas and several effects for abilities"): the squares, who is acted on / struck, a ring centred on the
// user by a strike ("Wild Swing"), the loader's refusals, the words. Game: tests/ability_conditions_fixture.h.
#include <algorithm>

#include "ability_conditions_fixture.h"
#include "battle/ability_text.h"
#include "core/grid/area.h"

using namespace ote;
using namespace acf;

namespace {

// hero (mage, blue) 4,1; pal (knight, blue) 4,0; far (knight, blue) 8,2; bandit (red) 3,1; thug (red) 5,1; lone (red) 0,2. Ids 0..5.
const char* kUnits = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":4,\"y\":1},"
                     "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":4,\"y\":0},"
                     "{\"type\":\"knight\",\"id\":\"far\",\"side\":\"blue\",\"x\":8,\"y\":2},"
                     "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1},"
                     "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":5,\"y\":1},"
                     "{\"type\":\"brigand\",\"id\":\"lone\",\"side\":\"red\",\"x\":0,\"y\":2}";

const char* kAbilities =
    "{\"id\":\"wave\",\"name\":\"Wave\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"ring\",\"radius\":1}},"
    "{\"id\":\"halo\",\"name\":\"Halo\",\"target\":\"unit\",\"range\":[1,3],\"inflict\":{\"condition\":\"haste\"},\"area\":{\"shape\":\"ring\",\"radius\":1}},"
    "{\"id\":\"rally\",\"name\":\"Rally\",\"target\":\"ally\",\"range\":[0,0],\"inflict\":{\"condition\":\"haste\"},\"area\":{\"shape\":\"field\"}},"
    "{\"id\":\"swing\",\"name\":\"Wild Swing\",\"effect\":{\"strike\":\"whirl\"}},"
    "{\"id\":\"quake\",\"name\":\"Quake\",\"effect\":{\"strike\":\"tremor\"}}";

const char* kWeapons =
    ",{\"id\":\"whirl\",\"name\":\"Whirl\",\"group\":\"spell\",\"range\":[0,0],\"area\":{\"shape\":\"ring\",\"radius\":1}}"
    ",{\"id\":\"tremor\",\"name\":\"Tremor\",\"group\":\"spell\",\"range\":[1,1],\"area\":{\"shape\":\"field\"}}";

Opts ringOpts() {
  Opts o;
  o.abilities = kAbilities;
  o.weaponsExtra = kWeapons;
  o.mageAbilities = "[\"wave\",\"halo\",\"rally\",\"swing\",\"quake\"]";
  o.units = kUnits;
  return o;
}

std::string withAbility(const char* name, const std::string& ability) {
  Opts o = ringOpts();
  o.abilities += "," + ability;
  return error(make(name, o));
}

std::string withWeapon(const char* name, const std::string& weapon) {
  Opts o = ringOpts();
  o.weaponsExtra += "," + weapon;
  return error(make(name, o));
}

std::vector<Cell> cellsOf(World& w, std::initializer_list<std::pair<int, int>> xy) {
  std::vector<Cell> out;
  for (const auto& p : xy) out.push_back(w.b().grid.at(p.first, p.second));
  std::sort(out.begin(), out.end());
  return out;
}

}  // namespace

// ---- the squares ----

TEST(ringfield_area_cells_of_a_ring_and_a_field) {
  const Grid g(GridType::Square, 9, 3);
  AreaDef ring;
  ring.shape = AreaShape::Ring;
  ring.radius = 1;
  std::vector<Cell> out;
  areaCells(g, g.at(4, 1), g.at(4, 1), ring, out);
  CHECK_EQ(out.size(), (size_t)4);
  CHECK(std::find(out.begin(), out.end(), g.at(4, 1)) == out.end());  // never the centre
  ring.radius = 2;
  areaCells(g, g.at(0, 0), g.at(4, 1), ring, out);
  std::vector<Cell> within;
  g.within(g.at(4, 1), 2, within);
  CHECK_EQ(out.size(), within.size() - 1);
  CHECK(std::is_sorted(out.begin(), out.end()));
  AreaDef field;
  field.shape = AreaShape::Field;
  areaCells(g, g.at(0, 0), g.at(8, 2), field, out);
  CHECK_EQ(out.size(), (size_t)27);  // every square, wherever it is aimed
  // Hexagons: a ring of 1 is the six neighbours.
  const Grid h(GridType::Hex, 7, 7, HexLayout::Pointy);
  ring.radius = 1;
  areaCells(h, h.at(3, 3), h.at(3, 3), ring, out);
  CHECK_EQ(out.size(), (size_t)6);
}

// ---- ability areas ----

TEST(ringfield_a_ring_ability_acts_on_the_units_around_the_square_only) {
  World w(make("rf_ring", ringOpts()));
  AbilityAreaForecast f;
  const Cell me = w.unit("hero").cell;
  CHECK(w.b().abilityAreaForecast(w.id("hero"), w.ability("wave"), me, me, f));
  CHECK(f.cells == cellsOf(w, {{3, 1}, {5, 1}, {4, 0}, {4, 2}}));
  CHECK_EQ(f.hits.size(), (size_t)2);  // bandit and thug; pal is no enemy, the user stands in the middle
  CHECK(f.hits[0].unit == w.id("bandit") && f.hits[1].unit == w.id("thug"));
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("wave"), -1, me)));
  CHECK(w.has("bandit", "burn") && w.has("thug", "burn") && !w.has("lone", "burn"));
  // Aimed at a unit's square, the unit there is left out ("unit": everyone else in the ring).
  World v(make("rf_ring2", ringOpts()));
  CHECK(v.b().abilityAreaForecast(v.id("hero"), v.ability("halo"), v.unit("hero").cell, v.unit("bandit").cell, f));
  for (const AbilityAreaHit& h : f.hits) CHECK(h.unit != v.id("bandit"));
  CHECK(std::find_if(f.hits.begin(), f.hits.end(), [&](const AbilityAreaHit& h) { return h.unit == v.id("hero"); }) != f.hits.end());
}

TEST(ringfield_a_field_ability_acts_on_its_side_everywhere) {
  World w(make("rf_field", ringOpts()));
  const Cell me = w.unit("hero").cell;
  AbilityAreaForecast f;
  CHECK(w.b().abilityAreaForecast(w.id("hero"), w.ability("rally"), me, me, f));
  CHECK_EQ(f.cells.size(), (size_t)27);
  CHECK_EQ(f.hits.size(), (size_t)3);  // hero, pal and far: the "ally" filter, the user included
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("rally"), -1, me)));
  CHECK(w.has("hero", "haste") && w.has("pal", "haste") && w.has("far", "haste"));
  CHECK(!w.has("bandit", "haste") && !w.has("lone", "haste"));
  CHECK(w.first(EventType::AbilityUsed) && w.first(EventType::AbilityUsed)->data.size() == 3);
}

// ---- weapon areas (through strike abilities) ----

TEST(ringfield_a_ring_strike_centred_on_the_user_hits_the_foes_around_it) {
  World w(make("rf_swing", ringOpts()));
  std::vector<AttackOption> opts;
  w.b().abilityStrikeOptions(w.id("hero"), w.ability("swing"), w.unit("hero").cell, opts);
  CHECK_EQ(opts.size(), (size_t)1);
  CHECK(opts.size() == 1 && opts[0].area && opts[0].cell == w.unit("hero").cell && opts[0].target == -1);
  const int hpPal = w.unit("pal").hp, hpHero = w.unit("hero").hp;
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("swing"), -1, w.unit("hero").cell)));
  std::vector<int> struck;
  for (const Event& e : w.events)
    if (e.type == EventType::Attack) {
      CHECK(e.flags & kArea);
      struck.push_back(e.b);
    }
  CHECK(struck == std::vector<int>({w.id("bandit"), w.id("thug")}));
  CHECK(w.unit("pal").hp == hpPal && w.unit("hero").hp == hpHero);  // no friendly fire, never the attacker
}

TEST(ringfield_a_field_strike_hits_every_foe_on_the_map) {
  World w(make("rf_quake", ringOpts()));
  const Cell aim = w.b().grid.at(4, 2);  // an empty square next to the user
  AreaForecast f;
  CHECK(w.b().abilityStrikeArea(w.id("hero"), w.ability("quake"), w.unit("hero").cell, aim, f));
  CHECK_EQ(f.victims.size(), (size_t)3);
  CHECK_EQ(f.primary, -1);
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("quake"), -1, aim)));
  int attacks = 0;
  for (const Event& e : w.events) attacks += e.type == EventType::Attack;
  CHECK_EQ(attacks, 3);  // bandit, thug and lone (far away)
}

TEST(ringfield_the_same_battle_twice) {
  auto run = [](const char* name) {
    Opts o = ringOpts();
    o.blueAi = true;
    World w(make(name, o), 3);
    int guard = 0;
    while (!w.b().over && aiTakeStep(w.b(), w.events) | true && ++guard < 600) {
    }
    return std::to_string(w.events.size()) + "/" + std::to_string(w.b().rng.state);
  };
  CHECK(run("rf_twice_a") == run("rf_twice_b"));
}

// ---- loader ----

TEST(ringfield_loads_and_refuses) {
  Database db;
  std::string err;
  CHECK(loadGame(make("rf_load", ringOpts()), db, err));
  CHECK_STR(err, "");
  CHECK(db.abilities[(size_t)db.findAbility("wave")].area.shape == AreaShape::Ring);
  CHECK(db.abilities[(size_t)db.findAbility("rally")].area.shape == AreaShape::Field);
  CHECK(db.weapons[(size_t)db.findWeapon("tremor")].area.shape == AreaShape::Field);
  CHECK(says(withAbility("rf_r1", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"ring\"}}"),
             "\"area\" of shape \"ring\" needs a \"radius\""));
  CHECK(says(withAbility("rf_r2", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"ring\",\"radius\":9}}"),
             "\"radius\""));
  CHECK(says(withAbility("rf_r3", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"field\",\"radius\":2}}"),
             "a \"field\" covers the whole map and takes no \"radius\""));
  CHECK(says(withAbility("rf_r4", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"field\",\"vertical\":1}}"),
             "a \"field\" covers the whole map at every height"));
  CHECK(says(withAbility("rf_r5", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"ring\",\"radius\":1,\"length\":2}}"),
             "\"length\" belongs to the shape \"line\" only"));
  CHECK(says(withAbility("rf_r6", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"donut\"}}"),
             "must be \"burst\", \"line\", \"radius\", \"cross\", \"ring\" or \"field\""));
  CHECK(says(withWeapon("rf_r7", "{\"id\":\"y\",\"area\":{\"shape\":\"field\",\"length\":3}}"), "\"length\" belongs to the shape \"line\" only"));
  CHECK(says(withWeapon("rf_r8", "{\"id\":\"y\",\"area\":{\"shape\":\"ring\"}}"), "needs a \"radius\""));
}

TEST(ringfield_words) {
  Database db;
  std::string err;
  CHECK(loadGame(make("rf_words", ringOpts()), db, err));
  CHECK(says(abilityEffectText(db, db.abilities[(size_t)db.findAbility("wave")]), "(area: a ring of 1)"));
  CHECK(says(abilityEffectText(db, db.abilities[(size_t)db.findAbility("rally")]), "(area: the whole map)"));
}
