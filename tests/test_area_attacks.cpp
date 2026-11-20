// Docs/SRPG.md "Area attacks and line fire": line fire and its blockers, what an area weapon may aim at, the order of an area attack's
// strikes, deferred deaths, the counter-attack of the aimed unit only, friendly fire, one use of the weapon and the ammunition, experience,
// the random stream, the enemy-range display and saves. Fixture tests/fixtures/area-fire (see area_fixture.h for its numbers).
#include <algorithm>

#include "area_fixture.h"
#include "core/battle/threat_area.h"
#include "core/grid/area.h"

using namespace ote;
using areatest::plain;
using areatest::Scene;
using areatest::U;

namespace {

int distOf(const Battle& b, Cell a, Cell c) { return b.grid.distance(a, c); }

}  // namespace

// ---- line fire ----

TEST(line_fire_targets_only_the_first_unit_on_a_straight_line) {
  Scene s("lf_basic", plain(10, 5),
          {{"rifleman", "blue", 0, 2, "r"}, {"frail", "red", 4, 2, "e1"}, {"dummy", "red", 6, 2, "e2"}, {"dummy", "red", 3, 3, "e3"}, {"dummy", "red", 2, 4, "e4"}});
  // e1 is the first unit along the row; e2 stands behind it; e3 and e4 are off the straight lines (e4 is on the column? no: x = 2).
  CHECK_STR(s.aimable("r", "rifle"), "4,2");
  Forecast f;
  CHECK(s.b().forecast(s.id("r"), s.id("e1"), s.weapon("rifle"), s.unit("r").cell, f));
  CHECK(!s.b().forecast(s.id("r"), s.id("e2"), s.weapon("rifle"), s.unit("r").cell, f));  // e1 is in the way
  CHECK(!s.b().forecast(s.id("r"), s.id("e3"), s.weapon("rifle"), s.unit("r").cell, f));  // not on a straight line
  std::string why;
  CHECK(!s.w->apply(Action::attack(s.id("r"), s.id("e2"), s.weapon("rifle")), &why));
  CHECK_STR(why, "that attack is not possible");
  // The first unit falls (frail: 5 hp, rifle 9): the one behind it can be shot next.
  CHECK(s.w->apply(Action::attack(s.id("r"), s.id("e1"), s.weapon("rifle"))));
  CHECK(!s.unit("e1").alive);
}

TEST(line_fire_is_stopped_by_a_friend_and_by_a_wall) {
  Scene friendly("lf_friend", plain(10, 5), {{"rifleman", "blue", 0, 2, "r"}, {"lancer", "blue", 2, 2, "ally"}, {"dummy", "red", 4, 2, "e1"}});
  CHECK_STR(friendly.aimable("r", "rifle"), "");  // an ally in the way: nothing beyond it
  std::vector<std::string> rows = plain(10, 5);
  rows[2][2] = '#';
  Scene wall("lf_wall", rows, {{"rifleman", "blue", 0, 2, "r"}, {"dummy", "red", 4, 2, "e1"}, {"dummy", "red", 0, 4, "e2"}});
  CHECK_STR(wall.aimable("r", "rifle"), "0,4");  // the wall blocks the row (it blocks sight); the column is open
  Scene open("lf_open", plain(10, 5), {{"rifleman", "blue", 0, 2, "r"}, {"dummy", "red", 1, 2, "e1"}, {"dummy", "red", 6, 2, "e2"}});
  CHECK_STR(open.aimable("r", "rifle"), "1,2");  // point blank counts; e2 is behind e1
  Scene far("lf_far", plain(10, 5), {{"rifleman", "blue", 0, 2, "r"}, {"dummy", "red", 7, 2, "e1"}});
  CHECK_STR(far.aimable("r", "rifle"), "");  // range 1 to 6
}

TEST(line_fire_a_unit_standing_in_the_attackers_old_square_does_not_block) {
  // Asked from another square (the computer plans moves): the shooter's own square, where it still stands, is not an obstacle.
  Scene s("lf_ignore", plain(10, 5), {{"rifleman", "blue", 1, 2, "r"}, {"dummy", "red", 5, 2, "e1"}});
  std::vector<AttackOption> opts;
  s.b().attackOptions(s.id("r"), s.at(0, 2), opts);  // from (0,2) the row passes over (1,2), where it stands now
  bool found = false;
  for (const AttackOption& o : opts) found = found || o.target == s.id("e1");
  CHECK(found);
}

TEST(line_fire_on_hexagons_has_six_directions) {
  const Grid g(GridType::Hex, 11, 9, HexLayout::Pointy);
  const Cell mid = g.at(5, 4);
  auto step = [&](Cell c, int dir, int n) {
    for (int i = 0; i < n; ++i) c = g.neighbor(c, dir);
    return c;
  };
  static const char* const kIds[6] = {"e0", "e1", "e2", "e3", "e4", "e5"};
  std::vector<U> units{{"rifleman", "blue", g.x(mid), g.y(mid), "r"}};
  for (int d = 0; d < 6; ++d) {
    const Cell c = step(mid, d, 3);
    units.push_back({"dummy", "red", g.x(c), g.y(c), kIds[d]});
  }
  const Cell bent = g.neighbor(g.neighbor(mid, 0), 1);  // two steps away, not on a straight line
  units.push_back({"dummy", "red", g.x(bent), g.y(bent), "bent"});
  Scene s("lf_hex", plain(11, 9), units, false, [&](skilltest::Copy& c) {
    c.swap("game.json", "\"formatVersion\": 1,", "\"formatVersion\": 1, \"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" },");
    c.put("maps/field.json", areatest::mapJson(11, 9, plain(11, 9), units, false, "hex"));
  });
  std::vector<AttackOption> opts;
  s.b().attackOptions(s.id("r"), s.unit("r").cell, opts);
  int hit = 0;
  for (const AttackOption& o : opts) {
    CHECK(o.target != s.id("bent"));
    ++hit;
  }
  CHECK_EQ(hit, 6);  // one target in each of the six directions
}

// ---- what an area weapon may aim at ----

TEST(area_weapon_aims_at_squares_that_would_hit_a_visible_enemy) {
  Scene s("area_aim", plain(10, 5), {{"mortarman", "blue", 0, 2, "m"}, {"dummy", "red", 4, 2, "e1"}, {"dummy", "red", 5, 3, "e2"}});
  // Brute force: every square 2..5 from the mortar with an enemy within 2 of it.
  std::vector<Cell> want;
  for (int c = 0; c < s.b().grid.cellCount(); ++c) {
    const int d = distOf(s.b(), s.unit("m").cell, c);
    if (d < 2 || d > 5) continue;
    for (const char* e : {"e1", "e2"})
      if (distOf(s.b(), c, s.unit(e).cell) <= 2) {
        want.push_back(c);
        break;
      }
  }
  CHECK_STR(s.aimable("m", "mortar"), areatest::cellsText(s.b().grid, want));
  CHECK(s.aimable("m", "mortar").find("2,2") != std::string::npos);  // an empty square is a target when an enemy is in the area
  CHECK(s.aimable("m", "mortar").find("1,2") == std::string::npos);  // inside the minimum range
  CHECK(s.fire("m", "mortar", 3, 2));                                // aimed at empty ground: e1 is within 2 of it
  CHECK(s.unit("e1").hp < 12);
}

TEST(area_weapon_needs_an_enemy_in_the_area) {
  // An ally alone in the area: nothing to aim at, even when the weapon would hit it (friendly fire).
  Scene s("area_noenemy", plain(10, 5), {{"flaker", "blue", 0, 2, "f"}, {"lancer", "blue", 3, 2, "ally"}, {"dummy", "red", 9, 4, "e1"}});
  CHECK_STR(s.aimable("f", "flak"), "");
  std::string why;
  CHECK(!s.fire("f", "flak", 3, 2, &why));
  CHECK_STR(why, "that attack is not possible");
  CHECK_EQ(s.unit("ally").hp, 30);
}

TEST(area_weapon_with_a_straight_line_aims_only_along_the_lines) {
  Scene s("area_beam", plain(10, 5), {{"beamer", "blue", 0, 2, "b"}, {"dummy", "red", 2, 2, "e1"}, {"dummy", "red", 2, 3, "e2"}});
  // Beam: length 3 from the target square on, range 1..3, straight lines only. Aimed at (1,2) it covers 1,2 2,2 3,2 (e1); at (2,2) it covers 2..4,2.
  CHECK_STR(s.aimable("b", "beam"), "1,2 2,2");
  CHECK(!s.fire("b", "beam", 2, 3));  // e2 is not on a line from the beamer
  CHECK(s.fire("b", "beam", 1, 2));
  CHECK(s.unit("e1").hp < 12);
  CHECK_EQ(s.unit("e2").hp, 12);
}

TEST(rail_combines_line_fire_and_a_piercing_beam) {
  Scene s("area_rail", plain(10, 5),
          {{"railer", "blue", 0, 2, "r"}, {"dummy", "red", 3, 2, "e1"}, {"dummy", "red", 5, 2, "e2"}, {"dummy", "red", 7, 2, "e3"}});
  // The first unit stops the shot: e1's square is the last that can be aimed at in that row.
  const std::string aim = s.aimable("r", "rail");
  CHECK(aim.find("3,2") != std::string::npos);
  CHECK(aim.find("5,2") == std::string::npos);
  CHECK(aim.find("2,2") != std::string::npos);  // an empty square before e1: the beam covers 2,2 to 5,2 and pierces e1 and e2
  CHECK(s.fire("r", "rail", 3, 2));              // 3,2 to 6,2: e1 and e2
  CHECK(s.unit("e1").hp < 12 && s.unit("e2").hp < 12);
  CHECK_EQ(s.unit("e3").hp, 12);
}

// ---- resolution ----

TEST(area_attack_strikes_in_unit_id_order_and_applies_deaths_afterwards) {
  Scene s("area_order", plain(10, 5),
          {{"mortarman", "blue", 0, 3, "m"}, {"frail", "red", 4, 3, "a"}, {"tough", "red", 5, 3, "b"}, {"frail", "red", 4, 4, "c"}});
  CHECK(s.fire("m", "mortar", 4, 3));
  // Unit ids: m 0, a 1, b 2, c 3. Strike order is the id order; both deaths come after the last strike, in the same order.
  CHECK_STR(s.story(), "A0>1k A0>2 A0>3k D1 D3");
  CHECK(!s.unit("a").alive && !s.unit("c").alive && s.unit("b").alive);
  CHECK_EQ(s.unit("b").hp, 60 - 8);
  for (const Event& e : s.w->events)
    if (e.type == EventType::Attack) CHECK((e.flags & kArea) != 0);
  CHECK(s.unit("m").done);  // the order is over
}

TEST(area_attack_gives_experience_for_each_enemy_struck) {
  Scene s("area_xp", plain(10, 5),
          {{"mortarman", "blue", 0, 3, "m"}, {"frail", "red", 4, 3, "a"}, {"tough", "red", 5, 3, "b"}, {"frail", "red", 4, 4, "c"}});
  CHECK(s.fire("m", "mortar", 4, 3));
  int sum = 0, n = 0;
  for (const Event& e : s.w->events)
    if (e.type == EventType::XpGained && e.a == s.id("m")) {
      sum += e.b;
      ++n;
    }
  CHECK_EQ(n, 3);
  CHECK_EQ(sum, 60 + 30 + 60);  // two kills (60 each) and one hit (30): the fight's usual rewards, once per victim
}

TEST(area_attack_only_the_aimed_unit_answers) {
  // p (aimed) and q are riflemen: a rifle answers along a clear straight line, at any range up to 6.
  Scene s("area_counter", plain(10, 5), {{"mortarman", "blue", 0, 3, "m"}, {"rifleman", "red", 4, 3, "p"}, {"rifleman", "red", 4, 4, "q"}});
  CHECK(s.fire("m", "mortar", 4, 3));
  CHECK_STR(s.story(), "A0>1 A0>2 A1>0c");  // one counter, from p; q is struck and says nothing
  CHECK_EQ(s.unit("m").hp, 30 - 9);
  // Aimed at q instead: q (4,4) is not on a straight line to the mortarman, so it cannot answer; p is only struck.
  Scene t("area_counter2", plain(10, 5), {{"mortarman", "blue", 0, 3, "m"}, {"rifleman", "red", 4, 3, "p"}, {"rifleman", "red", 4, 4, "q"}});
  CHECK(t.fire("m", "mortar", 4, 4));
  CHECK_STR(t.story(), "A0>1 A0>2");
  // A unit in the way closes the line of the answer too (the blue lancer is struck by the burst's friendly... no: radius 2, friendly fire off).
  Scene u("area_counter3", plain(10, 5),
          {{"mortarman", "blue", 0, 3, "m"}, {"rifleman", "red", 4, 3, "p"}, {"lancer", "red", 2, 3, "guard"}});
  CHECK(u.fire("m", "mortar", 4, 3));
  CHECK(u.story().find('c') == std::string::npos);
}

TEST(area_attack_friendly_fire_hits_allies_and_never_the_attacker) {
  Scene s("area_ff", plain(10, 5),
          {{"flaker", "blue", 0, 3, "f"}, {"lancer", "blue", 3, 4, "ally"}, {"dummy", "red", 3, 3, "e1"}, {"dummy", "red", 4, 3, "e2"}});
  CHECK(s.fire("f", "flak", 3, 3));  // radius 1 around (3,3): e1, e2, and the ally at (3,4)
  CHECK_STR(s.story().substr(0, 3), "A0>");
  CHECK_EQ(s.unit("ally").hp, 30 - 7);
  CHECK(s.unit("e1").hp < 12 && s.unit("e2").hp < 12);
  for (const Event& e : s.w->events)
    if (e.type == EventType::Attack) CHECK(e.b != s.id("f"));
  // The same without friendly fire (a bomb): the ally stands in the area and is left alone.
  Scene t("area_noff", plain(10, 5),
          {{"bomber", "blue", 0, 3, "b"}, {"lancer", "blue", 3, 4, "ally"}, {"dummy", "red", 3, 3, "e1"}, {"dummy", "red", 4, 3, "e2"}});
  CHECK(t.fire("b", "bomb", 3, 3));
  CHECK_EQ(t.unit("ally").hp, 30);
  CHECK_EQ(t.attackEvents(), 2);
}

TEST(area_attack_an_ally_on_the_aimed_square_does_not_answer) {
  Scene s("area_ally_aim", plain(10, 5), {{"flaker", "blue", 0, 3, "f"}, {"lancer", "blue", 1, 3, "ally"}, {"dummy", "red", 2, 3, "e1"}});
  CHECK(s.fire("f", "flak", 1, 3));  // aimed at the ally next to it: e1 is in the area too
  CHECK(s.story().find('c') == std::string::npos);
  CHECK_EQ(s.unit("ally").hp, 30 - 7);
}

TEST(area_attack_uses_one_ammunition_and_one_roll_per_victim_in_order) {
  Scene s("area_rng", plain(10, 5),
          {{"shellman", "blue", 0, 2, "s"}, {"dummy", "red", 4, 2, "a"}, {"dummy", "red", 5, 2, "b"}, {"dummy", "red", 4, 3, "c"}});
  const int weapon = s.weapon("shell");
  CHECK_EQ(s.b().ammo(s.id("s"), weapon), 2);
  Rng ref = s.b().rng;
  std::vector<bool> expectHit;
  for (int i = 0; i < 3; ++i) expectHit.push_back(ref.chance(70));  // hit 70, one roll per victim, in unit-id order
  CHECK(s.fire("s", "shell", 4, 2));
  CHECK_EQ(s.b().ammo(s.id("s"), weapon), 1);  // one attack, one shot of ammunition
  std::vector<bool> gotHit;
  std::vector<int> order;
  for (const Event& e : s.w->events)
    if (e.type == EventType::Attack) {
      gotHit.push_back((e.flags & kHit) != 0);
      order.push_back(e.b);
    }
  CHECK_EQ((int)gotHit.size(), 3);
  CHECK(gotHit == expectHit);
  CHECK(order == (std::vector<int>{s.id("a"), s.id("b"), s.id("c")}));
  CHECK(s.b().rng.state == ref.state);  // nothing else drew a number
}

TEST(area_attack_is_deterministic) {
  const std::vector<U> units{{"shellman", "blue", 0, 2, "s"}, {"dummy", "red", 4, 2, "a"}, {"dummy", "red", 5, 2, "b"}, {"dummy", "red", 4, 3, "c"}};
  Scene one("area_det1", plain(10, 5), units), two("area_det2", plain(10, 5), units);
  CHECK(one.fire("s", "shell", 4, 2));
  CHECK(two.fire("s", "shell", 4, 2));
  CHECK(savetest::sameEvents(one.w->events, two.w->events));
  CHECK_STR(savetest::stateText(one.b()), savetest::stateText(two.b()));
}

TEST(area_attack_forecast_lists_every_victim_with_its_numbers) {
  Scene s("area_fc", plain(10, 5), {{"mortarman", "blue", 0, 3, "m"}, {"rifleman", "red", 4, 3, "p"}, {"dummy", "red", 5, 3, "q"}, {"lancer", "blue", 4, 4, "ally"}});
  AreaForecast f;
  CHECK(s.b().areaForecast(s.id("m"), s.weapon("mortar"), s.unit("m").cell, s.at(4, 3), f));
  CHECK_EQ((int)f.victims.size(), 2);  // radius 2 and no friendly fire: the ally is not a victim
  CHECK(f.primary >= 0 && f.victims[(size_t)f.primary].unit == s.id("p"));
  CHECK_EQ(f.victims[0].hit.damage, 8);
  CHECK(f.fight.counter.can);  // the rifleman answers
  CHECK_EQ((int)f.cells.size(), 12);  // the diamond of 13, cut by the map edge (the bottom row is y = 4)
  // An empty square: no aimed unit and so no counter.
  CHECK(s.b().areaForecast(s.id("m"), s.weapon("mortar"), s.unit("m").cell, s.at(4, 2), f));
  CHECK_EQ(f.primary, -1);
  // A square where nothing is hit is no attack.
  CHECK(!s.b().areaForecast(s.id("m"), s.weapon("mortar"), s.unit("m").cell, s.at(2, 0), f));
  // The move rule is the weapon's own: a unit that has moved can still fire a weapon that allows it.
  CHECK(s.b().isAreaWeapon(s.weapon("mortar")) && !s.b().isAreaWeapon(s.weapon("rifle")));
}

// ---- the enemy-range display ----

TEST(enemy_range_shows_the_real_lines_of_line_fire_and_the_blast_of_area_weapons) {
  std::vector<std::string> rows = plain(10, 5);
  rows[2][3] = '#';
  Scene s("area_threat", rows, {{"rifleman", "blue", 0, 2, "r"}, {"mortarman", "blue", 0, 0, "m"}, {"railer", "blue", 0, 4, "rl"}});
  std::vector<uint8_t> mask;
  const Unit& r = s.unit("r");
  attackArea(s.b(), r, {r.cell}, mask);
  auto on = [&](int x, int y) { return mask[(size_t)s.at(x, y)] != 0; };
  CHECK(on(1, 2) && on(2, 2));       // along the row until the wall
  CHECK(!on(3, 2) && !on(4, 2));     // the wall, and everything behind it
  CHECK(on(0, 0) && on(0, 4));       // the column is open
  CHECK(!on(1, 1) && !on(1, 3));     // diagonals are not lines
  const Unit& m = s.unit("m");
  attackArea(s.b(), m, {m.cell}, mask);
  CHECK(on(7, 0) && !on(8, 0));      // range 5, and the blast reaches 2 beyond it
  CHECK(on(2, 2));                   // the blast around an aimed square comes back toward the shooter
  const Unit& rl = s.unit("rl");
  attackArea(s.b(), rl, {rl.cell}, mask);
  CHECK(on(9, 4));                   // aimed at 6,4 the beam (length 4) covers up to 9,4
}

// ---- saves ----

TEST(area_attack_changes_nothing_a_save_has_to_hold) {
  const std::vector<U> units{{"mortarman", "blue", 0, 3, "m"}, {"frail", "red", 4, 3, "a"}, {"tough", "red", 5, 3, "b"}, {"frail", "red", 4, 4, "c"}};
  Scene s("area_save", plain(10, 5), units);
  CHECK(s.fire("m", "mortar", 4, 3));
  Json j;
  saveBattleState(s.b(), j);
  Scene t("area_save2", plain(10, 5), units);
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  // And the restored battle goes on like the original.
  CHECK(s.w->apply(Action::endTurn()));
  CHECK(t.w->apply(Action::endTurn()));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
}
