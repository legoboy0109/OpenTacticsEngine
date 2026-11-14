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
  for (const Unit& u : d.battle.units) {
    if (u.side != 0) continue;
    Reach r;
    d.battle.reachable(u.id, r);
    CHECK(r.stops.size() > 1);
  }
  // Fighter Bram (unit 3) against soldier Tane (unit 5): 7 + 8 - 4 = 11.
  const Unit& a = d.battle.units[3];
  const Unit& b = d.battle.units[5];
  FormulaCtx ctx;
  std::vector<int> carried;
  d.battle.weaponsOf(a, carried);  // weapons are items here: his iron axe
  CHECK_EQ(carried.size(), size_t(1));
  if (carried.empty()) return;
  ctx.a = &a; ctx.b = &b; ctx.w = &d.db.weapons[carried[0]];
  Props scratchA, scratchB;
  ctx.ta = &d.battle.numbersAt(a.cell, scratchA); ctx.tb = &d.battle.numbersAt(b.cell, scratchB);
  double dmg = d.formulas.evalDamage(carried[0], ctx);
  CHECK_EQ((int)dmg, 11);
  double hit = d.formulas.eval(Formulas::Hit, ctx, -1);
  CHECK(hit > 20 && hit <= 150);
  // The demo's items: the lord's vulnerary, the storage, a shop on the forts, the enemy leader's drop.
  const Battle& bt = d.battle;
  CHECK(bt.units[0].items.size() == 2 && bt.itemOf(bt.units[0].items[1]).id == "vulnerary");
  CHECK(bt.sides[0].storage.size() == 1 && bt.sides[0].resources[0] == 700);
  CHECK(bt.storageOpen(bt.units[0]) && !bt.storageOpen(bt.units[2]));  // the leader; Lyra is not next to him
  CHECK(bt.units[4].items.size() == 2 && bt.units[4].items[1].drop);
  CHECK_EQ(bt.stat(bt.units[4], d.db.findStat("def")), 6 + 2);        // Gorm wears the ring
  CHECK(!d.db.structures[0].sells.empty());
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
