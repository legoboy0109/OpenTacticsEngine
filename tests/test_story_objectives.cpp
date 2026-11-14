// Objectives (docs/EVENTS.md "Objectives"): each kind, who they are judged for, how they meet the
// game's defeat rules.
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

std::vector<std::string> field() { return {".........", ".........", "........."}; }

void addGreen(StoryWorld& w, const char* team = "green") {
  MapSideDef g;
  g.id = "green";
  g.name = "Green";
  g.team = team;
  g.ai = true;
  w.map.sides.push_back(g);
}

}  // namespace

TEST(objective_defeated_unit_wins_while_enemies_are_left) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"soldier", 1, 8, 1, false}},
               {"eirik", "gorm", "grunt"});
  CHECK(w.play(R"([{"id":"w","when":{"battle":"won"},"do":[{"say":"Victory"}]}])",
               R"({"text":"Defeat Gorm","win":[{"defeated":{"unit":"gorm"}}]})"));
  CHECK_STR(w.battle.objectiveText(), "Defeat Gorm");
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(w.battle.over && w.battle.winnerTeam == 0);
  CHECK(w.battle.units[2].alive);                          // the rest of the enemy did not matter
  CHECK(w.fired("w"));
  const Event* e = w.last(EventType::BattleEnded);
  CHECK(e && e->a == 0);
}

TEST(objective_defeated_side_or_team_means_all_of_its_units) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"soldier", 2, 8, 1, false},
                         {"soldier", 1, 0, 2, false}},
               {"eirik", "gorm", "grunt", "red2"});
  addGreen(w);
  CHECK(w.play("", R"({"win":[{"defeated":{"side":"red"}}]})"));
  w.battle.units[1].hp = 1;
  w.battle.units[3].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(!w.battle.over);                                   // red2 still stands
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()) && w.act(Action::endTurn()));  // red, green, blue again
  CHECK(w.act(Action::attack(0, 3, 0)));
  CHECK(w.battle.over && w.battle.winnerTeam == 0);        // green lives, yet the objective was met

  StoryWorld t(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"soldier", 2, 8, 1, false}},
               {"eirik", "gorm", "grunt"});
  addGreen(t, "red");                                      // green is in red's team
  CHECK(t.play("", R"({"win":[{"defeated":{"team":"red"}}]})"));
  t.battle.units[1].hp = 1;
  CHECK(t.act(Action::attack(0, 1, 0)));
  CHECK(!t.battle.over);                                   // green still belongs to the team
}

TEST(objective_reach_wins_for_the_human_team_only_unless_by_says_otherwise) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 6, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play("", R"({"win":[{"reach":{"x":8,"y":0,"w":1,"h":3}}]})"));
  w.battle.units[0].stats[3] = 9;
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(1, w.at(8, 0))));               // the enemy gets there: nothing happens
  CHECK(!w.battle.over);
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(0, w.at(8, 2))));
  CHECK(w.battle.over && w.battle.winnerTeam == 0);

  StoryWorld by(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 5, 1, false}}, {"eirik", "gorm"});
  CHECK(by.play("", R"({"win":[{"reach":{"x":6,"y":1},"by":{"unit":"gorm"}}]})"));
  CHECK(by.act(Action::endTurn()));
  CHECK(by.act(Action::move(1, by.at(6, 1))));
  CHECK(by.battle.over && by.battle.winnerTeam == 0);      // "by" names who counts
}

TEST(objective_reach_loses_when_the_enemy_gets_there) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play("", R"({"lose":[{"reach":{"x":0,"y":0,"w":2,"h":3}}]})"));
  CHECK(!w.battle.over);                                   // eirik stands inside it from the start: his own team does not lose
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::move(1, w.at(1, 1))));
  CHECK(w.battle.over && w.battle.winnerTeam == 1);
}

TEST(objective_round_loses_and_survive_wins) {
  StoryWorld lose(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(lose.play("", R"({"lose":[{"round":3}]})"));
  for (int i = 0; i < 3; ++i) {
    CHECK(lose.act(Action::endTurn()));
    CHECK(!lose.battle.over);
  }
  CHECK(lose.act(Action::endTurn()));                      // round 3 begins
  CHECK(lose.battle.over && lose.battle.winnerTeam == 1 && lose.battle.round == 3);

  StoryWorld win(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(win.play("", R"({"win":[{"survive":3}]})"));
  for (int i = 0; i < 4; ++i) CHECK(win.act(Action::endTurn()));
  CHECK(win.battle.over && win.battle.winnerTeam == 0);
}

TEST(objective_losing_wins_over_winning_at_the_same_moment) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play("", R"({"win":[{"round":1}],"lose":[{"round":1}]})"));
  CHECK(w.battle.over && w.battle.winnerTeam == 1);
}

TEST(objective_missing_means_the_defeat_rules_alone) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}, {"soldier", 1, 8, 1, false}},
               {"eirik", "gorm", "grunt"});
  CHECK(w.play(R"([{"id":"e","when":{"battle":"start"},"do":[]}])"));
  CHECK_STR(w.battle.objectiveText(), "");
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(!w.battle.over);                                   // grunt is left: the battle goes on as it always did
  StoryWorld textOnly(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(textOnly.play("", R"({"text":"Just survive"})"));
  CHECK(!textOnly.battle.over);
  CHECK_STR(textOnly.battle.objectiveText(), "Just survive");
}

TEST(objective_the_game_defeat_rules_still_apply_to_everyone) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play("", R"({"win":[{"round":99}]})"));
  w.battle.units[0].hp = 1;
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::attack(1, 0, 0)));                   // blue's only unit falls: blue is out, as always
  CHECK(w.battle.over && w.battle.winnerTeam == 1);
}

TEST(objective_judged_for_the_first_human_side) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play("", ""));
  w.map.sides[0].ai = true;                                // blue is the computer's now, red is human
  w.map.sides[1].ai = false;
  Battle b;
  std::string err;
  CHECK(b.init(w.db, w.map, w.formulas, 1, err));
  CHECK_EQ(b.humanTeam(), 1);
  w.map.sides[1].ai = true;                                // nobody human: the first side's team
  CHECK(b.init(w.db, w.map, w.formulas, 1, err));
  CHECK_EQ(b.humanTeam(), 0);
}
