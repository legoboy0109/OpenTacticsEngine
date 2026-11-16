// Step A1 of docs/SRPG.md "Legality": Fog. What a menu offers is judged on what the team sees (a square holding an
// unseen enemy counts as free); when the ability is carried out and the square is in truth taken, it is blocked: the
// cost, the use and the cooldown are spent, AbilityUsed carries kBlocked and names the unit in the way.
#include "abilities_fixture.h"

using namespace ote;
using namespace abtest;

namespace {

// A blind blue user (0) at (4,4), a red target (1) next to it in direction 0, a red unit (2) hiding on `hidden`, and
// a blue observer (3) that sees the target but not the hidden square (soldier vision is 2).
struct Ambush {
  Grid g;
  XY hidden{0, 0}, observer{0, 0};
  AbWorld* w = nullptr;
  int ab = -1;
  Ambush(Kind k, bool fog, AbilityEffect e, bool behindUser, AbilityTarget target = AbilityTarget::Enemy)
      : g(gridOf(k, 9, 9)) {
    const XY u = {4, 4}, t = step(g, u, 0);
    hidden = behindUser ? step(g, u, g.directionCount() / 2) : step(g, t, 0);
    const Cell tc = g.at(t.x, t.y), hc = g.at(hidden.x, hidden.y);
    for (Cell c = 0; c < g.cellCount(); ++c) {
      if (c == g.at(u.x, u.y) || c == tc || c == hc || g.distance(c, tc) > 2 || g.distance(c, hc) < 3) continue;
      observer = {g.x(c), g.y(c)};
      break;
    }
    w = new AbWorld(k, fog, {{"blind", 0, u.x, u.y, false}, {"soldier", 1, t.x, t.y, false}, {"soldier", 1, hidden.x, hidden.y, false},
                             {"soldier", 0, observer.x, observer.y, false}});
    ab = w->give("blind", ability("m", target, e, 1));
    CHECK(w->begin());
  }
  ~Ambush() { delete w; }
  Ambush(const Ambush&) = delete;
  Ambush& operator=(const Ambush&) = delete;
};

}  // namespace

TEST(abilities_fog_unseen_enemy_blocks_a_push_but_the_cost_is_spent) {
  for (Kind k : kKinds) {
    Ambush a(k, true, AbilityEffect::Push, false);
    AbWorld& w = *a.w;
    CHECK(w.seesUnit(0, 1) && !w.seesUnit(0, 2));  // the setup: the target is seen, the hiding unit is not
    CHECK_STR(w.why(0, a.ab, 1), "");              // the menu believes the square behind it is free
    std::vector<int> us;
    std::vector<Cell> cs;
    w.battle.abilityTargets(0, a.ab, w.cellOf(0), us, cs);
    CHECK(us.size() == 1 && us[0] == 1);
    const Cell before = w.cellOf(1);
    CHECK(w.use(0, a.ab, 1));
    CHECK(w.un(1).cell == before);  // it stopped in front of the unit: zero squares
    CHECK(w.un(0).done);
    const auto used = w.of(EventType::AbilityUsed);
    CHECK(used.size() == 1 && used[0].flags == kBlocked && used[0].data.size() == 1 && used[0].data[0] == 2);
    CHECK_EQ(w.of(EventType::UnitRelocated).size(), (size_t)0);
  }
}

TEST(abilities_fog_without_fog_the_same_square_is_refused_up_front) {
  for (Kind k : kKinds) {
    Ambush a(k, false, AbilityEffect::Push, false);
    CHECK_STR(a.w->why(0, a.ab, 1), "there is no room to push it");
  }
}

TEST(abilities_fog_pull_and_move_behind_do_nothing_when_blocked) {
  for (Kind k : kKinds)
    for (AbilityEffect e : {AbilityEffect::Pull, AbilityEffect::MoveBehind}) {
      Ambush a(k, true, e, true);
      AbWorld& w = *a.w;
      CHECK(!w.seesUnit(0, 2));
      CHECK_STR(w.why(0, a.ab, 1), "");
      const Cell user = w.cellOf(0), target = w.cellOf(1);
      CHECK(w.use(0, a.ab, 1));
      CHECK(w.un(0).cell == user && w.un(1).cell == target);
      const auto used = w.of(EventType::AbilityUsed);
      CHECK(used.size() == 1 && used[0].flags == kBlocked && used[0].data[0] == 2);
      CHECK(w.un(0).done);
    }
}

TEST(abilities_fog_a_summon_square_with_an_unseen_enemy_is_offered_and_blocked) {
  for (Kind k : kKinds) {
    Ambush a(k, true, AbilityEffect::Summon, false, AbilityTarget::Cell);
    AbWorld& w = *a.w;
    AbilityDef& d = w.db.abilities[(size_t)a.ab];  // turn the placeholder into a summon of a soldier, range 1..2
    d.summon.type = w.db.findUnitType("soldier");
    d.minRange = 1;
    d.maxRange = 2;
    const Cell hidden = w.at(a.hidden.x, a.hidden.y);  // two squares from the user in direction 0
    CHECK(w.battle.grid.distance(w.cellOf(0), hidden) == 2);
    CHECK(!w.seesUnit(0, 2));
    CHECK_STR(w.why(0, a.ab, -1, hidden), "");
    std::vector<int> us;
    std::vector<Cell> cs;
    w.battle.abilityTargets(0, a.ab, w.cellOf(0), us, cs);
    bool offered = false;
    for (Cell c : cs) offered = offered || c == hidden;
    CHECK(offered);
    const size_t unitsBefore = w.battle.units.size();
    CHECK(w.use(0, a.ab, -1, hidden));
    CHECK_EQ(w.battle.units.size(), unitsBefore);  // nobody appeared
    const auto used = w.of(EventType::AbilityUsed);
    CHECK(used.size() == 1 && used[0].flags == kBlocked && used[0].data[0] == 2 && used[0].d == hidden);
    CHECK(w.un(0).done);
    CHECK(w.battle.abilityUseOf(w.un(0), a.ab) && w.battle.abilityUseOf(w.un(0), a.ab)->used == 1);
  }
}

TEST(abilities_fog_a_target_the_team_cannot_see_is_refused) {
  for (Kind k : kKinds) {
    Ambush a(k, true, AbilityEffect::Push, false);
    CHECK_STR(a.w->why(0, a.ab, 2), "you cannot see that unit");  // the hiding unit is not a target
  }
}
