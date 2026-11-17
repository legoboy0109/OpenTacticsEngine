// Staves, stage two, the data (docs/SRPG.md section 8 "Staves: stage two, as built"): the loader and its refusals for warp, pull,
// swap, repair and light, the "repairable" flag of an item, and the saved lights. Game: tests/fixtures/srpg-staves2.
#include "json_edit.h"
#include "srpg_staves2_fixture.h"

using namespace ote;
using namespace staves2test;
using namespace jsonedit;

namespace {
// The load message when `from` (a piece of the fixture's items.json) is replaced by `to`.
std::string itemsError(const char* name, const std::string& from, const std::string& to) {
  Copy c(name, "srpg-staves2");
  c.swap("data/items.json", from, to);
  return c.gameError();
}
const char* kWarp = "{ \"warp\": { \"range\": 4 } }";
const char* kRecall = "{ \"pull\": true }";
const char* kSwap = "{ \"swap\": true }";
const char* kFix = "{ \"repair\": \"full\" }";
const char* kLantern = "{ \"light\": { \"radius\": 2, \"turns\": 2 } }";
const std::string kP = "data/items.json: item \"";

Json parsed(const char* text) {
  Json j;
  std::string err;
  if (!Json::parse(text, j, err)) std::printf("  bad test json: %s\n", err.c_str());
  return j;
}
std::string restoreError(Yard& w, const Json& saved) {
  Battle r;
  std::string err;
  if (!r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err)) return "init failed: " + err;
  return restoreBattleState(r, saved, err) ? "" : err;
}
Json saveOf(Yard& w) {
  Json j;
  saveBattleState(w.b(), j);
  return j;
}
}  // namespace

TEST(staves2_the_fixture_loads_and_each_new_effect_is_read) {
  Copy c("s2_load", "srpg-staves2");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  auto staff = [&](const char* id) -> const StaffDef& { return db.staves[(size_t)db.items[(size_t)db.findItem(id)].staff]; };
  const StaffEffect& warp = staff("warp_staff").effects[0];
  CHECK(warp.kind == StaffEffect::Kind::Warp && warp.warpMin == 1 && warp.warpMax == 4 && warp.warpExpr < 0);
  const StaffEffect& banish = staff("banish_staff").effects[0];
  CHECK(banish.warpMin == 3 && banish.warpMax == 5);
  CHECK(staff("recall_staff").effects[0].kind == StaffEffect::Kind::Pull && staff("swap_staff").effects[0].kind == StaffEffect::Kind::Swap);
  const StaffEffect& fix = staff("fix_staff").effects[0];
  CHECK(fix.kind == StaffEffect::Kind::Repair && fix.amount == StaffEffect::Amount::Full);
  const StaffEffect& patch = staff("patch_staff").effects[0];
  CHECK(patch.amount == StaffEffect::Amount::Fixed && patch.value == 2);
  const StaffEffect& lantern = staff("lantern_staff").effects[0];
  CHECK(lantern.kind == StaffEffect::Kind::Light && lantern.radius == 2 && lantern.turns == 2);
  CHECK_EQ(staff("spark_staff").effects[0].turns, 1);  // no "turns": until the user's next turn starts
  CHECK(staff("lantern_staff").picksSquare() && !staff("lantern_staff").needsPick());
  CHECK(staff("kindly_pull_staff").movesUnit() && staff("kindly_pull_staff").needsPick());
  CHECK(!db.items[(size_t)db.findItem("glass_blade")].repairable && db.items[(size_t)db.findItem("iron_blade")].repairable);
  // A warp's range may be a formula too.
  Copy f("s2_load_formula", "srpg-staves2");
  f.swap("data/items.json", kWarp, "{ \"warp\": { \"range\": \"1 + a.mag / 3\" } }");
  CHECK_STR(f.gameError(), "");
}

TEST(staves2_loader_refusals_for_warp) {
  CHECK(saysIn(itemsError("s2_w1", kWarp, "{ \"warp\": 3 }"), kP + "warp_staff\": \"effects\" entry 1: \"warp\" must be { \"range\": n }"));
  CHECK(saysIn(itemsError("s2_w2", kWarp, "{ \"warp\": {} }"), "\"warp\" needs \"range\""));
  CHECK(saysIn(itemsError("s2_w3", kWarp, "{ \"warp\": { \"range\": 0 } }"), "\"warp\": \"range\" must be a whole number from 1 to 20"));
  CHECK(saysIn(itemsError("s2_w4", kWarp, "{ \"warp\": { \"range\": 21 } }"), "\"warp\": \"range\" must be a whole number from 1 to 20"));
  CHECK(saysIn(itemsError("s2_w5", kWarp, "{ \"warp\": { \"range\": [5, 2] } }"), "min not above max"));
  CHECK(saysIn(itemsError("s2_w6", kWarp, "{ \"warp\": { \"range\": 4, \"far\": 1 } }"), "\"warp\": unknown key \"far\""));
  CHECK(saysIn(itemsError("s2_w7", kWarp, "{ \"warp\": { \"range\": true } }"), "a whole number from 1 to 20, a list [min, max] or a formula"));
  CHECK(saysIn(itemsError("s2_w8", "\"range\": 3, \"target\": \"ally\", \"effects\": [ { \"warp\": { \"range\": 4 } } ]",
                          "\"range\": 3, \"target\": \"ally\", \"area\": { \"radius\": 1 }, \"effects\": [ { \"warp\": { \"range\": 4 } } ]"),
               "\"warp\" works on the one unit picked, so \"area\" must be \"single\""));
  CHECK(saysIn(itemsError("s2_w9", "\"range\": 3, \"target\": \"ally\", \"effects\": [ { \"warp\": { \"range\": 4 } } ]",
                          "\"range\": 3, \"target\": \"ally\", \"area\": \"all\", \"effects\": [ { \"warp\": { \"range\": 4 } } ]"),
               "so \"area\" must be \"single\""));
  // A warp may be used on its user ("self") and on a foe.
  CHECK_STR(itemsError("s2_w10", "\"target\": \"self\", \"effects\": [ { \"warp\": { \"range\": 3 } } ]", "\"target\": \"team\", \"range\": 2, \"effects\": [ { \"warp\": { \"range\": 3 } } ]"), "");
}

TEST(staves2_loader_refusals_for_pull_swap_and_the_combinations) {
  CHECK(saysIn(itemsError("s2_p1", kRecall, "{ \"pull\": false }"), "\"effects\" entry 1: \"pull\" must be true"));
  CHECK(saysIn(itemsError("s2_p2", kSwap, "{ \"swap\": 1 }"), "\"swap\" must be true"));
  CHECK(saysIn(itemsError("s2_p3", "\"range\": 8, \"target\": \"ally\", \"effects\": [ { \"pull\": true } ] }",
                          "\"target\": \"self\", \"effects\": [ { \"pull\": true } ] }"),
               "\"pull\" needs another unit, so \"target\" cannot be \"self\""));
  CHECK(saysIn(itemsError("s2_p4", "\"range\": 4, \"target\": \"ally\", \"effects\": [ { \"swap\": true } ]",
                          "\"target\": \"self\", \"effects\": [ { \"swap\": true } ]"),
               "\"swap\" needs another unit, so \"target\" cannot be \"self\""));
  CHECK(saysIn(itemsError("s2_p5", "\"range\": 4, \"target\": \"ally\", \"effects\": [ { \"swap\": true } ]",
                          "\"range\": 4, \"target\": \"ally\", \"area\": { \"radius\": 2 }, \"effects\": [ { \"swap\": true } ]"),
               "\"swap\" works on the one unit picked, so \"area\" must be \"single\""));
  CHECK(saysIn(itemsError("s2_p6", kRecall, "{ \"pull\": true }, { \"swap\": true }"), "only one of \"warp\", \"pull\" and \"swap\""));
  CHECK(saysIn(itemsError("s2_p7", kRecall, std::string(kRecall) + ", " + kWarp), "only one of \"warp\", \"pull\" and \"swap\""));
  CHECK(saysIn(itemsError("s2_p8", kFix, std::string(kFix) + ", " + kRecall), "\"repair\" cannot be put together with \"warp\", \"pull\" or \"swap\""));
  CHECK(saysIn(itemsError("s2_p9", kFix, std::string(kFix) + ", { \"repair\": 1 }"), "only one \"repair\""));
  CHECK(saysIn(itemsError("s2_p10", "\"range\": 2, \"target\": \"ally\", \"effects\": [ { \"repair\": \"full\" } ]",
                          "\"range\": 2, \"target\": \"ally\", \"area\": { \"radius\": 1 }, \"effects\": [ { \"repair\": \"full\" } ]"),
               "\"repair\" works on the one unit picked, so \"area\" must be \"single\""));
  // Heal and pull, and heal and repair, are fine; a heal still cannot go on a foe.
  CHECK_STR(itemsError("s2_p11", kFix, "{ \"heal\": 3 }, { \"repair\": 1 }"), "");
  CHECK(saysIn(itemsError("s2_p12", "\"target\": \"foe\", \"effects\": [ { \"pull\": true } ]", "\"target\": \"foe\", \"effects\": [ { \"pull\": true }, { \"heal\": 3 } ]"),
               "\"heal\" cannot target \"foe\""));
}

TEST(staves2_loader_refusals_for_repair_and_repairable) {
  CHECK(saysIn(itemsError("s2_r1", kFix, "{ \"repair\": 0 }"), "\"repair\" must be a whole number of uses from 1 to 999"));
  CHECK(saysIn(itemsError("s2_r2", kFix, "{ \"repair\": \"half\" }"), "\"repair\" must be \"full\" or a whole number of uses from 1 to 999"));
  CHECK(saysIn(itemsError("s2_r3", kFix, "{ \"repair\": { \"uses\": 2 } }"), "\"repair\" must be \"full\" or a whole number"));
  CHECK(saysIn(itemsError("s2_r4", "\"uses\": 4, \"repairable\": false", "\"uses\": 4, \"repairable\": \"no\""), "\"repairable\" must be true or false"));
  CHECK(saysIn(itemsError("s2_r5", "\"weapon\": \"short_sword\" }", "\"weapon\": \"short_sword\", \"repairable\": false }"),
               kP + "short_sword\": \"repairable\" only matters for an item with \"uses\""));
  CHECK_STR(itemsError("s2_r6", "\"weapon\": \"short_sword\" }", "\"weapon\": \"short_sword\", \"uses\": 3, \"repairable\": true }"), "");
}

TEST(staves2_loader_refusals_for_light) {
  CHECK(saysIn(itemsError("s2_l1", kLantern, "{ \"light\": 3 }"), "\"light\" must be { \"radius\": n, \"turns\": n }"));
  CHECK(saysIn(itemsError("s2_l2", kLantern, "{ \"light\": { \"turns\": 2 } }"), "\"light\": \"radius\" must be a whole number from 1 to 10"));
  CHECK(saysIn(itemsError("s2_l3", kLantern, "{ \"light\": { \"radius\": 11 } }"), "\"light\": \"radius\" must be a whole number from 1 to 10"));
  CHECK(saysIn(itemsError("s2_l4", kLantern, "{ \"light\": { \"radius\": 2, \"turns\": 0 } }"), "\"light\": \"turns\" must be a whole number from 1 to 10"));
  CHECK(saysIn(itemsError("s2_l5", kLantern, "{ \"light\": { \"radius\": 2, \"turns\": 11 } }"), "\"light\": \"turns\" must be a whole number from 1 to 10"));
  CHECK(saysIn(itemsError("s2_l6", kLantern, "{ \"light\": { \"radius\": 2, \"fade\": 1 } }"), "\"light\": unknown key \"fade\""));
  CHECK(saysIn(itemsError("s2_l7", kLantern, std::string(kLantern) + ", { \"heal\": 4 }"), "\"light\" picks a square, not a unit, so it cannot be put together with another effect"));
  CHECK(saysIn(itemsError("s2_l8", kLantern, std::string(kLantern) + ", " + kWarp), "cannot be put together with another effect"));
  const std::string at = "\"range\": 6, \"effects\": [ { \"light\": { \"radius\": 2, \"turns\": 2 } } ]";
  CHECK(saysIn(itemsError("s2_l9", at, "\"target\": \"ally\", " + at), "\"light\" picks a square, so the staff has no \"target\""));
  CHECK(saysIn(itemsError("s2_l10", at, "\"area\": \"single\", " + at), "so the staff has no \"area\""));
  CHECK(saysIn(itemsError("s2_l11", at, "\"hit\": 50, " + at), "so there is no unit to roll \"hit\" for"));
  CHECK(saysIn(itemsError("s2_l12", at, "\"if\": [ { \"hp\": 5 } ], " + at), "so there is no unit for \"if\" to judge"));
  CHECK(saysIn(itemsError("s2_l13", at, "\"xp\": 5, " + at), ""));  // an experience reward is fine
  CHECK(saysIn(itemsError("s2_l14", kLantern, "{ \"glow\": 1 }"), "unknown effect \"glow\" (a staff can: heal, warp, pull, swap, repair, light, inflict, cure)"));
}

TEST(staves2_a_light_is_saved_while_it_lasts_and_an_old_save_loads) {
  Yard w("s2_save_light", "mist");
  CHECK(!saveOf(w).has("lights"));  // nothing lit: the save is what it was
  CHECK(w.use("sage", "lantern_staff", nullptr, 7, 2));
  const Json saved = saveOf(w);
  CHECK(saved.has("lights") && saved["lights"].size() == 1);
  CHECK_EQ(saved["lights"][(size_t)0]["radius"].asInt(), 2);
  CHECK_EQ(saved["lights"][(size_t)0]["turns"].asInt(), 2);
  CHECK_EQ(saved["lights"][(size_t)0]["cell"].asInt(), (int)w.at(7, 2));
  // Round trip through text: the same state, the same sight.
  Json reread;
  std::string err;
  CHECK(Json::parse(saved.dump(0), reread, err));
  Battle r;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restoreBattleState(r, reread, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK_EQ((int)r.lights.size(), 1);
  CHECK(r.unitVisibleTo(0, r.units[(size_t)w.id("g2")]));
  // A save from before this existed has no key: no light.
  Battle r2;
  CHECK(r2.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restoreBattleState(r2, withoutAt(saved, {"lights"}), err));
  CHECK(r2.lights.empty() && !r2.unitVisibleTo(0, r2.units[(size_t)w.id("g2")]));
  // The light keeps its count down after a reload.
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::endTurn()));
  CHECK_EQ(saveOf(w)["lights"][(size_t)0]["turns"].asInt(), 1);
}

TEST(staves2_damaged_lights_in_a_save_are_refused_with_a_path) {
  Yard w("s2_save_damaged", "mist");
  CHECK(w.use("sage", "lantern_staff", nullptr, 7, 2));
  const Json saved = saveOf(w);
  auto bad = [&](const char* text) { return restoreError(w, withValue(saved, {"lights"}, parsed(text))); };
  CHECK(saysIn(bad("3"), "battle.lights: missing or not a list"));
  CHECK(saysIn(bad("[3]"), "battle.lights[0]: not an object"));
  CHECK(saysIn(bad("[{\"side\": 9, \"cell\": 3, \"radius\": 2, \"turns\": 1}]"), "battle.lights[0].side"));
  CHECK(saysIn(bad("[{\"side\": 0, \"cell\": 999, \"radius\": 2, \"turns\": 1}]"), "battle.lights[0].cell"));
  CHECK(saysIn(bad("[{\"side\": 0, \"cell\": 3, \"radius\": 0, \"turns\": 1}]"), "battle.lights[0].radius"));
  CHECK(saysIn(bad("[{\"side\": 0, \"cell\": 3, \"radius\": 2, \"turns\": 0}]"), "battle.lights[0].turns"));
  CHECK(saysIn(bad("[{\"side\": 0, \"cell\": 3, \"radius\": 2}]"), "battle.lights[0].turns"));
  CHECK_STR(bad("[]"), "");
  // A battle without fog has no lights.
  Yard plain("s2_save_nofog");
  CHECK(saysIn(restoreError(plain, withValue(saveOf(plain), {"lights"}, parsed("[{\"side\": 0, \"cell\": 3, \"radius\": 2, \"turns\": 1}]"))),
               "battle.lights: this battle has no fog of war"));
}
