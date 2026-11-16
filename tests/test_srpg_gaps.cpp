// Five small rule gaps of weapon ranks, weapon locks and the foe's weapon (docs/SRPG.md section 7, "Formulas in this
// document"): addUnit "ranks", "rankXp": 0, "wieldedByTypes", a character's "wields", and `fw.answers`.
#include "srpg_staves_fixture.h"
#include "srpg_triangle_fixture.h"

using namespace ote;
using namespace rankstest;
using stavestest::Clinic;

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

// ---- "rankXp": 0 -------------------------------------------------------------------------------------------------------

TEST(gaps_rank_xp_zero_trains_nothing_on_a_weapon_and_a_staff) {
  World w("gaps_xp0", "duel", [](Copy& c) {
    c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"100\"");
    c.swap("data/weapons.json", "\"rankXp\": 2", "\"rankXp\": 0");  // the steel sword
  });
  CHECK_EQ(w.rig.db.weapons[(size_t)w.weapon("steel_sword")].rankXp, 0);
  const int sword = w.group("sword");
  CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("steel_sword"))));
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)sword], 4);  // unchanged (it was 6 with rankXp 2)
  CHECK_EQ(w.unit("g1").weaponXp[(size_t)sword], 4);    // the grunt's bronze sword (default 1) still trains: 3 -> 4
  CHECK_EQ(w.count(EventType::RankUp), 0);

  Clinic s("gaps_xp0_staff", "clinic", [](Copy& c) {
    c.swap("data/items.json", "\"group\": \"staff\", \"rankXp\": 2", "\"group\": \"staff\", \"rankXp\": 0");
  });
  const int staff = s.rig.db.ranks.findGroup("staff");
  const int before = s.unit("wen").weaponXp[(size_t)staff];
  s.hurt("tarn", 5);
  CHECK(s.use("wen", "mend_staff", "tarn"));
  CHECK_EQ(s.unit("wen").weaponXp[(size_t)staff], before);
}

TEST(gaps_rank_xp_absent_keeps_the_default_and_bad_values_are_still_refused) {
  World w("gaps_xp_default");
  CHECK_EQ(w.rig.db.weapons[(size_t)w.weapon("bronze_sword")].rankXp, 1);
  for (const char* bad : {"100", "-1"}) {
    Copy c("gaps_xp_bad");
    c.swap("data/weapons.json", "\"rankXp\": 2", std::string("\"rankXp\": ") + bad);
    CHECK(saysIn(c.gameError(), "\"rankXp\" must be from 1 to 99, or 0 for a weapon that trains nothing"));
  }
}

// ---- fw.answers ---------------------------------------------------------------------------------------------------------

namespace {
// The triangle fixture with the damage formula `expr` and the lance reaching only 2 squares (so the lancer cannot answer a
// neighbour: it holds an answering weapon, out of range).
tritest::World answersWorld(const char* damage, bool counterRule = true) {
  return tritest::World("duel", [=](const std::string& dir) {
    tritest::swapText(dir + "/game.json", "\"damage\":  \"a.str + w.might - b.def + 3 * tri(w, fw)\"", std::string("\"damage\":  \"") + damage + "\"");
    tritest::swapText(dir + "/data/weapons.json", "\"group\": \"lance\",", "\"group\": \"lance\", \"range\": [2, 2],");
    if (!counterRule) tritest::swapText(dir + "/game.json", "\"counterAttacks\": true", "\"counterAttacks\": false");
  });
}
}  // namespace

TEST(gaps_fw_answers_is_set_only_when_the_foe_really_answers) {
  // base 5 + 5 - 2 = 8; +100 when fw.answers; +1000 when it is nil (not merely 0).
  const char* f = "a.str + w.might - b.def + (fw.answers and 100 or 0) + (fw.answers == nil and 1000 or 0)";
  tritest::World w = answersWorld(f);
  CHECK_EQ(w.forecast("sw", "ax", "iron_sword").attack.damage, 8 + 100);   // the axeman answers at range 1
  CHECK_EQ(w.forecast("sw", "la", "iron_sword").attack.damage, 8 + 1000);  // the lancer holds a lance, out of range: fw is it, no answer
  CHECK(!w.forecast("sw", "la", "iron_sword").counter.can);
  CHECK_EQ(w.forecast("sw", "mk", "iron_sword").attack.damage, 8 + 1000);  // the empty fw: answers is nil, not a truthy 0
  // The default of fw itself is unchanged: out of range it is still the lance.
  tritest::World g = answersWorld("(fw.id == 'iron_lance' and 7 or 0)");
  CHECK_EQ(g.forecast("sw", "la", "iron_sword").attack.damage, 7);
  // A counter: the foe is the attacker, whose weapon is certainly in use.
  CHECK_EQ(w.forecast("sw", "ax", "iron_sword").counter.damage, 5 + 6 - 2 + 100);
}

TEST(gaps_fw_answers_follows_the_counter_rule) {
  tritest::World w = answersWorld("a.str + w.might - b.def + (fw.answers and 100 or 0)", false);
  CHECK_EQ(w.forecast("sw", "ax", "iron_sword").attack.damage, 8);  // counterAttacks off: nobody answers
}

TEST(gaps_fw_answers_a_weapon_prop_of_that_name_wins) {
  tritest::World w("duel", [](const std::string& dir) {
    tritest::swapText(dir + "/game.json", "\"damage\":  \"a.str + w.might - b.def + 3 * tri(w, fw)\"", "\"damage\": \"fw.answers or -1\"");
    tritest::swapText(dir + "/data/weapons.json", "\"might\": 6, \"weight\": 5", "\"might\": 6, \"answers\": 42, \"weight\": 5");
  });
  CHECK_EQ(w.forecast("sw", "ax", "iron_sword").attack.damage, 42);  // the game's own prop
  CHECK_EQ(w.forecast("sw", "la", "iron_sword").attack.damage, 1);   // nil -> -1, clamped to the least damage
}
