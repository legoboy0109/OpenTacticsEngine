// docs/SRPG.md "Abilities that strike", "Healing by formula", "Unit resources": an ability that attacks with a weapons.json entry (formulas, hit formula,
// areas; never countered), heals by a percent or a formula, and pays MP that regenerates. Fixture: teaching_fixture.h.
#include "core/ai/ai.h"
#include "teaching_fixture.h"

using namespace ote;
using skilltest::saysIn;
using teachtest::Duel;

namespace {

const AbilityOption* optionOf(const std::vector<AbilityOption>& opts, int ability) {
  for (const AbilityOption& o : opts)
    if (o.ability == ability) return &o;
  return nullptr;
}

}  // namespace

TEST(a_strike_ability_takes_its_targets_and_numbers_from_its_weapon_and_is_never_countered) {
  Duel d("strikeab_forecast");
  const int aldric = d.w.id("aldric"), foe = d.w.id("foe");
  const int fire = d.ability("fire");
  CHECK_EQ(d.b().abilityStrikeWeapon(fire), d.db().findWeapon("fire_spell"));
  std::vector<int> units;
  std::vector<Cell> cells;
  d.b().abilityTargets(aldric, fire, d.unit("aldric").cell, units, cells);
  CHECK_EQ((int)units.size(), 1);  // the dummy next to him; the grunt and the shaman are 4 squares away (range 1..3)
  CHECK(!units.empty() && units[0] == foe);
  Forecast f;
  CHECK(d.b().abilityStrikeForecast(aldric, fire, foe, d.unit("aldric").cell, f));
  CHECK_EQ(f.attack.damage, 12);  // str 6 + might 8 - def 2 (the game's damage formula)
  CHECK_EQ(f.attack.hit, 100);    // the weapon's own "hit" formula
  CHECK(!f.counter.can);          // nothing answers an ability
  Forecast sword;
  CHECK(d.b().forecast(aldric, foe, d.db().findWeapon("sword"), d.unit("aldric").cell, sword));
  CHECK(sword.counter.can);  // (an ordinary attack is answered)
  // A weapon's "hit" formula sees both units: "b.def * 10" against def 2.
  CHECK(d.b().abilityStrikeForecast(aldric, d.ability("poke"), foe, d.unit("aldric").cell, f));
  CHECK_EQ(f.attack.hit, 20);
}

TEST(using_a_strike_ability_attacks_pays_its_price_and_ends_the_order) {
  Duel d("strikeab_use");
  const int aldric = d.w.id("aldric"), foe = d.w.id("foe");
  const int fire = d.ability("fire");
  std::vector<AbilityOption> opts;
  d.b().abilityOptions(aldric, d.unit("aldric").cell, opts);
  CHECK(optionOf(opts, fire) && optionOf(opts, fire)->ok);
  CHECK_EQ(d.b().poolOf(d.unit("aldric"), 0), 6);  // MP: max = str 6, starts full
  CHECK(d.w.apply(Action::useAbility(aldric, fire, foe)));
  int strikes = 0, counters = 0;
  for (const Event& e : d.w.events)
    if (e.type == EventType::Attack) {
      if (e.flags & kCounter) ++counters;
      else {
        ++strikes;
        CHECK_EQ(e.c, d.db().findWeapon("fire_spell"));
        CHECK_EQ(e.d, 12);
      }
    }
  CHECK_EQ(strikes, 1);
  CHECK_EQ(counters, 0);
  CHECK_EQ(d.unit("foe").hp, 188);
  CHECK_EQ(d.w.count(EventType::AbilityUsed), 1);
  const std::vector<Event> paid = d.of(EventType::PoolChanged);
  CHECK_EQ((int)paid.size(), 1);
  CHECK(!paid.empty() && paid[0].c == -4 && paid[0].d == 2);
  CHECK(d.unit("aldric").done);
  CHECK_EQ(d.of(EventType::XpGained).size(), (size_t)1);  // experience as for a fight
  // 2 MP left: the next use is greyed with the reason.
  d.b().abilityOptions(aldric, d.unit("aldric").cell, opts);
  CHECK(optionOf(opts, fire) && !optionOf(opts, fire)->ok);
  CHECK_STR(optionOf(opts, fire)->why, "needs 4 MP");
}

TEST(mp_regenerates_at_the_start_of_the_units_turn) {
  Duel d("strikeab_regen");
  const int aldric = d.w.id("aldric");
  CHECK(d.w.apply(Action::useAbility(aldric, d.ability("fire"), d.w.id("foe"))));
  CHECK(d.w.apply(Action::endTurn()));
  for (int i = 0; i < 60 && !d.b().over && d.b().activeSide != 0; ++i) aiTakeStep(d.b(), d.w.events);
  CHECK_EQ(d.b().activeSide, 0);
  CHECK_EQ(d.b().poolOf(d.unit("aldric"), 0), 3);  // +1 a turn
}

TEST(an_area_strike_hits_what_its_weapon_covers_and_nothing_answers) {
  Duel d("strikeab_area");
  const int aldric = d.w.id("aldric");
  const int flame = d.ability("flame");
  const Cell aim = d.unit("foe").cell;
  AreaForecast f;
  CHECK(d.b().abilityStrikeArea(aldric, flame, d.unit("aldric").cell, aim, f));
  CHECK(!f.victims.empty());
  CHECK(d.w.apply(Action::useAbility(aldric, flame, -1, aim)));
  int area = 0, counters = 0;
  for (const Event& e : d.w.events) {
    if (e.type != EventType::Attack) continue;
    if (e.flags & kCounter) ++counters;
    else if (e.flags & kArea) ++area;
  }
  CHECK_EQ(area, 1);  // the dummy (Aldric next to it is not hit: no friendly fire)
  CHECK_EQ(counters, 0);
  CHECK_EQ(d.unit("foe").hp, 200 - 9);  // 6 + 5 - 2
}

TEST(heal_by_percent_and_by_formula) {
  Duel d("strikeab_heal");
  const int aldric = d.w.id("aldric");
  d.unit("aldric").hp = 5;
  CHECK(d.w.apply(Action::useAbility(aldric, d.ability("patch"), -1)));
  CHECK_EQ(d.unit("aldric").hp, 20);  // half of 30
  CHECK_EQ(d.of(EventType::UnitHealed).size(), (size_t)1);
  Duel e("strikeab_heal2");
  e.unit("aldric").hp = 5;
  CHECK(e.w.apply(Action::useAbility(e.w.id("aldric"), e.ability("salve"), -1)));
  CHECK_EQ(e.unit("aldric").hp, 17);  // a.str * 2 = 12
}

TEST(the_ai_uses_a_strike_ability_when_it_beats_its_own_attack) {
  Duel d("strikeab_ai", false);
  // The red grunt gets "fire" (no price for it: MP needs str 4, it has 3) and stands next to Aldric.
  d.c.swap("data/abilities.json", R"("cost": "action", "spend": { "mp": 4 } },)", R"("cost": "action" },)");
  d.c.swap("data/units.json", R"("stats": { "hp": 12, "str": 3, "def": 0, "mov": 3 }, "weapons": ["sword"] })",
           R"("stats": { "hp": 12, "str": 3, "def": 0, "mov": 3 }, "weapons": ["sword"], "abilities": ["fire"] })");
  skilltest::World w("duel", d.c.dir);
  CHECK(w.apply(Action::endTurn()));
  int used = 0;
  for (int i = 0; i < 60 && !w.b().over && w.b().activeSide != 0; ++i) {
    w.events.clear();
    aiTakeStep(w.b(), w.events);
    for (const Event& e : w.events)
      if (e.type == EventType::AbilityUsed && e.a == w.id("weak") && e.b == w.db().findAbility("fire")) ++used;
  }
  CHECK_EQ(used, 1);  // fire (might 8, no counter) beats its sword (might 4)
}

TEST(the_loader_refuses_strikes_prices_and_pools_that_do_not_fit) {
  using teachtest::Copy;
  using teachtest::refusal;
  CHECK(saysIn(refusal("strikeab_r1", [](Copy& c) { c.swap("data/abilities.json", R"({ "strike": "fire_spell" }, "cost")", R"({ "strike": "fire_spell" }, "target": "enemy", "cost")"); }),
               "\"strike\" takes its targets and reach from the weapon: leave out \"target\" and \"range\""));
  CHECK(saysIn(refusal("strikeab_r2", [](Copy& c) { c.swap("data/abilities.json", R"({ "strike": "blaze" }, "cost": "action")", R"({ "strike": "blaze" }, "cost": "free")"); }),
               "\"strike\" ends the unit's turn: \"cost\" must be \"action\""));
  CHECK(saysIn(refusal("strikeab_r3", [](Copy& c) { c.swap("data/abilities.json", R"({ "strike": "blaze" })", R"({ "strike": "lava" })"); }),
               "\"strike\" names unknown weapon \"lava\""));
  CHECK(saysIn(refusal("strikeab_r4", [](Copy& c) { c.swap("data/abilities.json", R"("spend": { "mp": 4 })", R"("spend": { "hp": 4 })"); }),
               "\"spend\" names unknown resource \"hp\""));
  CHECK(saysIn(refusal("strikeab_r5", [](Copy& c) { c.swap("data/abilities.json", R"("spend": { "mp": 4 })", R"("spend": { "mp": 0 })"); }),
               "\"spend\": \"mp\" must be a whole number from 1 to 999"));
  CHECK(saysIn(refusal("strikeab_r6", [](Copy& c) { c.swap("game.json", R"("max": "str")", R"("max": "mag")"); }), "\"max\" names unknown stat \"mag\""));
  CHECK(saysIn(refusal("strikeab_r7", [](Copy& c) { c.swap("data/abilities.json", R"("heal": "a.str * 2")", R"("heal": "most")"); }),
               "(\"most\" is not one: a formula reads the units"));
  CHECK(saysIn(refusal("strikeab_r8", [](Copy& c) { c.swap("data/abilities.json", R"("heal": { "percent": 50 })", R"("heal": { "percent": 150 })"); }),
               "{ \"percent\": 1 to 100 }"));
  CHECK(saysIn(refusal("strikeab_r9", [](Copy& c) { c.swap("data/weapons.json", R"("hit": "100")", R"("hit": "")"); }), "\"hit\""));
}
