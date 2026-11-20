// "changeSide" with "ready": true (docs/EVENTS.md): a unit recruited in the middle of its new side's turn can act at
// once; without the key it keeps the moved / done state of its old side's last turn.
#include "save_helpers.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;

namespace {

std::vector<std::string> field() { return {".........", ".........", "........."}; }

std::string recruit(const char* extra) {
  return std::string(R"([{"id":"join","when":{"turn":2,"side":"blue"},"do":[{"changeSide":{"unit":"gorm"},"side":"blue")") + extra + "}]}]";
}

// Blue: eirik (0,1). Red: gorm (5,1) and grunt (8,1). Red's turn 1: gorm waits (done). Blue's turn 2 recruits gorm.
// `ap`: an action-point game, where red's gorm also moves and has made its attack before it is recruited.
struct Scene {
  StoryWorld w{field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 5, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm", "grunt"}};
  bool run(const char* extra, bool ap = false) {
    if (ap) {
      w.db.rules.ap.on = true;
      w.db.rules.ap.resource = 0;
    }
    if (!w.load(recruit(extra)).empty() || !w.begin()) return false;
    if (ap)
      for (SideState& s : w.battle.sides) s.resources[0] = 50;
    if (!w.act(Action::endTurn())) return false;  // red's turn
    if (ap) {
      if (!w.act(Action::move(1, w.at(6, 1)))) return false;
      w.battle.units[1].apAttacks = 1;  // as if it had attacked: the attack itself is not what is tested
    }
    return w.act(Action::wait(1)) && w.act(Action::endTurn());  // ... blue's turn 2: the event fires
  }
  const Unit& gorm() const { return w.battle.units[1]; }
};

}  // namespace

TEST(change_side_without_ready_keeps_the_old_turn_state) {
  Scene s;
  CHECK(s.run(""));
  CHECK(s.gorm().side == 0 && s.gorm().done);
  CHECK(!s.w.act(Action::move(1, s.w.at(7, 1))));
  CHECK(!s.w.act(Action::attack(1, 2, 0)));
  Scene f;
  CHECK(f.run(R"(,"ready":false)"));
  CHECK(f.gorm().side == 0 && f.gorm().done);  // "ready": false is the same as absent
}

TEST(change_side_ready_lets_the_recruit_move_and_attack_at_once) {
  Scene s;
  CHECK(s.run(R"(,"ready":true)"));
  CHECK(s.gorm().side == 0 && !s.gorm().moved && !s.gorm().done && !s.gorm().acted);
  CHECK(!s.w.battle.unitSpent(s.gorm()));
  CHECK(s.w.act(Action::move(1, s.w.at(7, 1))));
  CHECK(s.w.act(Action::attack(1, 2, 0)));
  CHECK(s.gorm().done);
}

TEST(change_side_ready_resets_the_action_point_counters) {
  Scene s;
  CHECK(s.run(R"(,"ready":true)", true));
  CHECK(s.gorm().apAttacks == 0 && s.gorm().moveSpent == 0 && !s.gorm().moved);
  CHECK(s.w.battle.apMayMove(s.gorm()) && s.w.battle.apMayAttack(s.gorm()));
  Scene n;
  CHECK(n.run("", true));
  CHECK(n.gorm().apAttacks == 1 && n.gorm().moveSpent > 0);  // without it the red turn's counters stay
}

TEST(change_side_ready_survives_a_save_round_trip) {
  Scene s;
  CHECK(s.run(R"(,"ready":true)"));
  Json saved;
  saveBattleState(s.w.battle, saved);
  Battle r;
  std::string err;
  CHECK(r.init(s.w.db, s.w.map, s.w.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(s.w.battle));
  CHECK(r.units[1].side == 0 && !r.units[1].done && !r.units[1].moved);
  std::vector<Event> ev;
  CHECK(r.apply(Action::move(1, s.w.at(7, 1)), ev));
}

TEST(change_side_loader_refuses_a_ready_that_is_not_true_or_false) {
  Scene s;
  std::string err = s.w.load(recruit(R"(,"ready":"yes")"));
  CHECK(err.find("\"ready\"") != std::string::npos && err.find("true or false") != std::string::npos);
  if (err.empty()) std::printf("  accepted\n");
}
