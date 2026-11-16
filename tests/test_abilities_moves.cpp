// Step A1 of docs/SRPG.md "Directions" and "Legality": push, pull, swap and moveBehind on the square grid and on
// both hexagon layouts, in every direction, at the map edge, against walls, units and structures.
#include "abilities_fixture.h"

using namespace ote;
using namespace abtest;

namespace {

// The user (unit 0, blue) stands at `u`, the target (unit 1, red) next to it in direction `d`; extra units follow.
struct Duel {
  Grid g;
  XY u, t;
  AbWorld w;
  int ab = -1;
  Duel(Kind k, XY user, int d, const AbilityDef& def, bool fog = false, const std::vector<UnitSpec>& extra = {},
       const std::vector<std::string>& rows = fogtest::field(9, 9))
      : g(gridOf(k, (int)rows[0].size(), (int)rows.size())), u(user), t(step(g, user, d)),
        w(k, fog, units(user, t, extra), rows) {
    ab = w.give("soldier", def);
    CHECK(w.begin());
  }
  static std::vector<UnitSpec> units(XY user, XY target, std::vector<UnitSpec> extra) {
    std::vector<UnitSpec> out = {{"soldier", 0, user.x, user.y, false}, {"soldier", 1, target.x, target.y, false}};
    out.insert(out.end(), extra.begin(), extra.end());
    return out;
  }
  Cell at(XY p) const { return w.at(p.x, p.y); }
  bool use() { return w.use(0, ab, 1); }
};

XY along(const Grid& g, XY from, int dir, int n) {
  for (int i = 0; i < n; ++i) from = step(g, from, dir);
  return from;
}

int opposite(const Grid& g, int d) { return (d + g.directionCount() / 2) % g.directionCount(); }

AbilityDef moveAbility(const char* id, AbilityEffect e, int amount = 0) {
  return ability(id, AbilityTarget::Enemy, e, amount);
}

}  // namespace

TEST(abilities_push_goes_straight_away_in_every_direction) {
  for (Kind k : kKinds) {
    const Grid g = gridOf(k, 9, 9);
    for (int d = 0; d < g.directionCount(); ++d) {
      Duel s(k, {4, 4}, d, moveAbility("shove", AbilityEffect::Push, 2));
      const XY goal = along(g, {4, 4}, d, 3);  // the user's square, the target's, then two more
      CHECK(s.use());
      CHECK(s.w.stands(1, s.at(goal)));
      CHECK(s.w.stands(0, s.at({4, 4})));
      CHECK(s.w.un(0).done);
      const auto used = s.w.of(EventType::AbilityUsed);
      CHECK_EQ(used.size(), (size_t)1);
      CHECK(used[0].a == 0 && used[0].b == s.ab && used[0].c == 1 && used[0].d == -1 && used[0].flags == 0);
      const auto moved = s.w.of(EventType::UnitRelocated);
      CHECK_EQ(moved.size(), (size_t)1);
      CHECK(moved[0].a == 1 && moved[0].b == s.at(s.t) && moved[0].c == s.at(goal));
      if (s.w.events.size() < 2 || s.w.events[s.w.events.size() - 2].type != EventType::AbilityUsed) {
        // AbilityUsed comes first, then what the effect did.
        int usedAt = -1, movedAt = -1;
        for (size_t i = 0; i < s.w.events.size(); ++i) {
          if (s.w.events[i].type == EventType::AbilityUsed) usedAt = (int)i;
          if (s.w.events[i].type == EventType::UnitRelocated) movedAt = (int)i;
        }
        CHECK(usedAt >= 0 && movedAt > usedAt);
      }
    }
  }
}

TEST(abilities_pull_swap_and_move_behind_use_the_opposite_side) {
  for (Kind k : kKinds) {
    const Grid g = gridOf(k, 9, 9);
    for (int d = 0; d < g.directionCount(); ++d) {
      const XY behind = step(g, {4, 4}, opposite(g, d));
      {
        Duel s(k, {4, 4}, d, moveAbility("yank", AbilityEffect::Pull));
        CHECK(s.use());
        CHECK(s.w.stands(0, s.at(behind)) && s.w.stands(1, s.at({4, 4})));  // the user steps back, the target follows
        const auto moved = s.w.of(EventType::UnitRelocated);
        CHECK_EQ(moved.size(), (size_t)2);  // the target first
        CHECK(moved[0].a == 1 && moved[1].a == 0);
        CHECK(moved[0].b == s.at(s.t) && moved[0].c == s.at({4, 4}));
        CHECK(moved[1].b == s.at({4, 4}) && moved[1].c == s.at(behind));
      }
      {
        Duel s(k, {4, 4}, d, moveAbility("trade", AbilityEffect::Swap));
        CHECK(s.use());
        CHECK(s.w.stands(0, s.at(s.t)) && s.w.stands(1, s.at({4, 4})));
        CHECK_EQ(s.w.of(EventType::UnitRelocated).size(), (size_t)2);
      }
      {
        Duel s(k, {4, 4}, d, moveAbility("flank", AbilityEffect::MoveBehind));
        CHECK(s.use());
        CHECK(s.w.stands(0, s.at({4, 4})) && s.w.stands(1, s.at(behind)));  // only the target moves
        CHECK_EQ(s.w.of(EventType::UnitRelocated).size(), (size_t)1);
      }
    }
  }
}

// Every user square and direction of a small map with one wall, against what the contract says in terms of Grid:
// the squares a unit may take are on the map, not a wall, and (for the first one needed) free of units.
TEST(abilities_legality_at_edges_and_walls_matches_the_contract) {
  std::vector<std::string> rows = fogtest::field(5, 5);
  rows[2][2] = '#';
  int tried = 0, refusedCount = 0;
  for (Kind k : kKinds) {
    const Grid g = gridOf(k, 5, 5);
    for (int ux = 0; ux < 5; ++ux)
      for (int uy = 0; uy < 5; ++uy)
        for (int d = 0; d < g.directionCount(); ++d) {
          const Cell uc = g.at(ux, uy), tc = g.neighbor(uc, d);
          if (tc == kNoCell || rows[(size_t)uy][(size_t)ux] == '#') continue;
          const XY t = {g.x(tc), g.y(tc)};
          if (rows[(size_t)t.y][(size_t)t.x] == '#') continue;
          auto open = [&](Cell c) { return c != kNoCell && rows[(size_t)g.y(c)][(size_t)g.x(c)] != '#'; };
          const Cell first = g.neighbor(tc, d), back = g.neighbor(uc, opposite(g, d));
          struct Row { AbilityEffect e; int amount; bool legal; Cell target, user; };
          // push 3: legal when the first square is open; it ends on the last open square of the line.
          Cell end = tc;
          for (int i = 0; i < 3 && open(g.neighbor(end, d)); ++i) end = g.neighbor(end, d);
          const Row rowsOf[] = {{AbilityEffect::Push, 3, open(first), end, uc},
                                {AbilityEffect::Pull, 0, open(back), uc, back},
                                {AbilityEffect::Swap, 0, true, uc, tc},
                                {AbilityEffect::MoveBehind, 0, open(back), back, uc}};
          for (const Row& r : rowsOf) {
            Duel s(k, {ux, uy}, d, moveAbility("m", r.e, r.amount), false, {}, rows);
            const std::string why = s.w.why(0, s.ab, 1);
            if (r.legal != why.empty()) std::printf("  %s u=%d,%d d=%d effect=%d: %s\n", kindName(k), ux, uy, d, (int)r.e, why.c_str());
            CHECK_EQ(r.legal, why.empty());
            ++tried;
            if (!r.legal) {
              ++refusedCount;
              continue;
            }
            CHECK(s.use());
            CHECK(s.w.un(1).cell == r.target);
            CHECK(s.w.un(0).cell == r.user);
          }
        }
  }
  CHECK(tried > 400 && refusedCount > 40);  // the loops really covered the edges
}

TEST(abilities_push_stops_before_a_unit_or_an_impassable_square) {
  for (Kind k : kKinds) {
    const Grid g = gridOf(k, 9, 9);
    const int d = 0;
    // A third unit two squares behind the target: a push of 3 moves it one square.
    const XY stop = along(g, {4, 4}, d, 3);
    Duel s(k, {4, 4}, d, moveAbility("shove", AbilityEffect::Push, 3), false, {{"soldier", 1, stop.x, stop.y, false}});
    CHECK(s.use());
    CHECK(s.w.stands(1, s.at(along(g, {4, 4}, d, 2))));
    CHECK_EQ(s.w.of(EventType::AbilityUsed)[0].flags, 0);  // a unit everybody sees is the ordinary end of a push

    // A wall two squares behind the target.
    std::vector<std::string> rows = fogtest::field(9, 9);
    const XY wall = along(g, {4, 4}, d, 3);
    rows[(size_t)wall.y][(size_t)wall.x] = '#';
    Duel t(k, {4, 4}, d, moveAbility("shove", AbilityEffect::Push, 3), false, {}, rows);
    CHECK(t.use());
    CHECK(t.w.stands(1, t.at(along(g, {4, 4}, d, 2))));
  }
}

TEST(abilities_structures_count_as_they_do_for_walking) {
  for (Kind k : kKinds) {
    const Grid g = gridOf(k, 9, 9);
    // A block (move cost 0) behind the target refuses the push; a mire (cost 4) does not.
    for (const char* structure : {"block", "mire"}) {
      const XY first = along(g, {4, 4}, 0, 2);
      // The structure goes onto the map before the world begins, so the world is built by hand here.
      AbWorld w(k, false, {{"soldier", 0, 4, 4, false}, {"soldier", 1, along(g, {4, 4}, 0, 1).x, along(g, {4, 4}, 0, 1).y, false}});
      w.place(structure, first.x, first.y);
      const int ab = w.give("soldier", moveAbility("shove", AbilityEffect::Push, 1));
      CHECK(w.begin());
      const bool blocked = std::string(structure) == "block";
      CHECK_EQ(blocked, !w.why(0, ab, 1).empty());
      if (!blocked) {
        CHECK(w.use(0, ab, 1));
        CHECK(w.un(1).cell == w.at(first.x, first.y));
      }
    }
  }
}

TEST(abilities_a_moved_unit_abandons_its_capture_and_fires_reach_events) {
  AbWorld w(Square, false, {{"soldier", 0, 3, 4, false}, {"soldier", 1, 4, 4, false}}, fogtest::field(9, 9), {"eirik", "gorm"});
  w.place("city", 4, 4);
  const int ab = w.give("soldier", moveAbility("shove", AbilityEffect::Push, 1));
  w.db.rules.capture = true;
  CHECK(w.load(R"([{"id":"r","when":{"reach":{"x":5,"y":4},"by":{"unit":"gorm"}},"do":[{"say":"arrived"}]}])") == "");
  CHECK(w.begin());
  w.mut(1).moved = true;
  w.battle.structures[(size_t)w.battle.structureAt[(size_t)w.at(4, 4)]].capturer = 1;
  w.battle.structures[(size_t)w.battle.structureAt[(size_t)w.at(4, 4)]].capture = 5;
  CHECK(w.use(0, ab, 1));
  CHECK(w.stands(1, w.at(5, 4)));
  const StructureState& st = w.structureAt(4, 4);
  CHECK(st.capturer == -1 && st.capture == 20);  // the capture it left starts over
  CHECK(w.fired("r"));
  CHECK(w.un(1).moved);                           // its turn flags are kept
}
