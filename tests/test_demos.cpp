// End-to-end smoke tests: the shipped demo games load, their Lua formulas
// compile, and a battle starts and produces sane forecasts.
#include "core/battle/battle.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "test.h"

using namespace ote;

namespace {
struct Demo {
  Database db;
  MapDef map;
  Formulas formulas;
  Battle battle;
  std::vector<Event> events;
  bool load(const char* game, const char* mapId) {
    std::string err;
    bool ok = loadGame(ote_test::sourceDir() + "/games/" + game, db, err) &&
              loadMap(db, mapId, map, err) && formulas.init(db, err) &&
              battle.init(db, map, formulas, 7, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    if (ok) battle.start(events);
    return ok;
  }
};
}  // namespace

TEST(demo_srpg_starts_and_forecasts) {
  Demo d;
  CHECK(d.load("demo-srpg", "bridge"));
  CHECK(d.db.rules.progression && d.db.rules.defeatLeaderDead);
  // Every blue unit can move somewhere, and a blue-vs-red forecast through
  // the scripted damage formula gives believable numbers.
  int blue = -1, red = -1;
  for (const Unit& u : d.battle.units) {
    if (u.side == 0) {
      Reach r;
      d.battle.reachable(u.id, r);
      CHECK(r.stops.size() > 1);
      if (blue < 0) blue = u.id;
    } else if (red < 0) red = u.id;
  }
  CHECK(blue >= 0 && red >= 0);
  StrikeForecast f;
  const Unit& a = d.battle.units[blue];
  const Unit& b = d.battle.units[red];
  FormulaCtx ctx;
  ctx.a = &a; ctx.b = &b; ctx.w = &d.db.weapons[d.battle.typeOf(a).weapons[0]];
  ctx.ta = &d.battle.terrainAt(a.cell); ctx.tb = &d.battle.terrainAt(b.cell);
  double dmg = d.formulas.evalDamage(d.battle.typeOf(a).weapons[0], ctx);
  CHECK(dmg > 0 && dmg < 40);
  double hit = d.formulas.eval(Formulas::Hit, ctx, -1);
  CHECK(hit > 20 && hit <= 150);
}

TEST(demo_economy_starts_with_income_and_production) {
  Demo d;
  CHECK(d.load("demo-economy", "twin_rivers"));
  CHECK(d.db.rules.economy && d.db.rules.production && d.db.rules.capture);
  CHECK(d.battle.sides[0].hadHq && d.battle.sides[1].hadHq);
  CHECK(d.battle.sides[0].resources[0] > 3000);  // start funds + first income
  // Blue can build at its factory.
  bool built = false;
  for (Cell c = 0; c < d.battle.grid.cellCount() && !built; ++c) {
    std::vector<int> opts;
    d.battle.buildOptions(c, opts);
    for (int t : opts)
      if (d.battle.canAfford(0, t)) { built = d.battle.apply(Action::build(c, t), d.events); break; }
  }
  CHECK(built);
}
