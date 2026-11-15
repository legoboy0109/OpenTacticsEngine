// The computer side and research / upgrades (docs/ECONOMY.md "AI"): it researches the cheapest technology
// it can pay while still affording a build, upgrades only with more than twice the cost to spare, never
// builds what is locked, and stays deterministic and finite.
#include "core/ai/ai.h"
#include "research_fixture.h"

using namespace ote;
using namespace restest;

namespace {

// One whole turn of the active side by the AI.
void aiTurn(ResearchWorld& w) {
  int side = w.battle.activeSide, round = w.battle.round;
  for (int i = 0; i < 500 && !w.battle.over && w.battle.activeSide == side && w.battle.round == round; ++i)
    aiTakeStep(w.battle, w.events);
}

std::vector<int> started(const ResearchWorld& w) {
  std::vector<int> out;
  for (const Event& e : w.events)
    if (e.type == EventType::ResearchStarted) out.push_back(e.b);
  return out;
}

int count(const ResearchWorld& w, EventType t) { return w.count(t); }

}  // namespace

TEST(ai_researches_the_cheapest_technology_one_at_a_time) {
  ResearchWorld w;
  CHECK(w.begin());
  w.events.clear();
  aiTurn(w);
  // swiftness (10, instant) first; then the next cheapest, frailty (50, lowest index of the two at 50), which takes a turn;
  // nothing else while that is under way.
  std::vector<int> s = started(w);
  CHECK_EQ(s.size(), size_t(2));
  if (s.size() == 2) {
    CHECK_EQ(s[0], (int)Swiftness);
    CHECK_EQ(s[1], (int)Frailty);
  }
  CHECK(w.battle.hasTechnology(0, Swiftness));
  CHECK_EQ(w.battle.researching(0), (int)Frailty);
}

TEST(ai_does_not_research_what_would_stop_it_building) {
  ResearchWorld w;
  CHECK(w.begin());
  w.battle.sides[0].resources[0] = 130;  // a soldier costs 100
  w.events.clear();
  aiTurn(w);
  std::vector<int> s = started(w);
  CHECK_EQ(s.size(), size_t(1));  // swiftness (10) leaves 120; nothing else leaves 100
  if (!s.empty()) CHECK_EQ(s[0], (int)Swiftness);
  CHECK(count(w, EventType::UnitBuilt) >= 1);
}

TEST(ai_does_not_research_what_it_cannot_pay) {
  ResearchWorld w;
  CHECK(w.begin());
  w.battle.sides[0].resources[0] = 5;
  w.events.clear();
  aiTurn(w);
  CHECK(started(w).empty());
}

TEST(ai_without_a_research_structure_does_not_research) {
  ResearchWorld w;
  w.db.rules.researchNeedsStructure = true;
  w.db.structures[Lab].research = false;  // (a unit could otherwise walk over and capture the lab)
  CHECK(w.begin());
  w.events.clear();
  aiTurn(w);
  CHECK(started(w).empty());
}

TEST(ai_with_nothing_to_build_still_researches) {
  ResearchWorld w;
  w.db.rules.production = false;
  CHECK(w.begin());
  w.battle.sides[0].resources[0] = 60;
  w.events.clear();
  aiTurn(w);
  CHECK(!started(w).empty());
}

TEST(ai_upgrades_only_with_more_than_twice_the_cost_to_spare) {
  for (int funds : {800, 801}) {
    ResearchWorld w;
    w.db.rules.production = false;
    w.db.rules.researchNeedsStructure = true;
    w.db.structures[Lab].research = false;  // no research in the way
    CHECK(w.begin());
    w.battle.sides[0].resources[0] = funds;                     // the factory's upgrade costs 400
    w.events.clear();
    aiTurn(w);
    CHECK_EQ(count(w, EventType::StructureUpgraded), funds == 800 ? 0 : 1);
    if (funds == 801) {
      CHECK_EQ(w.kindAt(2, 0), (int)Foundry);
      CHECK_EQ(w.funds(), 401);
    }
  }
}

TEST(ai_upgrades_come_after_its_builds) {
  ResearchWorld w;
  w.db.structures[Lab].research = false;
  CHECK(w.begin());
  w.events.clear();
  aiTurn(w);
  size_t lastBuild = 0, firstUpgrade = w.events.size();
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::UnitBuilt) lastBuild = i;
    if (w.events[i].type == EventType::StructureUpgraded && firstUpgrade == w.events.size()) firstUpgrade = i;
  }
  CHECK(count(w, EventType::UnitBuilt) > 0);
  CHECK(count(w, EventType::StructureUpgraded) > 0);
  CHECK(firstUpgrade > lastBuild);
}

TEST(ai_never_takes_a_free_upgrade) {
  ResearchWorld w;
  w.db.structures[(size_t)w.db.findStructure("factory")].upgrades[0].cost.clear();
  w.db.rules.production = false;
  w.db.structures[Lab].research = false;
  CHECK(w.begin());
  w.events.clear();
  aiTurn(w);  // must also end: a free upgrade could be repeated forever
  CHECK_EQ(w.kindAt(2, 0), w.db.findStructure("factory"));
}

TEST(ai_never_builds_a_locked_unit_type) {
  ResearchWorld w;
  w.db.structures[Lab].research = false;  // no research, so the tank stays locked
  w.db.unitTypes[2].cost = {{0, 300}};    // and it would be the AI's favourite
  CHECK(w.begin());
  for (int i = 0; i < 6; ++i) {
    aiTurn(w);  // blue
    aiTurn(w);  // red (also the AI here)
  }
  int tanks = 0;
  for (const Unit& u : w.battle.units)
    if (u.type == 2 && u.side == 0) ++tanks;
  CHECK_EQ(tanks, 0);
  CHECK(count(w, EventType::UnitBuilt) > 0);
}

TEST(ai_builds_a_tank_once_it_has_the_technology) {
  ResearchWorld w;
  w.db.structures[Lab].research = false;
  w.db.unitTypes[2].cost = {{0, 300}};  // the dearest useful unit is the one the AI buys
  CHECK(w.begin());
  w.battle.sides[0].researched[Armor] = 1;
  bool tank = false;
  for (int i = 0; i < 4 && !tank; ++i) {
    aiTurn(w);
    aiTurn(w);
    for (const Unit& u : w.battle.units) tank = tank || (u.type == 2 && u.side == 0);
  }
  CHECK(tank);
}

TEST(ai_with_research_and_upgrades_is_deterministic_and_finishes) {
  std::string first;
  for (int run = 0; run < 2; ++run) {
    ResearchWorld w;
    CHECK(w.begin(3));
    for (int turn = 0; turn < 24 && !w.battle.over; ++turn) aiTurn(w);
    std::string sig;
    for (const Event& e : w.events)
      sig += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + ";";
    CHECK(count(w, EventType::ResearchFinished) > 0);
    if (run == 0) first = sig;
    else CHECK_STR(sig, first);
  }
}

TEST(ai_researches_without_any_structure_by_default) {
  ResearchWorld w;
  w.db.structures[Lab].research = false;  // no lab at all; the default rule does not need one
  CHECK(w.begin());
  w.events.clear();
  aiTurn(w);
  CHECK(!started(w).empty());
}
