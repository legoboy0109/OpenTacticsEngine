// The player's enemy-range display (docs/SRPG.md "Enemy ranges"): the area of a marked enemy against the engine's own move and attack
// functions, fog (nothing may betray a unit the viewer cannot see), the cache (one computation per board change), the options and the
// game rule, and the time it takes on a big map.
#include <chrono>
#include <cstring>

#include "battle/enemy_ranges.h"
#include "core/battle/threat_area.h"
#include "fog_fixture.h"
#include "options/effective.h"
#include "options/option_defs.h"
#include "srpg_staves_fixture.h"

using namespace ote;

namespace {

using fixture::UnitSpec;

// An open field with a few obstacles: a wall, forest (cost 2). Blue is parked in the corner; red has a soldier, an archer and a tank.
std::vector<std::string> openField() {
  return {"..........",
          "..#.......",
          "..#..f....",
          ".....f..#.",
          "..ff......",
          "......#...",
          ".........."};
}

// The mask of `unit` as the display draws it (attack squares only: the area includes the move squares).
const ThreatArea& areaOf(const Battle& b, int unit, int team, ThreatArea& out) {
  Reach scratch;
  threatArea(b, unit, team, scratch, out);
  return out;
}

}  // namespace

TEST(enemy_ranges_match_the_engines_own_move_and_attack_functions) {
  fixture::World w(openField(), {{"soldier", 0, 0, 0, true}, {"soldier", 1, 6, 3, false}, {"archer", 1, 7, 1, false}, {"tank", 1, 8, 5, false}});
  w.db.weapons[2].afterMove = false;  // the tank's cannon: only from where it stands (artillery)
  CHECK(w.begin());
  const Battle& b = w.battle;
  CHECK_EQ((int)b.units.size(), 4);
  for (int e = 1; e <= 3; ++e) {
    ThreatArea area;
    areaOf(b, e, 0, area);
    // Movement: exactly the engine's stops for this unit.
    Reach r;
    b.reachable(e, r);
    std::vector<uint8_t> want((size_t)b.grid.cellCount(), 0);
    for (Cell s : r.stops) want[(size_t)s] = 1;
    CHECK(area.move == want);
    int differs = 0, hits = 0;
    for (Cell s = 0; s < (Cell)b.grid.cellCount(); ++s) {
      if (b.enterCost(s, 0) <= 0 || b.unitAt(s) >= 0) continue;  // nobody can stand there to be attacked
      // The oracle: a blue unit standing on `s`; can the enemy attack it from any square it can stop on?
      Battle t = b;
      t.occupant[(size_t)b.units[0].cell] = -1;
      t.units[0].cell = s;
      t.occupant[(size_t)s] = 0;
      bool attackable = false;
      std::vector<AttackOption> opts;
      for (Cell stop : r.stops) {
        if (stop == s) continue;
        t.attackOptions(e, stop, opts);
        for (const AttackOption& o : opts)
          if (o.target == 0) attackable = true;
      }
      if (attackable) ++hits;
      if (attackable != (area.attack[(size_t)s] != 0)) ++differs;
    }
    CHECK_EQ(differs, 0);
    CHECK(hits > 0);
  }
}

TEST(enemy_ranges_artillery_hits_only_from_its_own_square) {
  fixture::World w(openField(), {{"soldier", 0, 0, 0, true}, {"tank", 1, 8, 5, false}});
  w.db.weapons[2].afterMove = false;
  CHECK(w.begin());
  ThreatArea area;
  areaOf(w.battle, 1, 0, area);
  CHECK(area.moveCount > 1);
  // Cannon reach 1: only the four squares around where the tank stands, not around its other squares.
  int around = 0;
  for (size_t c = 0; c < area.attack.size(); ++c)
    if (area.attack[c] && w.battle.grid.distance(w.battle.units[1].cell, (Cell)c) > 1) ++around;
  CHECK_EQ(around, 0);
}

TEST(enemy_ranges_marks_toggle_combine_and_count) {
  fixture::World w(openField(), {{"soldier", 0, 0, 0, true}, {"soldier", 1, 6, 3, false}, {"archer", 1, 7, 1, false}});
  CHECK(w.begin());
  EnemyRanges er;
  bool now = false;
  er.update(w.battle, 0);
  CHECK(er.moveMask() == nullptr);
  CHECK(!er.toggleMark(w.battle, 0, 0, now));  // your own unit cannot be marked
  CHECK(er.toggleMark(w.battle, 0, 1, now) && now);
  er.update(w.battle, 0);
  CHECK_EQ(er.unitCount(), 1);
  CHECK(er.moveMask() != nullptr);
  const int one = er.moveSquares() + er.attackSquares();
  CHECK(er.toggleMark(w.battle, 0, 2, now) && now);
  er.update(w.battle, 0);
  CHECK_EQ(er.unitCount(), 2);
  CHECK(er.moveSquares() + er.attackSquares() >= one);  // the areas combine
  CHECK(er.toggleMark(w.battle, 0, 1, now) && !now);    // Confirm on a marked enemy unmarks it
  er.update(w.battle, 0);
  CHECK_EQ(er.unitCount(), 1);
  // "All" is the union of everything in sight.
  er.toggleAll();
  er.update(w.battle, 0);
  CHECK_EQ(er.unitCount(), 2);
  er.toggleAll();
  er.update(w.battle, 0);
  CHECK_EQ(er.unitCount(), 1);  // the mark made apart stays
  er.clearMarks();
  er.update(w.battle, 0);
  CHECK(er.moveMask() == nullptr);
}

TEST(enemy_ranges_attack_only_style_draws_one_tint_and_off_refuses_marks) {
  fixture::World w(openField(), {{"soldier", 0, 0, 0, true}, {"soldier", 1, 6, 3, false}});
  CHECK(w.begin());
  EnemyRanges er;
  bool now = false;
  er.setStyle(1);
  CHECK(er.toggleMark(w.battle, 0, 1, now));
  er.update(w.battle, 0);
  const int move1 = er.moveSquares(), attack1 = er.attackSquares();
  CHECK(move1 > 0 && attack1 > 0);
  er.setStyle(2);
  er.update(w.battle, 0);
  CHECK_EQ(er.moveSquares(), 0);
  CHECK_EQ(er.attackSquares(), move1 + attack1);
  er.setStyle(0);  // off: the marks go and nothing can be marked
  er.update(w.battle, 0);
  CHECK(er.moveMask() == nullptr);
  CHECK(!er.toggleMark(w.battle, 0, 1, now));
}

TEST(enemy_ranges_cache_recomputes_once_per_board_change) {
  fixture::World w(openField(), {{"soldier", 0, 0, 0, true}, {"soldier", 1, 6, 3, false}});
  CHECK(w.begin());
  EnemyRanges er;
  bool now = false;
  CHECK(er.toggleMark(w.battle, 0, 1, now));
  er.update(w.battle, 0);
  const int built = er.rebuilds();
  for (int i = 0; i < 100; ++i) er.update(w.battle, 0);  // frames without a change
  CHECK_EQ(er.rebuilds(), built);
  const uint64_t before = w.battle.stamp();
  CHECK(w.act(Action::move(0, w.at(1, 0))));  // the board changed
  CHECK(w.battle.stamp() != before);
  er.update(w.battle, 0);
  CHECK_EQ(er.rebuilds(), built + 1);
  // A restored copy (an undo) has the stamp it had.
  Battle copy = w.battle;
  CHECK_EQ(copy.stamp(), w.battle.stamp());
  er.update(copy, 0);
  CHECK_EQ(er.rebuilds(), built + 1);
}

TEST(enemy_ranges_follow_the_board_a_blocker_moving_or_a_unit_dying) {
  fixture::World w(openField(), {{"soldier", 0, 0, 0, true}, {"soldier", 0, 5, 3, false}, {"soldier", 1, 7, 3, false}, {"archer", 1, 9, 6, false}});
  CHECK(w.begin());
  EnemyRanges er;
  bool now = false;
  CHECK(er.toggleMark(w.battle, 0, 2, now));
  er.update(w.battle, 0);
  const std::vector<uint8_t> blocked = *er.moveMask();
  // The blue soldier next to it walks away: the enemy's reach grows.
  CHECK(w.act(Action::move(1, w.at(5, 6))));
  er.update(w.battle, 0);
  CHECK(*er.moveMask() != blocked);
  // A marked enemy that dies is dropped for good.
  w.battle.units[2].alive = false;
  w.battle.occupant[(size_t)w.battle.units[2].cell] = -1;
  w.battle.refreshSight();
  er.update(w.battle, 0);
  CHECK(!er.marked(2));
  CHECK_EQ(er.unitCount(), 0);
  CHECK(er.moveMask() == nullptr);
}

TEST(enemy_ranges_use_a_fresh_turn_not_the_flags_of_the_last_one) {
  fixture::World w(openField(), {{"soldier", 0, 0, 0, true}, {"soldier", 1, 6, 3, false}});
  CHECK(w.begin());
  ThreatArea fresh;
  areaOf(w.battle, 1, 0, fresh);
  // The enemy's flags of its last turn: it has acted and spent most of its movement ("move again").
  w.battle.units[1].acted = true;
  w.battle.units[1].moveSpent = 3;
  w.battle.units[1].moved = true;
  ThreatArea after;
  areaOf(w.battle, 1, 0, after);
  CHECK(fresh.move == after.move && fresh.attack == after.attack);
}

// ---- fog of war: nothing may betray a unit the viewer cannot see

TEST(enemy_ranges_a_hidden_enemy_cannot_be_marked_and_is_not_in_all) {
  fogtest::FogWorld w(fogtest::field(12, 5), {{"soldier", 0, 1, 2, false}, {"soldier", 1, 3, 2, false}, {"soldier", 1, 11, 4, false}});
  CHECK(w.begin());
  CHECK(w.seesUnit(0, 1));
  CHECK(!w.seesUnit(0, 2));
  EnemyRanges er;
  bool now = false;
  CHECK(er.toggleMark(w.battle, 0, 1, now));
  CHECK(!er.toggleMark(w.battle, 0, 2, now));  // it exists, but the player cannot know
  er.toggleAll();
  er.update(w.battle, 0);
  CHECK_EQ(er.unitCount(), 1);
  CHECK(!(*er.moveMask())[(size_t)w.at(11, 4)]);
  CHECK(!(*er.attackMask())[(size_t)w.at(11, 3)]);
  // A marked unit that becomes hidden drops out of the display (and comes back when it is seen again).
  CHECK(w.act(Action::move(0, w.at(0, 2))));
  CHECK(!w.seesUnit(0, 1));
  er.update(w.battle, 0);
  CHECK_EQ(er.unitCount(), 0);
  CHECK(er.moveMask() == nullptr);
}

TEST(enemy_ranges_a_hidden_unit_leaves_no_trace_in_the_picture_of_another) {
  // A visible red soldier has a hidden red soldier standing in its way. Without the archer the picture must be the same.
  auto picture = [](bool withHidden) {
    std::vector<fixture::UnitSpec> units = {{"soldier", 0, 1, 2, false}, {"soldier", 1, 3, 2, false}};
    if (withHidden) units.push_back({"soldier", 1, 4, 2, false});  // three squares from blue: unseen
    fogtest::FogWorld w(fogtest::field(12, 5), units);
    CHECK(w.begin());
    CHECK(!withHidden || !w.seesUnit(0, 2));
    EnemyRanges er;
    bool now = false;
    CHECK(er.toggleMark(w.battle, 0, 1, now));
    er.toggleAll();
    er.update(w.battle, 0);
    return std::make_pair(*er.moveMask(), *er.attackMask());
  };
  const auto a = picture(true), b = picture(false);
  CHECK(a.first == b.first);
  CHECK(a.second == b.second);
}

TEST(enemy_ranges_a_door_opened_out_of_sight_does_not_widen_the_picture) {
  // A "block" (cannot be entered) stands in front of the red soldier. Out of blue's sight it becomes a "gate" (cost 1). Blue last saw
  // the block, so the picture keeps it; the true reach (what the rules use) is wider.
  fogtest::FogWorld w(fogtest::field(14, 5), {{"soldier", 0, 0, 2, false}, {"soldier", 1, 2, 2, false}});
  w.place("block", 3, 2);
  CHECK(w.begin());
  CHECK(w.seesUnit(0, 1));
  CHECK(!w.sees(0, 3, 2));
  ThreatArea known, after, truth;
  areaOf(w.battle, 1, 0, known);
  const int si = w.battle.structureAt[(size_t)w.at(3, 2)];
  CHECK(si >= 0);
  w.battle.structures[(size_t)si].type = w.db.findStructure("gate");
  w.battle.refreshSight();
  areaOf(w.battle, 1, 0, after);
  CHECK(known.move == after.move && known.attack == after.attack);
  Reach r;
  w.battle.reachable(1, r);
  CHECK(r.stops.size() > (size_t)known.moveCount);  // the real walk got longer; the display must not say so
}

// ---- the options and the game rule

namespace {
int ranges(const LookLayer& args, const LookLayer& perGame, const LookLayer& global, bool gamePresent, bool gameAllows) {
  GameLook g;
  g.present = gamePresent;
  g.enemyRanges = gameAllows;
  g.styleIds = {"default"};
  return resolveLook("assets", args, perGame, global, g).enemyRanges;
}
LookLayer layer(const char* v) {
  LookLayer l;
  l.enemyRanges = v;
  return l;
}
}  // namespace

TEST(enemy_ranges_option_layers_and_the_game_rule) {
  CHECK_EQ(ranges(LookLayer(), LookLayer(), LookLayer(), true, true), 1);  // default: movement + attack
  CHECK_EQ(ranges(LookLayer(), LookLayer(), layer("off"), true, true), 0);
  CHECK_EQ(ranges(LookLayer(), LookLayer(), layer("attack"), true, true), 2);
  CHECK_EQ(ranges(LookLayer(), layer("both"), layer("off"), true, true), 1);  // this game's override beats the global one
  CHECK_EQ(ranges(layer("off"), layer("both"), layer("both"), true, true), 0);  // the command line first
  CHECK_EQ(ranges(LookLayer(), LookLayer(), layer("nonsense"), true, true), 1);  // unknown word = unset
  // A game that switched the display off: no setting turns it on.
  CHECK_EQ(ranges(LookLayer(), LookLayer(), LookLayer(), true, false), 0);
  CHECK_EQ(ranges(LookLayer(), layer("both"), layer("both"), true, false), 0);
  CHECK_EQ(ranges(LookLayer(), LookLayer(), LookLayer(), false, false), 1);  // (the launcher has no game)
}

TEST(enemy_ranges_option_row_and_settings_file) {
  const OptionDef* def = nullptr;
  for (const OptionDef& d : optionDefs())
    if (!strcmp(d.key, "enemyRanges")) def = &d;
  CHECK(def != nullptr);
  if (!def) return;
  CHECK(std::string(def->label) == "Enemy ranges");
  OptionContext ctx;
  std::vector<OptionChoice> rows = def->choices(ctx);
  CHECK_EQ((int)rows.size(), 4);
  CHECK(rows[0].value.isNull() && rows[0].label == "Game default");
  CHECK(rows[1].value.asString() == "both" && rows[2].value.asString() == "attack" && rows[3].value.asString() == "off");
  Settings s;
  s.setGlobal("enemyRanges", Json::makeString("off"));
  CHECK(globalLayer(s).enemyRanges == "off");
  // No other row's label starts like this one (the options tests look rows up by their first words).
  for (const OptionDef& d : optionDefs())
    if (&d != def) CHECK(std::strncmp(d.label, "Enemy", 5) != 0);
}

TEST(enemy_ranges_game_rule_is_read_and_checked) {
  const char* rules = "\"rules\": { \"defeat\": [\"noUnits\"],";
  rankstest::Copy plain("enemy_ranges_rule_default", "srpg-staves");
  Database db;
  std::string err;
  CHECK(loadGame(plain.dir, db, err));
  CHECK(db.enemyRanges);  // on unless the game says otherwise
  rankstest::Copy off("enemy_ranges_rule_off", "srpg-staves");
  off.swap("game.json", rules, "\"rules\": { \"display\": { \"enemyRanges\": false }, \"defeat\": [\"noUnits\"],");
  Database d2;
  CHECK(loadGame(off.dir, d2, err));
  CHECK(!d2.enemyRanges);
  GameInfo info;
  CHECK(loadGameInfo(off.dir, info, err));
  CHECK(!info.enemyRanges);
  rankstest::Copy typo("enemy_ranges_rule_typo", "srpg-staves");
  typo.swap("game.json", rules, "\"rules\": { \"display\": { \"enemyRange\": false }, \"defeat\": [\"noUnits\"],");
  CHECK(rankstest::saysIn(typo.gameError(), "unknown key \"enemyRange\""));
  rankstest::Copy word("enemy_ranges_rule_word", "srpg-staves");
  word.swap("game.json", rules, "\"rules\": { \"display\": { \"enemyRanges\": \"no\" }, \"defeat\": [\"noUnits\"],");
  CHECK(!word.gameError().empty());
}

// ---- cost

TEST(enemy_ranges_union_of_sixty_enemies_is_cheap) {
  // 64 x 64 squares, 60 enemies in a spread, 3 blue units: "show all" once per board change.
  std::vector<std::string> rows((size_t)64, std::string((size_t)64, '.'));
  for (int y = 8; y < 64; y += 9)
    for (int x = 0; x < 50; ++x) rows[(size_t)y][(size_t)x] = (x % 11 == 5) ? '.' : 'f';  // forest belts with gaps
  std::vector<UnitSpec> units = {{"soldier", 0, 0, 0, true}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 2, 0, false}};
  const char* kinds[3] = {"soldier", "archer", "tank"};
  for (int i = 0; i < 60; ++i) units.push_back({kinds[i % 3], 1, 4 + (i * 7) % 58, 4 + (i * 13) % 56, false});
  fixture::World w(rows, units);
  CHECK(w.begin());
  EnemyRanges er;
  er.toggleAll();
  using Clock = std::chrono::steady_clock;
  const Clock::time_point t0 = Clock::now();
  er.update(w.battle, 0);
  const long us = (long)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t0).count();
  std::printf("  enemy ranges: 60 enemies on 64x64: %d units, %d move, %d attack squares, %ld us\n", er.unitCount(), er.moveSquares(), er.attackSquares(), us);
  CHECK_EQ(er.unitCount(), 60);
  CHECK(us < 2000000);  // a loose guard; the real numbers are in the report
  const Clock::time_point t1 = Clock::now();
  for (int i = 0; i < 1000; ++i) er.update(w.battle, 0);  // 1000 frames without a change
  const long frames = (long)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - t1).count();
  std::printf("  1000 unchanged frames: %ld us\n", frames);
  CHECK(frames < 50000);
}
