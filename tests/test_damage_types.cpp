// Docs/SRPG.md "Damage types": game.json "damageTypes" / "resistRange", a weapon's "damageType", the "resist" of unit types and
// characters, the formula variables `rs` / `ha` / `hb` / `dh` / `w.damageType`, "ignoresTerrain", the skill condition, the info rows
// and the computer opponent. Fixture tests/fixtures/damage-types: every fighter has str 6 / def 2 and the damage formula is
// max(1, str + might - def - tb.def - rs); every weapon has might 4, so a plain hit does 8. Woods (1,1) give tb.def 2.
#include "battle/unit_info_rows.h"
#include "core/ai/ai.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::saysIn;
using skilltest::World;

namespace {

const char* kFix = "damage-types";

std::string fixtureDir() { return ote_test::sourceDir() + "/tests/fixtures/damage-types"; }

Forecast forecastOf(World& w, const char* from, const char* to, int cellX = -1, int cellY = -1) {
  Forecast f;
  Unit& a = w.unit(from);
  const Cell at = cellX < 0 ? a.cell : w.at(cellX, cellY);
  CHECK(w.b().forecast(w.id(from), w.id(to), w.b().restingWeapon(a), at, f));
  return f;
}

// The info panel's resistance rows of a unit as "Label=value;" (what statRows lists beyond the stats).
std::string resistRows(World& w, const Unit& u) {
  std::string out;
  for (const InfoRow& r : statRows(w.b(), u))
    if (r.label.find(" res") != std::string::npos) out += r.label + "=" + r.value + ";";
  return out;
}

}  // namespace

// ---- loader ----

TEST(damage_types_fixture_loads_with_the_declared_types) {
  Copy c("dt_load", kFix);
  CHECK((c.gameError()) == std::string());
  World w("field", fixtureDir());
  const DamageTypes& dt = w.db().damageTypes;
  CHECK_EQ((int)dt.list.size(), 3);
  CHECK(dt.list[2].ignoresTerrain && !dt.list[0].ignoresTerrain);
  CHECK(dt.hasRange && dt.resistMin == -3 && dt.resistMax == 3);
  const Database& db = w.db();
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("slasher")].damageType, 0);  // the first listed type by default
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("zapper")].damageType, 1);
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("stick")].damageType, 1);  // no "damageType", but "damageKind": "emf" names a declared type
}

TEST(damage_types_loader_refuses_bad_data_naming_the_key) {
  Copy c("dt_bad", kFix);
  c.swap("data/weapons.json", "\"damageType\": \"emf\"", "\"damageType\": \"plasma\"");
  CHECK(saysIn(c.gameError(), "\"damageType\" names unknown damage type \"plasma\""));
  Copy dup("dt_dup", kFix);
  dup.swap("game.json", "{ \"id\": \"emf\", \"name\": \"EMF\" }", "{ \"id\": \"kinetic\" }");
  CHECK(saysIn(dup.gameError(), "duplicate id"));
  Copy key("dt_key", kFix);
  key.swap("game.json", "{ \"id\": \"emf\", \"name\": \"EMF\" }", "{ \"id\": \"emf\", \"colour\": 3 }");
  CHECK(saysIn(key.gameError(), "unknown key \"colour\""));
  Copy frac("dt_frac", kFix);
  frac.swap("data/units.json", "\"fire\": -3", "\"fire\": 1.5");
  CHECK(saysIn(frac.gameError(), "\"resist\".\"fire\" must be a whole number"));
  Copy unk("dt_unk", kFix);
  unk.swap("data/units.json", "\"fire\": -3", "\"plasma\": 1");
  CHECK(saysIn(unk.gameError(), "\"resist\" names unknown damage type \"plasma\""));
  Copy range("dt_range", kFix);
  range.swap("data/units.json", "\"fire\": -3", "\"fire\": 4");
  CHECK(saysIn(range.gameError(), "must be from -3 to 3"));
  Copy chr("dt_chr", kFix);
  chr.swap("data/characters.json", "\"kinetic\": -1", "\"plasma\": -1");
  CHECK(saysIn(chr.gameError(), "\"resist\" names unknown damage type \"plasma\""));
  Copy none("dt_none", kFix);
  none.dropValue("game.json", "damageTypes");
  none.dropValue("game.json", "resistRange");
  CHECK(saysIn(none.gameError(), "needs \"damageTypes\" in game.json"));
  Copy cond("dt_cond", kFix);
  cond.swap("data/skills.json", "\"damageType\": \"fire\" }", "\"damageType\": \"plasma\" }");
  CHECK(saysIn(cond.gameError(), "\"damageType\" names no damage type"));
}

// ---- the formula side ----

TEST(damage_types_rs_in_forecast_counter_and_terrain) {
  World w("field", fixtureDir());
  // Kinetic into plating (resist kinetic 3) standing in woods (def 2): 6 + 4 - 2 - 2 - 3 = 3. Its answer, kinetic into a gunner with no
  // resistance on plain ground: 8.
  Forecast f = forecastOf(w, "gunner", "wall");
  CHECK_EQ(f.attack.damage, 3);
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.damage, 8);
  // EMF into plating (resist emf -2), from two squares away so nothing answers: 6 + 4 - 2 - 2 + 2 = 8.
  f = forecastOf(w, "zapper", "wall");
  CHECK_EQ(f.attack.damage, 8);
  CHECK(!f.counter.can);
  // Fire (ignoresTerrain) into the same plating from the square below: the woods' defense is not counted: 6 + 4 - 2 - 0 - 1 = 7.
  f = forecastOf(w, "burner", "wall", 1, 2);
  CHECK_EQ(f.attack.damage, 7);
  CHECK_EQ(f.counter.damage, 8);
  // The counter uses the answering weapon's type: tin (fire resist -3) is answered by a torch: 6 + 4 - 2 + 3 = 11; its own hit is 8.
  f = forecastOf(w, "tin", "tinfoe");
  CHECK_EQ(f.attack.damage, 8);
  CHECK_EQ(f.counter.damage, 11);
  // The minimum of 1 comes from the game's own formula (max(1, ...)).
  f = forecastOf(w, "kid", "wallfoe");
  CHECK_EQ(f.attack.damage, 1);
}

TEST(damage_types_forecast_equals_the_applied_damage) {
  World w("field", fixtureDir());
  const Forecast f = forecastOf(w, "zapper", "wall");
  const int hp = w.unit("wall").hp;
  CHECK(w.apply(Action::attack(w.id("zapper"), w.id("wall"), w.b().restingWeapon(w.unit("zapper")))));
  int dealt = -1;
  for (const Event& e : w.events)
    if (e.type == EventType::Attack && e.a == w.id("zapper")) dealt = e.d;
  CHECK_EQ(dealt, f.attack.damage);
  CHECK_EQ(w.unit("wall").hp, hp - 8);
  // A fight with an answer: gunner into plating, 3 down and 8 back.
  World w2("field", fixtureDir());
  const int gunnerHp = w2.unit("gunner").hp;
  CHECK(w2.apply(Action::attack(w2.id("gunner"), w2.id("wall"), w2.b().restingWeapon(w2.unit("gunner")))));
  CHECK_EQ(w2.unit("wall").hp, 40 - 3);
  CHECK_EQ(w2.unit("gunner").hp, gunnerHp - 8);
}

TEST(damage_types_resist_of_character_overrides_the_unit_type) {
  World w("field", fixtureDir());
  const int ada = w.id("ada");
  CHECK(ada >= 0);
  CHECK_EQ(w.b().resistOf(w.b().units[(size_t)ada], 0), -1);  // the character's own kinetic
  CHECK_EQ(w.b().resistOf(w.b().units[(size_t)ada], 1), -2);  // not written: the unit type's emf
  CHECK_EQ(w.b().resistOf(w.b().units[(size_t)ada], 2), 1);
  CHECK_EQ(w.b().resistOf(w.unit("wall"), 0), 3);
  CHECK_EQ(w.b().resistOf(w.unit("gunner"), 1), 0);  // no entry: 0
  CHECK_EQ(w.b().resistOf(w.unit("gunner"), 9), 0);  // not a type: 0
}

TEST(damage_types_w_damageType_is_text_and_damageKind_is_untouched) {
  Copy c("dt_wtype", kFix);
  c.swap("game.json", "math.max(1, a.str + w.might - b.def - (tb.def or 0) - rs)",
         "(w.damageType == 'kinetic' and 1 or 0) + (w.damageType == 'emf' and 20 or 0) + (w.damageType == 'fire' and 300 or 0) + (w.damageKind == 'emf' and 5000 or 0)");
  World w("field", c.dir);
  CHECK_EQ(forecastOf(w, "gunner", "wall").attack.damage, 1);
  CHECK_EQ(forecastOf(w, "zapper", "wall").attack.damage, 20);
  CHECK_EQ(forecastOf(w, "burner", "wall", 1, 2).attack.damage, 300);
}

TEST(damage_types_height_variables) {
  Copy c("dt_height", kFix);
  c.swap("game.json", "math.max(1, a.str + w.might - b.def - (tb.def or 0) - rs)", "ha * 100 + hb * 10 + dh + 500");
  World w("field", c.dir);
  // The gunner stands at height 2 and the plating at 1: ha 2, hb 1, dh 1; in the answer the plating is `a`: ha 1, hb 2, dh -1.
  const Forecast f = forecastOf(w, "gunner", "wall");
  CHECK_EQ(f.attack.damage, 711);
  CHECK_EQ(f.counter.damage, 619);
  // A map without heights: all 0 (the computer test's map has none).
  Copy c2("dt_height2", kFix);
  c2.swap("game.json", "math.max(1, a.str + w.might - b.def - (tb.def or 0) - rs)", "ha * 100 + hb * 10 + dh + 500");
  std::string ai;
  CHECK(readTextFile(fixtureDir() + "/maps/ai.json", ai));
  c2.put("maps/ai.json", ai);
  World flat("ai", c2.dir);
  CHECK_EQ(forecastOf(flat, "shooter", "soft", 5, 0).attack.damage, 500);
}

TEST(damage_types_skill_condition_tests_the_attackers_type_and_the_foes_with_who) {
  World w("field", fixtureDir());
  // "fire_edge" (damageType fire, +3 damage): pyro's torch into plain-ground plating (fire resist 1): 6 + 4 - 2 - 1 = 7, +3 = 10.
  CHECK_EQ(forecastOf(w, "pyro", "wallfoe").attack.damage, 10);
  // A kinetic weapon does not trigger it.
  CHECK_EQ(forecastOf(w, "kid", "wallfoe").attack.damage, 1);
  // "fire_ward" ("who": "foe": the foe's weapon is fire, -3 damage taken): the burner's torch into the ward from (2,0): 8 - 3 = 5.
  CHECK_EQ(forecastOf(w, "burner", "ward", 2, 0).attack.damage, 5);
  // Against a kinetic attacker the ward takes full damage.
  CHECK_EQ(forecastOf(w, "gunner", "ward", 2, 0).attack.damage, 8);
}

// ---- the game without types ----

TEST(damage_types_a_game_without_types_sees_zero_and_empty) {
  Copy c("dt_without", "srpg-skillblocks2");
  c.swap("game.json", "a.str + w.might * vs - b.def", "a.str + w.might * vs - b.def + rs + ha + hb + dh");
  World w("field", c.dir);
  CHECK(!w.db().damageTypes.on());
  for (const WeaponDef& wd : w.db().weapons) CHECK_EQ(wd.damageType, -1);
  Forecast f;
  CHECK(w.b().forecast(w.id("thief"), w.id("mark"), w.b().restingWeapon(w.unit("thief")), w.unit("thief").cell, f));
  CHECK_EQ(f.attack.damage, 4);  // the same as without the new variables (5 + 3 - 4)
  CHECK_EQ(w.b().resistOf(w.unit("mark"), 0), 0);
}

// ---- the player's rows ----

TEST(damage_types_info_rows_list_non_zero_resistances_only_in_a_typed_game) {
  World w("field", fixtureDir());
  CHECK((resistRows(w, w.unit("wall"))) == std::string("Kinetic res=+3;EMF res=-2;Fire res=+1;"));
  CHECK((resistRows(w, w.unit("tin"))) == std::string("Fire res=-3;"));
  CHECK((resistRows(w, w.unit("gunner"))) == std::string());
  Copy c("dt_rows_plain", "srpg-skillblocks2");
  World plain("field", c.dir);
  CHECK((resistRows(plain, plain.unit("thief"))) == std::string());
}

// ---- the computer opponent ----

namespace {

// The unit the computer's first attack of the turn went at (story id), "" when it did not attack.
std::string firstTarget(World& w) {
  std::vector<Event> ev;
  for (int guard = 0; guard < 6; ++guard) {
    ev.clear();
    const bool more = aiTakeStep(w.b(), ev);
    for (const Event& e : ev)
      if (e.type == EventType::Attack) return w.b().units[(size_t)e.b].storyId;
    if (!more) break;
  }
  return "";
}

}  // namespace

TEST(damage_types_computer_picks_the_target_that_resists_less_either_way_round) {
  {
    World w("ai", fixtureDir());  // soft at (4,0), armor (resists EMF 3) at (4,2)
    CHECK((firstTarget(w)) == std::string("soft"));
  }
  Copy c("dt_ai_swap", kFix);
  std::string ai;
  CHECK(readTextFile(fixtureDir() + "/maps/ai.json", ai));
  c.put("maps/ai.json", ai);
  c.swap("maps/ai.json", "\"y\": 0, \"id\": \"soft\"", "\"y\": 2, \"id\": \"soft\"");
  c.swap("maps/ai.json", "\"y\": 2, \"id\": \"hard\"", "\"y\": 0, \"id\": \"hard\"");
  World w("ai", c.dir);
  CHECK((firstTarget(w)) == std::string("soft"));
}
