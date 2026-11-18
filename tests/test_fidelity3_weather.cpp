// "Fidelity 3", item 4: weather that closes a way ("weatherCost": { "rain": { "foot": "blocked" } }; docs/ECONOMY.md "Weather and movement cost").
#include "fidelity3_fixture.h"
#include "core/ai/ai.h"

using namespace ote;

namespace {

// Swamp (cost 2 foot / 3 wheel) at 3,1 and 4,1; rain closes it to foot soldiers, snow opens it for wheels at cost 1.
f3::Opts game(const std::string& weatherCost = "{\"rain\":{\"foot\":\"blocked\"},\"snow\":{\"wheel\":1}}", const std::string& mapExtra = "\"weather\": \"rain\",") {
  f3::Opts o;
  o.base.terrain = "{\"id\":\"plain\"},{\"id\":\"swamp\",\"moveCost\":{\"foot\":2,\"wheel\":3},\"weatherCost\":" + weatherCost + "}";
  o.base.mapExtra = mapExtra;
  return o;
}

}  // namespace

TEST(fid3_weather_blocked_is_loaded_and_closes_the_terrain) {
  f3::World w(f3::make("wb_cost", game()));
  const Cell swamp = w.at(3, 1);
  CHECK_EQ(w.b().enterCost(swamp, 0), 0);  // rain: closed to foot (cost <= 0 = cannot enter, like any impassable square)
  CHECK_EQ(w.b().enterCost(swamp, 1), 3);  // the wheel has no rain entry
  CHECK_EQ(w.b().enterCost(w.at(0, 0), 0), 1);
  w.b().storyState.weather = "snow";
  CHECK_EQ(w.b().enterCost(swamp, 0), 2);  // snow says nothing about foot
  CHECK_EQ(w.b().enterCost(swamp, 1), 1);
  w.b().storyState.weather.clear();
  CHECK_EQ(w.b().enterCost(swamp, 0), 2);  // clear sky: the terrain's own number again
}

TEST(fid3_weather_blocked_refusals) {
  CHECK_STR(f3::error(f3::make("wb_ok", game())), "");
  CHECK(f3::says(f3::error(f3::make("wb_word", game("{\"rain\":{\"foot\":\"closed\"}}"))), "or \"blocked\""));
  CHECK(f3::says(f3::error(f3::make("wb_bool", game("{\"rain\":{\"foot\":true}}"))), "from 1 to 99, or \"blocked\""));
  CHECK(f3::says(f3::error(f3::make("wb_zero", game("{\"rain\":{\"foot\":0}}"))), "from 1 to 99"));  // 0 stays refused: the spelling is "blocked"
  CHECK_STR(f3::error(f3::make("wb_ok2", game("{\"rain\":{\"foot\":\"blocked\",\"wheel\":\"blocked\"}}"))), "");
}

TEST(fid3_weather_blocked_a_unit_cannot_enter_and_the_reach_follows) {
  // The hero (move 4) at 1,1: the swamp at 3,1 costs 2 + ... reachable in clear weather (1 + 2 = 3), not in rain.
  const std::string events = "{\"id\":\"clear_now\",\"when\":{\"turn\":2},\"do\":[{\"weather\":\"none\"}]}";
  f3::Opts o = game();
  o.base.events = events;
  o.mapUnits = "{\"type\":\"knight\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1,\"leader\":true},"
               "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":8,\"y\":2}";
  f3::World w(f3::make("wb_reach", o));
  Reach r;
  w.b().reachable(w.id("hero"), r);
  CHECK_EQ(r.cost[(size_t)w.at(3, 1)], -1);
  CHECK(r.cost[(size_t)w.at(2, 0)] > 0);  // the way round it is open
  CHECK(!w.run(Action::move(w.id("hero"), w.at(3, 1))));  // refused: it cannot enter
  w.round();  // round 2: the story clears the sky
  w.b().reachable(w.id("hero"), r);
  CHECK(r.cost[(size_t)w.at(3, 1)] > 0);
  CHECK(w.run(Action::move(w.id("hero"), w.at(3, 1))));
}

TEST(fid3_weather_blocked_a_unit_standing_there_stays_and_may_leave) {
  // The hero stands on the swamp when the rain starts: nobody moves it, and it may leave by any open neighbour.
  f3::Opts o = game("{\"rain\":{\"foot\":\"blocked\"}}", "");
  o.mapUnits = "{\"type\":\"knight\",\"id\":\"hero\",\"side\":\"blue\",\"x\":3,\"y\":1,\"leader\":true},"
               "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":8,\"y\":2}";
  f3::World w(f3::make("wb_stay", o));
  const int hero = w.id("hero");
  const Cell where = w.unit("hero").cell;
  w.b().storyState.weather = "rain";  // the story's weather command does exactly this
  CHECK_EQ(w.b().enterCost(where, 0), 0);
  CHECK_EQ(w.unit("hero").cell, where);
  Reach r;
  w.b().reachable(hero, r);
  CHECK_EQ(r.cost[(size_t)where], 0);          // it can stay put
  CHECK(r.cost[(size_t)w.at(3, 0)] == 1);      // and leave by a passable neighbour
  CHECK_EQ(r.cost[(size_t)w.at(4, 1)], -1);    // but not onto the other swamp square, closed as well
  CHECK(w.run(Action::move(hero, w.at(3, 0))));
  CHECK_EQ(w.unit("hero").cell, w.at(3, 0));
}

TEST(fid3_weather_blocked_the_computer_opponent_plans_around_it) {
  f3::Opts o = game("{\"rain\":{\"foot\":\"blocked\"}}");
  o.mapUnits = "{\"type\":\"knight\",\"id\":\"hero\",\"side\":\"blue\",\"x\":0,\"y\":1,\"leader\":true},"
               "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":4,\"y\":1}";  // standing in the closed swamp
  f3::World w(f3::make("wb_ai", o));
  std::vector<Event> events;
  CHECK(w.run(Action::endTurn()));  // red's turn
  for (int i = 0; i < 20 && w.b().activeSide == 1; ++i) aiTakeStep(w.b(), events);
  const Unit& bandit = w.unit("bandit");
  CHECK(bandit.cell != w.at(3, 1));  // it never stepped onto the other closed square
  CHECK(bandit.cell != w.at(4, 1) || bandit.done);  // it left, or the AI chose to stay; either way no crash and a finished turn
}

TEST(fid3_a_game_without_blocked_is_unchanged) {
  // The fidelity 2 weather costs read exactly as before.
  f3::World w(f3::make("wb_plain", game("{\"rain\":{\"foot\":4},\"snow\":{\"foot\":1}}")));
  CHECK_EQ(w.b().enterCost(w.at(3, 1), 0), 4);
}
