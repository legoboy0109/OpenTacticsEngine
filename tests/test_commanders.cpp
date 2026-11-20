// Commanders (docs/COMMANDERS.md), the rules: the loader and its refusals, passive skills with filters, what fills the meter and what caps it,
// UsePower (afford, refuse), the instant effects, timed skills that end on time, cost growth. Fixture tests/fixtures/commanders
// (numbers in commander_fixture.h). Saves, factions, action points, the speed order and the computer opponent: test_commanders_more.cpp.
#include "commander_fixture.h"

using namespace ote;
using namespace cmdtest;

// ---- loader ----

TEST(commanders_fixture_loads_with_every_key) {
  Copy c("cmd_load", "commanders");
  CHECK_STR(c.gameError(), "");
  World w("field", fixtureDir());
  const Database& db = w.db();
  CHECK_EQ((int)db.commanders.size(), 3);
  const CommanderDef& ada = db.commanders[(size_t)db.findCommander("ada")];
  CHECK_STR(ada.name, "Ada");
  CHECK_EQ(ada.meterMax, 100);
  CHECK_EQ(ada.perDamageDealt, 2);
  CHECK_EQ(ada.perDamageTaken, 1);
  CHECK_EQ(ada.perTurn, 0);
  CHECK_EQ((int)ada.passive.size(), 1);
  CHECK_STR(ada.passive[0].filterTag, "infantry");
  CHECK_EQ((int)ada.powers.size(), 2);
  CHECK_EQ(ada.powers[0].cost, 40);
  CHECK_EQ(ada.powers[0].turns, 1);  // the default: until the side's next turn
  CHECK_EQ(ada.powers[1].turns, 2);
  CHECK_EQ(ada.powers[1].growthPercent, 50);
  CHECK(ada.powers[1].effects[0].kind == CommanderEffect::Kind::Refresh);
  const CommanderDef& bram = db.commanders[(size_t)db.findCommander("bram")];
  CHECK_EQ(bram.perTurn, 10);
  CHECK(bram.powers[0].modifiers.empty());
  CHECK_EQ((int)bram.powers[0].effects.size(), 2);
  CHECK_EQ(bram.powers[0].effects[0].amount, 4);
  CHECK_EQ(bram.powers[0].effects[1].resources[0].amount, 25);
  const CommanderDef& cato = db.commanders[(size_t)db.findCommander("cato")];
  CHECK(cato.passive.empty());
  CHECK_STR(cato.powers[1].effects[0].filterTag, "heavy");
  const FactionDef& order = db.factions[(size_t)db.findFaction("order")];
  CHECK(order.limitCommanders && order.commanders.size() == 2);
  CHECK(!db.factions[(size_t)db.findFaction("free")].limitCommanders);
  CHECK_EQ(w.b().sides[0].commander, db.findCommander("ada"));
  CHECK_EQ(w.b().sides[1].commander, db.findCommander("bram"));
  CHECK_EQ(w.b().meterOf(0), 0);
}

TEST(a_game_without_commanders_loads_and_its_sides_have_none) {
  Copy c("cmd_none", "commanders");
  c.drop("data/commanders.json");
  c.swap("data/factions.json", ", \"commanders\": [\"ada\", \"cato\"]", "");
  c.swap("data/factions.json", ", \"commanders\": [\"bram\"]", "");
  CHECK(saysIn(c.mapError("field"), "\"commander\" needs data/commanders.json"));  // the map still names one
  World w("plain", fixtureDir());
  CHECK_EQ(w.b().sides[0].commander, -1);
  CHECK(w.b().commanderOf(0) == nullptr);
  CHECK_EQ(w.b().meterOf(0), 0);
  CHECK_EQ(w.b().activePower(0), -1);
  std::string why;
  CHECK(!w.b().canUsePower(0, 0, &why));
  CHECK_STR(why, "this side has no commander");
  CHECK(saysIn(w.refusal(Action::usePower(0)), "no commander"));
  // A game that never heard of commanders: no key in the save, no commander in any side.
  World p("field", ote_test::sourceDir() + "/tests/fixtures/research-branches");
  CHECK(p.db().commanders.empty());
  Json j;
  saveBattleState(p.b(), j);
  CHECK(!j["sides"][size_t(0)].has("commander"));
}

TEST(loader_refusals_name_the_file_the_entry_and_the_key) {
  struct Case {
    const char* what;
    const char* from;
    const char* to;
    const char* needle;
  };
  const Case cases[] = {
      {"an unknown passive skill", "\"skill\": \"bulwark\", \"filter\": { \"tag\": \"infantry\" } } ],\n    \"meter\"", "\"skill\": \"nope\" } ],\n    \"meter\"",
       "commanders.json: commander \"ada\": \"passive\": names unknown skill \"nope\""},
      {"an unknown power skill", "\"modifiers\": [\"frenzy\"]", "\"modifiers\": [\"nope\"]",
       "power \"rally\": \"modifiers\": names unknown skill \"nope\""},
      {"an unknown resource", "{ \"gold\": 25 }", "{ \"mana\": 25 }", "power \"storm\": \"effects\": \"giveResource\" names unknown resource \"mana\""},
      {"a duplicate commander", "\"id\": \"bram\"", "\"id\": \"ada\"", "commander \"ada\": duplicate id"},
      {"a duplicate power", "\"id\": \"surge\"", "\"id\": \"rally\"", "power \"rally\": duplicate power id"},
      {"a cost over the meter", "\"cost\": 80", "\"cost\": 101", "power \"surge\": \"cost\" must be from 1 to the meter's \"max\" (100)"},
      {"a cost of nothing", "\"cost\": 40", "\"cost\": 0", "power \"rally\": \"cost\" must be from 1"},
      {"a bad effect kind", "{ \"heal\": 5 }", "{ \"explode\": 5 }", "power \"rally\": \"effects\": has \"explode\" but no effect"},
      {"two effects in one entry", "{ \"heal\": 5 }", "{ \"heal\": 5, \"damage\": 2 }", "holds one effect only"},
      {"a key an effect does not take", "{ \"heal\": 5 }", "{ \"heal\": 5, \"to\": \"enemies\" }", "\"heal\" does not take the key \"to\""},
      {"damage to allies", "\"to\": \"enemies\"", "\"to\": \"allies\"", "\"damage\".\"to\" must be \"enemies\""},
      {"a heal of nothing", "{ \"heal\": 5 }", "{ \"heal\": 0 }", "\"heal\" must be a whole number from 1 to 9999"},
      {"a refresh that is not a filter", "{ \"refresh\": true }", "{ \"refresh\": 3 }", "\"refresh\" must be a filter"},
      {"a meter that fills by nothing", "\"perDamageDealt\": 1, \"perTurn\": 10", "\"perTurn\": 0", "\"meter\": fills by nothing"},
      {"an unknown meter key", "\"max\": 60", "\"maximum\": 60, \"max\": 60", "\"meter\": unknown key \"maximum\""},
      {"an unknown key", "\"description\": \"Slow to anger", "\"mood\": 1, \"description\": \"Slow to anger", "commander \"cato\": unknown key \"mood\""},
      {"turns without timed skills", "\"cost\": 30,", "\"cost\": 30, \"turns\": 2,", "\"turns\" only means something with \"modifiers\""},
      {"a power that does nothing", "\"effects\": [ { \"heal\": 8, \"filter\": { \"tag\": \"heavy\" } } ] } ] }", "\"description\": \"nothing\" } ] }",
       "power \"mend\": has neither \"modifiers\" nor \"effects\""},
      {"a filter that is not a tag", "{ \"tag\": \"heavy\" }", "{ \"tag\": \"flying\" }", "is not a tag of any unit type"},
      {"a skill twice", "\"passive\": [ \"fleet\", ", "\"passive\": [ \"fleet\", \"fleet\", ", "\"passive\" lists \"fleet\" twice"},
      {"a wrong turns number", "\"turns\": 2,", "\"turns\": 0,", "\"turns\" must be a whole number from 1 to 99"},
      {"a missing meter", "\"meter\": { \"max\": 50, \"perDamageTaken\": 3 },", "", "commander \"cato\": missing required \"meter\""},
  };
  for (const Case& k : cases) {
    std::printf("  case: %s\n", k.what);
    CHECK(saysIn(commandersError("cmd_bad", k.from, k.to), k.needle));
  }
  Copy c("cmd_empty", "commanders");
  c.put("data/commanders.json", "{ \"commanders\": [] }");
  CHECK(saysIn(c.gameError(), "\"commanders\" is empty"));
}

TEST(more_than_four_powers_are_refused) {
  Copy c("cmd_five", "commanders");
  std::string powers;
  for (int i = 0; i < 5; ++i) powers += std::string(i ? "," : "") + "{ \"id\": \"p" + std::to_string(i) + "\", \"cost\": 10, \"effects\": [ { \"heal\": 1 } ] }";
  c.put("data/commanders.json", "{ \"commanders\": [ { \"id\": \"x\", \"meter\": { \"max\": 50, \"perTurn\": 5 }, \"powers\": [" + powers + "] } ] }");
  c.drop("data/factions.json");
  c.put("maps/field.json", c.read("maps/plain.json"));
  CHECK(saysIn(c.gameError(), "commander \"x\": \"powers\" lists 5 powers (at most 4)"));
  c.put("data/commanders.json", "{ \"commanders\": [ { \"id\": \"x\", \"meter\": { \"max\": 50, \"perTurn\": 5 }, \"powers\": [" +
                                    powers.substr(0, powers.rfind(",{")) + "] } ] }");
  CHECK_STR(c.gameError(), "");  // four are fine
}

TEST(a_faction_and_a_map_side_naming_what_does_not_exist_are_refused) {
  CHECK(saysIn(([&] {
                 Copy c("cmd_fac1", "commanders");
                 c.swap("data/factions.json", "[\"ada\", \"cato\"]", "[\"ada\", \"zed\"]");
                 return c.gameError();
               })(),
               "faction \"order\": \"commanders\" names unknown commander \"zed\" (data/commanders.json)"));
  CHECK(saysIn(([&] {
                 Copy c("cmd_fac2", "commanders");
                 c.swap("data/factions.json", "[\"bram\"]", "[]");
                 return c.gameError();
               })(),
               "faction \"horde\": \"commanders\" is empty"));
  CHECK(saysIn(([&] {
                 Copy c("cmd_fac3", "commanders");
                 c.swap("data/factions.json", "[\"ada\", \"cato\"]", "[\"ada\", \"ada\"]");
                 return c.gameError();
               })(),
               "\"commanders\" lists \"ada\" twice"));
  Copy m("cmd_map", "commanders");
  m.swap("maps/field.json", "\"commander\": \"ada\"", "\"commander\": \"zed\"");
  CHECK(saysIn(m.mapError("field"), "side \"blue\": \"commander\" names unknown commander \"zed\" (data/commanders.json has: ada, bram, cato; or \"choose\")"));
  // "choose" and a name both need the file.
  Copy g("cmd_nofile", "commanders");
  g.drop("data/commanders.json");
  g.drop("data/factions.json");
  g.put("maps/pick.json", g.read("maps/plain.json"));
  g.swap("maps/pick.json", "\"id\": \"blue\"", "\"id\": \"blue\", \"commander\": \"choose\"");
  CHECK(saysIn(g.mapError("pick"), "side \"blue\": \"commander\" needs data/commanders.json"));
  g.put("data/factions.json", "{ \"factions\": [ { \"id\": \"a\", \"commanders\": [\"ada\"] } ] }");
  CHECK(saysIn(g.gameError(), "\"commanders\" needs data/commanders.json"));
}

// ---- passive skills ----

TEST(passive_skills_apply_with_their_filters_and_after_the_factions) {
  World w("field", fixtureDir());
  CHECK_STR(w.skillIds(w.unit("sol")), "keen bulwark");  // the faction's keen first, then Ada's bulwark (infantry only)
  CHECK_STR(w.skillIds(w.unit("kni")), "");
  CHECK_STR(w.skillIds(w.unit("gol")), "");
  CHECK_EQ(w.stat(w.unit("sol"), "def"), 4);
  CHECK_EQ(w.stat(w.unit("sol"), "str"), 8);
  CHECK_EQ(w.stat(w.unit("kni"), "def"), 4);
  // Bram: fleet for everybody, brittle only for the heavy (a weakness is a passive too).
  CHECK_STR(w.skillIds(w.unit("foe")), "fleet");
  CHECK_STR(w.skillIds(w.unit("rgo")), "fleet brittle");
  CHECK_EQ(w.stat(w.unit("rgo"), "def"), 3);
  CHECK_EQ(w.stat(w.unit("rgo"), "mov"), 3);
  CHECK_EQ(w.stat(w.unit("foe"), "mov"), 4);
  // They are the side's modifiers, tagged with their source, and never saved.
  CHECK(w.b().sides[0].modifiers[0].faction >= 0);
  CHECK(w.b().sides[0].modifiers[1].commander == w.db().findCommander("ada") && w.b().sides[0].modifiers[1].power == -1);
}

// ---- the meter ----

TEST(the_meter_fills_per_point_of_strike_damage_dealt_and_taken) {
  World w("field", fixtureDir());
  CHECK(soldierHitsFoe(w));
  // Blue's soldier (str 8 with keen) hits the foe (def 2) for 6 and takes the counter 7 - (2 + 2 bulwark) = 3.
  CHECK_EQ(dealtBy(w.b(), w.events, 0), 6);
  CHECK_EQ(dealtBy(w.b(), w.events, 1), 3);
  CHECK_EQ(w.b().meterOf(0), 6 * 2 + 3 * 1);  // Ada: 2 per damage dealt, 1 per damage taken
  CHECK_EQ(w.b().meterOf(1), 3 * 1);          // Bram: 1 per damage dealt, nothing for damage taken
  CHECK_EQ(w.b().meterMax(0), 100);
}

TEST(the_meter_is_capped_and_a_side_without_a_commander_has_none) {
  World w("field", fixtureDir());
  w.b().sides[0].meter = 95;
  CHECK(soldierHitsFoe(w));
  CHECK_EQ(w.b().meterOf(0), 100);
  World p("plain", fixtureDir());
  CHECK(p.apply(Action::move(p.id("sol"), p.at(4, 1))));
  CHECK(p.apply(Action::attack(p.id("sol"), p.id("foe"), p.db().findWeapon("sword"))));
  CHECK_EQ(p.b().meterOf(0), 0);
  CHECK_EQ(p.b().meterMax(0), 0);
}

TEST(the_per_turn_points_come_when_the_sides_turn_starts) {
  World w("field", fixtureDir());
  CHECK_EQ(w.b().meterOf(1), 0);
  CHECK(w.apply(Action::endTurn()));  // red's turn starts: Bram gains 10
  CHECK_EQ(w.b().meterOf(1), 10);
  CHECK(w.apply(Action::endTurn()));
  CHECK_EQ(w.b().meterOf(0), 0);  // Ada has no per-turn points
  CHECK_EQ(w.b().meterOf(1), 10);
  CHECK(w.apply(Action::endTurn()));
  CHECK_EQ(w.b().meterOf(1), 20);
}

TEST(the_meter_does_not_fill_while_the_sides_power_is_active) {
  World w("field", fixtureDir());
  w.b().sides[0].meter = 100;
  CHECK(w.apply(Action::usePower(powerIndex(w, "ada", "rally"))));
  CHECK_EQ(w.b().meterOf(0), 60);
  CHECK_EQ(w.b().activePower(0), 0);
  // Strikes while it is active add nothing (the soldier is stronger now: frenzy), neither dealt nor taken.
  CHECK(soldierHitsFoe(w));
  CHECK_EQ(w.b().meterOf(0), 60);
  CHECK_EQ(w.b().meterOf(1), 3);  // the other side's meter is not stopped by it
  // Once it has ended the meter fills again.
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));
  CHECK_EQ(w.b().activePower(0), -1);
  CHECK(w.apply(Action::attack(w.id("sol"), w.id("foe"), w.db().findWeapon("sword"))));  // without frenzy: 6 dealt, 3 taken
  CHECK_EQ(w.b().meterOf(0), 60 + 6 * 2 + 3);
}

// ---- using a power ----

TEST(a_power_needs_the_meter_the_sides_turn_and_no_active_power) {
  World w("field", fixtureDir());
  const int rally = powerIndex(w, "ada", "rally");
  std::string why;
  CHECK(!w.b().canUsePower(0, rally, &why));
  CHECK_STR(why, "needs 40 more");
  CHECK_EQ(w.b().powerShortBy(0, rally), 40);
  CHECK(!w.apply(Action::usePower(rally), &why));
  CHECK_STR(why, "needs 40 more");
  CHECK_EQ(w.b().meterOf(0), 0);  // refused: nothing changed
  w.b().sides[0].meter = 39;
  CHECK_EQ(w.b().powerShortBy(0, rally), 1);
  w.b().sides[0].meter = 40;
  CHECK(w.b().canUsePower(0, rally));
  CHECK(!w.b().canUsePower(1, 0, &why));  // red's turn is not now
  CHECK_STR(why, "it is not that side's turn");
  CHECK(!w.b().canUsePower(0, 5, &why));
  CHECK_STR(why, "that commander has no such power");
  CHECK(!w.b().canUsePower(0, -1, &why));
  // Used: paid, announced, active.
  CHECK(w.apply(Action::usePower(rally)));
  CHECK_EQ(w.count(EventType::PowerUsed), 1);
  const Event* used = nullptr;
  for (const Event& e : w.events)
    if (e.type == EventType::PowerUsed) used = &e;
  CHECK(used && used->a == 0 && used->b == w.db().findCommander("ada") && used->c == rally && used->d == 40 && used->data[0] == 0);
  CHECK_EQ(w.b().meterOf(0), 0);
  CHECK_EQ(w.b().activePower(0), rally);
  CHECK_EQ(w.b().powerTurnsLeft(0), 1);
  CHECK_EQ(w.b().powerUses(0, rally), 1);
  // Not another one while it lasts, however full the meter is.
  w.b().sides[0].meter = 100;
  CHECK(!w.apply(Action::usePower(powerIndex(w, "ada", "surge")), &why));
  CHECK_STR(why, "a power is active already");
  CHECK(!w.apply(Action::usePower(rally), &why));
}

TEST(an_instant_power_is_over_at_once_and_never_blocks_the_next) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::endTurn()));  // red / Bram
  w.b().sides[1].meter = 60;
  CHECK(w.apply(Action::usePower(0)));
  CHECK_EQ(w.b().meterOf(1), 30);
  CHECK_EQ(w.b().activePower(1), -1);  // storm has no timed skills: nothing stays active
  CHECK_EQ(w.count(EventType::PowerEnded), 0);
  CHECK(w.apply(Action::usePower(0)));  // and the meter may pay for it again
  CHECK_EQ(w.b().meterOf(1), 0);
  CHECK_EQ(w.b().powerUses(1, 0), 2);
}

// ---- the instant effects ----

TEST(heal_heals_every_matching_unit_of_the_side_up_to_its_maximum) {
  World w("field", fixtureDir());
  w.unit("sol").hp = 10;
  w.unit("kni").hp = 26;
  w.unit("foe").hp = 5;  // not the side's
  w.b().sides[0].meter = 40;
  CHECK(w.apply(Action::usePower(powerIndex(w, "ada", "rally"))));
  CHECK_EQ(w.unit("sol").hp, 15);
  CHECK_EQ(w.unit("kni").hp, 28);  // only the 2 it lacked
  CHECK_EQ(w.unit("gol").hp, 36);
  CHECK_EQ(w.unit("foe").hp, 5);
  CHECK_EQ(w.count(EventType::UnitHealed), 2);  // the full-health golem has nothing to say
}

TEST(a_heal_filter_limits_it_to_the_matching_units) {
  Copy c("cmd_cato", "commanders");
  c.swap("maps/field.json", "\"commander\": \"ada\"", "\"commander\": \"cato\"");
  World w("field", c.dir);
  w.unit("sol").hp = 10;
  w.unit("gol").hp = 20;
  w.b().sides[0].meter = 25;
  CHECK(w.apply(Action::usePower(powerIndex(w, "cato", "mend"))));
  CHECK_EQ(w.unit("gol").hp, 28);  // heavy: healed
  CHECK_EQ(w.unit("sol").hp, 10);  // infantry: not
}

TEST(damage_hurts_every_enemy_but_never_defeats_and_gold_arrives) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::endTurn()));
  w.unit("sol").hp = 3;
  w.b().sides[1].meter = 30;
  const int before = gold(w, 1);
  CHECK(w.apply(Action::usePower(0)));
  CHECK_EQ(w.unit("sol").hp, 1);  // 3 - 4 would defeat it: it keeps 1
  CHECK_EQ(w.unit("kni").hp, 24);
  CHECK_EQ(w.unit("gol").hp, 32);
  CHECK_EQ(w.unit("foe").hp, 20);  // its own side is untouched
  CHECK(w.unit("sol").alive);
  CHECK_EQ(w.count(EventType::UnitHurt), 3);
  CHECK_EQ(gold(w, 1), before + 25);
  CHECK(!w.b().over);
}

TEST(refresh_gives_the_acted_units_their_turn_back) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::wait(w.id("sol"))));
  CHECK(w.apply(Action::move(w.id("kni"), w.at(3, 0))));
  CHECK(w.unit("sol").done && w.unit("kni").moved);
  w.b().sides[0].meter = 100;
  CHECK(w.apply(Action::usePower(powerIndex(w, "ada", "surge"))));
  CHECK(!w.unit("sol").done && !w.unit("kni").moved);
  CHECK_EQ(w.count(EventType::UnitRefreshed), 2);  // the golem never acted: nothing to give back
  CHECK(w.apply(Action::wait(w.id("sol"))));      // and it can act again
}

// ---- timed skills ----

TEST(a_power_gives_its_skills_while_it_lasts_and_ends_when_the_turn_comes_round) {
  World w("field", fixtureDir());
  const int rally = powerIndex(w, "ada", "rally");
  w.b().sides[0].meter = 40;
  CHECK_EQ(w.stat(w.unit("sol"), "str"), 8);
  CHECK(w.apply(Action::usePower(rally)));
  CHECK_STR(w.skillIds(w.unit("sol")), "keen bulwark frenzy");  // the power's skills come last
  CHECK_EQ(w.stat(w.unit("sol"), "str"), 11);
  CHECK_EQ(w.stat(w.unit("kni"), "str"), 12);  // for every unit: no filter
  CHECK_EQ(w.stat(w.unit("foe"), "str"), 7);
  CHECK(w.apply(Action::endTurn()));  // red's turn
  CHECK_EQ(w.b().activePower(0), rally);
  CHECK_EQ(w.stat(w.unit("sol"), "str"), 11);  // it lasts through the enemy's turn
  CHECK(w.apply(Action::endTurn()));  // blue's turn starts: it ends
  CHECK_EQ(w.count(EventType::PowerEnded), 1);
  const Event* ended = nullptr;
  for (const Event& e : w.events)
    if (e.type == EventType::PowerEnded) ended = &e;
  CHECK(ended && ended->a == 0 && ended->c == rally);
  CHECK_EQ(w.b().activePower(0), -1);
  CHECK_STR(w.skillIds(w.unit("sol")), "keen bulwark");
  CHECK_EQ(w.stat(w.unit("sol"), "str"), 8);
}

TEST(a_power_of_two_turns_lasts_two_of_the_sides_turns) {
  World w("field", fixtureDir());
  const int surge = powerIndex(w, "ada", "surge");
  w.b().sides[0].meter = 100;
  CHECK(w.apply(Action::usePower(surge)));
  CHECK_EQ(w.b().powerTurnsLeft(0), 2);
  CHECK_EQ(w.stat(w.unit("kni"), "mov"), 5);
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));  // blue's second turn starts: one left
  CHECK_EQ(w.b().activePower(0), surge);
  CHECK_EQ(w.b().powerTurnsLeft(0), 1);
  CHECK_EQ(w.count(EventType::PowerEnded), 0);
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));  // the third: gone
  CHECK_EQ(w.count(EventType::PowerEnded), 1);
  CHECK_EQ(w.stat(w.unit("kni"), "mov"), 4);
}

TEST(a_technology_never_ends_up_behind_a_powers_skills) {
  Copy c("cmd_tech_order", "commanders");
  c.put("data/research.json", "{ \"technologies\": [ { \"id\": \"drill\", \"name\": \"Drill\", \"cost\": { \"gold\": 10 }, \"turns\": 1, \"grants\": { \"skills\": [\"fleet\"] } } ] }");
  World w("field", c.dir);
  w.b().sides[0].meter = 40;
  CHECK(w.apply(Action::usePower(0)));
  CHECK(w.apply(Action::research(w.db().findTechnology("drill"))));
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));  // the power ends, the technology finishes
  CHECK_STR(w.skillIds(w.unit("sol")), "keen bulwark fleet");
  w.b().sides[0].meter = 40;
  CHECK(w.apply(Action::usePower(0)));
  CHECK_STR(w.skillIds(w.unit("sol")), "keen bulwark fleet frenzy");
}

// ---- cost growth ----

TEST(each_use_makes_a_power_dearer_but_never_beyond_the_meter) {
  World w("field", fixtureDir());
  const int surge = powerIndex(w, "ada", "surge");  // 80, +50% of 80 per use, the meter tops at 100
  CHECK_EQ(w.b().powerCost(0, surge), 80);
  w.b().sides[0].meter = 100;
  CHECK(w.apply(Action::usePower(surge)));
  CHECK_EQ(w.b().meterOf(0), 20);
  CHECK_EQ(w.b().powerCost(0, surge), 100);  // 120 cut to the top
  CHECK_EQ(w.b().powerCost(0, powerIndex(w, "ada", "rally")), 40);  // the other power has no growth
  Copy c("cmd_growth", "commanders");
  c.swap("data/commanders.json", "\"cost\": 40,", "\"cost\": 40, \"costGrowthPercent\": 25,");
  World g("field", c.dir);
  const int rally = powerIndex(g, "ada", "rally");
  CHECK_EQ(g.b().powerCost(0, rally), 40);
  g.b().sides[0].meter = 100;
  CHECK(g.apply(Action::usePower(rally)));
  CHECK_EQ(g.b().powerCost(0, rally), 50);  // 40 + 25% of 40
  CHECK_EQ(g.b().meterOf(0), 60);
  CHECK(g.apply(Action::endTurn()));
  CHECK(g.apply(Action::endTurn()));
  CHECK(g.apply(Action::usePower(rally)));
  CHECK_EQ(g.b().meterOf(0), 10);  // paid 50
  CHECK_EQ(g.b().powerCost(0, rally), 60);
}
