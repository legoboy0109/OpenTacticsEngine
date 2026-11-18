// docs/DIFFICULTY.md "One unit, different numbers per difficulty": placements limited to disjoint difficulty lists may share
// a square and an id; only placements that can exist in one battle conflict. References by id, battle building, saves.
#include "difficulty_fixture.h"
#include "core/campaign/difficulty.h"
#include "save_helpers.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

void refusedWith(const std::string& err, const std::string& needle) {
  if (!has(err, needle)) std::printf("  got: %s\n  wanted: %s\n", err.c_str(), needle.c_str());
  CHECK(has(err, needle));
}

// Gorm, three times on one square: his attack is 3 / 6 / 9 on easy / normal / hard. Eirik next to him; a unit after them.
const char* kVariantUnits = R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1,"name":"Eirik"},
  {"id":"gorm","type":"grunt","side":"red","x":1,"y":1,"difficulty":["easy"],"stats":{"atk":3}},
  {"id":"gorm","type":"grunt","side":"red","x":1,"y":1,"difficulty":["normal"],"stats":{"atk":6}},
  {"id":"gorm","type":"grunt","side":"red","x":1,"y":1,"difficulty":["hard"],"stats":{"atk":9}},
  {"id":"tail","type":"grunt","side":"red","x":7,"y":2} ])";

// References by the shared id: a battle-start behaviour, a defeat trigger, an objective.
const char* kVariantEvents = R"([
  {"id":"calm","when":{"battle":"start"},"do":[{"setBehaviour":"hold","units":{"unit":"gorm"}}]},
  {"id":"fell","when":{"defeated":{"unit":"gorm"}},"do":[{"setSwitch":"bridge_cut","to":true}]} ])";

std::string variantMap(const std::string& units = kVariantUnits, const std::string& events = kVariantEvents,
                       const std::string& objectives = R"({"text":"Fell Gorm","win":[{"defeated":{"unit":"gorm"}}]})") {
  std::string m = camptest::mapJson(8, 3, camptest::sides("0", "[]"), units, events);
  m.insert(m.find(", \"events\":"), ", \"objectives\": " + objectives);
  return m;
}

const Unit& unit(const Battle& b, const char* id) { return b.units[(size_t)b.unitByStoryId(id)]; }

}  // namespace

TEST(variants_disjoint_difficulties_share_a_square_and_an_id_and_the_right_one_is_built) {
  difftest::Game g;
  std::string err;
  CHECK(g.load("variants_build", difftest::kDifficulties, variantMap(), camptest::kCampaignJson, &err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  const int atk = g.g.db.findStat("atk");
  const char* names[] = {"easy", "normal", "hard"};
  const int wanted[] = {3, 6, 9};
  for (int i = 0; i < 3; ++i) {
    Battle b;
    std::vector<Event> ev;
    CHECK(g.arena(b, ev, names[i]));
    CHECK_EQ(b.units.size(), (size_t)3);  // eirik, the one gorm, tail: placement order among the units that exist
    CHECK(b.units[0].storyId == "eirik" && b.units[1].storyId == "gorm" && b.units[2].storyId == "tail");
    CHECK_EQ(b.units[1].stats[(size_t)atk], wanted[i]);
    CHECK_EQ(b.occupant[(size_t)b.grid.at(1, 1)], 1);
    CHECK_EQ(b.unitByStoryId("gorm"), 1);
    // the reference by id reached the one that exists
    CHECK(b.units[1].behaviour.mode == AiMode::Hold);
  }
}

TEST(variants_references_by_the_shared_id_work_on_each_difficulty) {
  difftest::Game g;
  CHECK(g.load("variants_refs", difftest::kDifficulties, variantMap()));
  const int cut = g.g.db.findSwitch("bridge_cut");
  for (const char* d : {"easy", "normal", "hard"}) {
    Battle b;
    std::vector<Event> ev;
    CHECK(g.arena(b, ev, d));
    CHECK_STR(b.objectiveText(), "Fell Gorm");
    CHECK(!b.storyState.switches[(size_t)cut]);
    b.units[1].hp = 1;
    CHECK(b.apply(Action::attack(0, 1, 0), ev));
    CHECK(!unit(b, "gorm").alive);
    CHECK(b.storyState.switches[(size_t)cut]);  // the defeated trigger
    CHECK(b.over && b.winnerTeam == 0);         // the objective
  }
}

TEST(variants_saves_round_trip_on_each_difficulty) {
  difftest::Game g;
  CHECK(g.load("variants_saves", difftest::kDifficulties, variantMap()));
  const int atk = g.g.db.findStat("atk");
  const char* names[] = {"easy", "normal", "hard"};
  for (int i = 0; i < 3; ++i) {
    Battle b;
    std::vector<Event> ev;
    CHECK(g.arena(b, ev, names[i]));
    Json saved;
    saveBattleState(b, saved);
    Battle fresh;  // built at another difficulty first: the save wins and brings its own units
    MapDef map;
    std::string err;
    CHECK(loadMap(g.g.db, "skirmish", map, err));
    fresh.setDifficulty((i + 1) % 3);
    CHECK(fresh.init(g.g.db, map, g.g.formulas, 1, err));
    CHECK(restoreBattleState(fresh, saved, err));
    CHECK_EQ(fresh.difficulty, i);
    CHECK_EQ(unit(fresh, "gorm").stats[(size_t)atk], 3 + 3 * i);
    CHECK(savetest::stateText(fresh) == savetest::stateText(b));
  }
}

TEST(variants_add_unit_variants_share_an_id_and_only_the_right_one_is_added) {
  const std::string units = R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1,"name":"Eirik"},
    {"id":"tail","type":"grunt","side":"red","x":7,"y":2} ])";
  const std::string events = R"([ {"id":"come","when":{"battle":"start"},"do":[
    {"addUnit":{"id":"ally","type":"grunt","side":"red","x":5,"y":0,"difficulty":["easy"],"stats":{"atk":3}}},
    {"addUnit":{"id":"ally","type":"grunt","side":"red","x":5,"y":0,"difficulty":["normal","hard"],"stats":{"atk":7}}} ]},
    {"id":"use","when":{"battle":"start"},"do":[{"setBehaviour":"hold","units":{"unit":"ally"}}]} ])";
  difftest::Game g;
  CHECK(g.load("variants_add", difftest::kDifficulties, variantMap(units, events, "{\"win\":[{\"defeated\":{\"unit\":\"ally\"}}]}")));
  const int atk = g.g.db.findStat("atk");
  const char* names[] = {"easy", "normal", "hard"};
  for (int i = 0; i < 3; ++i) {
    Battle b;
    std::vector<Event> ev;
    CHECK(g.arena(b, ev, names[i]));
    CHECK_EQ(b.units.size(), (size_t)3);
    CHECK_EQ(unit(b, "ally").stats[(size_t)atk], i == 0 ? 3 : 7);
  }
  // an addUnit that shares its id with a map unit on a common difficulty is refused, naming it
  const std::string clash = R"([ {"id":"come","when":{"battle":"start"},"do":[
    {"addUnit":{"id":"tail","type":"grunt","side":"red","x":5,"y":0,"difficulty":["hard"]}} ]} ])";
  refusedWith(g.loadError("variants_add2", difftest::kDifficulties, variantMap(units, clash, "{\"win\":[{\"defeated\":\"tail\"}]}")),
              "already used by another unit of the map on difficulty \"hard\"");
  const std::string two = R"([ {"id":"come","when":{"battle":"start"},"do":[
    {"addUnit":{"id":"a","type":"grunt","side":"red","x":5,"y":0,"difficulty":["easy","normal"]}},
    {"addUnit":{"id":"a","type":"grunt","side":"red","x":5,"y":0,"difficulty":["normal"]}} ]} ])";
  refusedWith(g.loadError("variants_add3", difftest::kDifficulties, variantMap(units, two, "{\"win\":[{\"defeated\":\"tail\"}]}")), "on difficulty \"normal\"");
}

TEST(variants_overlapping_or_unrestricted_placements_are_still_refused_with_the_difficulty_named) {
  difftest::Game g;
  auto units = [](const std::string& second) {
    return R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1},
      {"id":"gorm","type":"grunt","side":"red","x":1,"y":1,"difficulty":["easy","normal"]},)" + second + "]";
  };
  auto err = [&](const std::string& name, const std::string& second) {
    return g.loadError(name, difftest::kDifficulties, camptest::mapJson(8, 3, camptest::sides("0", "[]"), units(second)));
  };
  // same square, lists overlap on normal
  refusedWith(err("variants_o1", R"({"type":"grunt","side":"red","x":1,"y":1,"difficulty":["normal","hard"]})"),
              "shares a cell with units[1] on difficulty \"normal\"");
  // same id, lists overlap on normal
  refusedWith(err("variants_o2", R"({"id":"gorm","type":"grunt","side":"red","x":3,"y":1,"difficulty":["normal","hard"]})"),
              "\"id\" \"gorm\" is already used by units[1] on difficulty \"normal\"");
  // a variant and an unrestricted unit on one square: the unrestricted one exists on every difficulty
  refusedWith(err("variants_o3", R"({"type":"grunt","side":"red","x":1,"y":1})"), "shares a cell with units[1] on difficulty \"easy\"");
  refusedWith(err("variants_o4", R"({"id":"gorm","type":"grunt","side":"red","x":3,"y":1})"), "is already used by units[1] on difficulty \"easy\"");
  // two unrestricted units keep the plain message
  const std::string plain = g.loadError("variants_o5", difftest::kDifficulties,
                                        camptest::mapJson(8, 3, camptest::sides("0", "[]"),
                                                          R"([ {"type":"grunt","side":"red","x":1,"y":1}, {"type":"grunt","side":"red","x":1,"y":1} ])"));
  CHECK(plain.find("shares a cell with units[0]") != std::string::npos && plain.find("difficulty") == std::string::npos);
  // disjoint: fine
  CHECK(err("variants_ok", R"({"id":"gorm","type":"grunt","side":"red","x":1,"y":1,"difficulty":["hard"]})").empty());
}

TEST(variants_a_character_may_be_placed_once_per_difficulty) {
  difftest::Game g;
  g.g.charactersJson = R"({ "characters": [ { "id": "lyra", "name": "Lyra", "type": "ranger" } ] })";
  // the same rule for a character (one person): disjoint lists coexist, overlapping ones do not
  const std::string overlap = g.loadError("variants_c1", difftest::kDifficulties,
      camptest::mapJson(8, 3, camptest::sides("0", "[]"),
                        R"([ {"character":"lyra","type":"ranger","side":"red","x":2,"y":1,"difficulty":["easy","hard"]},
                             {"character":"lyra","type":"ranger","side":"red","x":3,"y":1,"difficulty":["hard"]} ])"));
  refusedWith(overlap, "is already placed by units[0] on difficulty \"hard\"");
  CHECK(g.loadError("variants_c2", difftest::kDifficulties,
      camptest::mapJson(8, 3, camptest::sides("0", "[]"),
                        R"([ {"character":"lyra","type":"ranger","side":"red","x":2,"y":1,"difficulty":["easy","hard"]},
                             {"character":"lyra","type":"ranger","side":"red","x":3,"y":1,"difficulty":["normal"]} ])")).empty());
}
