// Step A1 of docs/SRPG.md "Summoning": Defeat and objectives. Summoned units do not keep a side in the battle; filters by
// side, team or "any" ignore them for defeated / dead / alive, filters by type or tag see them.
#include "abilities_summons_fixture.h"

using namespace ote;
using namespace abtest;

namespace {

// Red's soldier (a human-less side: the tests act for it) summons a hound that stays when it falls (vanish: false),
// then blue's lord defeats the soldier. The hound is red's only unit left.
void soldierFalls(Camp& c) {
  CHECK(c.w.act(Action::endTurn()));
  CHECK(c.w.use(1, c.call, -1, c.w.at(6, 2)));
  CHECK(c.w.act(Action::endTurn()));
  c.w.mut(1).hp = 1;
  c.w.battle.occupant[(size_t)c.w.cellOf(0)] = -1;
  c.w.mut(0).cell = c.w.at(7, 3);
  c.w.battle.occupant[(size_t)c.w.at(7, 3)] = 0;
  CHECK(c.w.act(Action::attack(0, 1, 0)));
}

void staying(AbilityDef& d) { d.summon.vanish = false; }

}  // namespace

TEST(summons_do_not_keep_a_side_in_the_battle) {
  Camp c;
  c.setup(staying);
  soldierFalls(c);
  const int h = c.summonOf(1);
  CHECK(!c.w.un(1).alive);
  CHECK(c.w.battle.sides[1].defeated);  // noUnits counts only units that were not summoned
  CHECK(!c.w.un(h).alive);              // when a side falls its summons leave with it
  CHECK(c.w.battle.over && c.w.battle.winnerTeam == 0);
  CHECK_EQ(c.w.count(EventType::SideDefeated), 1);
  CHECK(c.w.battle.occupant[(size_t)c.w.at(6, 2)] == -1);
}

TEST(summons_a_summon_alone_is_not_enough_but_an_ordinary_unit_is) {
  // Red has a second ordinary soldier: with it, the side lives on beside the hound.
  Camp c;
  c.w.map.units.push_back(c.w.map.units[1]);
  c.w.map.units.back().x = 8;
  c.setup(staying);
  soldierFalls(c);
  CHECK(!c.w.battle.sides[1].defeated && !c.w.battle.over);
  CHECK(c.w.un(c.summonOf(1)).alive);
}

TEST(summons_objectives_by_side_or_team_ignore_them_by_type_see_them) {
  struct Case {
    const char* filter;
    bool wonWhenSoldierFell;
  };
  const Case cases[] = {{R"({"side":"red"})", true}, {R"({"team":"red"})", true}, {R"({"type":"hound"})", false}};
  for (const Case& k : cases) {
    Camp c;
    c.w.db.rules.defeatNoUnits = false;  // only the objective decides
    CHECK(c.w.load("", std::string(R"({"win":[{"defeated":)") + k.filter + "}]}").empty());
    c.setup(staying);
    soldierFalls(c);
    CHECK_EQ(c.w.battle.over, k.wonWhenSoldierFell);
    CHECK(c.w.un(c.summonOf(1)).alive);
    if (k.wonWhenSoldierFell) CHECK(c.w.battle.winnerTeam == 0);
  }
  // By type, the battle is won once the hound itself is defeated.
  Camp c;
  c.w.db.rules.defeatNoUnits = false;
  CHECK(c.w.load("", R"({"win":[{"defeated":{"type":"hound"}}]})").empty());
  c.setup(staying);
  soldierFalls(c);
  const int h = c.summonOf(1);
  c.w.mut(h).hp = 1;
  c.w.mut(0).done = false;
  c.w.mut(0).moved = false;
  c.w.battle.occupant[(size_t)c.w.cellOf(0)] = -1;
  c.w.mut(0).cell = c.w.at(5, 2);
  c.w.battle.occupant[(size_t)c.w.at(5, 2)] = 0;
  CHECK(c.w.act(Action::attack(0, h, 0)));
  CHECK(c.w.battle.over && c.w.battle.winnerTeam == 0);
}

TEST(summons_story_conditions_by_side_ignore_them_by_type_see_them) {
  Camp c;
  c.w.db.rules.defeatNoUnits = false;
  CHECK(c.w.load(R"([
    {"id":"deadRed","when":{"turn":3,"side":"blue"},"if":[{"dead":{"side":"red"}}],"do":[]},
    {"id":"aliveRed","when":{"turn":3,"side":"blue"},"if":[{"alive":{"side":"red"}}],"do":[]},
    {"id":"aliveHound","when":{"turn":3,"side":"blue"},"if":[{"alive":{"type":"hound"}}],"do":[]},
    {"id":"deadHound","when":{"turn":3,"side":"blue"},"if":[{"dead":{"type":"hound"}}],"do":[]}])")
              .empty());
  c.setup(staying);
  soldierFalls(c);
  CHECK(c.w.act(Action::endTurn()));  // red's turn, then blue's third: the events look at the state then
  CHECK(c.w.act(Action::endTurn()));
  CHECK(c.w.fired("deadRed"));      // red's soldier is gone; the hound is not counted for "side"
  CHECK(!c.w.fired("aliveRed"));
  CHECK(c.w.fired("aliveHound"));   // by type it is seen
  CHECK(!c.w.fired("deadHound"));
}
