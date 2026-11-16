// Step A1 of docs/SRPG.md "Summoning": a new unit appears (stats, level, side, items), the limit, vanishing (and in
// turn), lifetime, canAct, experience. The defeat rule and the filters of objectives and conditions are in
// test_abilities_summons_rules.cpp.
#include "abilities_summons_fixture.h"

using namespace ote;
using namespace abtest;

TEST(summons_a_new_unit_appears_on_the_summoners_side) {
  Camp c;
  c.setup();
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  const int h = c.summonOf(0);
  CHECK(h >= 0);
  const Unit& u = c.w.un(h);
  CHECK(u.alive && u.side == 0 && u.type == c.hound && u.cell == c.w.at(3, 2));
  CHECK_STR(u.name, "Hound");
  CHECK_EQ(u.level, 3);  // "summoner": the summoner's level
  CHECK(u.storyId.empty() && u.character == -1);
  CHECK(u.stats == (std::vector<int>{6, 3, 1, 3}) && u.hp == 6);
  CHECK(u.items.size() == 1 && !u.items[0].drop);  // its items never drop
  CHECK(u.summoner == 0 && u.summonAbility == c.call && u.summonLeft == -1);
  CHECK(u.moved && u.done);  // canAct is false: it arrives with its turn finished
  CHECK(c.w.battle.occupant[(size_t)c.w.at(3, 2)] == h);
  CHECK(c.w.un(0).done);
  const auto used = c.w.of(EventType::AbilityUsed);
  CHECK(used.size() == 1 && used[0].a == 0 && used[0].c == -1 && used[0].d == c.w.at(3, 2) && used[0].flags == 0);
  const auto added = c.w.of(EventType::UnitAdded);
  CHECK(added.size() == 1 && added[0].a == h && added[0].b == c.w.at(3, 2));
  CHECK(c.w.nextTurn());  // an ordinary unit from then on: it acts on its side's next turn
  CHECK(!c.w.un(h).done && c.w.act(Action::move(h, c.w.at(4, 2))));
}

TEST(summons_level_and_can_act_follow_the_ability) {
  Camp c;
  c.setup([](AbilityDef& d) {
    d.summon.level = 7;
    d.summon.canAct = true;
  });
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  const int h = c.summonOf(0);
  CHECK_EQ(c.w.un(h).level, 7);
  CHECK(!c.w.un(h).done && !c.w.un(h).moved);
  CHECK(c.w.act(Action::move(h, c.w.at(4, 2))));  // it can move and act at once
}

TEST(summons_the_square_must_be_free_in_range_and_standable) {
  Camp c;
  c.setup();
  CHECK_STR(c.w.why(0, c.call, -1, c.w.at(2, 2)), "that square is out of range");  // its own square is no target
  c.w.battle.occupant[(size_t)c.w.at(3, 2)] = 2;  // somebody there for a moment
  CHECK_STR(c.w.why(0, c.call, -1, c.w.at(3, 2)), "that square is occupied");
  c.w.battle.occupant[(size_t)c.w.at(3, 2)] = -1;
  CHECK_STR(c.w.why(0, c.call, -1, c.w.at(5, 2)), "that square is out of range");
  CHECK_STR(c.w.why(0, c.call, -1, kNoCell), "that square is not on the map");
  CHECK_STR(c.w.why(0, c.call, -1, c.w.at(3, 2)), "");
  std::vector<int> us;
  std::vector<Cell> cs;
  c.w.battle.abilityTargets(0, c.call, c.w.cellOf(0), us, cs);
  CHECK(us.empty());
  for (Cell cell : cs) {
    const int dist = c.w.battle.grid.distance(c.w.cellOf(0), cell);
    CHECK(dist >= 1 && dist <= 2 && c.w.battle.occupant[(size_t)cell] == -1);
  }
  CHECK_EQ(cs.size(), (size_t)12);  // the diamond of radius 2 minus its middle on open ground
}

TEST(summons_the_limit_counts_living_summons_of_this_ability_by_this_summoner) {
  Camp c;
  c.setup([](AbilityDef& d) { d.summon.limit = 2; });
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  CHECK(c.w.nextTurn());
  CHECK(c.w.use(0, c.call, -1, c.w.at(2, 3)));
  CHECK(c.w.nextTurn());
  CHECK_STR(c.w.why(0, c.call, -1, c.w.at(1, 2)), "already has 2 summoned Hounds");
  std::vector<AbilityOption> menu;
  c.w.battle.abilityOptions(0, c.w.cellOf(0), menu);
  CHECK(!menu[0].ok && menu[0].why == "already has 2 summoned Hounds");
  // A hound that is gone frees a place.
  const int gone = c.summonOf(0);
  c.w.battle.occupant[(size_t)c.w.un(gone).cell] = -1;
  c.w.mut(gone).alive = false;
  CHECK_STR(c.w.why(0, c.call, -1, c.w.at(1, 2)), "");
}

TEST(summons_a_limit_of_one_says_so_in_the_singular_and_is_per_summoner) {
  Camp c;
  c.setup();
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  CHECK(c.w.nextTurn());
  CHECK_STR(c.w.why(0, c.call, -1, c.w.at(1, 2)), "already has 1 summoned Hound");
  // Red's soldier has its own count.
  CHECK(c.w.act(Action::endTurn()));
  CHECK_STR(c.w.why(1, c.call, -1, c.w.at(6, 2)), "");
}

TEST(summons_vanish_when_the_summoner_falls_and_in_turn_without_a_defeat_or_xp) {
  Camp c(true);
  // The hound has an ability of its own: it summons an imp. Both leave with the lord.
  const int imp = addHound(c.w, "imp", "Imp");
  const int cast = 0;  // the first ability of the game; "call" becomes the second
  c.w.db.unitTypes[(size_t)c.hound].abilities.push_back(cast);
  c.w.db.abilities.push_back(AbWorld::summon("cast", imp));
  c.setup([](AbilityDef& d) { d.summon.canAct = true; });
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  const int h = c.summonOf(0);
  CHECK(c.w.use(h, cast, -1, c.w.at(3, 3)));
  const int i = c.summonOf(h);
  CHECK(i >= 0 && c.w.un(i).alive);
  c.redKillsLord();
  CHECK(!c.w.un(0).alive && !c.w.un(h).alive && !c.w.un(i).alive);
  CHECK(c.w.battle.occupant[(size_t)c.w.at(3, 2)] == -1 && c.w.battle.occupant[(size_t)c.w.at(3, 3)] == -1);
  CHECK_EQ(c.w.count(EventType::UnitDied), 1);     // only the lord fell
  CHECK_EQ(c.w.count(EventType::UnitRemoved), 2);  // the hound, then the imp: not a defeat
  CHECK_EQ(c.w.count(EventType::XpGained), 1);     // the killer's; nothing for what vanished
  CHECK(c.w.last(EventType::XpGained)->a == 1);
  CHECK(!c.w.battle.over);  // blue still has its archer
}

TEST(summons_stay_when_the_ability_says_vanish_false) {
  Camp c;
  c.setup([](AbilityDef& d) { d.summon.vanish = false; });
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  const int h = c.summonOf(0);
  c.redKillsLord();
  CHECK(!c.w.un(0).alive && c.w.un(h).alive);
  CHECK_EQ(c.w.count(EventType::UnitRemoved), 0);
}

TEST(summons_vanish_when_a_story_event_removes_the_summoner) {
  Camp c;
  CHECK(c.w.load(R"([{"id":"gone","when":{"turn":2,"side":"blue"},"do":[{"removeUnit":"lord"}]}])").empty());
  c.setup();
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  const int h = c.summonOf(0);
  CHECK(c.w.nextTurn());
  CHECK(!c.w.un(0).alive && !c.w.un(h).alive);
  CHECK_EQ(c.w.count(EventType::UnitRemoved), 2);
  CHECK_EQ(c.w.count(EventType::UnitDied), 0);
}

TEST(summons_a_lifetime_counts_the_summoners_sides_turns) {
  Camp c;
  c.setup([](AbilityDef& d) { d.summon.lifetime = 2; });
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  const int h = c.summonOf(0);
  CHECK_EQ(c.w.un(h).summonLeft, 2);
  CHECK(c.w.nextTurn());  // its side's next turn: it stays, one turn used
  CHECK(c.w.un(h).alive && c.w.un(h).summonLeft == 1);
  CHECK(c.w.nextTurn());
  CHECK(c.w.un(h).alive && c.w.un(h).summonLeft == 0);
  CHECK(c.w.nextTurn());  // "leaves when the next one begins"
  CHECK(!c.w.un(h).alive && c.w.battle.occupant[(size_t)c.w.at(3, 2)] == -1);
  CHECK_EQ(c.w.count(EventType::UnitRemoved), 1);
  CHECK_EQ(c.w.count(EventType::UnitDied), 0);
  // The place is free again for another.
  CHECK(c.w.why(0, c.call, -1, c.w.at(3, 2)).empty());
}

TEST(summons_a_summon_that_has_no_lifetime_stays_and_a_lifetime_of_one_gives_one_turn) {
  Camp c;
  c.setup([](AbilityDef& d) { d.summon.lifetime = 1; });
  CHECK(c.w.use(0, c.call, -1, c.w.at(3, 2)));
  const int h = c.summonOf(0);
  CHECK(c.w.nextTurn());
  CHECK(c.w.un(h).alive && !c.w.un(h).done);  // it has its one turn
  CHECK(c.w.nextTurn());
  CHECK(!c.w.un(h).alive);
}

TEST(summons_experience_a_summon_gives_it_unless_givesXp_is_false) {
  for (int gives = 0; gives < 2; ++gives)
    for (int kill = 0; kill < 2; ++kill) {
      Camp c(true);
      c.setup([&](AbilityDef& d) { d.summon.givesXp = gives == 1; });
      CHECK(c.w.act(Action::endTurn()));
      CHECK(c.w.use(1, c.call, -1, c.w.at(6, 2)));  // red's soldier summons a hound
      const int h = c.summonOf(1);
      CHECK(c.w.act(Action::endTurn()));
      if (kill) c.w.mut(h).hp = 1;
      c.w.mut(0).cell = c.w.at(5, 2);
      c.w.battle.occupant[(size_t)c.w.at(2, 2)] = -1;
      c.w.battle.occupant[(size_t)c.w.at(5, 2)] = 0;
      c.w.events.clear();
      CHECK(c.w.act(Action::attack(0, h, 0)));
      int toLord = 0, toHound = 0;
      for (const Event& e : c.w.events) {
        toLord += e.type == EventType::XpGained && e.a == 0;
        toHound += e.type == EventType::XpGained && e.a == h;
      }
      CHECK_EQ(toLord, gives == 1 ? 1 : 0);  // hitting or defeating it
      CHECK_EQ(toHound, kill ? 0 : 1);       // the hound earns XP like any unit, by countering (not when it fell)
    }
}

TEST(summons_squares_in_range_on_every_grid) {
  const size_t want[] = {12, 18, 18};  // the cells within two steps, minus the user's own: diamond / hexagon of radius 2
  for (size_t k = 0; k < 3; ++k) {
    AbWorld w(kKinds[k], false, {{"soldier", 0, 4, 4, false}, {"soldier", 1, 8, 8, false}});
    const int hound = addHound(w);
    const int call = w.give("soldier", AbWorld::summon("call", hound));
    CHECK(w.begin());
    std::vector<int> us;
    std::vector<Cell> cs;
    w.battle.abilityTargets(0, call, w.cellOf(0), us, cs);
    CHECK_EQ(cs.size(), want[k]);
    for (Cell c : cs) CHECK(w.battle.grid.distance(w.cellOf(0), c) <= 2);
    CHECK(w.use(0, call, -1, cs[0]));
    CHECK(w.battle.unitAt(cs[0]) >= 2 && w.un(w.battle.unitAt(cs[0])).summoner == 0);
  }
}
