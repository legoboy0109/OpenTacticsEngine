// The data side of the fidelity options on games on disk (tests/fixtures/srpg-conditions and srpg-ranks): the loader and its refusals,
// a staff's `xpMiss`, an ability's `xp` and `inflict`, a unit type's `maxLevel`, `computerCaps`.
#include "srpg_conditions_fixture.h"

using namespace ote;
using namespace condtest;
using rankstest::saysIn;

namespace {

const char* kAbilities = R"({ "abilities": [
  { "id": "venom_touch", "name": "Venom Touch", "target": "enemy", "range": [1, 1], "effect": { "push": 1 },
    "inflict": { "condition": "poison", "turns": 2 }, "xp": 4 },
  { "id": "dance", "name": "Dance", "target": "ally", "range": [1, 1], "effect": { "refresh": true },
    "inflict": { "condition": "curse" }, "xp": "a.level * 10 + 5", "cost": "free" } ] })";

// The conditions fixture with kay (the fighter type) given both abilities.
void withAbilities(Copy& c) {
  c.put("data/abilities.json", kAbilities);
  c.swap("data/units.json", "\"wields\": [\"sword\", \"lance\"] },\n  { \"id\": \"mage\"", "\"wields\": [\"sword\", \"lance\"], \"abilities\": [\"venom_touch\", \"dance\"] },\n  { \"id\": \"mage\"");
}

int abilityIndex(Field& f, const char* id) { return f.rig.db.findAbility(id); }

// The conditions fixture's "poison_staff" line, with the staff's hit formula and an extra key replaced.
void staffMissing(Copy& c, const char* hit, const char* extra) {
  c.swap("data/items.json", "\"hit\": \"70 + a.mag * 5 - b.res * 5 - dist * 2\", \"xp\": 10",
         std::string("\"hit\": \"") + hit + "\", \"xp\": 10" + extra);
}

}  // namespace

// ---- a staff's xpMiss ----

TEST(fidelity_staff_miss_pays_xp_miss_and_a_hit_pays_xp) {
  Field f("fid_xpmiss", "field", [](Copy& c) { staffMissing(c, "0", ", \"xpMiss\": 3"); });
  f.place("mira", 3, 2);
  const int before = f.unit("mira").xp;
  CHECK(f.useStaff("mira", "poison_staff", "r1"));
  CHECK_EQ(f.count(EventType::StaffMissed), 1);
  CHECK_EQ(f.unit("mira").xp - before, 3);  // the miss pays xpMiss, not xp (10)
  Field g("fid_xphit", "field", [](Copy& c) { staffMissing(c, "100", ", \"xpMiss\": 3"); });
  g.place("mira", 3, 2);
  CHECK(g.useStaff("mira", "poison_staff", "r1"));
  CHECK_EQ(g.count(EventType::StaffMissed), 0);
  CHECK_EQ(g.unit("mira").xp, 10);  // a hit pays xp as always
}

TEST(fidelity_staff_miss_without_xp_miss_pays_xp_as_today) {
  Field f("fid_xpmiss_none", "field", [](Copy& c) { staffMissing(c, "0", ""); });
  f.place("mira", 3, 2);
  CHECK(f.useStaff("mira", "poison_staff", "r1"));
  CHECK_EQ(f.count(EventType::StaffMissed), 1);
  CHECK_EQ(f.unit("mira").xp, 10);
}

TEST(fidelity_staff_xp_miss_can_be_a_formula_and_zero) {
  Field f("fid_xpmiss_formula", "field", [](Copy& c) { staffMissing(c, "0", ", \"xpMiss\": \"a.level + 1\""); });
  f.place("mira", 3, 2);
  CHECK(f.useStaff("mira", "poison_staff", "r1"));
  CHECK_EQ(f.unit("mira").xp, f.unit("mira").level + 1);
  Field g("fid_xpmiss_zero", "field", [](Copy& c) { staffMissing(c, "0", ", \"xpMiss\": 0"); });
  g.place("mira", 3, 2);
  CHECK(g.useStaff("mira", "poison_staff", "r1"));
  CHECK_EQ(g.unit("mira").xp, 0);
}

TEST(fidelity_staff_xp_miss_refusals) {
  Copy c("fid_xpmiss_refuse", "srpg-conditions");
  c.swap("data/items.json", "\"hit\": \"70 + a.mag * 5 - b.res * 5 - dist * 2\", \"xp\": 10", "\"hit\": \"50\", \"xp\": 10, \"xpMiss\": -2");
  CHECK(saysIn(c.gameError(), "\"xpMiss\" must be a whole number from 0 to"));
  Copy d("fid_xpmiss_nohit", "srpg-conditions");
  d.swap("data/items.json", "\"hit\": \"70 + a.mag * 5 - b.res * 5 - dist * 2\", \"xp\": 10", "\"xp\": 10, \"xpMiss\": 2");
  CHECK(saysIn(d.gameError(), "\"xpMiss\" needs a \"hit\""));
}

// ---- abilities: xp and inflict ----

TEST(fidelity_ability_inflicts_a_condition_and_gives_xp) {
  Field f("fid_ability", "field", withAbilities);
  const int touch = abilityIndex(f, "venom_touch");
  CHECK(touch >= 0);
  f.place("kay", 6, 4);  // next to Rye at 7,4 (a free square behind him)
  const int xp = f.unit("kay").xp;
  CHECK(f.run(Action::useAbility(f.id("kay"), touch, f.id("r3"))));
  CHECK(f.has("r3", "poison"));
  CHECK_EQ(f.left("r3", "poison"), 2);  // "turns": 2 overrides the condition's own 3
  CHECK_EQ(f.unit("kay").xp - xp, 4);
  CHECK_EQ(f.count(EventType::ConditionApplied), 1);
}

TEST(fidelity_ability_dance_inflicts_on_an_ally_with_an_xp_formula) {
  Field f("fid_dance", "field", withAbilities);
  const int dance = abilityIndex(f, "dance");
  f.place("kay", 1, 4);
  f.unit("nell").done = true;  // it has acted: the refresh has something to do
  f.unit("nell").acted = true;
  const int xp = f.unit("kay").xp;
  CHECK(f.run(Action::useAbility(f.id("kay"), dance, f.id("nell"))));
  CHECK(!f.unit("nell").done);                // refreshed
  CHECK(f.has("nell", "curse"));              // the downside
  CHECK_EQ(f.left("nell", "curse"), 3);       // the condition's own duration
  CHECK_EQ(f.unit("kay").xp - xp, f.unit("kay").level * 10 + 5);
  CHECK(!f.unit("kay").done);                 // a free action: kay can still act
}

TEST(fidelity_ability_inflict_respects_immunity) {
  Field f("fid_immune", "field", [](Copy& c) {
    withAbilities(c);
    // Bo (knight with iron_will: immune to poison and sleep) is the one the poison touch lands on.
  });
  const int touch = abilityIndex(f, "venom_touch");
  f.unit("bo").side = f.unit("r1").side;  // make Bo an enemy of kay for this check
  f.place("kay", 2, 3);                  // next to Bo at 2,4
  CHECK(f.run(Action::useAbility(f.id("kay"), touch, f.id("bo"))));
  CHECK(!f.has("bo", "poison"));
  CHECK_EQ(f.count(EventType::ConditionResisted), 1);
}

TEST(fidelity_ability_without_the_keys_is_as_before) {
  Field f("fid_ability_plain", "field", [](Copy& c) {
    withAbilities(c);
    c.swap("data/abilities.json", ",\n    \"inflict\": { \"condition\": \"poison\", \"turns\": 2 }, \"xp\": 4", "");
  });
  const int touch = abilityIndex(f, "venom_touch");
  f.place("kay", 6, 4);
  const int xp = f.unit("kay").xp;
  CHECK(f.run(Action::useAbility(f.id("kay"), touch, f.id("r3"))));
  CHECK(!f.has("r3", "poison"));
  CHECK_EQ(f.unit("kay").xp, xp);
}

TEST(fidelity_ability_loader_refusals) {
  struct Case { const char* from; const char* to; const char* says; };
  const Case cases[] = {
      {"\"inflict\": { \"condition\": \"poison\", \"turns\": 2 }", "\"inflict\": { \"condition\": \"nonesuch\" }", "unknown condition"},
      {"\"inflict\": { \"condition\": \"poison\", \"turns\": 2 }", "\"inflict\": { \"condition\": \"poison\", \"chance\": 50 }", "unknown key"},
      {"\"inflict\": { \"condition\": \"poison\", \"turns\": 2 }", "\"inflict\": \"poison\"", "\"inflict\" must be"},
      {"\"xp\": 4", "\"xp\": -1", "\"xp\" must be a whole number"},
      {"\"xp\": 4", "\"xp\": true", "\"xp\" must be a whole number"},
  };
  for (const Case& k : cases) {
    Copy c("fid_ability_refuse", "srpg-conditions");
    withAbilities(c);
    c.swap("data/abilities.json", k.from, k.to);
    CHECK(saysIn(c.gameError(), k.says));
  }
  Copy summon("fid_ability_summon", "srpg-conditions");
  withAbilities(summon);
  summon.put("data/abilities.json", R"({ "abilities": [ { "id": "call", "target": "cell", "range": [1, 2], "effect": { "summon": { "type": "raider" } },
    "inflict": { "condition": "poison" } } ] })");
  CHECK(saysIn(summon.gameError(), "\"inflict\" needs a target unit"));
  Copy none("fid_ability_noprog", "srpg-conditions");
  withAbilities(none);
  none.swap("game.json", "\"progression\": { \"enabled\": true,", "\"progression\": { \"enabled\": false,");
  CHECK(saysIn(none.gameError(), "\"xp\" needs experience"));
}

// ---- rules.progression extras ----

TEST(fidelity_progression_extras_load_and_refuse) {
  Copy c("fid_prog", "srpg-conditions");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.rules.xpNoDamageExpr < 0 && db.rules.computerGains);  // a game without the keys: today's
  c.swap("game.json", "\"maxLevel\": 20,", "\"maxLevel\": 20, \"xpNoDamage\": 5, \"computerGains\": false,");
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.rules.xpNoDamageExpr >= 0 && !db.rules.computerGains);
  c.swap("game.json", "\"xpNoDamage\": 5", "\"xpNoDamage\": -3");
  CHECK(saysIn(c.gameError(), "\"xpNoDamage\" must be a whole number"));
  c.swap("game.json", "\"xpNoDamage\": -3", "\"xpNoDamage\": \"a.level +\"");
  Formulas compiled;
  CHECK(loadGame(c.dir, db, err) && !compiled.init(db, err));  // a formula that does not compile stops the game starting
  c.swap("game.json", "\"xpNoDamage\": \"a.level +\", \"computerGains\": false,", "\"computerGains\": 1,");
  CHECK(!c.gameError().empty());
}

TEST(fidelity_xp_no_damage_in_a_loaded_game_and_computer_gains) {
  // Kay fights Rook with hit 0: no damage either way; xpNoDamage pays both fighters, and computerGains false only kay.
  auto tweak = [](bool computerGains) {
    return [computerGains](Copy& c) {
      c.swap("game.json", "\"formulas\": { \"damage\": \"a.str + w.might - b.def\", \"hit\": \"90\" }", "\"formulas\": { \"damage\": \"a.str + w.might - b.def\", \"hit\": \"0\" }");
      c.swap("game.json", "\"maxLevel\": 20,", std::string("\"maxLevel\": 20, \"xpNoDamage\": \"a.level + 2\", \"computerGains\": ") + (computerGains ? "true" : "false") + ",");
    };
  };
  Field f("fid_nodamage_a", "field", tweak(true));
  f.place("kay", 4, 2);
  CHECK(f.strike("kay", "r1"));
  CHECK_EQ(f.unit("kay").xp, 3);
  CHECK_EQ(f.unit("r1").xp, 3);
  Field g("fid_nodamage_b", "field", tweak(false));
  g.place("kay", 4, 2);
  CHECK(g.strike("kay", "r1"));
  CHECK_EQ(g.unit("kay").xp, 3);
  CHECK_EQ(g.unit("r1").xp, 0);
}

// ---- level cap per unit type ----

TEST(fidelity_unit_max_level_loads_refuses_and_caps_a_loaded_unit) {
  Field f("fid_maxlevel", "field", [](Copy& c) {
    c.swap("data/units.json", "\"wields\": [\"sword\", \"lance\"] },\n  { \"id\": \"mage\"", "\"wields\": [\"sword\", \"lance\"], \"maxLevel\": 1 },\n  { \"id\": \"mage\"");
  });
  const int fighter = f.rig.db.findUnitType("fighter");
  CHECK_EQ(f.rig.db.unitTypes[(size_t)fighter].maxLevel, 1);
  CHECK_EQ(f.rig.db.unitTypes[(size_t)f.rig.db.findUnitType("mage")].maxLevel, -1);
  CHECK_EQ(f.b().levelCap(f.unit("kay")), 1);
  CHECK_EQ(f.b().levelCap(f.unit("mira")), 20);
  for (const char* bad : {"-1", "100", "\"x\"", "1.5"}) {
    Copy c("fid_maxlevel_refuse", "srpg-conditions");
    c.swap("data/units.json", "\"wields\": [\"sword\", \"lance\"] },\n  { \"id\": \"mage\"", std::string("\"wields\": [\"sword\", \"lance\"], \"maxLevel\": ") + bad + " },\n  { \"id\": \"mage\"");
    CHECK(saysIn(c.gameError(), "\"maxLevel\" must be a whole number from 0"));
  }
  Copy n("fid_maxlevel_noprog", "srpg-conditions");
  n.swap("data/units.json", "\"wields\": [\"sword\", \"lance\"] },\n  { \"id\": \"mage\"", "\"wields\": [\"sword\", \"lance\"], \"maxLevel\": 3 },\n  { \"id\": \"mage\"");
  n.swap("game.json", "\"progression\": { \"enabled\": true,", "\"progression\": { \"enabled\": false,");
  std::string items = n.read("data/items.json");  // the staves' own "xp" would be refused first
  for (size_t at = items.find(", \"xp\": 10"); at != std::string::npos; at = items.find(", \"xp\": 10")) items.erase(at, 11);
  n.put("data/items.json", items);
  CHECK(saysIn(n.gameError(), "\"maxLevel\" needs experience"));
}

TEST(fidelity_promotion_resets_to_the_new_types_own_cap) {
  // The cap belongs to the type the unit is NOW: a promotion to a type with no cap of its own goes back to the game's.
  Field f("fid_maxlevel_promo", "field", [](Copy& c) {
    c.swap("data/units.json", "\"wields\": [\"sword\", \"lance\"] },\n  { \"id\": \"mage\"", "\"wields\": [\"sword\", \"lance\"], \"maxLevel\": 1 },\n  { \"id\": \"mage\"");
  });
  Unit& kay = f.unit("kay");
  CHECK_EQ(f.b().levelCap(kay), 1);
  kay.type = f.rig.db.findUnitType("knight");  // what a promotion does to the unit's type
  CHECK_EQ(f.b().levelCap(kay), 20);
}

// ---- stat caps for computer units ----

TEST(fidelity_computer_caps_loader) {
  Copy c("fid_ccaps", "srpg-ranks");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.computerCaps.empty());
  c.put("data/caps.json", "{ \"caps\": { \"str\": 10, \"hp\": 40 }, \"computerCaps\": { \"hp\": 60 } }");
  CHECK(loadGame(c.dir, db, err));
  CHECK_EQ(db.computerCaps[(size_t)db.findStat("hp")], 60);
  CHECK_EQ(db.computerCaps[(size_t)db.findStat("str")], -1);
  CHECK_EQ(db.statCaps[(size_t)db.findStat("hp")], 40);
  c.put("data/caps.json", "{ \"caps\": { \"hp\": 40 }, \"computerCaps\": { \"nonesuch\": 60 } }");
  CHECK(saysIn(c.gameError(), "names unknown stat"));
  c.put("data/caps.json", "{ \"caps\": { \"hp\": 40 }, \"computerCaps\": { \"hp\": 0 } }");
  CHECK(saysIn(c.gameError(), "must be a whole number from 1 to"));
  c.put("data/caps.json", "{ \"caps\": { \"hp\": 40 }, \"computerCaps\": [] }");
  CHECK(!c.gameError().empty());
  c.put("data/caps.json", "{ \"caps\": { \"hp\": 40 }, \"cpuCaps\": {} }");
  CHECK(saysIn(c.gameError(), "unknown key"));
}
