// Research in a battle (docs/ECONOMY.md): StartResearch, the countdown, pausing, unlocking unit types,
// stat bonuses, and the queries the player and the AI use.
#include "research_fixture.h"

using namespace ote;
using namespace restest;

TEST(research_pays_then_finishes_at_the_start_of_the_sides_turn) {
  ResearchWorld w;
  CHECK(w.begin());
  const int funds = w.funds();
  w.events.clear();
  CHECK(w.act(Action::research(Tools)));  // 100, 1 turn
  CHECK_EQ(w.funds(), funds - 100);
  CHECK_EQ(w.battle.researching(0), (int)Tools);
  CHECK_EQ(w.battle.researchTurnsLeft(0), 1);
  CHECK(!w.battle.hasTechnology(0, Tools));
  CHECK_EQ(w.events.size(), size_t(2));
  CHECK(w.events[0].type == EventType::ResourceChanged);
  CHECK(w.events[1].type == EventType::ResearchStarted);
  CHECK_EQ(w.events[1].a, 0);
  CHECK_EQ(w.events[1].b, (int)Tools);
  CHECK_EQ(w.events[1].c, 1);
  // Red's turn does not count it down; blue's next turn does, after TurnStarted and income.
  w.events.clear();
  CHECK(w.act(Action::endTurn()));
  CHECK_EQ(w.battle.researchTurnsLeft(0), 1);
  CHECK_EQ(w.count(EventType::ResearchFinished), 0);
  CHECK(w.act(Action::endTurn()));
  CHECK(w.battle.hasTechnology(0, Tools));
  CHECK_EQ(w.battle.researching(0), -1);
  CHECK_EQ(w.count(EventType::ResearchFinished), 1);
  const Event* e = w.last(EventType::ResearchFinished);
  CHECK(e && e->a == 0 && e->b == (int)Tools);
  // It comes after the turn began.
  size_t turn = 0, fin = 0;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::TurnStarted && w.events[i].a == 0) turn = i;
    if (w.events[i].type == EventType::ResearchFinished) fin = i;
  }
  CHECK(fin > turn);
}

TEST(research_with_two_turns_takes_two_of_the_sides_turns) {
  ResearchWorld w;
  CHECK(w.begin());
  w.battle.sides[0].researched[Tools] = 1;
  CHECK(w.act(Action::research(Masonry)));  // 2 turns
  CHECK_EQ(w.battle.researchTurnsLeft(0), 2);
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.researchTurnsLeft(0), 1);
  CHECK(!w.battle.hasTechnology(0, Masonry));
  CHECK(w.nextTurn());
  CHECK(w.battle.hasTechnology(0, Masonry));
}

TEST(research_with_zero_turns_finishes_at_once) {
  ResearchWorld w;
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::research(Armor)));
  CHECK(w.battle.hasTechnology(0, Armor));
  CHECK_EQ(w.battle.researching(0), -1);
  CHECK_EQ(w.events.size(), size_t(3));  // paid, started, finished
  CHECK(w.events[1].type == EventType::ResearchStarted);
  CHECK_EQ(w.events[1].c, 0);
  CHECK(w.events[2].type == EventType::ResearchFinished);
  // Another one may start in the same turn.
  CHECK(w.act(Action::research(Tools)));
}

TEST(research_waits_without_a_research_structure_and_keeps_its_progress) {
  ResearchWorld w;
  w.db.rules.researchNeedsStructure = true;
  CHECK(w.begin());
  w.battle.sides[0].researched[Tools] = 1;
  CHECK(w.act(Action::research(Masonry)));  // 2 turns
  int lab = w.structure(0, 0);
  w.battle.structures[(size_t)lab].owner = 1;  // the lab is lost
  CHECK(!w.battle.canResearch(0));
  CHECK(w.nextTurn());
  CHECK(w.nextTurn());
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.researchTurnsLeft(0), 2);  // nothing counted
  CHECK_EQ(w.battle.researching(0), (int)Masonry);
  w.battle.structures[(size_t)lab].owner = 0;  // won back
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.researchTurnsLeft(0), 1);
  CHECK(w.nextTurn());
  CHECK(w.battle.hasTechnology(0, Masonry));
}

TEST(research_refusals_each_have_a_reason) {
  ResearchWorld w;
  w.db.rules.researchNeedsStructure = true;
  CHECK(w.begin());
  CHECK_STR(w.refused(Action::research(Masonry)), "needs Tools");
  CHECK_STR(w.refused(Action::research(99)), "no such technology");
  CHECK_STR(w.refused(Action::research(-1)), "no such technology");
  w.battle.sides[0].resources[0] = 99;
  CHECK_STR(w.refused(Action::research(Tools)), "not enough Funds");
  w.battle.sides[0].resources[0] = 2000;
  CHECK_STR(w.refused(Action::research(Tools)), "");
  // One at a time.
  CHECK(w.act(Action::research(Tools)));
  CHECK_STR(w.refused(Action::research(Frailty)), "already researching Tools");
  // Finished ones are not offered again.
  w.battle.sides[0].researching = -1;
  w.battle.sides[0].researched[Tools] = 1;
  CHECK_STR(w.refused(Action::research(Tools)), "already researched");
  // No research structure.
  w.battle.structures[(size_t)w.structure(0, 0)].owner = -1;
  CHECK_STR(w.refused(Action::research(Frailty)), "needs a structure that can research");
  w.battle.structures[(size_t)w.structure(0, 0)].owner = 0;
  // The battle is over.
  w.battle.over = true;
  CHECK_STR(w.refused(Action::research(Frailty)), "the battle is over");
}

TEST(research_options_say_why_not) {
  ResearchWorld w;
  CHECK(w.begin());
  std::vector<ResearchOption> options;
  w.battle.researchOptions(0, options);
  CHECK_EQ(options.size(), w.db.research.size());
  CHECK(options[Tools].ok);
  CHECK(!options[Masonry].ok);
  CHECK_STR(options[Masonry].why, "needs Tools");
  w.battle.sides[0].resources[0] = 150;
  w.battle.researchOptions(0, options);
  CHECK(options[Tools].ok);  // 100
  CHECK(!options[Armor].ok);  // 300
  CHECK_STR(options[Armor].why, "not enough Funds");
  // The one under way and the finished ones are not listed; the rest says why it cannot start.
  w.battle.sides[0].resources[0] = 2000;
  CHECK(w.act(Action::research(Tools)));
  w.battle.researchOptions(0, options);
  CHECK_EQ(options.size(), w.db.research.size() - 1);
  for (const ResearchOption& o : options) {
    CHECK(o.tech != (int)Tools);
    CHECK(!o.ok);
    CHECK_STR(o.why, "already researching Tools");
  }
  // Another side's list: it is not that side's turn.
  w.battle.researchOptions(1, options);
  CHECK(!options.empty());
  CHECK_STR(options[0].why, "it is not that side's turn");
  w.battle.researchOptions(7, options);
  CHECK(options.empty());
}

TEST(research_one_side_does_not_give_the_other_anything) {
  ResearchWorld w;
  CHECK(w.begin());
  CHECK(w.act(Action::research(Armor)));
  CHECK(w.battle.hasTechnology(0, Armor));
  CHECK(!w.battle.hasTechnology(1, Armor));
  CHECK(!w.battle.hasTechnology(5, Armor));
  CHECK(!w.battle.hasTechnology(0, -1));
  CHECK(!w.battle.hasTechnology(0, 99));
}

TEST(research_unlocks_unit_types_for_building) {
  ResearchWorld w;
  CHECK(w.begin());
  Cell castle = w.at(4, 0);
  // The tank is locked: not offered, refused with the technologies that would do.
  std::vector<int> options;
  w.battle.buildOptions(castle, options);
  CHECK_EQ(options.size(), size_t(1));  // the soldier
  w.battle.buildOptions(castle, options, true);
  CHECK_EQ(options.size(), size_t(2));
  CHECK(!w.battle.unitUnlocked(0, 2));
  CHECK(w.battle.unitUnlocked(0, 0));  // nothing unlocks a soldier
  CHECK_STR(w.refused(Action::build(castle, 2)), "needs Armor or Forge");
  std::vector<int> techs;
  w.battle.unlockedBy(2, techs);
  CHECK_EQ(techs.size(), size_t(2));
  w.battle.unlockedBy(0, techs);
  CHECK(techs.empty());
  std::string why;
  CHECK(!w.battle.canBuild(castle, 2, &why));
  // Any ONE of the technologies is enough: forge, without armor.
  w.battle.sides[0].researched[Forge] = 1;
  CHECK(w.battle.unitUnlocked(0, 2));
  CHECK(w.battle.canBuild(castle, 2));
  w.battle.buildOptions(castle, options);
  CHECK_EQ(options.size(), size_t(2));
  w.events.clear();
  CHECK(w.act(Action::build(castle, 2)));
  CHECK_EQ(w.count(EventType::UnitBuilt), 1);
  // The other side has not unlocked it.
  CHECK(!w.battle.unitUnlocked(1, 2));
}

TEST(research_an_unlock_can_be_researched_and_used_in_the_same_turn) {
  ResearchWorld w;
  CHECK(w.begin());
  Cell castle = w.at(4, 0);
  CHECK(!w.battle.canBuild(castle, 2));
  CHECK(w.act(Action::research(Armor)));  // instant
  CHECK(w.battle.canBuild(castle, 2));
}

TEST(research_bonus_applies_to_new_and_old_units_of_the_side_and_the_type_only) {
  ResearchWorld w;
  CHECK(w.begin());
  Battle& b = w.battle;
  const int defIdx = 2;
  CHECK_EQ(b.stat(b.units[0], defIdx), 2);  // blue soldier
  w.battle.sides[0].researched[Tools] = 1;  // +1 def to blue soldiers
  CHECK_EQ(b.stat(b.units[0], defIdx), 3);  // existing unit
  CHECK_EQ(b.stat(b.units[1], defIdx), 2);  // red soldier: other side
  CHECK_EQ(b.units[0].stats[(size_t)defIdx], 2);  // the unit's own value is untouched
  // A unit built afterwards has it too; a tank (other type) does not.
  CHECK(w.act(Action::research(Armor)));
  Cell castle = w.at(4, 0);
  CHECK(w.act(Action::build(castle, 0)));
  CHECK_EQ(b.stat(b.units.back(), defIdx), 3);
  CHECK_EQ(b.stat(b.units.back(), 1), 5);  // atk: no bonus without forge
  // The tank (other type): tools do not touch it.
  CHECK(w.act(Action::build(w.at(5, 0), 2)));
  CHECK_EQ(b.stat(b.units.back(), defIdx), 5);
}

TEST(research_bonus_without_types_is_for_every_unit_and_reaches_the_formulas) {
  ResearchWorld w;
  CHECK(w.begin());
  Battle& b = w.battle;
  // Put the red soldier next to the blue one and look at the forecast: atk 5 - def 2 = 3.
  b.occupant[(size_t)b.units[1].cell] = -1;
  b.units[1].cell = w.at(1, 2);
  b.occupant[(size_t)w.at(1, 2)] = 1;
  Forecast f;
  CHECK(b.forecast(0, 1, 0, b.units[0].cell, f));
  CHECK_EQ(f.attack.damage, 3);
  b.sides[0].researched[Forge] = 1;  // everybody of blue: +1 atk
  CHECK(b.forecast(0, 1, 0, b.units[0].cell, f));
  CHECK_EQ(f.attack.damage, 4);
  CHECK_EQ(f.counter.damage, 3);  // red's counter is unchanged
  b.sides[0].researched[Tools] = 1;  // blue's soldier is tougher against red's counter: 5 - 3 = 2
  CHECK(b.forecast(0, 1, 0, b.units[0].cell, f));
  CHECK_EQ(f.counter.damage, 2);
}

TEST(research_bonus_to_movement_counts) {
  ResearchWorld w;
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK_EQ(b.movePoints(b.units[0]), 3);
  CHECK(w.act(Action::research(Swiftness)));
  CHECK_EQ(b.movePoints(b.units[0]), 4);
  Reach reach;
  b.reachable(0, reach);
  CHECK(reach.canStop(w.at(4, 2)));  // 4 squares away along the row: only with the bonus
}

TEST(research_that_lowers_maximum_hit_points_clamps_them) {
  ResearchWorld w;
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK_EQ(b.units[0].hp, 10);
  w.events.clear();
  CHECK(w.act(Action::research(Frailty)));  // everybody -3 hp, after one turn
  CHECK_EQ(b.units[0].hp, 10);
  CHECK(w.nextTurn());
  CHECK_EQ(b.maxHp(b.units[0]), 7);
  CHECK_EQ(b.units[0].hp, 7);   // clamped, like an accessory that is taken off
  CHECK_EQ(b.units[1].hp, 10);  // the other side is unaffected
  // New units are built at the lowered maximum.
  CHECK(w.act(Action::build(w.at(4, 0), 0)));
  CHECK_EQ(b.units.back().hp, 7);
}

TEST(research_that_raises_maximum_hit_points_does_not_heal) {
  ResearchWorld w;
  CHECK(w.begin());
  Battle& b = w.battle;
  b.units[0].hp = 4;
  w.battle.sides[0].researched[Vigor] = 1;
  CHECK_EQ(b.maxHp(b.units[0]), 12);
  CHECK_EQ(b.units[0].hp, 4);
  CHECK(w.act(Action::build(w.at(4, 0), 0)));
  CHECK_EQ(b.units.back().hp, 12);  // new units start at the full, raised maximum
}

TEST(research_with_no_data_changes_nothing) {
  // The plain fixture game has no technologies: stat() is the old stat, and nothing is offered.
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  CHECK_EQ(w.battle.stat(w.battle.units[0], 2), 2);
  std::vector<ResearchOption> options;
  w.battle.researchOptions(0, options);
  CHECK(options.empty());
  CHECK(!w.battle.canResearch(0));
  CHECK(w.battle.unitUnlocked(0, 2));
}

TEST(research_is_deterministic) {
  std::string first;
  for (int run = 0; run < 2; ++run) {
    ResearchWorld w;
    CHECK(w.begin(5));
    CHECK(w.act(Action::research(Tools)));
    CHECK(w.act(Action::upgrade(w.structure(2, 0), Foundry)));
    CHECK(w.nextTurn());
    CHECK(w.act(Action::research(Masonry)));
    CHECK(w.nextTurn());
    CHECK(w.nextTurn());
    std::string sig;
    for (const Event& e : w.events) sig += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + ";";
    if (run == 0) first = sig;
    else CHECK_STR(sig, first);
  }
}

// Default rule (docs/ECONOMY.md "Research"): no building needed; the countdown never pauses.
TEST(research_needs_no_structure_by_default) {
  ResearchWorld w;
  CHECK(!w.db.rules.researchNeedsStructure);
  CHECK(w.begin());
  for (size_t s = 0; s < w.battle.structures.size(); ++s)
    if (w.battle.structures[s].owner == 0) w.battle.structures[s].owner = -1;  // blue owns nothing at all
  CHECK(w.battle.canResearch(0));
  CHECK(w.battle.canResearch(1));
  std::vector<ResearchOption> options;
  w.battle.researchOptions(0, options);
  CHECK(!options.empty());
  for (const ResearchOption& o : options)
    CHECK(!has(o.why, "structure"));
  w.battle.sides[0].researched[Tools] = 1;
  CHECK(w.act(Action::research(Masonry)));  // 2 turns, no building anywhere
  CHECK(w.nextTurn());
  CHECK_EQ(w.battle.researchTurnsLeft(0), 1);  // counted
  CHECK(w.nextTurn());
  CHECK(w.battle.hasTechnology(0, Masonry));
}

TEST(research_with_needs_structure_follows_the_structures_flag) {
  ResearchWorld w;
  w.db.rules.researchNeedsStructure = true;
  CHECK(w.begin());
  CHECK(w.battle.canResearch(0));
  w.battle.structures[(size_t)w.structure(0, 0)].owner = -1;
  CHECK(!w.battle.canResearch(0));
  std::vector<ResearchOption> options;
  w.battle.researchOptions(0, options);
  for (const ResearchOption& o : options) {
    CHECK(!o.ok);
    CHECK_STR(o.why, "needs a structure that can research");
  }
}

TEST(a_game_without_technologies_cannot_research_either_way) {
  ResearchWorld w;
  w.db.research.clear();
  CHECK(w.begin());
  CHECK(!w.battle.canResearch(0));
}
