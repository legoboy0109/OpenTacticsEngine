// Areas and several effects for abilities (docs/SRPG.md "Areas and several effects for abilities"): an "area" on heal / cure / inflict abilities
// (who in it is acted on, the order, legality, range 0, the line shape, height), a heal that also cures, and a list of "inflict" entries; the
// loader's refusals; the pick hint and the ability's words. Charged areas, determinism and the computer: test_ability_areas_ai.cpp.
// Game: tests/ability_conditions_fixture.h (9 x 3 plain map).
#include <algorithm>

#include "ability_conditions_fixture.h"
#include "battle/ability_area_text.h"
#include "battle/ability_text.h"

using namespace ote;
using namespace acf;

namespace {

// hero (mage, blue) 1,1; pal (knight, blue) 2,1; bandit (brigand, red) 3,1; thug (brigand, red) 4,1. Unit ids 0..3 in that order.
const char* kUnits = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
                     "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":2,\"y\":1},"
                     "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1},"
                     "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":4,\"y\":1}";

const char* kAbilities =
    "{\"id\":\"cura\",\"name\":\"Cura\",\"target\":\"unit\",\"range\":[0,3],\"effect\":{\"heal\":10},\"area\":{\"shape\":\"radius\",\"radius\":1}},"
    "{\"id\":\"curaa\",\"name\":\"Ally Cura\",\"target\":\"ally\",\"range\":[0,3],\"effect\":{\"heal\":10},\"area\":{\"shape\":\"radius\",\"radius\":1}},"
    "{\"id\":\"sleepga\",\"name\":\"Sleepga\",\"target\":\"enemy\",\"range\":[1,4],\"inflict\":{\"condition\":\"sleep\",\"chance\":50},\"area\":{\"shape\":\"burst\"}},"
    "{\"id\":\"barrier\",\"name\":\"Barrier\",\"target\":\"ally\",\"range\":[0,2],\"inflict\":[{\"condition\":\"protect\"},{\"condition\":\"haste\",\"turns\":2}],"
    "\"area\":{\"shape\":\"burst\"}},"
    "{\"id\":\"esunaga\",\"name\":\"Esunaga\",\"target\":\"ally\",\"range\":[0,3],\"effect\":{\"cure\":\"harmful\"},\"area\":{\"shape\":\"radius\",\"radius\":2}},"
    "{\"id\":\"firstaid\",\"name\":\"First Aid\",\"target\":\"unit\",\"range\":[1,1],\"effect\":{\"heal\":5,\"cure\":[\"burn\"]}},"
    "{\"id\":\"gaze\",\"name\":\"Gaze\",\"target\":\"enemy\",\"range\":[1,3],\"inflict\":[{\"condition\":\"sleep\",\"chance\":50},{\"condition\":\"burn\",\"chance\":\"40\"}]},"
    "{\"id\":\"beam\",\"name\":\"Beam\",\"target\":\"enemy\",\"range\":[1,1],\"inflict\":{\"condition\":\"burn\"},\"area\":{\"shape\":\"line\",\"length\":3}}";

Opts areaOpts() {
  Opts o;
  o.abilities = kAbilities;
  o.mageAbilities = "[\"cura\",\"curaa\",\"sleepga\",\"barrier\",\"esunaga\",\"firstaid\",\"gaze\",\"beam\"]";
  o.units = kUnits;
  return o;
}

std::string withArea(const char* name, const std::string& ability, const std::string& rules = "") {
  Opts o = areaOpts();
  o.abilities += "," + ability;
  o.rules = rules;
  return error(make(name, o));
}

Action at(World& w, const char* ability, int x, int y) { return Action::useAbility(w.id("hero"), w.ability(ability), -1, w.b().grid.at(x, y)); }

void give(Unit& u, int condition, int left) { u.conditions.push_back(ConditionState{condition, left}); }

}  // namespace

// ---- loader ----

TEST(abarea_loads_area_range_zero_heal_and_cure_and_inflict_lists) {
  Database db;
  std::string err;
  CHECK(loadGame(make("abarea_load", areaOpts()), db, err));
  CHECK_STR(err, "");
  const AbilityDef& cura = db.abilities[(size_t)db.findAbility("cura")];
  CHECK(cura.area.shape == AreaShape::Radius && cura.area.radius == 1 && cura.area.vertical == -1 && !cura.area.friendlyFire);
  CHECK(cura.minRange == 0 && cura.maxRange == 3 && cura.aimsSquare() && cura.target == AbilityTarget::Unit);
  CHECK(db.abilities[(size_t)db.findAbility("sleepga")].area.radius == 1);  // a burst is radius 1
  const AbilityDef& aid = db.abilities[(size_t)db.findAbility("firstaid")];
  CHECK(aid.effect == AbilityEffect::Heal && aid.healCures && aid.amount == 5 && aid.cures == std::vector<int>({db.findCondition("burn")}));
  CHECK(!aid.area.on() && !aid.aimsSquare());
  const AbilityDef& barrier = db.abilities[(size_t)db.findAbility("barrier")];
  CHECK_EQ(barrier.inflicts.size(), (size_t)2);
  CHECK(barrier.inflicts[0].cond == db.findCondition("protect") && barrier.inflicts[1].cond == db.findCondition("haste") && barrier.inflicts[1].turns == 2);
  CHECK(barrier.inflictCond == db.findCondition("protect"));  // the first entry is mirrored into the single fields
  const AbilityDef& gaze = db.abilities[(size_t)db.findAbility("gaze")];
  CHECK(gaze.inflicts[0].chance == 50 && gaze.inflicts[0].rolls() && gaze.inflicts[1].chanceExpr >= 0 && gaze.inflictChance == 50);
  // A single object is one entry, exactly as before.
  const AbilityDef& beam = db.abilities[(size_t)db.findAbility("beam")];
  CHECK(beam.inflicts.size() == 1 && !beam.inflicts[0].rolls() && beam.area.shape == AreaShape::Line && beam.area.length == 3);
}

TEST(abarea_loader_refusals) {
  const char* fx = "\"area\":{\"shape\":\"burst\"}";
  CHECK(says(withArea("r1", std::string("{\"id\":\"x\",\"target\":\"unit\",\"effect\":{\"push\":1},") + fx + "}"), "\"area\" goes with \"heal\", \"cure\""));
  CHECK(says(withArea("r2", std::string("{\"id\":\"x\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"knight\"}},") + fx + "}"),
             "\"area\" goes with"));
  CHECK(says(withArea("r3", std::string("{\"id\":\"x\",\"effect\":{\"strike\":\"fire_spell\"},") + fx + "}"), "\"strike\" takes its area from the weapon"));
  CHECK(says(withArea("r4", std::string("{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"refresh\":true},") + fx + "}"), "\"area\" goes with"));
  CHECK(says(withArea("r5", std::string("{\"id\":\"x\",\"target\":\"enemy\",\"effect\":{\"steal\":true},") + fx + "}"), "\"area\" goes with"));
  CHECK(says(withArea("r6", std::string("{\"id\":\"x\",\"target\":\"self\",\"effect\":{\"heal\":5},") + fx + "}"),
             "\"area\" needs \"target\" \"ally\", \"enemy\" or \"unit\""));
  CHECK(says(withArea("r7", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\",\"friendlyFire\":true}}"),
             "\"friendlyFire\" is for weapons"));
  CHECK(says(withArea("r8", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\",\"apExtra\":1}}"), "\"apExtra\" is for weapons"));
  CHECK(says(withArea("r9", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\",\"size\":1}}"),
             "\"area\" has an unknown key \"size\""));
  CHECK(says(withArea("r10", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"radius\"}}"), "needs a \"radius\""));
  CHECK(says(withArea("r11", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"blob\"}}"), "\"shape\" must be"));
  CHECK(says(withArea("r12", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\",\"vertical\":1}}"),
             "\"vertical\" needs \"rules\""));
  CHECK(says(withArea("r13", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":5}"), "area"));
  // Range 0 only with an area.
  CHECK(says(withArea("r14", "{\"id\":\"x\",\"target\":\"ally\",\"range\":[0,2],\"effect\":{\"heal\":5}}"), "whole numbers from 1 to 20"));
  CHECK(says(withArea("r15", "{\"id\":\"x\",\"target\":\"ally\",\"range\":[-1,2],\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\"}}"),
             "whole numbers from 0 to 20 (0 = the user's own square)"));
  // An area has no one target unit: "who": "foe" conditions are refused.
  CHECK(says(withArea("r16", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\"},"
                             "\"if\":[{\"hp\":{\"op\":\"<\",\"percent\":50},\"who\":\"foe\"}]}"),
             "with an \"area\""));
  // Only heal and cure go together.
  CHECK(says(withArea("e1", "{\"id\":\"x\",\"target\":\"unit\",\"effect\":{\"heal\":5,\"push\":1}}"), "(only heal and cure may go together)"));
  CHECK(says(withArea("e2", "{\"id\":\"x\",\"target\":\"enemy\",\"effect\":{\"heal\":5,\"cure\":\"all\"}}"), "\"heal\" cannot target \"enemy\""));
  CHECK(says(withArea("e3", "{\"id\":\"x\",\"target\":\"unit\",\"effect\":{\"heal\":5,\"cure\":\"nope\"}}"), "\"cure\" must be"));
  CHECK(says(withArea("e4", "{\"id\":\"x\",\"target\":\"unit\",\"effect\":{\"heal\":5,\"cure\":[\"frost\"]}}"), "unknown condition \"frost\""));
  // Inflict lists.
  CHECK(says(withArea("i1", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":[]}"), "a list of 1 to 8"));
  CHECK(says(withArea("i2", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":[{\"condition\":\"sleep\"},{\"condition\":\"sleep\"}]}"), "names \"sleep\" twice"));
  CHECK(says(withArea("i3", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":[{\"condition\":\"sleep\"},{\"condition\":\"frost\"}]}"),
             "entry 2: \"inflict\" names unknown condition \"frost\""));
  CHECK(says(withArea("i4", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":[{\"condition\":\"sleep\",\"chance\":0}]}"), "entry 1: \"inflict\": \"chance\" must be"));
  CHECK(says(withArea("i5", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":[{\"condition\":\"sleep\"},5]}"), "entry 2"));
  CHECK(says(withArea("i6", "{\"id\":\"x\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"knight\"}},\"inflict\":[{\"condition\":\"sleep\"}]}"),
             "\"inflict\" needs a target unit"));
  // The same keys load where they fit.
  CHECK_STR(withArea("ok1", "{\"id\":\"x\",\"target\":\"enemy\",\"range\":[0,0],\"inflict\":[{\"condition\":\"sleep\"}],\"area\":{\"shape\":\"cross\",\"radius\":2}}"), "");
  CHECK_STR(withArea("ok2", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":{\"percent\":30},\"cure\":\"harmful\"},\"area\":{\"shape\":\"burst\"}}"), "");
  CHECK_STR(withArea("ok3", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\",\"vertical\":1}}",
                     "\"height\":{\"rangeBonus\":{\"perLevels\":1,\"max\":2}},"),
            "");
}

TEST(abarea_charge_targets_unit_is_refused_with_an_area) {
  const std::string speed = "\"turnOrder\":{\"mode\":\"speed\",\"stat\":\"mov\",\"threshold\":100},";
  CHECK(says(withArea("ch1", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\"},\"chargeSpeed\":20,\"chargeTargets\":\"unit\"}", speed),
             "keeps the square it aims at"));
  CHECK_STR(withArea("ch2", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"area\":{\"shape\":\"burst\"},\"chargeSpeed\":20,\"chargeTargets\":\"cell\"}", speed), "");
}

// ---- an area ----

TEST(abarea_heals_every_unit_the_target_accepts_in_unit_id_order) {
  World w(make("abarea_unit", areaOpts()));
  w.unit("pal").hp = 20;
  w.unit("bandit").hp = 10;
  w.unit("thug").hp = 10;  // 4,1: outside a radius 1 around 2,1
  CHECK(w.run(at(w, "cura", 2, 1)));
  const Event* used = w.first(EventType::AbilityUsed);
  CHECK(used && used->c == -1 && used->d == w.b().grid.at(2, 1) && (used->flags & kArea));
  CHECK(used && used->data == std::vector<int>({w.id("pal"), w.id("bandit")}));  // the hero is at full HP: not acted on
  CHECK_EQ(w.count(EventType::UnitHealed), 2);
  CHECK_EQ(w.unit("pal").hp, 30);
  CHECK_EQ(w.unit("bandit").hp, 20);  // "unit": foes too
  CHECK_EQ(w.unit("thug").hp, 10);
  CHECK(w.unit("hero").done);  // "cost": "action" by default
  // The heals come in unit-id order, after AbilityUsed.
  int seen = 0;
  for (const Event& e : w.events)
    if (e.type == EventType::UnitHealed) CHECK_EQ(e.a, seen++ == 0 ? w.id("pal") : w.id("bandit"));
}

TEST(abarea_ally_filter_includes_the_user_and_leaves_foes_out) {
  World w(make("abarea_ally", areaOpts()));
  w.unit("hero").hp = 5;
  w.unit("pal").hp = 5;
  w.unit("bandit").hp = 5;
  CHECK(w.run(at(w, "curaa", 2, 1)));
  CHECK_EQ(w.unit("hero").hp, 15);  // the user is in the area: it is healed too
  CHECK_EQ(w.unit("pal").hp, 15);
  CHECK_EQ(w.unit("bandit").hp, 5);  // an enemy: "ally" leaves it out
}

TEST(abarea_range_zero_aims_at_the_users_own_square) {
  World w(make("abarea_zero", areaOpts()));
  w.unit("hero").hp = 5;
  std::vector<int> us;
  std::vector<Cell> cells;
  w.b().abilityTargets(w.id("hero"), w.ability("curaa"), w.unit("hero").cell, us, cells);
  CHECK(us.empty());
  CHECK(std::find(cells.begin(), cells.end(), w.unit("hero").cell) != cells.end());
  CHECK(w.run(at(w, "curaa", 1, 1)));
  CHECK_EQ(w.unit("hero").hp, 15);
}

TEST(abarea_is_legal_only_where_someone_would_be_affected) {
  World w(make("abarea_legal", areaOpts()));
  // Everybody at full HP: no square of "cura" has anyone to heal.
  CHECK_STR(w.why(at(w, "cura", 2, 1)), "nobody there would be affected");
  std::vector<AbilityOption> opts;
  w.b().abilityOptions(w.id("hero"), w.unit("hero").cell, opts);
  for (const AbilityOption& o : opts)
    if (o.ability == w.ability("cura")) CHECK(!o.ok && o.why == "nothing to target");
  w.unit("pal").hp = 20;
  CHECK_STR(w.why(at(w, "cura", 2, 1)), "");
  CHECK_STR(w.why(at(w, "cura", 2, 0)), "");                  // 2,1 is next to 2,0
  CHECK_STR(w.why(at(w, "cura", 0, 0)), "nobody there would be affected");
  CHECK_STR(w.why(at(w, "cura", 5, 1)), "that square is out of range");  // 4 squares away (range 0-3)
  // The squares offered are exactly the legal ones.
  std::vector<int> us;
  std::vector<Cell> cells;
  w.b().abilityTargets(w.id("hero"), w.ability("cura"), w.unit("hero").cell, us, cells);
  for (Cell c : cells) CHECK_STR(w.why(Action::useAbility(w.id("hero"), w.ability("cura"), -1, c)), "");
  CHECK(!cells.empty());
}

TEST(abarea_enemy_area_rolls_once_per_unit_in_id_order) {
  World w(make("abarea_sleepga", areaOpts()), 5);
  const Rng before = w.b().rng;
  CHECK(w.run(at(w, "sleepga", 4, 1)));  // a burst around 4,1: bandit (3,1) and thug (4,1)
  Rng r = before;
  int rolled = 0;
  for (const Event& e : w.events) {
    if (e.type == EventType::ConditionApplied || (e.type == EventType::ConditionResisted && (e.flags & kMissed))) {
      CHECK_EQ(e.a, rolled == 0 ? w.id("bandit") : w.id("thug"));
      ++rolled;
    }
  }
  CHECK_EQ(rolled, 2);
  r.range(100);
  r.range(100);
  CHECK(w.b().rng.state == r.state);  // exactly two draws
  CHECK(!w.has("pal", "sleep") && !w.has("hero", "sleep"));
}

TEST(abarea_inflict_list_in_an_ally_area_applies_each_in_order) {
  World w(make("abarea_barrier", areaOpts()));
  CHECK(w.run(at(w, "barrier", 1, 1)));  // a burst on the user: hero and pal (2,1)
  std::vector<std::pair<int, int>> applied;
  for (const Event& e : w.events)
    if (e.type == EventType::ConditionApplied) applied.push_back({e.a, e.b});
  const std::vector<std::pair<int, int>> want = {{w.id("hero"), w.cond("protect")}, {w.id("hero"), w.cond("haste")},
                                                {w.id("pal"), w.cond("protect")}, {w.id("pal"), w.cond("haste")}};
  CHECK(applied == want);
  // Both have both now: nobody is left to affect there.
  w.unit("hero").done = false;
  CHECK_STR(w.why(at(w, "barrier", 1, 1)), "nobody there would be affected");
}

TEST(abarea_cure_area_and_line_shape) {
  World w(make("abarea_cure_line", areaOpts()));
  give(w.unit("pal"), w.cond("burn"), 2);
  give(w.unit("bandit"), w.cond("burn"), 2);
  CHECK(w.run(at(w, "esunaga", 1, 1)));
  CHECK(!w.has("pal", "burn"));
  CHECK(w.has("bandit", "burn"));  // "ally": a foe keeps its curse
  // A line of 3 aimed at the square next to the user: 2,1 (pal: an ally, left out), 3,1 (bandit), 4,1 (thug).
  World v(make("abarea_line", areaOpts()));
  CHECK(v.run(at(v, "beam", 2, 1)));
  CHECK(v.has("bandit", "burn") && v.has("thug", "burn") && !v.has("pal", "burn"));
  // A line needs a direction: the user's own square is not in range [1, 1] anyway, and a diagonal is out of range.
  World x(make("abarea_line2", areaOpts()));
  CHECK_STR(x.why(at(x, "beam", 1, 0)), "nobody there would be affected");
}

TEST(abarea_vertical_leaves_out_squares_of_another_height) {
  Opts o = areaOpts();
  o.rules = "\"height\":{\"rangeBonus\":{\"perLevels\":1,\"max\":2}},";
  o.abilities += ",{\"id\":\"flat\",\"name\":\"Flat\",\"target\":\"unit\",\"range\":[0,3],\"effect\":{\"heal\":10},\"area\":{\"shape\":\"radius\",\"radius\":1,\"vertical\":0}}";
  o.mageAbilities = "[\"flat\",\"cura\"]";
  o.mapExtra = "\"heights\":[[0,0,0,0,0,0,0,0,0],[0,0,0,2,0,0,0,0,0],[0,0,0,0,0,0,0,0,0]],";  // the bandit's square (3,1) is 2 levels up
  World w(make("abarea_vertical", o));
  w.unit("pal").hp = 5;
  w.unit("bandit").hp = 5;
  CHECK(w.run(at(w, "flat", 2, 1)));
  CHECK_EQ(w.unit("pal").hp, 15);
  CHECK_EQ(w.unit("bandit").hp, 5);  // beyond "vertical": 0
}

TEST(abarea_experience_is_paid_once_per_use) {
  Opts o = areaOpts();
  o.rules = "\"progression\":{\"enabled\":true,\"xpPerLevel\":100,\"maxLevel\":20,\"xpHit\":\"0\",\"xpKill\":\"0\"},";
  o.abilities += ",{\"id\":\"curax\",\"name\":\"Cura X\",\"target\":\"unit\",\"range\":[0,3],\"effect\":{\"heal\":10},\"area\":{\"shape\":\"radius\",\"radius\":1},\"xp\":12}";
  o.mageAbilities = "[\"curax\"]";
  World w(make("abarea_xp", o));
  w.unit("pal").hp = 5;
  w.unit("bandit").hp = 5;
  const int xp = w.unit("hero").xp;
  CHECK(w.run(at(w, "curax", 2, 1)));
  CHECK_EQ(w.unit("hero").xp, xp + 12);  // two healed, paid once
}

// ---- several effects ----

TEST(abarea_heal_and_cure_heals_first_then_cures_either_part_is_enough) {
  World w(make("abarea_firstaid", areaOpts()));
  const Action aid = Action::useAbility(w.id("hero"), w.ability("firstaid"), w.id("pal"));
  CHECK_STR(w.why(aid), "that unit needs neither healing nor a cure");
  w.unit("pal").hp = 20;
  give(w.unit("pal"), w.cond("burn"), 2);
  CHECK(w.run(aid));
  int healed = -1, cured = -1, i = 0;
  for (const Event& e : w.events) {
    if (e.type == EventType::UnitHealed) healed = i;
    if (e.type == EventType::ConditionEnded && e.c == kConditionCured) cured = i;
    ++i;
  }
  CHECK(healed >= 0 && cured > healed);  // the heal first, then the cure
  CHECK_EQ(w.unit("pal").hp, 25);
  CHECK(!w.has("pal", "burn"));
  // Only burned (full HP): legal, it cures. Only hurt: legal, it heals.
  World v(make("abarea_firstaid2", areaOpts()));
  give(v.unit("pal"), v.cond("burn"), 2);
  CHECK(v.run(Action::useAbility(v.id("hero"), v.ability("firstaid"), v.id("pal"))));
  CHECK(!v.has("pal", "burn"));
  CHECK_EQ(v.count(EventType::UnitHealed), 0);
  World x(make("abarea_firstaid3", areaOpts()));
  x.unit("pal").hp = 1;
  CHECK(x.run(Action::useAbility(x.id("hero"), x.ability("firstaid"), x.id("pal"))));
  CHECK_EQ(x.unit("pal").hp, 6);
}

TEST(abarea_inflict_list_rolls_each_entry_on_its_own) {
  World w(make("abarea_gaze", areaOpts()), 3);
  const Rng before = w.b().rng;
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("gaze"), w.id("bandit"))));
  Rng r = before;
  r.range(100);
  r.range(100);
  CHECK(w.b().rng.state == r.state);  // two entries with a chance: two rolls
  int outcomes = 0;
  for (const Event& e : w.events)
    outcomes += e.type == EventType::ConditionApplied || (e.type == EventType::ConditionResisted && (e.flags & kMissed));
  CHECK_EQ(outcomes, 2);
  // The chance of each entry, by its index.
  CHECK_EQ(w.b().abilityInflictChance(w.id("hero"), w.ability("gaze"), w.id("thug"), w.unit("hero").cell, 0), 50);
  CHECK_EQ(w.b().abilityInflictChance(w.id("hero"), w.ability("gaze"), w.id("thug"), w.unit("hero").cell, 1), 40);
  CHECK_EQ(w.b().abilityInflictChance(w.id("hero"), w.ability("gaze"), w.id("thug"), w.unit("hero").cell, 2), -1);
}

TEST(abarea_inflict_list_immune_to_one_still_takes_the_other_and_refuses_when_all_are_had) {
  World w(make("abarea_gaze_ward", areaOpts()), 3);
  give(w.unit("bandit"), w.cond("ward"), -1);  // immune to sleep
  const Rng before = w.b().rng;
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("gaze"), w.id("bandit"))));
  const Event* resisted = w.first(EventType::ConditionResisted);
  CHECK(resisted && resisted->b == w.cond("sleep") && !(resisted->flags & kMissed));  // immune: no roll for it
  Rng r = before;
  r.range(100);
  CHECK(w.b().rng.state == r.state);  // one roll: burn's
  // Both had already (sleep immune, burn on for as long): refused.
  World v(make("abarea_gaze_had", areaOpts()));
  give(v.unit("bandit"), v.cond("ward"), -1);
  give(v.unit("bandit"), v.cond("burn"), 3);
  CHECK_STR(v.why(Action::useAbility(v.id("hero"), v.ability("gaze"), v.id("bandit"))), "that unit already has them");
}

// ---- what the player shows ----

TEST(abarea_words_and_pick_hint) {
  World w(make("abarea_words", areaOpts()));
  const Database& db = w.rig.db;
  CHECK_STR(abilityEffectText(db, db.abilities[(size_t)w.ability("firstaid")]), "Heals 5 HP and cures Burn");
  CHECK_STR(abilityEffectText(db, db.abilities[(size_t)w.ability("barrier")]), "Inflicts Protect and Haste for 2 turns (area: a burst)");
  CHECK_STR(abilityEffectText(db, db.abilities[(size_t)w.ability("gaze")]), "Inflicts Sleep (50%) and Burn (a chance)");
  CHECK_STR(abilityEffectText(db, db.abilities[(size_t)w.ability("cura")]), "Heals 10 HP (area: radius 1)");
  CHECK_STR(abilityTargetText(db.abilities[(size_t)w.ability("curaa")]), "a square (allies in the area)");
  CHECK_STR(abilityRangeText(db.abilities[(size_t)w.ability("cura")]), "range 0-3");
  w.unit("pal").hp = 20;
  w.unit("bandit").hp = 15;
  std::vector<Cell> covered;
  CHECK_STR(abilityAreaLine(w.b(), w.id("hero"), w.ability("cura"), w.b().grid.at(2, 1), covered), "Cura: 2 units (Knight +10 HP, Brigand +5 HP)");
  CHECK_EQ(covered.size(), (size_t)5);
  CHECK_STR(abilityAreaLine(w.b(), w.id("hero"), w.ability("sleepga"), w.b().grid.at(4, 1), covered), "Sleepga: 2 units (Brigand Sleep 50%, Brigand Sleep 50%)");
  CHECK_STR(abilityAreaLine(w.b(), w.id("hero"), w.ability("cura"), w.b().grid.at(7, 1), covered), "");
  CHECK(covered.empty());
}
