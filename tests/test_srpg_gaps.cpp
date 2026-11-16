// Five small rule gaps of weapon ranks, weapon locks and the foe's weapon (docs/SRPG.md section 7, "Formulas in this
// document"): addUnit "ranks", "rankXp": 0, "wieldedByTypes", a character's "wields", and `fw.answers`.
#include "srpg_ranks_fixture.h"

using namespace ote;
using namespace rankstest;

namespace {
// The duel map with an event that adds a unit through addUnit (`unitJson` is the command's unit), once, at battle start;
// optionally inside an "if" (to prove nested commands read the same keys).
std::string duelWithAdd(const std::string& unitJson, bool nested) {
  std::string add = "{ \"addUnit\": " + unitJson + " }";
  if (nested) add = "{ \"if\": [ { \"alive\": \"kell\" } ], \"then\": [ " + add + " ] }";
  return "{ \"name\": \"Duel\", \"grid\": { \"type\": \"square\", \"width\": 6, \"height\": 3 }, \"legend\": { \".\": \"plain\" },"
         "\"tiles\": [\"......\", \"......\", \"......\"],"
         "\"sides\": [ { \"id\": \"blue\", \"controller\": \"human\" }, { \"id\": \"red\", \"controller\": \"ai\" } ],"
         "\"units\": [ { \"character\": \"kell\", \"side\": \"blue\", \"x\": 1, \"y\": 1, \"leader\": true },"
         "{ \"type\": \"grunt\", \"id\": \"g1\", \"side\": \"red\", \"x\": 5, \"y\": 2 } ],"
         "\"events\": [ { \"id\": \"more\", \"when\": { \"battle\": \"start\" }, \"do\": [ " + add + " ] } ] }";
}
}  // namespace

TEST(gaps_add_unit_takes_ranks_like_a_map_unit) {
  for (bool nested : {false, true}) {
    World w("gaps_add_ranks", "duel", [&](Copy& c) {
      c.put("maps/duel.json", duelWithAdd("{ \"type\": \"grunt\", \"id\": \"g9\", \"side\": \"red\", \"x\": 4, \"y\": 0, "
                                          "\"ranks\": { \"sword\": \"b\", \"bow\": 5 } }", nested));
    });
    const Unit& g = w.unit("g9");
    CHECK_EQ(g.weaponXp[(size_t)w.group("sword")], 12);  // rank b = experience 12, replaces the class's "d"
    CHECK_EQ(g.weaponXp[(size_t)w.group("bow")], 5);     // a number is experience
    CHECK_EQ(w.unit("g1").weaponXp[(size_t)w.group("sword")], 3);  // others untouched
  }
}

TEST(gaps_add_unit_ranks_refusals_are_the_map_units) {
  auto err = [](const char* ranks) {
    Copy c("gaps_add_ranks_err");
    c.put("maps/duel.json", duelWithAdd(std::string("{ \"type\": \"grunt\", \"side\": \"red\", \"x\": 4, \"y\": 0, \"ranks\": ") + ranks + " }", false));
    return c.mapError("duel");
  };
  CHECK(saysIn(err("{ \"axe\": \"b\" }"), "\"ranks\" names unknown weapon group \"axe\""));
  CHECK(saysIn(err("{ \"sword\": \"z\" }"), "names unknown rank \"z\""));
  CHECK(saysIn(err("{ \"sword\": 5000 }"), "must be a rank id or weapon experience from 0 to"));
  CHECK(saysIn(err("[1]"), "\"ranks\""));
}
