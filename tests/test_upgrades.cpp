// Upgradable structures in a battle (docs/ECONOMY.md): the UpgradeStructure action, its refusals, the
// menu query, and that every rule that asks "what kind is this structure" follows the change.
#include "research_fixture.h"

using namespace ote;
using namespace restest;

TEST(upgrade_changes_the_kind_and_pays) {
  ResearchWorld w;
  CHECK(w.begin());
  const int funds = w.funds();
  int s = w.structure(2, 0);
  w.battle.structures[(size_t)s].capture = 5;  // as if a capture had been under way
  w.events.clear();
  CHECK(w.act(Action::upgrade(s, Foundry)));
  CHECK_EQ(w.kindAt(2, 0), (int)Foundry);
  CHECK_EQ(w.funds(), funds - 400);
  CHECK_EQ(w.battle.structures[(size_t)s].owner, 0);  // same place, same owner
  CHECK_EQ(w.battle.structures[(size_t)s].capturer, -1);
  CHECK_EQ(w.battle.structures[(size_t)s].capture, w.db.structures[Foundry].capturePoints);  // reset for the new kind
  // Events: the payment, then the upgrade.
  CHECK_EQ(w.events.size(), size_t(2));
  CHECK(w.events[0].type == EventType::ResourceChanged);
  CHECK_EQ(w.events[0].c, -400);
  CHECK(w.events[1].type == EventType::StructureUpgraded);
  CHECK_EQ(w.events[1].a, s);
  CHECK_EQ(w.events[1].b, w.db.findStructure("factory"));
  CHECK_EQ(w.events[1].c, (int)Foundry);
  // The old kind has no way back: foundry has no upgrades of its own.
  CHECK_STR(w.refused(Action::upgrade(s, w.db.findStructure("factory"))), "that structure cannot be upgraded to that");
}

TEST(upgrade_follows_every_rule_that_reads_the_kind) {
  ResearchWorld w;
  CHECK(w.begin());
  // Production: a foundry builds tanks, the factory did not.
  Cell fc = w.at(2, 0);
  std::vector<int> options;
  w.battle.buildOptions(fc, options);
  CHECK_EQ(options.size(), size_t(1));
  CHECK(w.act(Action::upgrade(w.structure(2, 0), Foundry)));
  w.battle.buildOptions(fc, options, true);  // the tank is still locked behind research: only listed with includeLocked
  CHECK_EQ(options.size(), size_t(2));
  w.battle.buildOptions(fc, options);
  CHECK_EQ(options.size(), size_t(1));
  // Capture points and the hq tag.
  int hq = w.structure(3, 0);
  CHECK(w.act(Action::upgrade(hq, Keep)));
  CHECK_EQ(w.battle.defOf(w.battle.structures[(size_t)hq]).capturePoints, 40);
  CHECK(w.battle.defOf(w.battle.structures[(size_t)hq]).tags[0] == "hq");
}

TEST(upgrade_of_a_neutral_structure_is_refused) {
  ResearchWorld w;
  CHECK(w.begin());
  CHECK_STR(w.refused(Action::upgrade(w.structure(0, 1), Road)), "you do not own that structure");
}

TEST(upgrade_changes_movement_cost_and_the_numbers_formulas_see) {
  ResearchWorld w;
  for (MapStructureDef& ms : w.map.structures)
    if (ms.x == 0 && ms.y == 1) ms.side = 0;  // blue owns the mire
  CHECK(w.begin());
  Cell mire = w.at(0, 1);
  CHECK_EQ(w.battle.enterCost(mire, 0), 4);
  Props scratch;
  CHECK(w.battle.numbersAt(mire, scratch).empty());
  CHECK(w.act(Action::upgrade(w.structure(0, 1), Road)));
  CHECK_EQ(w.battle.enterCost(mire, 0), 1);
  const Props& p = w.battle.numbersAt(mire, scratch);
  CHECK_EQ(p.size(), size_t(1));
  CHECK_EQ(p[0].value, 2.0);
}

TEST(upgrade_gives_the_new_kinds_income_next_turn) {
  ResearchWorld w;
  CHECK(w.begin());
  // The city (income 100) needs masonry before it can become a town (income 200): grant it.
  w.battle.sides[0].researched[Masonry] = 1;
  CHECK(w.act(Action::upgrade(w.structure(1, 0), Town)));
  int before = w.funds();
  CHECK(w.nextTurn());
  // castle 100 + town 200 (the city's 100 is gone).
  CHECK_EQ(w.funds() - before, 300);
}

TEST(upgrade_refusals_each_have_a_reason) {
  ResearchWorld w;
  CHECK(w.begin());
  int city = w.structure(1, 0), factory = w.structure(2, 0), redLab = w.structure(3, 2);
  // Needs the technology.
  CHECK_STR(w.refused(Action::upgrade(city, Town)), "needs Masonry");
  // Not an upgrade of that kind.
  CHECK_STR(w.refused(Action::upgrade(factory, Town)), "that structure cannot be upgraded to that");
  // Not the side's structure.
  CHECK_STR(w.refused(Action::upgrade(redLab, Town)), "you do not own that structure");
  // Unknown structure.
  CHECK_STR(w.refused(Action::upgrade(99, Foundry)), "no such structure");
  CHECK_STR(w.refused(Action::upgrade(-1, Foundry)), "no such structure");
  // An enemy unit on any square of it.
  w.battle.units[1].cell = w.at(2, 0);  // the red soldier steps onto the factory
  w.battle.occupant[(size_t)w.at(5, 2)] = -1;
  w.battle.occupant[(size_t)w.at(2, 0)] = 1;
  CHECK_STR(w.refused(Action::upgrade(factory, Foundry)), "an enemy unit stands on it");
  // ... but one of its own side is fine.
  w.battle.units[1].side = 0;
  CHECK_STR(w.refused(Action::upgrade(factory, Foundry)), "");
  // Not enough funds.
  w.battle.sides[0].resources[0] = 399;
  CHECK_STR(w.refused(Action::upgrade(factory, Foundry)), "not enough Funds");
  w.battle.sides[0].resources[0] = 400;
  CHECK_STR(w.refused(Action::upgrade(factory, Foundry)), "");
  // The battle is over.
  w.battle.over = true;
  CHECK_STR(w.refused(Action::upgrade(factory, Foundry)), "the battle is over");
}

TEST(upgrade_is_the_active_sides_only) {
  ResearchWorld w;
  CHECK(w.begin());
  CHECK(w.act(Action::endTurn()));  // red's turn: blue's structures are not red's to upgrade
  CHECK_STR(w.refused(Action::upgrade(w.structure(2, 0), Foundry)), "you do not own that structure");
  std::vector<UpgradeOption> options;
  w.battle.upgradeOptions(w.structure(2, 0), options);
  CHECK(options.empty());
}

TEST(upgrade_options_list_every_upgrade_with_a_reason) {
  ResearchWorld w;
  CHECK(w.begin());
  std::vector<UpgradeOption> options;
  w.battle.upgradeOptions(w.structure(1, 0), options);  // the city
  CHECK_EQ(options.size(), size_t(1));
  CHECK_EQ(options[0].to, (int)Town);
  CHECK(!options[0].ok);
  CHECK_STR(options[0].why, "needs Masonry");
  CHECK_EQ(options[0].def->cost[0].amount, 300);
  w.battle.sides[0].researched[Masonry] = 1;
  w.battle.upgradeOptions(w.structure(1, 0), options);
  CHECK(options[0].ok);
  w.battle.sides[0].resources[0] = 10;
  w.battle.upgradeOptions(w.structure(1, 0), options);
  CHECK(!options[0].ok);
  CHECK_STR(options[0].why, "not enough Funds");
  // A structure with no upgrades, and an index out of range.
  w.battle.upgradeOptions(w.structure(0, 0), options);  // the lab
  CHECK(options.empty());
  w.battle.upgradeOptions(77, options);
  CHECK(options.empty());
}

TEST(upgrade_is_free_of_any_unit_and_leaves_units_alone) {
  ResearchWorld w;
  CHECK(w.begin());
  Unit before = w.battle.units[0];
  CHECK(w.act(Action::upgrade(w.structure(2, 0), Foundry)));
  CHECK(!w.battle.units[0].done && !w.battle.units[0].moved);
  CHECK_EQ(w.battle.units[0].hp, before.hp);
}

TEST(upgrade_copy_and_replay_agree) {
  ResearchWorld a;
  CHECK(a.begin());
  Battle copy = a.battle;
  std::vector<Event> evA, evB;
  CHECK(a.battle.apply(Action::upgrade(a.structure(2, 0), Foundry), evA));
  CHECK(copy.apply(Action::upgrade(a.structure(2, 0), Foundry), evB));
  CHECK_EQ(evA.size(), evB.size());
  CHECK_EQ(a.battle.structures[2].type, copy.structures[2].type);
  CHECK_EQ(a.battle.sides[0].resources[0], copy.sides[0].resources[0]);
}
