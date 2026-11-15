// Hexagon games in play: movement and attack ranges, structure footprints in battle, saves, the AI.
#include <algorithm>
#include <set>

#include "core/ai/ai.h"
#include "core/battle/save.h"
#include "hex_fixture.h"
#include "save_helpers.h"

using namespace ote;
using namespace hexfix;
using namespace savetest;

namespace {

std::string fixtureDir(int flat) { return ote_test::sourceDir() + (flat ? "/tests/fixtures/hex-flat" : "/tests/fixtures/hex-pointy"); }

std::string unit(const char* type, const char* side, int x, int y) {
  return std::string("{\"type\":\"") + type + "\",\"side\":\"" + side + "\",\"x\":" + std::to_string(x) + ",\"y\":" + std::to_string(y) + "}";
}

}  // namespace

TEST(hex_movement_range_is_the_cells_within_the_move_on_an_open_map) {
  for (int flat = 0; flat < 2; ++flat)
    for (int sx = 3; sx <= 4; ++sx)
      for (int sy = 3; sy <= 4; ++sy) {
        // Blue grunt (move 3) in the middle of a plain 9 x 9 map, red far in a corner.
        std::string units = unit("grunt", "blue", sx, sy) + "," + unit("grunt", "red", 8, 8);
        std::string dir = makeGame("mv", hexGrid(flat ? "flat" : "pointy"), "", plainMap("hex", 9, 9, "", units));
        Rig rig;
        CHECK(rig.load(dir, "m"));
        rig.start();
        const Battle& b = rig.battle;
        CHECK_EQ(b.units[0].cell, b.grid.at(sx, sy));
        Reach reach;
        b.reachable(0, reach);
        int bad = 0;
        for (Cell c = 0; c < b.grid.cellCount(); ++c) {
          int d = b.grid.distance(b.units[0].cell, c);
          int want = d <= 3 ? d : -1;
          if (c == b.units[1].cell) want = -1;
          if (reach.cost[(size_t)c] != want) ++bad;
        }
        CHECK_EQ(bad, 0);
      }
}

TEST(hex_attack_range_is_the_distance_band_of_the_weapon) {
  for (int flat = 0; flat < 2; ++flat) {
    // A gun (range 2..3) in the middle, red grunts on every other cell.
    std::string reds;
    Grid g(GridType::Hex, 9, 9, flat ? HexLayout::Flat : HexLayout::Pointy);
    for (Cell c = 0; c < g.cellCount(); ++c)
      if (c != g.at(4, 4)) reds += "," + unit("grunt", "red", g.x(c), g.y(c));
    std::string dir = makeGame("rng", hexGrid(flat ? "flat" : "pointy"), "", plainMap("hex", 9, 9, "", unit("gun", "blue", 4, 4) + reds));
    Rig rig;
    CHECK(rig.load(dir, "m"));
    rig.start();
    const Battle& b = rig.battle;
    std::vector<AttackOption> opts;
    b.attackOptions(0, b.units[0].cell, opts);
    std::set<Cell> got, want;
    for (const AttackOption& o : opts) got.insert(b.units[(size_t)o.target].cell);
    for (Cell c = 0; c < g.cellCount(); ++c) {
      int d = g.distance(g.at(4, 4), c);
      if (d >= 2 && d <= 3) want.insert(c);
    }
    CHECK(got == want);
    CHECK_EQ((int)want.size(), 6 * 2 + 6 * 3);  // rings of 12 and 18 hexagons, all inside the 9 x 9 map
  }
}

TEST(hex_ring_structures_fill_every_covered_hexagon_in_battle) {
  for (int flat = 0; flat < 2; ++flat) {
    std::string dir = makeGame("fp", hexGrid(flat ? "flat" : "pointy"),
                               "{ \"structures\": [ {\"id\":\"city\"}, {\"id\":\"fort\",\"rings\":1,\"capturePoints\":10,\"tags\":[\"hq\"]} ] }",
                               plainMap("hex", 9, 9, "{\"type\":\"fort\",\"x\":4,\"y\":4,\"side\":\"blue\"},{\"type\":\"city\",\"x\":0,\"y\":0}",
                                        unit("grunt", "blue", 8, 8) + "," + unit("grunt", "red", 0, 8)));
    Rig rig;
    CHECK(rig.load(dir, "m"));
    const Battle& b = rig.battle;
    std::vector<Cell> cells;
    b.structureCells(0, cells);
    CHECK_EQ((int)cells.size(), 7);
    int covered = 0;
    for (Cell c = 0; c < b.grid.cellCount(); ++c)
      if (b.structureAt[(size_t)c] == 0) ++covered;
    CHECK_EQ(covered, 7);
    for (Cell c : cells) CHECK_EQ(b.structureIndexAt(c), 0);
    CHECK_EQ(b.structureIndexAt(b.grid.at(0, 0)), 1);
    int neighbours = 0;  // the ring is the middle's neighbours
    for (int i = 0; i < 6; ++i) neighbours += b.structureIndexAt(b.grid.neighbor(b.grid.at(4, 4), i)) == 0;
    CHECK_EQ(neighbours, 6);
  }
}

TEST(hex_battle_rejects_a_ring_structure_that_does_not_fit_when_built_in_code) {
  Rig rig;
  std::string dir = makeGame("fp2", hexGrid("pointy"), "{ \"structures\": [ {\"id\":\"fort\",\"rings\":1} ] }",
                             plainMap("hex", 9, 9, "{\"type\":\"fort\",\"x\":4,\"y\":4}", unit("grunt", "blue", 8, 8) + "," + unit("grunt", "red", 0, 8)));
  CHECK(rig.load(dir, "m"));
  MapDef bad = rig.map;
  bad.structures[0].x = 0;  // the ring would leave the map
  Battle b;
  std::string err;
  CHECK(!b.init(rig.db, bad, rig.formulas, 1, err));
  CHECK(has(err, "does not fit inside the map"));
  MapDef twice = rig.map;
  twice.structures.push_back(twice.structures[0]);
  CHECK(!b.init(rig.db, twice, rig.formulas, 1, err));
  CHECK(has(err, "overlaps another structure"));
}

TEST(hex_fixtures_load_and_a_battle_saves_and_restores_exactly) {
  for (int flat = 0; flat < 2; ++flat) {
    Rig live;
    CHECK(live.load(fixtureDir(flat), "skirmish", 7));
    CHECK(live.battle.grid.isHex());
    CHECK(live.battle.grid.layout() == (flat ? HexLayout::Flat : HexLayout::Pointy));
    live.start();
    std::vector<Event> ev;
    playAi(live.battle, 80, ev);  // well into the battle: captures, builds, a rotated turn
    Json saved;
    saveBattleState(live.battle, saved);
    Json parsed;
    std::string err;
    CHECK(Json::parse(saved.dump(2), parsed, err));
    Rig fresh;
    CHECK(fresh.load(fixtureDir(flat), "skirmish", 999));
    CHECK(restoreBattleState(fresh.battle, parsed, err));
    CHECK_STR(stateText(live.battle), stateText(fresh.battle));
    CHECK(live.battle.occupant == fresh.battle.occupant);
    CHECK(live.battle.structureAt == fresh.battle.structureAt);
    Battle a = live.battle;
    std::vector<Event> ea, eb;
    playAi(a, 300, ea);
    playAi(fresh.battle, 300, eb);
    CHECK(sameEvents(ea, eb));
    CHECK_STR(stateText(a), stateText(fresh.battle));
  }
}

TEST(hex_saves_are_tied_to_the_grid_through_the_data_hash) {
  std::string h[2];
  for (int flat = 0; flat < 2; ++flat) {
    Rig rig;
    CHECK(rig.load(fixtureDir(flat), "skirmish"));
    std::string err;
    CHECK(computeDataHash(rig.db, "skirmish", h[flat], err));
  }
  CHECK(h[0] != h[1]);  // the two fixtures differ by the layout (and the name) only
}

TEST(hex_ai_plays_60_steps_on_both_fixtures_without_an_illegal_action) {
  for (int flat = 0; flat < 2; ++flat) {
    Rig rig;
    CHECK(rig.load(fixtureDir(flat), "skirmish", 5));
    rig.start();
    std::vector<Event> ev;
    int steps = 0, rejected = 0;
    for (; steps < 60 && !rig.battle.over; ++steps) {
      Battle before = rig.battle;
      size_t n = ev.size();
      aiTakeStep(rig.battle, ev);  // false = it ended its turn; the next call plays the other side
      // An illegal action is refused by apply() and leaves no event; a step that changed nothing is a stuck AI.
      if (ev.size() == n && stateText(before) == stateText(rig.battle)) ++rejected;
    }
    CHECK_EQ(rejected, 0);
    CHECK(steps == 60 || rig.battle.over);
    bool moved = false;
    for (const Event& e : ev) moved = moved || e.type == EventType::UnitMoved;
    CHECK(moved);
  }
}

TEST(hex_ai_battles_are_deterministic_for_a_seed) {
  for (int flat = 0; flat < 2; ++flat) {
    std::string text[2];
    for (int run = 0; run < 2; ++run) {
      Rig rig;
      CHECK(rig.load(fixtureDir(flat), "skirmish", 11));
      rig.start();
      std::vector<Event> ev;
      playAi(rig.battle, 400, ev);
      text[run] = stateText(rig.battle);
    }
    CHECK_STR(text[0], text[1]);
  }
}
