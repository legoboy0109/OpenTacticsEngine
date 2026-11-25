// Battle-scene and unit-art follow-ups: per-side sheets, assign by side / weapon (the full order), the frame limit and the sheet
// limit, the extra animation slots, and walking frames of unit pictures (docs/BATTLE_SCENE.md, docs/GRAPHICS.md "Walking").
#include "posix_compat.h"

#include <cstdio>

#include "battle/fight_timeline.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// Like test_battle_art.cpp's game: types knight / archer, character eirik, weapons sword (blade) and longbow (bow), sides a / b.
// Sheets: "fighters" and "fighters2" 8 x 4 cells of 32 x 32, "small" 2 x 1 cells of 32 x 32, "fx" 4 x 1 of 16 x 16, "walk" 4 x 3
// cells of 32 x 32 (the units' own sheet "men" is 2 x 1 cells of 16 x 16), "huge" 4096 x 64 (too large).
std::string makeGame(const char* name, const std::string& battleJson, const std::string& unitsJson = "", bool huge = false) {
  std::string root = ote_test::tempDir() + "/followup_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"formulas\": {\"damage\":\"1\"},"
                      " \"maps\": [{\"id\":\"m\",\"name\":\"M\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json",
                      "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"},{\"id\":\"longbow\",\"group\":\"bow\",\"range\":[2,2]}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"sword\",\"longbow\"]},"
                      "{\"id\":\"archer\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"longbow\"]}] }"));
  CHECK(writeTextFile(root + "/data/characters.json", "{ \"characters\": [{\"id\":\"eirik\",\"name\":\"Eirik\",\"type\":\"knight\"}] }"));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"name\": \"M\", \"grid\": {\"type\":\"square\",\"width\":2,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"..\"],"
                      " \"sides\": [{\"id\":\"a\",\"controller\":\"human\"},{\"id\":\"b\",\"controller\":\"ai\"}],"
                      " \"units\": [{\"type\":\"knight\",\"side\":\"a\",\"x\":0,\"y\":0},{\"type\":\"archer\",\"side\":\"b\",\"x\":1,\"y\":0}] }"));
  auto png = [&](const char* file, int w, int h) {
    unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, (unsigned char)(w >> 8),
                           (unsigned char)w, 0, 0, (unsigned char)(h >> 8), (unsigned char)h, 8, 6};
    FILE* f = std::fopen((root + "/art/" + file).c_str(), "wb");
    CHECK(f != nullptr);
    if (f) {
      std::fwrite(b, 1, sizeof b, f);
      std::fclose(f);
    }
  };
  png("fighters.png", 256, 128);
  png("fighters2.png", 256, 128);
  png("small.png", 64, 32);
  png("fx.png", 64, 16);
  png("walk.png", 128, 96);
  png("men.png", 32, 16);
  png("huge.png", 4096, 64);
  std::string sheets =
      "{\"id\":\"fighters\",\"file\":\"art/fighters.png\",\"cellWidth\":32,\"cellHeight\":32},"
      "{\"id\":\"fighters2\",\"file\":\"art/fighters2.png\",\"cellWidth\":32,\"cellHeight\":32},"
      "{\"id\":\"small\",\"file\":\"art/small.png\",\"cellWidth\":32,\"cellHeight\":32},"
      "{\"id\":\"fx\",\"file\":\"art/fx.png\",\"cellWidth\":16,\"cellHeight\":16},"
      "{\"id\":\"walk\",\"file\":\"art/walk.png\",\"cellWidth\":32,\"cellHeight\":32},"
      "{\"id\":\"men\",\"file\":\"art/men.png\",\"cellWidth\":16,\"cellHeight\":16}";
  if (huge) sheets += ",{\"id\":\"huge\",\"file\":\"art/huge.png\",\"cellWidth\":64,\"cellHeight\":64}";
  CHECK(writeTextFile(root + "/data/graphics.json",
                      "{ \"sheets\": [" + sheets + "], " + (unitsJson.empty() ? "" : "\"units\": " + unitsJson + ",") + " \"battle\": " + battleJson + " }"));
  return root;
}

// "n" frames of the same cell.
std::string repeated(int n) {
  std::string s = "[";
  for (int i = 0; i < n; ++i) s += std::string(i ? "," : "") + "[0,0]";
  return s + "]";
}

void expectRefused(const char* name, const std::string& battle, const char* a, const char* b = "", const std::string& units = "") {
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame(name, battle, units), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json") && has(err, a) && has(err, b));
}

const char* kStand = "\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}";

}  // namespace

// ---- per-side sheets ----------------------------------------------------------------------------------------------------------

TEST(battle_side_sheets_set_level_and_animation_level) {
  const std::string body =
      std::string("{ \"sets\": { \"knight\": { \"sides\": { \"a\": {\"sheet\":\"fighters2\"} }, \"animations\": {") + kStand + ","
      "  \"attack\": {\"sheet\":\"fighters\",\"frames\":[[0,1],[1,1]],\"sides\":{\"b\":{\"sheet\":\"fighters2\"}}} } } } }";
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame("sides", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleSet& k = db.graphics.battle.sets[0];
  const int f1 = db.graphics.findSheet("fighters"), f2 = db.graphics.findSheet("fighters2");
  CHECK_EQ(k.anims[BaStand].sheetFor("a"), f2);  // the set's "sides" gives side a another sheet for every animation ...
  CHECK_EQ(k.anims[BaStand].sheetFor("b"), f1);
  CHECK_EQ(k.anims[BaStand].sheetFor("zzz"), f1);  // a side with no entry: the base sheet
  CHECK_EQ(k.anims[BaAttack].sheetFor("a"), f2);   // ... also the attack, which has its own entry for side b only
  CHECK_EQ(k.anims[BaAttack].sheetFor("b"), f2);   // the animation's own entry
}

TEST(battle_side_sheets_refusals) {
  expectRefused("side_unknown", std::string("{ \"sets\": { \"k\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]],"
                                            "\"sides\":{\"a\":{\"sheet\":\"nope\"}}} } } } }"),
                "unknown sheet \"nope\"", "side \"a\"");
  expectRefused("side_layout", std::string("{ \"sets\": { \"k\": { \"sides\": { \"a\": {\"sheet\":\"fx\"} }, \"animations\": {") + kStand + "} } } }",
                "same layout", "16 x 16");
  expectRefused("side_outside", std::string("{ \"sets\": { \"k\": { \"sides\": { \"a\": {\"sheet\":\"small\"} }, \"animations\": {"
                                            "\"stand\": {\"sheet\":\"fighters\",\"frames\":[[3,0]]} } } } }"),
                "outside sheet \"small\"", "same layout");
  expectRefused("side_shape", std::string("{ \"sets\": { \"k\": { \"sides\": [1], \"animations\": {") + kStand + "} } } }", "\"sides\" must be an object");
}

// ---- assign: side and weapon, and the order ----------------------------------------------------------------------------------------

TEST(battle_assign_order_table) {
  std::string sets = "\"sets\": {";
  for (int i = 0; i < 10; ++i) sets += std::string(i ? "," : "") + "\"s" + std::to_string(i) + "\": { \"animations\": {" + kStand + "} }";
  sets += "}";
  // Every kind of rule; the name says what it is (s0 = type, s1 = group ...).
  const std::string body = "{ " + sets + ", \"assign\": ["
                           "{\"set\":\"s0\",\"type\":\"knight\"},"
                           "{\"set\":\"s1\",\"group\":\"blade\"},"
                           "{\"set\":\"s2\",\"weapon\":\"sword\"},"
                           "{\"set\":\"s3\",\"type\":\"knight\",\"group\":\"blade\"},"
                           "{\"set\":\"s4\",\"type\":\"knight\",\"weapon\":\"sword\"},"
                           "{\"set\":\"s5\",\"character\":\"eirik\"},"
                           "{\"set\":\"s6\",\"character\":\"eirik\",\"group\":\"blade\"},"
                           "{\"set\":\"s7\",\"character\":\"eirik\",\"weapon\":\"sword\"},"
                           "{\"set\":\"s8\",\"side\":\"b\"},"
                           "{\"set\":\"s9\",\"type\":\"knight\",\"side\":\"b\"} ] }";
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame("order", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleArt& a = db.graphics.battle;
  const int knight = db.findUnitType("knight"), archer = db.findUnitType("archer"), eirik = db.findCharacter("eirik");
  const int sword = db.findWeapon("sword"), bow = db.findWeapon("longbow");
  auto name = [&](int character, int type, const char* group, int weapon, const char* side) {
    const int s = a.resolveSet(character, type, group, weapon, side);
    return s < 0 ? std::string("-") : a.sets[(size_t)s].id;
  };
  // character + weapon > character + group > character > type + weapon > type + group > weapon > group > type > side alone
  CHECK(name(eirik, knight, "blade", sword, "a") == "s7");
  CHECK(name(eirik, knight, "blade", -1, "a") == "s6");
  CHECK(name(eirik, knight, "bow", bow, "a") == "s5");
  CHECK(name(eirik, knight, "", -1, "b") == "s5");   // (a side rule of a lower level does not lift itself over a character)
  CHECK(name(-1, knight, "blade", sword, "a") == "s4");
  CHECK(name(-1, knight, "blade", -1, "a") == "s3");
  CHECK(name(-1, archer, "blade", sword, "a") == "s2");
  CHECK(name(-1, archer, "blade", -1, "a") == "s1");
  CHECK(name(-1, knight, "bow", bow, "a") == "s0");
  CHECK(name(-1, knight, "", -1, "a") == "s0");
  CHECK(name(-1, knight, "blade", sword, "b") == "s4");  // a side never beats a more specific level
  CHECK(name(-1, knight, "", -1, "b") == "s9");          // ... but wins a tie: type + side beats type alone, and a side alone
  CHECK(name(-1, archer, "", -1, "b") == "s8");
  CHECK(name(-1, archer, "bow", bow, "a") == "-");       // nothing fits: the map picture
  CHECK(name(-1, archer, "", -1, "") == "-");
}

TEST(battle_assign_refusals) {
  const std::string sets = std::string("\"sets\": { \"k\": { \"animations\": {") + kStand + "} } }";
  expectRefused("as_both", "{ " + sets + ", \"assign\": [{\"set\":\"k\",\"weapon\":\"sword\",\"group\":\"blade\"}] }", "\"weapon\" and a \"group\"");
  expectRefused("as_weapon", "{ " + sets + ", \"assign\": [{\"set\":\"k\",\"weapon\":\"axe\"}] }", "no weapon with the id \"axe\"");
  expectRefused("as_empty", "{ " + sets + ", \"assign\": [{\"set\":\"k\"}] }", "\"weapon\" or \"side\"");
  expectRefused("as_dupe", "{ " + sets + ", \"assign\": [{\"set\":\"k\",\"side\":\"a\"},{\"set\":\"k\",\"side\":\"a\"}] }", "already given");
  Database db;
  std::string err;  // the same weapon for two sides is not a duplicate
  CHECK(loadGame(makeGame("as_ok", "{ " + sets + ", \"assign\": [{\"set\":\"k\",\"weapon\":\"sword\",\"side\":\"a\"},{\"set\":\"k\",\"weapon\":\"sword\",\"side\":\"b\"}] }"), db, err));
}

// ---- frame limit and sheet limit ---------------------------------------------------------------------------------------------------

TEST(battle_frame_limit_is_512) {
  auto body = [&](int n) {
    return "{ \"sets\": { \"k\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]},"
           "\"attack\": {\"sheet\":\"fighters\",\"frames\":" + repeated(n) + ",\"ms\":20,\"hit\":" + std::to_string(n - 1) + "} } } } }";
  };
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame("frames100", body(100)), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (ok) {
    const BattleAnim& at = db.graphics.battle.sets[0].anims[BaAttack];
    CHECK_EQ((int)at.frames.size(), 100);
    CHECK_EQ(at.totalMs(), 2000);
    CHECK_EQ(at.hitFrame(), 99);
    CHECK_EQ(at.frameAt(1990), 99);  // timing does not assume small counts
    CHECK_EQ(at.frameAt(1000), 50);
  }
  CHECK(loadGame(makeGame("frames512", body(512)), db, err));
  expectRefused("frames513", body(513), "between 1 and 512 cells");
  CHECK_EQ(kMaxBattleFrames, 512);
}

TEST(battle_sheet_larger_than_the_limit_is_refused) {
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame("huge", "{}", "", true), db, err);
  CHECK(!ok);
  CHECK(has(err, "4096 x 64") && has(err, "2048 x 2048"));
  if (ok || !has(err, "2048")) std::printf("  message was: %s\n", err.c_str());
}

// ---- more animation slots ------------------------------------------------------------------------------------------------------------

TEST(battle_extra_slots_load_and_fall_back) {
  const std::string body =
      std::string("{ \"sets\": { \"k\": { \"animations\": {") + kStand + ","
      "\"standRanged\": {\"sheet\":\"fighters\",\"frames\":[[1,0]]},"
      "\"critRanged\": {\"sheet\":\"fighters\",\"frames\":[[1,1],[2,1]],\"hit\":1},"
      "\"dodgeRanged\": {\"sheet\":\"fighters\",\"frames\":[[1,2]]},"
      "\"miss\": {\"sheet\":\"fighters\",\"frames\":[[3,1],[4,1]],\"hit\":1} } } } }";
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame("slots", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleSet& k = db.graphics.battle.sets[0];
  CHECK(k.anims[BaStandRanged].any() && k.anims[BaStandRanged].loop);  // (a stand loops, the others do not)
  CHECK(!k.anims[BaCritRanged].loop && !k.anims[BaMiss].loop);
  CHECK(k.anims[BaCritRanged].any() && k.anims[BaDodgeRanged].any() && k.anims[BaMiss].any());
  BattleAnimId id;
  CHECK(battleAnimFromKey("critRanged", id) && id == BaCritRanged);
  CHECK(battleAnimFromKey("miss", id) && id == BaMiss);
  CHECK(std::string(battleAnimKey(BaStandRanged)) == "standRanged");

  // Which animation plays (the pure part of the scene).
  FighterTimes none;
  FighterTimes t;
  t.has[BaAttack] = t.has[BaCrit] = t.has[BaRanged] = true;
  CHECK_EQ(attackAnimFor(t, false, false, false, true), BaAttack);  // a whiff with no "miss": the attack
  CHECK_EQ(attackAnimFor(t, true, true, false), BaCrit);            // a ranged crit with no "critRanged": the crit
  t.has[BaMiss] = t.has[BaCritRanged] = true;
  CHECK_EQ(attackAnimFor(t, false, false, false, true), BaMiss);
  CHECK_EQ(attackAnimFor(t, false, true, false, true), BaMiss);     // a ranged whiff: also "miss"
  CHECK_EQ(attackAnimFor(t, true, true, false), BaCritRanged);
  CHECK_EQ(attackAnimFor(t, true, false, false), BaCrit);           // a melee crit does not use critRanged
  CHECK_EQ(attackAnimFor(t, false, false, false, false), BaAttack);
  CHECK_EQ(attackAnimFor(none, false, false, false, true), BaAttack);
}

TEST(battle_timeline_uses_miss_and_dodge_ranged) {
  FightInfo info;
  info.units[1] = {1, "A", 3, 20, 24, 2};
  info.units[2] = {2, "B", 5, 10, 30, 3};
  info.distance = 2;  // ranged
  Event e;
  e.type = EventType::Attack;
  e.a = 1;
  e.b = 2;
  e.flags = 0;  // a miss
  FightScript s = buildFightScript({e}, info);
  CHECK(s.valid);
  FighterTimes plain, extra;
  plain.has[BaAttack] = plain.has[BaDodge] = true;
  plain.total[BaAttack] = 400;
  plain.total[BaDodge] = 200;
  extra = plain;
  extra.has[BaMiss] = extra.has[BaDodgeRanged] = true;
  extra.total[BaMiss] = 350;
  extra.total[BaDodgeRanged] = 260;
  for (int a = 0; a < kBattleAnims; ++a) plain.spawnAt[a] = extra.spawnAt[a] = -1;
  FighterTimes with[2] = {extra, extra}, without[2] = {plain, plain};
  FightTimeline a = buildTimeline(s, with, {}), b = buildTimeline(s, without, {});
  const int atk = s.initiator, def = 1 - s.initiator;
  CHECK_EQ(a.poses[atk][0].anim, BaMiss);
  CHECK_EQ(a.poses[def][0].anim, BaDodgeRanged);
  CHECK_EQ(b.poses[atk][0].anim, BaAttack);  // no "miss": the attack
  CHECK_EQ(b.poses[def][0].anim, BaDodge);   // no "dodgeRanged": the dodge
}

// ---- walking frames --------------------------------------------------------------------------------------------------------------

TEST(walk_loads_directions_and_mirroring) {
  const std::string units =
      "{ \"knight\": { \"sheet\":\"men\",\"cell\":[0,0],\"walk\": { \"sheet\":\"walk\",\"down\":[[0,0],[1,0]],\"up\":[[0,1],[1,1]],"
      "\"side\":[[0,2],[1,2],[2,2]],\"frameMs\":90 },"
      "   \"sides\": { \"b\": { \"sheet\":\"men\",\"cell\":[1,0],\"walk\": { \"sheet\":\"men\",\"down\":[[1,0]],\"left\":[[0,0]],\"right\":[[1,0]] } } } },"
      "  \"archer\": { \"sheet\":\"men\",\"cell\":[1,0],\"walk\": { \"sheet\":\"walk\",\"left\":[[3,0]] } } }";
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame("walk", "{}", units), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const GraphicsSet* set = db.graphics.set(0);
  const UnitLook& k = set->units[(size_t)db.findUnitType("knight")];
  const WalkLook& w = k.base.walk;
  CHECK(w.any());
  CHECK_EQ(w.sheet, db.graphics.findSheet("walk"));
  CHECK_EQ(w.frameMs, 90);
  CHECK_EQ((int)w.frames[WalkDown].size(), 2);
  CHECK_EQ((int)w.frames[WalkUp].size(), 2);
  CHECK_EQ((int)w.frames[WalkRight].size(), 3);  // "side" faces right ...
  CHECK(!w.mirrored[WalkRight]);
  CHECK_EQ((int)w.frames[WalkLeft].size(), 3);   // ... and the left is the same frames turned round
  CHECK(w.mirrored[WalkLeft] && w.frames[WalkLeft][2].col == 2 && w.frames[WalkLeft][2].row == 2);
  CHECK_EQ(w.cell(WalkDown, 0).col, 0);
  CHECK_EQ(w.cell(WalkDown, 95).col, 1);
  CHECK_EQ(w.cell(WalkDown, 185).col, 0);  // loops
  // A side's look is a whole look of its own: its walk is its own (left and right both given: nothing is turned round).
  const WalkLook& s = k.forSide("b").walk;
  CHECK_EQ(s.sheet, db.graphics.findSheet("men"));
  CHECK(!s.mirrored[WalkLeft] && !s.mirrored[WalkRight]);
  CHECK_EQ(s.frames[WalkRight][0].col, 1);
  CHECK(s.frames[WalkUp].empty());
  CHECK(!k.forSide("a").walk.mirrored[WalkRight] && k.forSide("a").walk.any());  // an unlisted side: the base look
  // "left" alone: the right is it turned round.
  const WalkLook& a = set->units[(size_t)db.findUnitType("archer")].base.walk;
  CHECK(a.mirrored[WalkRight] && !a.mirrored[WalkLeft] && a.frames[WalkRight][0].col == 3);
}

TEST(walk_refusals) {
  auto unit = [&](const std::string& walk) { return "{ \"knight\": { \"sheet\":\"men\",\"cell\":[0,0],\"walk\": " + walk + " } }"; };
  expectRefused("w_empty", "{}", "needs at least one of", "walk", unit("{ \"sheet\":\"walk\" }"));
  expectRefused("w_key", "{}", "unknown key \"jump\"", "walk", unit("{ \"down\":[[0,0]],\"jump\":1 }"));
  expectRefused("w_sheet", "{}", "unknown sheet \"zip\"", "walk", unit("{ \"sheet\":\"zip\",\"down\":[[0,0]] }"));
  expectRefused("w_outside", "{}", "outside sheet \"walk\"", "walk", unit("{ \"sheet\":\"walk\",\"down\":[[9,0]] }"));
  expectRefused("w_own_sheet", "{}", "outside sheet \"men\"", "walk", unit("{ \"down\":[[5,0]] }"));  // no "sheet": the picture's own, 2 x 1 cells
  expectRefused("w_ms", "{}", "\"frameMs\" must be between 20 and 60000", "walk", unit("{ \"sheet\":\"walk\",\"down\":[[0,0]],\"frameMs\":5 }"));
  expectRefused("w_list", "{}", "\"down\" must list between 1 and 64 cells", "walk", unit("{ \"sheet\":\"walk\",\"down\":[] }"));
  expectRefused("w_shape", "{}", "must be an object", "walk", unit("[1]"));
  expectRefused("w_terrain", "{}", "only a unit picture", "walk",
                "{ }, \"terrain\": { \"plain\": { \"sheet\":\"men\",\"cell\":[0,0],\"walk\": { \"down\":[[0,0]] } } }");
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("w_ok", "{}", unit("{ \"sheet\":\"walk\",\"down\":[[0,0]] }")), db, err));
}

TEST(walk_direction_mapping_square_hex_iso) {
  WalkLook all;
  all.sheet = 0;
  for (int d = 0; d <= WalkLeft; ++d) all.frames[d].push_back(SpriteCell());  // (the four axis directions; the diagonals: test_iso_map_art.cpp)
  // Squares: the step on the screen is the step on the map (y grows downwards).
  CHECK_EQ(all.pick(0, 24), WalkDown);
  CHECK_EQ(all.pick(0, -24), WalkUp);
  CHECK_EQ(all.pick(24, 0), WalkRight);
  CHECK_EQ(all.pick(-24, 0), WalkLeft);
  // Hexagons with pointy tops: the six steps are east, west and the four diagonals (x +-0.5, y +-0.866): the diagonals are closer
  // to up / down than to the side.
  CHECK_EQ(all.pick(1, 0), WalkRight);
  CHECK_EQ(all.pick(-1, 0), WalkLeft);
  CHECK_EQ(all.pick(0.5f, 0.866f), WalkDown);
  CHECK_EQ(all.pick(-0.5f, 0.866f), WalkDown);
  CHECK_EQ(all.pick(0.5f, -0.866f), WalkUp);
  CHECK_EQ(all.pick(-0.5f, -0.866f), WalkUp);
  // Hexagons with flat tops: north, south and the four diagonals (x +-0.866, y +-0.5): the diagonals are closer to the side.
  CHECK_EQ(all.pick(0, 1), WalkDown);
  CHECK_EQ(all.pick(0, -1), WalkUp);
  CHECK_EQ(all.pick(0.866f, 0.5f), WalkRight);
  CHECK_EQ(all.pick(-0.866f, 0.5f), WalkLeft);
  CHECK_EQ(all.pick(0.866f, -0.5f), WalkRight);
  CHECK_EQ(all.pick(-0.866f, -0.5f), WalkLeft);
  // 2D isometric (a 2:1 diamond, a step of one square is half a tile across and half a tile down): the four steps go down-right,
  // down-left, up-left and up-right on the screen, nearer the side than the vertical.
  CHECK_EQ(all.pick(32, 16), WalkRight);
  CHECK_EQ(all.pick(-32, 16), WalkLeft);
  CHECK_EQ(all.pick(-32, -16), WalkLeft);
  CHECK_EQ(all.pick(32, -16), WalkRight);
  // An exact 45 degrees goes to down / up.
  CHECK_EQ(all.pick(10, 10), WalkDown);
  CHECK_EQ(all.pick(10, -10), WalkUp);
  // Only some directions given: the nearest of those. With only "side" (right, left): a vertical step is a tie, right wins.
  WalkLook sideOnly;
  sideOnly.sheet = 0;
  sideOnly.frames[WalkRight].push_back(SpriteCell());
  sideOnly.frames[WalkLeft].push_back(SpriteCell());
  CHECK_EQ(sideOnly.pick(-3, 1), WalkLeft);
  CHECK_EQ(sideOnly.pick(0, 5), WalkRight);
  WalkLook downOnly;
  downOnly.sheet = 0;
  downOnly.frames[WalkDown].push_back(SpriteCell());
  CHECK_EQ(downOnly.pick(5, -1), WalkDown);
  WalkLook nothing;
  CHECK_EQ(nothing.pick(1, 1), -1);
}
