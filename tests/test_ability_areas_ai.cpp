// Areas and several effects for abilities, part 2 (docs/SRPG.md "Areas and several effects for abilities"; docs/AI.md "Abilities with an area"): the
// computer's use of area heals and area conditions (summed over the units, never a waste), charged areas in the speed turn order (the square is kept;
// a save in the middle of one round-trips), and determinism by seed. Games: tests/ability_conditions_fixture.h, tests/fixtures/speed-order.
#include "ability_conditions_fixture.h"
#include "core/ai/ai_internal.h"
#include "save_helpers.h"
#include "speed_fixture.h"

using namespace ote;

namespace {

const char* kAreaAbilities =
    "{\"id\":\"cura\",\"name\":\"Cura\",\"target\":\"unit\",\"range\":[0,3],\"effect\":{\"heal\":10},\"area\":{\"shape\":\"radius\",\"radius\":1}},"
    "{\"id\":\"curaz\",\"name\":\"Cura Zero\",\"target\":\"unit\",\"range\":[0,0],\"effect\":{\"heal\":10},\"area\":{\"shape\":\"radius\",\"radius\":1}},"
    "{\"id\":\"sleepz\",\"name\":\"Sleep Zero\",\"target\":\"unit\",\"range\":[0,0],\"inflict\":{\"condition\":\"sleep\"},\"area\":{\"shape\":\"burst\"}},"
    "{\"id\":\"barrier\",\"name\":\"Barrier\",\"target\":\"ally\",\"range\":[0,2],\"inflict\":[{\"condition\":\"protect\"},{\"condition\":\"haste\"}],"
    "\"area\":{\"shape\":\"burst\"}}";

acf::Opts aiOpts(const char* brigandAbilities, const std::string& units) {
  acf::Opts o;
  o.abilities = kAreaAbilities;
  o.mageAbilities = "[]";
  o.brigandAbilities = brigandAbilities;
  o.units = units;
  return o;
}

std::string dump(const std::vector<Event>& events) {
  std::string out;
  for (const Event& e : events)
    out += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + "," + std::to_string(e.d) + "," +
           std::to_string(e.flags) + ";";
  return out;
}

}  // namespace

// ---- the computer ----

TEST(abarea_ai_heals_its_hurt_units_with_one_area) {
  // Blue is far away (no attack this turn); two hurt brigands stand side by side.
  acf::World w(acf::make("abarea_ai_heal", aiOpts("[\"cura\"]",
                                                  "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":0,\"y\":0},"
                                                  "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":7,\"y\":1},"
                                                  "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":8,\"y\":1}")));
  w.unit("bandit").hp = 8;
  w.unit("thug").hp = 8;
  w.redTurn();
  CHECK(w.first(EventType::AbilityUsed) != nullptr);
  // The first heals both (8 -> 18); the second brigand then heals both again, up to their maximum.
  CHECK_EQ(w.unit("bandit").hp, 20);
  CHECK_EQ(w.unit("thug").hp, 20);
}

TEST(abarea_ai_never_uses_an_area_that_helps_foes_more) {
  // The brigand at 4,1 between two hurt blue units; "curaz" can only be aimed at its own square, so it would heal both foes too.
  acf::World w(acf::make("abarea_ai_waste", aiOpts("[\"curaz\",\"cura\",\"sleepz\"]",
                                                   "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":3,\"y\":1},"
                                                   "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":5,\"y\":1},"
                                                   "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":4,\"y\":1},"
                                                   "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":4,\"y\":0}")));
  w.unit("hero").hp = 10;
  w.unit("pal").hp = 10;
  w.unit("bandit").hp = 10;
  CHECK(w.run(Action::endTurn()));
  const Unit& u = w.unit("bandit");
  const Battle& b = w.b();
  ai::Order best;
  ai::considerAreaAbilities(b, u, u.cell, 0, best);
  CHECK(best.kind == ActionType::Ability);
  CHECK(best.ability != w.ability("curaz"));  // +500 for itself, -333 for each foe: never
  CHECK(best.ability != w.ability("sleepz"));  // a curse on the thug (its own team) as much as on the foes: 0 at best, never used
  // Where "cura" goes, nobody of blue is in the area.
  AbilityAreaForecast f;
  CHECK(b.abilityAreaForecast(u.id, best.ability, u.cell, best.cell, f));
  for (const AbilityAreaHit& h : f.hits) CHECK(!b.isEnemy(u, b.units[(size_t)h.unit]));
}

TEST(abarea_ai_alone_with_only_wasteful_areas_does_nothing_with_them) {
  acf::World w(acf::make("abarea_ai_none", aiOpts("[\"curaz\",\"sleepz\"]",
                                                  "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":3,\"y\":1},"
                                                  "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":5,\"y\":1},"
                                                  "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":4,\"y\":1},"
                                                  "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":4,\"y\":0}")));
  w.unit("hero").hp = 10;
  w.unit("pal").hp = 10;
  w.unit("bandit").hp = 10;
  CHECK(w.run(Action::endTurn()));
  ai::Order best;
  ai::considerAreaAbilities(w.b(), w.unit("bandit"), w.unit("bandit").cell, 0, best);
  CHECK(best.kind == ActionType::Wait);  // nothing worth more than 0
}

TEST(abarea_ai_gives_both_blessings_to_its_own_side) {
  acf::World w(acf::make("abarea_ai_barrier", aiOpts("[\"barrier\"]",
                                                     "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":0,\"y\":0},"
                                                     "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":7,\"y\":1},"
                                                     "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":8,\"y\":1}")));
  w.redTurn();
  CHECK(w.has("bandit", "protect") && w.has("bandit", "haste"));
  CHECK(w.has("thug", "protect") && w.has("thug", "haste"));
  CHECK(!w.has("hero", "protect"));
}

TEST(abarea_ai_play_is_the_same_twice) {
  const std::string units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
                            "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
                            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":6,\"y\":1},"
                            "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":7,\"y\":0}";
  std::string first;
  for (int run = 0; run < 2; ++run) {
    acf::Opts o = aiOpts("[\"cura\",\"barrier\"]", units);
    o.mageAbilities = "[\"cura\",\"barrier\"]";
    o.blueAi = true;
    o.abilities += ",{\"id\":\"sleepga\",\"name\":\"Sleepga\",\"target\":\"enemy\",\"range\":[1,4],\"inflict\":{\"condition\":\"sleep\",\"chance\":50},"
                   "\"area\":{\"shape\":\"burst\"}}";
    o.mageAbilities = "[\"cura\",\"barrier\",\"sleepga\"]";
    acf::World w(acf::make(run == 0 ? "abarea_ai_twice_a" : "abarea_ai_twice_b", o), 21);
    std::vector<Event> all;
    for (int step = 0; step < 300 && !w.b().over; ++step) {
      std::vector<Event> ev;
      if (!aiTakeStep(w.b(), ev)) break;
      all.insert(all.end(), ev.begin(), ev.end());
    }
    if (run == 0) first = dump(all);
    else CHECK_STR(dump(all), first);
    bool area = false;  // (the run really used an area ability)
    for (const Event& e : all) area = area || (e.type == EventType::AbilityUsed && (e.flags & kArea));
    CHECK(area);
  }
}

// ---- charged, in the speed turn order ----

namespace {

void addMendAll(skilltest::Copy& c) {
  c.swap("data/abilities.json", "{ \"abilities\": [",
         "{ \"abilities\": [ { \"id\": \"mendall\", \"name\": \"Mend All\", \"target\": \"ally\", \"range\": [1, 3], \"effect\": { \"heal\": 10 },"
         " \"area\": { \"shape\": \"radius\", \"radius\": 1 }, \"chargeSpeed\": 20 },");
  c.swap("data/units.json", "\"abilities\": [\"mend\", \"hush\"]", "\"abilities\": [\"mend\", \"hush\", \"mendall\"]");
}

bool castAt(speedtest::Scene& s, const char* who, const char* ability, int x, int y) {
  return s.w->apply(Action::useAbility(s.id(who), s.ability(ability), -1, s.at(x, y)));
}

}  // namespace

TEST(abarea_a_charged_area_keeps_its_square) {
  const std::vector<speedtest::U> units{{"mage", "blue", 3, 2, "mg"}, {"dancer", "blue", 5, 2, "da"}, {"dummy", "red", 10, 2, "foe"}};
  speedtest::Scene s("abarea_ch_keep", speedtest::plain(12, 5), units, false, addMendAll);
  s.unit("da").hp = 5;
  CHECK(castAt(s, "mg", "mendall", 5, 2));
  CHECK_EQ(s.b().pending.size(), (size_t)1);
  CHECK(!s.b().pending[0].follow && s.b().pending[0].cell == s.at(5, 2));
  CHECK_EQ(s.unit("da").hp, 5);  // nothing yet
  // A save now round-trips with the square.
  Json j;
  saveBattleState(s.b(), j);
  speedtest::Scene t("abarea_ch_keep2", speedtest::plain(12, 5), units, false, addMendAll);
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  for (speedtest::Scene* x : {&s, &t}) {
    CHECK(x->go("da", 5, 3));  // still inside the radius around 5,2
    x->wait("da");
    x->pass();
    for (int i = 0; i < 4 && !x->b().pending.empty(); ++i) x->pass();
  }
  CHECK(s.b().pending.empty());
  CHECK_EQ(s.unit("da").hp, 15);
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
}

TEST(abarea_a_charged_area_with_nobody_left_in_it_fizzles) {
  const std::vector<speedtest::U> units{{"mage", "blue", 3, 2, "mg"}, {"dancer", "blue", 5, 2, "da"}, {"dummy", "red", 10, 2, "foe"}};
  speedtest::Scene s("abarea_ch_fizzle", speedtest::plain(12, 5), units, false, addMendAll);
  s.unit("da").hp = 5;
  CHECK(castAt(s, "mg", "mendall", 5, 2));
  CHECK(s.go("da", 8, 2));  // out of the area
  s.wait("da");
  bool fizzled = false;
  for (int i = 0; i < 6 && !s.b().pending.empty(); ++i) {
    s.pass();
    for (const Event& e : s.w->events) fizzled = fizzled || (e.type == EventType::ChargeCancelled && e.d == kChargeFizzled);
  }
  CHECK(fizzled);
  CHECK_EQ(s.unit("da").hp, 5);
}
