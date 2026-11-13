#include "battle_fixture.h"

using namespace ote;
using fixture::World;

TEST(battle_movement_costs_walls_and_blocking) {
  // Soldier (mov 3) at (0,0). Forest costs 2, wall impassable.
  World w({"..f..",
           ".#...",
           "....."},
          {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 1, 0, 2, false}});
  CHECK(w.begin());
  Reach r;
  w.battle.reachable(0, r);
  CHECK_EQ(r.cost[w.at(0, 0)], 0);
  CHECK_EQ(r.cost[w.at(1, 0)], 1);   // through an ally...
  CHECK(!r.canStop(w.at(1, 0)));     // ...but cannot stop on it
  CHECK_EQ(r.cost[w.at(2, 0)], 3);   // forest costs 2
  CHECK_EQ(r.cost[w.at(3, 0)], -1);  // out of movement
  CHECK_EQ(r.cost[w.at(1, 1)], -1);  // wall
  CHECK_EQ(r.cost[w.at(0, 2)], -1);  // enemy blocks
  CHECK_EQ(r.cost[w.at(0, 1)], 1);

  CHECK(!w.act(Action::move(0, w.at(1, 0))));  // occupied
  CHECK(!w.act(Action::move(0, w.at(4, 2))));  // too far
  CHECK(!w.act(Action::move(2, w.at(1, 2))));  // not red's turn
  CHECK(w.act(Action::move(0, w.at(2, 0))));
  CHECK_EQ(w.battle.units[0].cell, w.at(2, 0));
  CHECK_EQ(w.battle.unitAt(w.at(0, 0)), -1);
  CHECK_EQ(w.battle.unitAt(w.at(2, 0)), 0);
  const Event* mv = w.last(EventType::UnitMoved);
  CHECK(mv && mv->data.size() == 3 && mv->data.front() == w.at(0, 0) && mv->data.back() == w.at(2, 0));
  CHECK(!w.act(Action::move(0, w.at(1, 0))));  // already moved
}

TEST(battle_attack_counter_and_rout) {
  World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  CHECK(w.begin());
  Forecast f;
  CHECK(w.battle.forecast(0, 1, 0, w.at(0, 0), f));
  CHECK_EQ(f.attack.damage, 3);  // atk 5 - def 2
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.damage, 3);

  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(w.battle.units[1].hp, 7);
  CHECK_EQ(w.battle.units[0].hp, 7);  // countered
  CHECK(w.battle.units[0].done);
  CHECK(!w.act(Action::attack(0, 1, 0)));  // already acted
  CHECK_EQ(w.count(EventType::Attack), 2);
  CHECK(w.last(EventType::Attack)->flags & kCounter);

  // Trade blows until someone dies; blue strikes first each round so red
  // (hp 7 -> 4 -> 1 -> dead) falls first.
  for (int i = 0; i < 10 && !w.battle.over; ++i) {
    CHECK(w.act(Action::endTurn()));
    if (w.battle.activeSide == 0) w.act(Action::attack(0, 1, 0));
  }
  CHECK(w.battle.over);
  CHECK_EQ(w.battle.winnerTeam, w.battle.sides[0].team);
  CHECK(!w.battle.units[1].alive);
  CHECK_EQ(w.count(EventType::SideDefeated), 1);
  CHECK_EQ(w.count(EventType::BattleEnded), 1);
  CHECK(!w.act(Action::endTurn()));  // nothing works after the end
}

TEST(battle_ranged_no_counter_and_versus_block) {
  World w({"....."}, {{"archer", 0, 0, 0, false}, {"soldier", 1, 2, 0, false},
                      {"tank", 1, 4, 0, false}, {"soldier", 0, 3, 0, false}});
  CHECK(w.begin());
  std::vector<AttackOption> opts;
  w.battle.attackOptions(0, w.at(0, 0), opts);
  CHECK_EQ((int)opts.size(), 1);  // only the soldier at distance 2
  CHECK(w.act(Action::attack(0, 1, 1)));
  CHECK_EQ(w.battle.units[1].hp, 8);  // 4 - 2
  CHECK_EQ(w.battle.units[0].hp, 8);  // sword cannot answer at range 2
  // Sword's versus table has no entry for tanks: not targetable at all.
  w.battle.attackOptions(3, w.at(3, 0), opts);
  CHECK_EQ((int)opts.size(), 1);  // the adjacent soldier only
  CHECK(!w.act(Action::attack(3, 2, 0)));
}

TEST(battle_turns_and_income) {
  World w({"c..c"}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  w.db.rules.economy = true;
  w.map.owner[0] = 0;
  w.map.owner[3] = 1;
  CHECK(w.begin());
  CHECK_EQ(w.battle.sides[0].resources[0], 100);  // income on turn start
  CHECK_EQ(w.battle.sides[1].resources[0], 0);
  CHECK(w.act(Action::move(0, w.at(1, 0))));
  CHECK(w.act(Action::wait(0)));
  CHECK(w.act(Action::endTurn()));
  CHECK_EQ(w.battle.activeSide, 1);
  CHECK_EQ(w.battle.round, 1);
  CHECK_EQ(w.battle.sides[1].resources[0], 100);
  CHECK(w.act(Action::endTurn()));
  CHECK_EQ(w.battle.activeSide, 0);
  CHECK_EQ(w.battle.round, 2);
  CHECK(!w.battle.units[0].moved && !w.battle.units[0].done);  // refreshed
  CHECK_EQ(w.battle.sides[0].resources[0], 200);
}

TEST(battle_capture_progress_reset_and_hq_defeat) {
  World w({".H.H"}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false},
                     {"archer", 0, 2, 0, false}});
  w.db.rules.capture = true;
  w.db.rules.defeatLostHq = true;
  w.db.rules.fCaptureAmount = "a.hp";  // 10 per action, hq has 20 points
  w.map.owner[1] = 0;
  w.map.owner[3] = 1;
  CHECK(w.begin());
  CHECK(!w.battle.canCaptureAt(0, w.at(1, 0)));  // own building
  CHECK(!w.battle.canCaptureAt(2, w.at(3, 0)));  // archers cannot capture


  // Progress, reset and completion on a separate small map.
  World n({"H.c."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 3, 0, false}});
  n.db.rules.capture = true;
  n.db.rules.defeatLostHq = true;
  n.db.rules.defeatNoUnits = false;
  n.db.rules.fCaptureAmount = "a.hp";
  n.map.owner[0] = 1;  // red owns the HQ at x=0
  CHECK(n.begin());
  CHECK(n.act(Action::move(0, n.at(2, 0))));
  CHECK(n.act(Action::capture(0)));
  CHECK_EQ(n.battle.cells[n.at(2, 0)].capture, 10);
  CHECK_EQ(n.battle.cells[n.at(2, 0)].owner, -1);
  CHECK(n.act(Action::endTurn()));
  CHECK(n.act(Action::endTurn()));
  // Walking away resets progress.
  CHECK(n.act(Action::move(0, n.at(1, 0))));
  CHECK_EQ(n.battle.cells[n.at(2, 0)].capture, 20);
  CHECK(n.act(Action::endTurn()));
  CHECK(n.act(Action::endTurn()));
  // Take the enemy HQ in two actions -> red is defeated.
  CHECK(n.act(Action::move(0, n.at(0, 0))));
  CHECK(n.act(Action::capture(0)));
  CHECK(!n.battle.over);
  CHECK(n.act(Action::endTurn()));
  CHECK(n.act(Action::endTurn()));
  CHECK(n.act(Action::capture(0)));
  CHECK_EQ(n.battle.cells[n.at(0, 0)].owner, 0);
  CHECK_EQ(n.count(EventType::CellCaptured), 1);
  CHECK(n.battle.over);
  CHECK_EQ(n.battle.winnerTeam, n.battle.sides[0].team);
}

TEST(battle_production) {
  World w({"F..F"}, {{"soldier", 1, 3, 0, false}});
  w.db.rules.production = true;
  w.map.owner[0] = 0;
  w.map.sides[0].resources = {{0, 150}};
  CHECK(w.begin());
  std::vector<int> opts;
  w.battle.buildOptions(w.at(0, 0), opts);
  CHECK_EQ((int)opts.size(), 1);
  w.battle.buildOptions(w.at(3, 0), opts);
  CHECK_EQ((int)opts.size(), 0);  // not ours
  CHECK(!w.battle.sides[0].everHadUnits);
  CHECK(!w.battle.over);  // no units yet, but never had any -> not defeated
  CHECK(w.act(Action::build(w.at(0, 0), 0)));
  CHECK_EQ(w.battle.sides[0].resources[0], 50);
  CHECK_EQ((int)w.battle.units.size(), 2);
  CHECK(w.battle.units[1].done);                  // built units wait a turn
  CHECK(!w.act(Action::build(w.at(0, 0), 0)));    // occupied
  CHECK(!w.act(Action::move(1, w.at(1, 0))));
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(1, w.at(1, 0))));
  CHECK(!w.act(Action::build(w.at(0, 0), 0)));    // 50 funds < 100
}

TEST(battle_progression_levels_up) {
  World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  w.db.rules.progression = true;
  w.db.rules.fXpHit = "60";
  w.db.rules.fXpKill = "100";
  CHECK(w.begin());
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(w.battle.units[0].xp, 60);
  CHECK_EQ(w.battle.units[1].xp, 60);  // defender earns for its counter
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::attack(0, 1, 0)));
  // 120 xp -> level 2 with 20 left; growths hp 100%, atk 100%.
  CHECK_EQ(w.battle.units[0].level, 2);
  CHECK_EQ(w.battle.units[0].xp, 20);
  CHECK_EQ(w.battle.units[0].stats[0], 11);
  CHECK_EQ(w.battle.units[0].stats[1], 6);
  CHECK_EQ(w.battle.units[0].stats[2], 2);
  const Event* up = w.last(EventType::LevelUp);
  CHECK(up && up->a == 0 && up->b == 2 && up->data[0] == 1 && up->data[2] == 0);
}

TEST(battle_hit_and_strikes_formulas) {
  World miss({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  miss.db.rules.fHit = "0";
  CHECK(miss.begin());
  CHECK(miss.act(Action::attack(0, 1, 0)));
  CHECK_EQ(miss.battle.units[1].hp, 10);
  CHECK_EQ(miss.battle.units[0].hp, 10);
  CHECK(!(miss.last(EventType::Attack)->flags & kHit));

  World twice({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  twice.db.rules.fStrikes = "2";
  twice.db.rules.counterAttacks = false;
  CHECK(twice.begin());
  CHECK(twice.act(Action::attack(0, 1, 0)));
  CHECK_EQ(twice.battle.units[1].hp, 4);
  CHECK_EQ(twice.battle.units[0].hp, 10);

  World crit({"...."}, {{"soldier", 0, 0, 0, false}, {"tank", 1, 1, 0, false}});
  crit.db.weapons[0].versus.clear();  // let swords hit tanks here
  crit.db.rules.fDamage = "4";
  crit.db.rules.fCrit = "100";
  crit.db.rules.critMultiplier = 3;
  crit.db.rules.counterAttacks = false;
  CHECK(crit.begin());
  CHECK(crit.act(Action::attack(0, 1, 0)));
  CHECK_EQ(crit.battle.units[1].hp, 8);  // 20 - 4*3
  CHECK(crit.last(EventType::Attack)->flags & kCrit);
}

TEST(battle_leader_death_ends_battle) {
  World w({"...."}, {{"tank", 0, 0, 0, false}, {"archer", 1, 1, 0, true},
                     {"soldier", 1, 3, 0, false}});
  w.db.rules.defeatLeaderDead = true;
  w.db.rules.fDamage = "99";
  CHECK(w.begin());
  CHECK(w.act(Action::attack(0, 1, 2)));
  CHECK(w.battle.over);
  CHECK(!w.battle.units[2].alive);  // the rest of the side leaves the field
  CHECK_EQ(w.battle.winnerTeam, w.battle.sides[0].team);
}

TEST(battle_active_side_wiped_by_counter_passes_turn) {
  // Blue's only unit dies to the counter while a third side still exists.
  World w({"....."}, {{"archer", 0, 0, 0, false}, {"tank", 1, 1, 0, false}});
  w.db.weapons[1].minRange = 1;  // let the bow hit adjacent
  w.db.weapons[1].maxRange = 1;
  w.db.rules.fDamage = "a.atk";  // tank counter deals 8 = archer's whole hp
  MapSideDef green; green.id = "green"; green.name = "Green";
  w.map.sides.push_back(green);
  MapUnitDef g; g.type = 0; g.side = 2; g.x = 4; g.y = 0; g.stats = w.db.unitTypes[0].stats;
  w.map.units.push_back(g);
  CHECK(w.begin());
  CHECK(w.act(Action::attack(0, 1, 1)));
  CHECK(!w.battle.units[0].alive);
  CHECK(w.battle.sides[0].defeated);
  CHECK(!w.battle.over);
  CHECK_EQ(w.battle.activeSide, 1);  // turn moved on automatically
}

TEST(battle_is_deterministic_and_copyable) {
  auto run = [](uint32_t seed, std::vector<int>& hpOut) {
    World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
    w.db.rules.fHit = "50";
    w.db.rules.fCrit = "30";
    if (!w.begin(seed)) return;
    for (int i = 0; i < 6 && !w.battle.over; ++i) {
      w.act(Action::attack(w.battle.activeSide, 1 - w.battle.activeSide, 0));
      w.act(Action::endTurn());
    }
    hpOut = {w.battle.units[0].hp, w.battle.units[1].hp};
  };
  std::vector<int> a, b;
  run(42, a);
  run(42, b);
  CHECK(a == b && !a.empty());

  World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  CHECK(w.begin());
  Battle snapshot = w.battle;  // undo point
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(snapshot.units[1].hp, 10);
  CHECK_EQ(w.battle.units[1].hp, 7);
  std::vector<Event> ev;
  CHECK(snapshot.apply(Action::attack(0, 1, 0), ev));  // the copy still works
  CHECK_EQ(snapshot.units[1].hp, 7);
}

TEST(formulas_report_syntax_errors_and_survive_runtime_errors) {
  World bad({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  bad.db.rules.fDamage = "a.atk +";
  std::string err;
  CHECK(!bad.formulas.init(bad.db, err));
  CHECK(err.find("formulas.damage") != std::string::npos);

  World rt({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  rt.db.rules.fDamage = "a.nope.field";  // runtime error -> treated as 0 damage
  ote::setLogLevel(ote::LogLevel::Error);
  ote::setLogSink([](ote::LogLevel, const char*) {});
  CHECK(rt.begin());
  CHECK(rt.act(Action::attack(0, 1, 0)));
  CHECK_EQ(rt.battle.units[1].hp, 10);
  ote::setLogSink(nullptr);
  ote::setLogLevel(ote::LogLevel::Info);
}
