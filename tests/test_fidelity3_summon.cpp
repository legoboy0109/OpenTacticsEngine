// "Fidelity 3", item 2: summon item candidates with weights, a level ceiling, "nothing" and up to 32 candidates (docs/SRPG.md "Summoning").
#include "fidelity3_fixture.h"

using namespace ote;

namespace {

// A game whose hero (knight) has one ability "call_x": a summon of the hound with these candidates.
std::string game(const char* name, const std::string& items, const char* pick = "random") {
  f3::Opts o;
  o.abilities = "{\"id\":\"call_x\",\"name\":\"Call X\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"hound\",\"limit\":3,\"canAct\":true,"
                "\"items\":[" + items + "],\"pick\":\"" + pick + "\"}}}";
  o.skills = "{\"id\":\"none\",\"blocks\":[{\"when\":\"always\",\"do\":[{\"stats\":{\"atk\":1}}]}]}";
  o.items = "{\"id\":\"iron_sword\",\"kind\":\"weapon\",\"weapon\":\"sword\",\"uses\":3},{\"id\":\"old_axe\",\"kind\":\"weapon\",\"weapon\":\"axe\"},"
            "{\"id\":\"fang\",\"kind\":\"weapon\",\"weapon\":\"sword\"},{\"id\":\"big_fang\",\"kind\":\"weapon\",\"weapon\":\"axe\"}";
  o.units = "{\"id\":\"knight\",\"name\":\"Knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":30,\"atk\":8,\"def\":2,\"mov\":4},\"wields\":[\"blade\"],"
            "\"items\":[\"iron_sword\"],\"abilities\":[\"call_x\"]},"
            "{\"id\":\"brigand\",\"name\":\"Brigand\",\"moveType\":\"foot\",\"stats\":{\"hp\":20,\"atk\":5,\"def\":1,\"mov\":3},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]},"
            "{\"id\":\"hound\",\"name\":\"Hound\",\"moveType\":\"foot\",\"stats\":{\"hp\":8,\"atk\":3,\"def\":1,\"mov\":4},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]}";
  return f3::make(name, o);
}

const char* kTwo = "{\"item\":\"fang\"},{\"item\":\"big_fang\"}";

// Summons once and returns the item the hound carries (-1 = none carried).
int summoned(f3::World& w, int* count = nullptr) {
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("call_x"), -1, w.at(1, 0))));
  const Unit& pet = w.b().units[(size_t)w.of(EventType::UnitAdded)[0].a];
  if (count) *count = (int)pet.items.size();
  return pet.items.empty() ? -1 : pet.items[0].item;
}

// Moves the random generator on `k` steps so that tests meet different rolls.
void advance(f3::World& w, int k) {
  for (int i = 0; i < k; ++i) w.b().rng.next();
}

}  // namespace

TEST(fid3_summon_weights_all_one_are_exactly_todays_roll) {
  for (int k = 0; k < 12; ++k) {
    f3::World plain(game("sw_plain", kTwo)), weighted(game("sw_one", "{\"item\":\"fang\",\"weight\":1},{\"item\":\"big_fang\",\"weight\":1}"));
    advance(plain, k);
    advance(weighted, k);
    Rng expect = plain.b().rng;
    const int index = expect.range(2);  // today: one roll among the n that fit
    const int a = summoned(plain), b = summoned(weighted);
    CHECK_EQ(a, plain.db().findItem(index == 0 ? "fang" : "big_fang"));
    CHECK_EQ(a, b);
    CHECK(plain.sameRng(weighted.b().rng));
    CHECK(plain.sameRng(expect));  // exactly one roll was used
  }
}

TEST(fid3_summon_weights_share_the_one_roll) {
  // fang weight 1, big_fang weight 3: a roll in [0,4): 0 = fang, 1..3 = big_fang.
  int fang = 0, big = 0;
  for (int k = 0; k < 40; ++k) {
    f3::World w(game("sw_w", "{\"item\":\"fang\",\"weight\":1},{\"item\":\"big_fang\",\"weight\":3}"));
    advance(w, k);
    Rng expect = w.b().rng;
    const int roll = expect.range(4);
    const int got = summoned(w);
    CHECK_EQ(got, w.db().findItem(roll == 0 ? "fang" : "big_fang"));
    CHECK(w.sameRng(expect));
    (roll == 0 ? fang : big)++;
  }
  CHECK(fang > 0 && big > fang);  // both came up, the heavier one more often
}

TEST(fid3_summon_weight_only_counts_among_those_that_fit) {
  // At level 1 only fang fits (big_fang needs 5): no roll at all, whatever the weights.
  f3::World w(game("sw_fit", "{\"item\":\"fang\",\"weight\":1},{\"item\":\"big_fang\",\"minLevel\":5,\"weight\":1000}"));
  const Rng before = w.b().rng;
  CHECK_EQ(summoned(w), w.db().findItem("fang"));
  CHECK(w.sameRng(before));
}

TEST(fid3_summon_max_level_is_a_ceiling) {
  const std::string items = "{\"item\":\"fang\",\"maxLevel\":4},{\"item\":\"big_fang\",\"minLevel\":5}";
  for (int level : {1, 4, 5, 9}) {
    f3::World w(game(("sm_l" + std::to_string(level)).c_str(), items, "highest"));
    w.unit("hero").level = level;
    CHECK_EQ(summoned(w), w.db().findItem(level <= 4 ? "fang" : "big_fang"));
  }
  // Nothing fits (level 8 is above the only candidate's ceiling): the type's own items, as when no minLevel is reached.
  f3::World w(game("sm_none", "{\"item\":\"fang\",\"maxLevel\":4}"));
  w.unit("hero").level = 8;
  CHECK_EQ(summoned(w), w.db().findItem("old_axe"));
}

TEST(fid3_summon_nothing_candidate_carries_no_item) {
  // "highest": the nothing candidate has the highest minLevel that fits.
  f3::World w(game("sn_hi", "{\"item\":\"fang\"},{\"item\":null,\"minLevel\":3}", "highest"));
  w.unit("hero").level = 3;
  int count = -1;
  CHECK_EQ(summoned(w, &count), -1);
  CHECK_EQ(count, 0);
  // Below its level the fang is carried.
  f3::World low(game("sn_low", "{\"item\":\"fang\"},{\"item\":null,\"minLevel\":3}", "highest"));
  CHECK_EQ(summoned(low), low.db().findItem("fang"));
  // "random": a nothing candidate may come up like any other (weighted), and it is one roll.
  bool sawNothing = false, sawFang = false;
  for (int k = 0; k < 30; ++k) {
    f3::World r(game("sn_rand", "{\"item\":\"fang\"},{\"item\":null}"));
    advance(r, k);
    Rng expect = r.b().rng;
    const int roll = expect.range(2);
    const int got = summoned(r, &count);
    CHECK_EQ(got == -1, roll == 1);
    CHECK(r.sameRng(expect));
    (got == -1 ? sawNothing : sawFang) = true;
  }
  CHECK(sawNothing && sawFang);
}

TEST(fid3_summon_candidates_up_to_32_and_loader_refusals) {
  std::string many;
  for (int i = 0; i < 32; ++i) many += std::string(i ? "," : "") + "{\"item\":\"fang\"}";
  CHECK_STR(f3::error(game("sl_32", many)), "");
  CHECK(f3::says(f3::error(game("sl_33", many + ",{\"item\":\"fang\"}")), "1 to 32 candidates"));
  CHECK(f3::says(f3::error(game("sl_w0", "{\"item\":\"fang\",\"weight\":0}")), "\"weight\" must be a whole number from 1 to 1000"));
  CHECK(f3::says(f3::error(game("sl_w1001", "{\"item\":\"fang\",\"weight\":1001}")), "from 1 to 1000"));
  CHECK(f3::says(f3::error(game("sl_wfrac", "{\"item\":\"fang\",\"weight\":1.5}")), "from 1 to 1000"));
  CHECK_STR(f3::error(game("sl_w1000", "{\"item\":\"fang\",\"weight\":1000}")), "");
  CHECK(f3::says(f3::error(game("sl_max", "{\"item\":\"fang\",\"minLevel\":5,\"maxLevel\":4}")), "\"maxLevel\" must be a whole number from \"minLevel\" (5)"));
  CHECK(f3::says(f3::error(game("sl_max0", "{\"item\":\"fang\",\"maxLevel\":0}")), "\"maxLevel\""));
  CHECK(f3::says(f3::error(game("sl_item", "{\"item\":5}")), "or null for no item"));
  CHECK(f3::says(f3::error(game("sl_key", "{\"item\":\"fang\",\"chance\":2}")), "unknown key \"chance\""));
  CHECK(f3::says(f3::error(game("sl_unk", "{\"item\":\"nope\"}")), "unknown item \"nope\""));
}
