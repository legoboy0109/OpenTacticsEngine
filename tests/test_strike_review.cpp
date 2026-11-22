// Regression tests for review findings on strike abilities and learning from equipment (docs/SRPG.md "Abilities that strike", docs/JOBS.md "Learning
// from equipment"). Each failed before its fix. Fixture: teaching_fixture.h.
#include <algorithm>

#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "core/jobs/mastery.h"
#include "teaching_fixture.h"

using namespace ote;
using skilltest::saysIn;
using teachtest::Duel;

TEST(an_area_strike_ability_follows_its_own_after_move_not_the_weapons) {
  Duel d("strikerev_aftermove", false);
  d.c.swap("data/weapons.json", R"("area": { "shape": "burst" },)", R"("area": { "shape": "burst" }, "afterMove": false,)");
  skilltest::World w("duel", d.c.dir);
  CHECK(w.apply(Action::move(w.id("aldric"), w.at(2, 3))));
  const int flame = w.db().findAbility("flame");
  std::vector<int> units;
  std::vector<Cell> cells;
  w.b().abilityTargets(w.id("aldric"), flame, w.unit("aldric").cell, units, cells);
  CHECK(std::find(cells.begin(), cells.end(), w.unit("foe").cell) != cells.end());
  CHECK(w.apply(Action::useAbility(w.id("aldric"), flame, -1, w.unit("foe").cell)));
}

TEST(entries_of_a_job_unlocked_by_a_mastered_item_entry_can_be_bought) {
  Duel d("strikerev_buy", false);
  d.c.swap("data/jobs.json", R"("requires": [ { "job": "squire", "level": 2 } ],
    "jpLevels")", R"("requires": [ { "mastered": "brave" } ],
    "jpLevels")");
  jobtest::Game g;
  CHECK(g.load(d.c.dir));
  CHECK(g.startChapter());
  CHECK(g.winAndFinish());  // aldric is on the roster now, in the camp
  RosterUnit* r = g.rosterUnit("aldric");
  CHECK(r != nullptr);
  if (!r) return;
  const int mage = g.job("mage");
  r->mastery = {{0, 20}};  // Brave mastered
  r->jobs.at(mage).earned = 100;
  std::string why;
  CHECK(jobLearnReason(g.db, r->jobs, g.entry("mend"), why, &r->mastery));
  CHECK(jobsLearn(g.db, g.pt, "aldric", g.entry("mend"), why));
  if (!why.empty()) std::printf("  %s\n", why.c_str());
}

TEST(the_ai_only_strikes_targets_the_abilitys_conditions_allow) {
  Duel d("strikerev_aiif", false);
  // The red grunt's "fire" works only on a foe below half health; a target it refuses must never be planned.
  d.c.swap("data/abilities.json", R"("cost": "action", "spend": { "mp": 4 } },)",
           R"("cost": "action", "if": [ { "hp": { "op": "<", "percent": 50 }, "who": "foe" } ] },)");
  d.c.swap("data/units.json", R"("stats": { "hp": 12, "str": 3, "def": 0, "mov": 3 }, "weapons": ["sword"] })",
           R"("stats": { "hp": 12, "str": 3, "def": 0, "mov": 3 }, "weapons": ["sword"], "abilities": ["fire"] })");
  skilltest::World w("duel", d.c.dir);
  // From 3,3 it reaches Aldric (healthy: refused) and the plain squire (wounded: allowed), who score alike; Aldric comes first in unit order.
  w.unit("plain").hp = 14;
  const Unit& weak = w.unit("weak");
  ai::Stance st;
  ai::Order best;
  ai::considerStrikes(w.b(), weak, w.at(3, 3), 0, st, best);
  CHECK(best.kind == ActionType::Ability);
  CHECK_EQ(best.target, w.id("plain"));  // (Aldric would be refused: the grunt's turn wasted)
}

TEST(the_squares_offered_for_an_area_strike_include_the_reach_high_ground_adds) {
  Duel d("strikerev_height", false);
  // Height rules: +1 square of reach per level above the target, at most 2. Aldric stands 2 levels up; the grunt at 5,3 is 4 squares away (blaze: 3).
  d.c.swap("game.json", R"("rules": {)", R"("rules": { "height": { "rangeBonus": { "perLevels": 1, "max": 2 } },)");
  d.c.swap("maps/duel.json", R"("tiles": ["........", "........", "........", "........", "........"],)",
           R"("tiles": ["........", "........", "........", "........", "........"],
  "heights": [[0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0],[0,0,2,0,0,0,0,0],[0,0,0,0,0,0,0,0],[0,0,0,0,0,0,0,0]],)");
  skilltest::World w("duel", d.c.dir);
  const int flame = w.db().findAbility("flame");
  std::vector<AttackOption> options;
  w.b().abilityStrikeOptions(w.id("aldric"), flame, w.unit("aldric").cell, options);
  bool offered = false;
  for (const AttackOption& o : options) offered = offered || o.cell == w.unit("weak").cell;
  CHECK(offered);  // what the AI may aim at ...
  std::vector<int> units;
  std::vector<Cell> cells;
  w.b().abilityTargets(w.id("aldric"), flame, w.unit("aldric").cell, units, cells);
  CHECK(std::find(cells.begin(), cells.end(), w.unit("weak").cell) != cells.end());  // ... the player may pick too
}

TEST(an_area_strike_refuses_conditions_on_a_target_unit) {
  const std::string err = teachtest::refusal("strikerev_areaif", [](teachtest::Copy& c) {
    c.swap("data/abilities.json", R"({ "strike": "blaze" }, "cost": "action")",
           R"({ "strike": "blaze" }, "cost": "action", "if": [ { "hp": { "op": "<", "percent": 50 }, "who": "foe" } ])");
  });
  CHECK(saysIn(err, "ability \"flame\""));
  CHECK(saysIn(err, "\"who\": \"foe\" cannot be used in an ability that targets a square"));
  // A single-target strike keeps them (they are checked against the unit struck).
  CHECK_STR(teachtest::refusal("strikerev_oneif", [](teachtest::Copy& c) {
              c.swap("data/abilities.json", R"("cost": "action", "spend": { "mp": 4 } },)",
                     R"("cost": "action", "spend": { "mp": 4 }, "if": [ { "hp": { "op": "<", "percent": 50 }, "who": "foe" } ] },)");
            }),
            "");
}

TEST(a_weapon_taught_skill_counts_only_for_that_weapon) {
  // Carried weapons (fixture srpg-conditions): Kay holds the Iron Blade, which teaches "wary". Asked for another weapon (a forecast with it, a strike
  // ability's weapon) or for no weapon, the blade's lesson is not there; the weapon in hand (-1) has it.
  skilltest::Copy c("strikerev_wskill", "srpg-conditions");
  c.swap("data/items.json", R"({ "id": "iron_blade", "name": "Iron Blade", "kind": "weapon", "weapon": "iron_blade" })",
         R"({ "id": "iron_blade", "name": "Iron Blade", "kind": "weapon", "weapon": "iron_blade", "teaches": [ { "skill": "wary", "points": 50 } ] })");
  skilltest::World w("field", c.dir);
  const Unit& kay = w.unit("kay");
  auto has = [&](int weapon) {
    std::vector<int> list;
    w.b().skillsOf(kay, weapon, list);
    return std::find(list.begin(), list.end(), w.skill("wary")) != list.end();
  };
  CHECK(has(-1));
  CHECK(has(w.db().findWeapon("iron_blade")));
  CHECK(!has(w.db().findWeapon("short_sword")));
  CHECK(!has(-2));
}

TEST(the_ai_keeps_its_own_attack_when_a_strike_ability_scores_the_same) {
  Duel d("strikerev_tie", false);
  // The grunt's weapon and its ability "zap" strike alike (Fire, range 2..3: nobody can strike back, no MP spent), so they score the same; the
  // attack of its own stays (docs/AI.md "Abilities that strike").
  d.c.swap("data/weapons.json", R"("id": "fire_spell", "name": "Fire", "range": [1, 3])", R"("id": "fire_spell", "name": "Fire", "range": [2, 3])");
  d.c.swap("data/abilities.json", R"({ "abilities": [)", R"({ "abilities": [
      { "id": "zap", "name": "Zap", "effect": { "strike": "fire_spell" }, "cost": "action" },)");
  d.c.swap("data/units.json", R"("stats": { "hp": 12, "str": 3, "def": 0, "mov": 3 }, "weapons": ["sword"] })",
           R"("stats": { "hp": 12, "str": 3, "def": 0, "mov": 3 }, "weapons": ["fire_spell"], "abilities": ["zap"] })");
  skilltest::World w("duel", d.c.dir);
  ai::Context cx(w.b());
  const ai::Order o = ai::bestOrder(cx, w.unit("weak"));
  CHECK(o.kind == ActionType::Attack);
}
