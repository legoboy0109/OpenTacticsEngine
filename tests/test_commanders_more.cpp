// Commanders (docs/COMMANDERS.md), around the rules: saves (mid-power too, damaged ones refused), factions and the "choose" picks, action points,
// the speed turn order, and the computer opponent (it uses powers, never stalls, two computer sides finish the same way twice).
// Fixture tests/fixtures/commanders (numbers in commander_fixture.h).
#include "commander_fixture.h"

using namespace ote;
using namespace cmdtest;
using namespace jsonedit;

// ---- saves ----

TEST(the_commander_is_saved_with_its_meter_power_and_uses_and_round_trips) {
  World w("field", fixtureDir());
  Json j;
  saveBattleState(w.b(), j);
  const Json& blue = j["sides"][size_t(0)]["commander"];
  CHECK_STR(blue["id"].asString(), "ada");
  CHECK_EQ(blue["meter"].asInt(), 0);
  CHECK(!blue.has("power") && !blue.has("left"));
  CHECK_EQ(blue["uses"].size(), size_t(2));
  CHECK_STR(j["sides"][size_t(1)]["commander"]["id"].asString(), "bram");
  CHECK(!j["sides"][size_t(0)].has("modifiers"));  // passive skills are rebuilt, not saved
  w.b().sides[0].meter = 55;
  CHECK(w.apply(Action::usePower(powerIndex(w, "ada", "rally"))));
  Json k;
  saveBattleState(w.b(), k);
  const Json& mid = k["sides"][size_t(0)]["commander"];
  CHECK_EQ(mid["meter"].asInt(), 15);
  CHECK_EQ(mid["power"].asInt(), 0);
  CHECK_EQ(mid["left"].asInt(), 1);
  CHECK_EQ(mid["uses"][size_t(0)].asInt(), 1);
  CHECK(!k["sides"][size_t(0)].has("modifiers"));
}

TEST(a_save_in_the_middle_of_a_power_plays_on_exactly_like_the_live_battle) {
  World w("field", fixtureDir());
  w.b().sides[0].meter = 100;
  CHECK(w.apply(Action::usePower(powerIndex(w, "ada", "surge"))));  // two turns
  Battle r;
  std::string err;
  CHECK(restoreInto(w, r, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK_EQ(r.activePower(0), w.b().activePower(0));
  CHECK_EQ(r.powerTurnsLeft(0), 2);
  CHECK_EQ(r.powerCost(0, 1), w.b().powerCost(0, 1));  // the use count came back: the price is the grown one
  CHECK_EQ(r.powerUses(0, 1), 1);
  std::vector<int> a, b;
  w.b().skillsOf(w.unit("kni"), -1, a);
  r.skillsOf(r.units[(size_t)w.id("kni")], -1, b);
  CHECK(a == b);
  CHECK_STR(w.skillIds(w.unit("sol")), "keen bulwark fleet");
  // Both go on: the power ends on the same turn, with the same events.
  std::vector<Event> live, restored;
  for (int i = 0; i < 4; ++i) {
    CHECK(w.b().apply(Action::endTurn(), live));
    CHECK(r.apply(Action::endTurn(), restored));
  }
  CHECK(savetest::sameEvents(live, restored));
  CHECK_EQ(r.activePower(0), -1);
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
}

TEST(a_technology_and_a_power_keep_their_order_through_a_save) {
  Copy c("cmd_tech_save", "commanders");
  c.put("data/research.json", "{ \"technologies\": [ { \"id\": \"drill\", \"name\": \"Drill\", \"cost\": { \"gold\": 10 }, \"turns\": 1, \"grants\": { \"skills\": [\"fleet\"] } } ] }");
  World w("field", c.dir);
  CHECK(w.apply(Action::research(w.db().findTechnology("drill"))));
  w.b().sides[0].meter = 40;
  CHECK(w.apply(Action::usePower(0)));
  CHECK(w.apply(Action::endTurn()));
  Battle r;
  std::string err;
  CHECK(restoreInto(w, r, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK_STR(w.skillIds(w.unit("sol")), "keen bulwark frenzy");
  std::vector<int> list;
  r.skillsOf(r.units[(size_t)w.id("sol")], -1, list);
  CHECK_EQ((int)list.size(), 3);
  CHECK(r.sides[0].modifiers.back().power == 0);  // the power's skills come last, as live
}

TEST(damaged_commander_saves_are_refused_with_the_field) {
  World w("field", fixtureDir());
  w.b().sides[0].meter = 55;
  CHECK(w.apply(Action::usePower(powerIndex(w, "ada", "surge") - 1)));  // rally, active
  Json good;
  saveBattleState(w.b(), good);
  auto refusal = [&](const Json& bad) {
    Battle r;
    std::string err;
    CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err));
    return restoreBattleState(r, bad, err) ? std::string() : err;
  };
  CHECK_STR(refusal(good), "");
  const std::vector<std::string> c = {"sides", "#0", "commander"};
  auto at = [&](const char* key) { return std::vector<std::string>{"sides", "#0", "commander", key}; };
  CHECK(saysIn(refusal(withValue(good, at("id"), str("zed"))), "names unknown commander \"zed\""));
  CHECK(saysIn(refusal(withValue(good, at("id"), num(1))), "not a commander id"));
  CHECK(saysIn(refusal(withValue(good, at("meter"), num(101))), "sides[0].commander.meter"));
  CHECK(saysIn(refusal(withValue(good, at("meter"), num(-1))), "outside 0..100"));
  CHECK(saysIn(refusal(withValue(good, at("power"), num(2))), "sides[0].commander.power"));
  CHECK(saysIn(refusal(withValue(good, at("left"), num(2))), "sides[0].commander.left"));  // rally lasts 1
  CHECK(saysIn(refusal(withValue(good, at("left"), num(0))), "outside 1..1"));
  CHECK(saysIn(refusal(withoutAt(good, at("left"))), "sides[0].commander.left: missing"));
  CHECK(saysIn(refusal(withValue(good, at("uses"), list({0, 0, 0}))), "has 3 entries, expected 2"));
  CHECK(saysIn(refusal(withValue(good, at("uses"), list({0, 0}))), "the active power was never used"));
  CHECK(saysIn(refusal(withValue(good, at("uses"), list({-1, 1}))), "not a whole number from 0"));
  CHECK(saysIn(refusal(withoutAt(good, at("uses"))), "sides[0].commander.uses: missing"));
  CHECK(saysIn(refusal(withoutAt(good, c)), "sides[0].commander: missing (the map gives this side a commander)"));
  // A power that has no timed skills cannot be the active one (bram's storm).
  Json bram = withValue(good, {"sides", "#1", "commander", "power"}, num(0));
  CHECK(saysIn(refusal(withValue(bram, {"sides", "#1", "commander", "left"}, num(1))), "has no timed skills, it cannot be active"));
  // "left" with no power.
  Json idle;
  saveBattleState(World("field", fixtureDir()).b(), idle);
  CHECK(saysIn(refusal(withValue(idle, at("left"), num(1))), "there is no active power"));
  // A save of a game without commanders cannot carry one.
  World p("field", ote_test::sourceDir() + "/tests/fixtures/research-branches");
  Json other;
  saveBattleState(p.b(), other);
  Battle r;
  std::string err;
  CHECK(r.init(p.rig.db, p.rig.map, p.rig.formulas, 7, err));
  CHECK(!restoreBattleState(r, withValue(other, c, good["sides"][size_t(0)]["commander"]), err));
  CHECK(saysIn(err, "the game has no commanders"));
}

TEST(a_persons_pick_lives_in_the_save) {
  // The player's pick lives only in the save: a map with "choose" restores whichever commander the save names.
  World w("pick", fixtureDir());
  w.b().sides[0].meter = 7;
  Json j;
  saveBattleState(w.b(), j);
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 3, err));
  CHECK(restoreBattleState(r, withValue(j, {"sides", "#0", "commander", "id"}, str("cato")), err));
  CHECK_EQ(r.sides[0].commander, w.db().findCommander("cato"));
  CHECK_EQ(r.sides[0].meter, 7);
}

// ---- factions and the picks ----

TEST(a_choose_side_takes_a_commander_the_faction_lists_and_the_seed_decides_for_the_computer) {
  World w("field", fixtureDir());
  const Database& db = w.db();
  const int order = db.findFaction("order"), horde = db.findFaction("horde"), freeCo = db.findFaction("free");
  const int ada = db.findCommander("ada"), bram = db.findCommander("bram"), cato = db.findCommander("cato");
  // A person's side takes the first one the faction allows; without a faction the first of the game.
  CHECK_EQ(Battle::defaultCommanderChoice(db, 1, 0, false, order), ada);
  CHECK_EQ(Battle::defaultCommanderChoice(db, 1, 0, false, horde), bram);
  CHECK_EQ(Battle::defaultCommanderChoice(db, 1, 0, false, freeCo), ada);
  CHECK_EQ(Battle::defaultCommanderChoice(db, 1, 0, false, -1), ada);
  bool sawAda = false, sawCato = false, sawAny[3] = {false, false, false};
  for (unsigned seed = 1; seed <= 60; ++seed) {
    const int o = Battle::defaultCommanderChoice(db, seed, 1, true, order);
    CHECK(o == ada || o == cato);
    sawAda = sawAda || o == ada;
    sawCato = sawCato || o == cato;
    CHECK_EQ(Battle::defaultCommanderChoice(db, seed, 1, true, horde), bram);
    CHECK_EQ(o, Battle::defaultCommanderChoice(db, seed, 1, true, order));  // the same seed, the same pick
    sawAny[Battle::defaultCommanderChoice(db, seed, 1, true, freeCo)] = true;
  }
  CHECK(sawAda && sawCato);
  CHECK(sawAny[0] && sawAny[1] && sawAny[2]);
}

TEST(the_pick_map_settles_factions_then_commanders_and_leaves_the_random_stream_alone) {
  World f("field", fixtureDir());
  for (unsigned seed = 1; seed <= 25; ++seed) {
    savetest::Rig rig;
    CHECK(rig.load(fixtureDir(), "pick", seed));
    const SideState& red = rig.battle.sides[1];
    CHECK(red.faction >= 0 && red.commander >= 0);
    const FactionDef& fac = rig.db.factions[(size_t)red.faction];
    if (fac.limitCommanders) {
      bool listed = false;
      for (int c : fac.commanders) listed = listed || c == red.commander;
      CHECK(listed);
    }
    const SideState& blue = rig.battle.sides[0];
    CHECK_EQ(blue.faction, 0);  // a person's side without a pick takes the first
    CHECK_EQ(blue.commander, 0);
    savetest::Rig again;
    CHECK(again.load(fixtureDir(), "field", seed));
    CHECK_EQ(rig.battle.rng.next(), again.battle.rng.next());  // a "choose" shifts nothing
  }
}

TEST(a_commander_the_map_names_wins_over_the_factions_list_and_a_person_pick_is_taken) {
  Copy c("cmd_wins", "commanders");
  c.swap("maps/field.json", "\"commander\": \"bram\"", "\"commander\": \"ada\"");  // red is horde, whose list says bram only
  World w("field", c.dir);
  CHECK_EQ(w.b().sides[1].commander, w.db().findCommander("ada"));
  // The player writes the person's pick into the map the way it does for factions.
  savetest::Rig rig;
  std::string err;
  CHECK(loadGame(fixtureDir(), rig.db, err) && loadMap(rig.db, "pick", rig.map, err) && rig.formulas.init(rig.db, err));
  rig.map.sides[0].faction = rig.db.findFaction("order");
  rig.map.sides[0].factionChoose = false;
  rig.map.sides[0].commander = rig.db.findCommander("cato");
  rig.map.sides[0].commanderChoose = false;
  CHECK(rig.battle.init(rig.db, rig.map, rig.formulas, 5, err));
  CHECK_EQ(rig.battle.sides[0].commander, rig.db.findCommander("cato"));
  CHECK_EQ(rig.battle.sides[0].powerUses.size(), size_t(2));
  CHECK_EQ(rig.battle.meterMax(0), 50);
}

TEST(the_commanders_data_is_part_of_the_saves_version) {
  std::string a, b, err;
  Copy c("cmd_hash", "commanders");
  Database db1, db2;
  CHECK(loadGame(c.dir, db1, err));
  CHECK(computeDataHash(db1, "field", a, err));
  c.swap("data/commanders.json", "\"cost\": 40", "\"cost\": 41");
  CHECK(loadGame(c.dir, db2, err));
  CHECK(computeDataHash(db2, "field", b, err));
  CHECK(a != b);
  c.swap("data/commanders.json", "\"cost\": 41", "\"cost\": 40");
  c.swap("data/commanders.json", "\"name\": \"Ada\"", "\"name\": \"Adda\"");  // a name is cosmetic
  Database db3;
  std::string d;
  CHECK(loadGame(c.dir, db3, err));
  CHECK(computeDataHash(db3, "field", d, err));
  CHECK_STR(a, d);
}

// ---- action points and the speed turn order ----

namespace {

// The commanders fixture's skills and commanders written into a copy of another fixture, and its map sides given commanders.
void giveCommanders(Copy& c, const char* blueCommander, const char* redCommander) {
  std::string skills;
  readTextFile(fixtureDir() + "/data/skills.json", skills);
  c.put("data/skills.json", skills);
  // The fixture's commanders without the unit filters (these games have other unit types).
  c.put("data/commanders.json",
        "{ \"commanders\": [\n"
        "  { \"id\": \"ada\", \"passive\": [\"bulwark\"], \"meter\": { \"max\": 100, \"perDamageDealt\": 2, \"perDamageTaken\": 1 },\n"
        "    \"powers\": [ { \"id\": \"rally\", \"cost\": 40, \"modifiers\": [\"frenzy\"], \"effects\": [ { \"heal\": 5 } ] },\n"
        "                 { \"id\": \"surge\", \"cost\": 80, \"turns\": 2, \"modifiers\": [\"fleet\"], \"effects\": [ { \"refresh\": true } ] } ] },\n"
        "  { \"id\": \"bram\", \"passive\": [\"fleet\"], \"meter\": { \"max\": 60, \"perDamageDealt\": 1, \"perTurn\": 10 },\n"
        "    \"powers\": [ { \"id\": \"storm\", \"cost\": 30, \"effects\": [ { \"damage\": 4, \"to\": \"enemies\" }, { \"giveResource\": { \"gold\": 25 } } ] } ] } ] }");
  c.swap("maps/field.json", "\"id\": \"blue\"", std::string("\"id\": \"blue\", \"commander\": \"") + blueCommander + "\"");
  c.swap("maps/field.json", "\"id\": \"red\"", std::string("\"id\": \"red\", \"commander\": \"") + redCommander + "\"");
}

}  // namespace

TEST(in_an_action_point_game_a_power_costs_no_action_points) {
  Copy c("cmd_ap", "action-points");
  giveCommanders(c, "ada", "bram");
  World w("field", c.dir);
  CHECK(w.db().rules.ap.on);
  const int ap = w.db().findResource("ap");
  const int pool = w.b().apPool(0);
  CHECK(pool > 0);
  w.b().sides[0].meter = 40;
  CHECK(w.apply(Action::usePower(0)));
  CHECK_EQ(w.b().sides[0].resources[(size_t)ap], pool);  // nothing paid
  CHECK_EQ(w.b().apPool(0), pool);
  CHECK_EQ(w.b().activePower(0), 0);
  // The skills of the power count (the lancer: str 6 + 3), and the units still act on the shared pool.
  CHECK_EQ(w.stat(w.unit("lan"), "str"), 9);
  CHECK(w.apply(Action::move(w.id("lan"), w.at(3, 2))));
  CHECK(w.b().apPool(0) < pool);
  // Meter points come from strikes there too.
  CHECK_EQ(w.b().meterOf(0), 0);
  w.b().sides[1].meter = 0;
  CHECK(w.apply(Action::endTurn()));
  CHECK_EQ(w.b().meterOf(1), 10);  // Bram's per-turn points
  // The computer side uses one (full meter) and its step ends in the usual way.
  w.b().sides[1].meter = 60;
  std::vector<Event> ev;
  aiTakeStep(w.b(), ev);
  int used = 0;
  for (const Event& e : ev) used += e.type == EventType::PowerUsed;
  CHECK_EQ(used, 1);
}

TEST(in_the_speed_turn_order_a_power_is_used_in_a_units_turn_and_counts_rounds) {
  Copy c("cmd_speed", "speed-order");
  giveCommanders(c, "ada", "bram");
  World w("field", c.dir);
  Battle& b = w.b();
  CHECK(b.speedGame());
  CHECK_EQ(b.activeSide, 0);  // the scout of blue has the first turn
  std::string why;
  b.sides[0].meter = 40;
  b.sides[1].meter = 30;
  CHECK(b.canUsePower(0, 0));
  CHECK(!b.canUsePower(1, 0, &why));  // it is not red's unit that has the turn
  CHECK(why.find("not that side's") != std::string::npos);
  const int round0 = b.round;
  CHECK(w.apply(Action::usePower(0)));
  CHECK_EQ(b.activePower(0), 0);
  // Everybody waits until the next round starts: the power ends right there, with the round.
  int ended = 0, guard = 0;
  bool endedWithRound = false;
  while (b.round == round0 && guard++ < 60) {
    CHECK(w.apply(Action::wait(b.activeUnit)));
    if (w.count(EventType::PowerEnded)) {
      ended += w.count(EventType::PowerEnded);
      endedWithRound = w.count(EventType::RoundStarted) > 0;
    }
    if (b.activeSide == 1 && b.round == round0) {
      CHECK(!b.canUsePower(0, 0));  // blue's power cannot be used in a red unit's turn
      CHECK(b.canUsePower(1, 0));
    }
  }
  CHECK(b.round > round0);
  CHECK_EQ(ended, 1);
  CHECK(endedWithRound);
  CHECK_EQ(b.activePower(0), -1);
  CHECK_EQ(b.meterOf(1), 40);  // Bram's 10 per round: the new round added to the 30 it had
}

TEST(a_commander_and_the_speed_save_round_trip) {
  Copy c("cmd_speed_save", "speed-order");
  giveCommanders(c, "ada", "bram");
  World w("field", c.dir);
  w.b().sides[0].meter = 100;
  CHECK(w.apply(Action::usePower(1)));  // surge: refresh (nobody has acted) and two rounds
  Battle r;
  std::string err;
  CHECK(restoreInto(w, r, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK_EQ(r.powerTurnsLeft(0), 2);
}

// ---- the computer opponent ----

namespace {

// A wide map with the armies far apart, red led by `redCommander`.
std::string farMap(const char* redCommander) {
  return std::string("{ \"name\": \"Far\", \"grid\": { \"type\": \"square\", \"width\": 20, \"height\": 3 }, \"legend\": { \".\": \"plain\" },\n"
                     "  \"tiles\": [\"....................\", \"....................\", \"....................\"],\n"
                     "  \"sides\": [ { \"id\": \"blue\", \"controller\": \"human\", \"faction\": \"order\", \"commander\": \"ada\" },\n"
                     "             { \"id\": \"red\", \"controller\": \"ai\", \"faction\": \"free\", \"commander\": \"") +
         redCommander +
         "\" } ],\n"
         "  \"units\": [ { \"type\": \"soldier\", \"side\": \"blue\", \"x\": 0, \"y\": 1, \"id\": \"sol\", \"leader\": true },\n"
         "    { \"type\": \"knight\", \"side\": \"blue\", \"x\": 0, \"y\": 0, \"id\": \"kni\" },\n"
         "    { \"type\": \"soldier\", \"side\": \"red\", \"x\": 19, \"y\": 1, \"id\": \"foe\", \"leader\": true },\n"
         "    { \"type\": \"golem\", \"side\": \"red\", \"x\": 19, \"y\": 2, \"id\": \"rgo\" } ] }";
}

int powersUsedInTurn(World& w, int cap = 400) {
  int used = 0;
  const int side = w.b().activeSide, round = w.b().round;
  std::vector<Event> ev;
  for (int i = 0; i < cap && !w.b().over && w.b().activeSide == side && w.b().round == round; ++i) {
    ev.clear();
    aiTakeStep(w.b(), ev);
    for (const Event& e : ev) used += e.type == EventType::PowerUsed;
  }
  CHECK(w.b().over || w.b().activeSide != side || w.b().round != round);  // the turn ended: it never stalls
  return used;
}

}  // namespace

TEST(the_computer_uses_a_power_at_once_when_its_meter_is_full) {
  World w("field", fixtureDir());
  CHECK(w.apply(Action::endTurn()));  // red / Bram: the meter has 10 from the turn start
  w.b().sides[1].meter = 60;
  std::vector<Event> ev;
  aiTakeStep(w.b(), ev);
  CHECK(!ev.empty() && ev[0].type == EventType::PowerUsed);  // before any unit moves
  CHECK_EQ(w.b().meterOf(1), 30);
}

TEST(the_computer_uses_a_power_when_the_fighting_is_about_to_start_and_not_before) {
  {
    Copy c("cmd_near", "commanders");
    c.swap("maps/field.json", "\"commander\": \"bram\"", "\"commander\": \"cato\"");  // timed skills only: "wall"
    c.swap("maps/field.json", "\"x\": 6, \"y\": 2, \"id\": \"rgo\"", "\"x\": 5, \"y\": 2, \"id\": \"rgo\"");  // both red units can reach a blue one
    World w("field", c.dir);  // the armies are two or three squares apart
    CHECK(w.apply(Action::endTurn()));
    w.b().sides[1].meter = 20;
    CHECK_EQ(powersUsedInTurn(w), 1);
    CHECK_EQ(w.b().meterOf(1), 0);
  }
  {
    Copy c("cmd_far", "commanders");
    c.put("maps/field.json", farMap("cato"));
    World w("field", c.dir);
    CHECK(w.apply(Action::endTurn()));
    w.b().sides[1].meter = 25;  // enough for either of Cato's powers, not full (50)
    CHECK_EQ(powersUsedInTurn(w), 0);  // nothing in reach and nobody hurt: it saves the meter
    CHECK_EQ(w.b().meterOf(1), 25);
  }
}

TEST(the_computer_heals_only_when_enough_is_missing) {
  // Far from the enemy only the heal is judged. Ada's "rally" heals every unit 5 (and gives timed skills, which need a fight to be worth it): hurt
  // units count by what they would really get back, and twice the amount is the least that pays.
  Copy far("cmd_ai_heal_far", "commanders");
  far.put("maps/field.json", farMap("ada"));
  World f("field", far.dir);
  CHECK(f.apply(Action::endTurn()));
  f.b().sides[1].meter = 40;  // rally's price, not full (100)
  f.unit("foe").hp = 15;      // one unit missing 5: below twice the amount
  CHECK_EQ(powersUsedInTurn(f), 0);
  CHECK_EQ(f.b().meterOf(1), 40);
  CHECK(f.apply(Action::endTurn()));  // red's turn again
  f.unit("foe").hp = 10;
  f.unit("rgo").hp = 30;  // 5 + 5 really healed = twice the amount
  std::vector<Event> ev;
  aiTakeStep(f.b(), ev);
  int used = 0;
  for (const Event& e : ev) used += e.type == EventType::PowerUsed;
  CHECK_EQ(used, 1);
  CHECK_EQ(f.unit("foe").hp, 15);
  CHECK_EQ(f.unit("rgo").hp, 35);
  // Near the enemy the same meter is spent without anybody hurt: the timed skills are worth it once the fighting starts.
  Copy near("cmd_ai_heal_near", "commanders");
  near.swap("maps/field.json", "\"faction\": \"horde\", \"commander\": \"bram\"", "\"faction\": \"free\", \"commander\": \"ada\"");
  near.swap("maps/field.json", "\"x\": 6, \"y\": 2, \"id\": \"rgo\"", "\"x\": 5, \"y\": 2, \"id\": \"rgo\"");
  World w("field", near.dir);
  CHECK(w.apply(Action::endTurn()));
  w.b().sides[1].meter = 40;
  ev.clear();
  aiTakeStep(w.b(), ev);
  used = 0;
  for (const Event& e : ev) used += e.type == EventType::PowerUsed;
  CHECK_EQ(used, 1);
  CHECK_EQ(w.b().activePower(1), 0);
}

TEST(two_computer_sides_with_commanders_finish_the_same_way_twice) {
  int finished = 0, usedAny = 0, different = 0;
  for (unsigned seed = 1; seed <= 14; ++seed) {
    std::vector<Event> first, second;
    std::string state1, state2;
    int cmd0 = -1, cmd1 = -1;
    for (int pass = 0; pass < 2; ++pass) {
      savetest::Rig rig;
      CHECK(rig.load(fixtureDir(), "duel", seed));
      cmd0 = rig.battle.sides[0].commander;
      cmd1 = rig.battle.sides[1].commander;
      std::vector<Event>& ev = pass ? second : first;
      rig.battle.start(ev);
      for (int i = 0; i < 6000 && !rig.battle.over; ++i) aiTakeStep(rig.battle, ev);
      (pass ? state2 : state1) = savetest::stateText(rig.battle);
      if (pass == 0) {
        CHECK(rig.battle.over);  // never stalls
        finished += rig.battle.over;
      }
    }
    CHECK(savetest::sameEvents(first, second));
    CHECK_STR(state1, state2);
    for (const Event& e : first) usedAny += e.type == EventType::PowerUsed;
    different += cmd0 != cmd1;
  }
  CHECK_EQ(finished, 14);
  CHECK(usedAny > 0);    // the computer does use its powers
  CHECK(different > 0);  // and some of these games had two different commanders
}
