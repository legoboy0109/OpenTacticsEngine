// The battle scene's art: data/graphics.json "battle" (docs/BATTLE_SCENE.md): loader, refusals, which set a fighter
// uses (character > weapon group > class > none), the game default and a map's own background.
#include <sys/stat.h>

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// Two unit types, one character, two weapons (groups blade and bow). Sheets: "fighters" 8 x 4 cells of 32 x 32 (256 x 128 px),
// "fx" 4 x 1 cells of 16 x 16, "bg" 2 x 1 cells of 120 x 68.
std::string makeGame(const char* name, const std::string& battleJson, const std::string& gameExtra = "", const std::string& mapExtra = "") {
  std::string root = ote_test::tempDir() + "/battleart_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"formulas\": {\"damage\":\"1\"}," + gameExtra +
                      " \"maps\": [{\"id\":\"m\",\"name\":\"M\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"},{\"id\":\"wood\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json",
                      "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"},{\"id\":\"longbow\",\"group\":\"bow\",\"range\":[2,2]}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"sword\",\"longbow\"]},"
                      "{\"id\":\"archer\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"longbow\"]}] }"));
  CHECK(writeTextFile(root + "/data/characters.json", "{ \"characters\": [{\"id\":\"eirik\",\"name\":\"Eirik\",\"type\":\"knight\"}] }"));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"name\": \"M\", \"grid\": {\"type\":\"square\",\"width\":2,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"..\"]," + mapExtra +
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
  png("fx.png", 64, 16);
  png("bg.png", 240, 68);
  CHECK(writeTextFile(root + "/data/graphics.json",
                      "{ \"sheets\": [ {\"id\":\"fighters\",\"file\":\"art/fighters.png\",\"cellWidth\":32,\"cellHeight\":32},"
                      "                {\"id\":\"fx\",\"file\":\"art/fx.png\",\"cellWidth\":16,\"cellHeight\":16},"
                      "                {\"id\":\"bg\",\"file\":\"art/bg.png\",\"cellWidth\":120,\"cellHeight\":68} ],"
                      " \"battle\": " + battleJson + " }"));
  return root;
}

const char* kSets =
    "\"sets\": {"
    "  \"knight\": { \"anchor\": [16, 30], \"animations\": {"
    "     \"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0],[1,0]],\"ms\":[300,200]},"
    "     \"attack\": {\"sheet\":\"fighters\",\"frames\":[[0,1],[1,1],[2,1],[3,1]],\"ms\":[100,100,80,150],\"hit\":2,\"spawn\":1},"
    "     \"hurt\": {\"sheet\":\"fighters\",\"frames\":[[0,2],[1,2]],\"ms\":90} } },"
    "  \"lancer\": { \"faces\": \"left\", \"mirror\": false, \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[4,0]]} } },"
    "  \"hero\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[5,0]]} } },"
    "  \"bowman\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[6,0]]} } } }";

void expectError(const char* name, const std::string& battle, const char* a, const char* b = "") {
  Database db;
  std::string err;
  bool ok = loadGame(makeGame(name, battle), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json") && has(err, a) && has(err, b));
}

}  // namespace

TEST(battle_art_loads_sets_and_beats) {
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("sets", std::string("{ ") + kSets + " }"), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleArt& a = db.graphics.battle;
  CHECK_EQ((int)a.sets.size(), 4);
  const BattleSet& k = a.sets[(size_t)a.findSet("knight")];
  CHECK_EQ(k.anchorX, 16);
  CHECK_EQ(k.anchorY, 30);
  CHECK(!k.facesLeft && k.mirror);
  CHECK(k.anims[BaStand].any() && k.anims[BaStand].loop);
  CHECK(k.anims[BaAttack].any() && !k.anims[BaAttack].loop);
  CHECK(!k.anims[BaCrit].any() && !k.anims[BaDefeat].any());
  CHECK_EQ(k.anims[BaAttack].hit, 2);
  CHECK_EQ(k.anims[BaAttack].spawn, 1);
  CHECK_EQ(k.anims[BaAttack].totalMs(), 430);
  CHECK_EQ(k.anims[BaAttack].startMs(2), 200);
  CHECK_EQ(k.anims[BaHurt].ms.size(), (size_t)2);  // one number = every frame
  CHECK_EQ(k.anims[BaHurt].ms[1], 90);
  CHECK_EQ(k.anims[BaStand].hitFrame(), 1);  // no "hit": the last frame
  // frame lookup: looping wraps, one-shot holds the last frame
  CHECK_EQ(k.anims[BaStand].frameAt(0), 0);
  CHECK_EQ(k.anims[BaStand].frameAt(350), 1);
  CHECK_EQ(k.anims[BaStand].frameAt(510), 0);
  CHECK_EQ(k.anims[BaAttack].frameAt(99999), 3);
  const BattleSet& l = a.sets[(size_t)a.findSet("lancer")];
  CHECK(l.facesLeft && !l.mirror);
  CHECK_EQ(l.anims[BaStand].ms[0], 120);  // default per-frame time
}

TEST(battle_art_resolution_order) {
  std::string body = std::string("{ ") + kSets +
                     ", \"assign\": ["
                     "  {\"set\":\"knight\",\"type\":\"knight\"},"
                     "  {\"set\":\"lancer\",\"type\":\"knight\",\"group\":\"blade\"},"
                     "  {\"set\":\"bowman\",\"group\":\"bow\"},"
                     "  {\"set\":\"hero\",\"character\":\"eirik\"} ] }";
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("resolve", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleArt& a = db.graphics.battle;
  auto name = [&](int character, int type, const char* group) {
    int s = a.resolveSet(character, type, group);
    return s < 0 ? std::string("-") : a.sets[(size_t)s].id;
  };
  const int knight = db.findUnitType("knight"), archer = db.findUnitType("archer"), eirik = db.findCharacter("eirik");
  CHECK(name(-1, knight, "") == "knight");         // class alone
  CHECK(name(-1, knight, "blade") == "lancer");    // class + weapon group beats the class
  CHECK(name(-1, knight, "bow") == "bowman");      // the group alone beats the class alone
  CHECK(name(eirik, knight, "blade") == "hero");   // a character beats a class + group
  CHECK(name(eirik, knight, "bow") == "hero");     // ... and a group alone
  CHECK(name(-1, archer, "") == "-");              // nothing fits: the map picture is used
  CHECK(name(-1, archer, "bow") == "bowman");
  CHECK(name(-1, archer, "blade") == "-");
}

TEST(battle_art_effects_and_backgrounds) {
  std::string body = std::string("{ ") + kSets +
                     ", \"effects\": [ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0],[1,0]],\"mode\":\"travel\",\"travelMs\":180},"
                     "                 {\"group\":\"blade\",\"sheet\":\"fx\",\"frames\":[[2,0]]} ],"
                     " \"backgrounds\": { \"meadow\": {\"sheet\":\"bg\",\"cell\":[0,0]}, \"wood\": {\"sheet\":\"bg\",\"cell\":[1,0]} },"
                     " \"terrainBackground\": { \"wood\": \"wood\" }, \"defaultBackground\": \"meadow\" }";
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("fx", body, "", " \"battleBackground\": \"wood\","), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const BattleArt& a = db.graphics.battle;
  CHECK_EQ((int)a.effects.size(), 2);
  const int bow = db.findWeapon("longbow"), sword = db.findWeapon("sword");
  CHECK_EQ(a.resolveEffect(bow, "bow"), 0);
  CHECK(a.effects[0].travel && a.effects[0].travelMs == 180);
  CHECK_EQ(a.resolveEffect(sword, "blade"), 1);  // by group
  CHECK(!a.effects[1].travel);
  CHECK_EQ(a.resolveEffect(-1, "none"), -1);
  CHECK_EQ(a.defaultBackground, a.findBackground("meadow"));
  CHECK_EQ(a.terrainBackground[(size_t)db.findTerrain("wood")], a.findBackground("wood"));
  CHECK_EQ(a.terrainBackground[(size_t)db.findTerrain("plain")], -1);
  MapDef m;
  CHECK(loadMap(db, "m", m, err));
  CHECK_EQ(m.battleBackground, a.findBackground("wood"));
}

TEST(battle_scene_game_default) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("def_none", "{}"), db, err));
  CHECK(!db.battleSceneDefault);  // existing games stay on the map
  CHECK(loadGame(makeGame("def_scene", "{}", " \"battleScene\": \"scene\","), db, err));
  CHECK(db.battleSceneDefault);
  CHECK(loadGame(makeGame("def_map", "{}", " \"battleScene\": \"map\","), db, err));
  CHECK(!db.battleSceneDefault);
  CHECK(!loadGame(makeGame("def_bad", "{}", " \"battleScene\": \"both\","), db, err));
  CHECK(has(err, "game.json") && has(err, "battleScene") && has(err, "\"both\""));
}

TEST(battle_art_refusals_are_readable) {
  expectError("b_type", "[]", "\"battle\" must be an object");
  expectError("b_noStand", "{ \"sets\": {\"s\": {\"animations\": {\"attack\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}}}} }", "battle set \"s\"", "\"stand\"");
  expectError("b_noAnims", "{ \"sets\": {\"s\": {}} }", "battle set \"s\"", "animations");
  expectError("b_anim", "{ \"sets\": {\"s\": {\"animations\": {\"jump\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}}}} }", "unknown animation \"jump\"");
  expectError("b_sheet", "{ \"sets\": {\"s\": {\"animations\": {\"stand\": {\"sheet\":\"nope\",\"frames\":[[0,0]]}}}} }", "unknown sheet \"nope\"");
  expectError("b_outside", "{ \"sets\": {\"s\": {\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0],[8,0]]}}}} }", "frame [8, 0] is outside sheet \"fighters\"");
  expectError("b_empty", "{ \"sets\": {\"s\": {\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[]}}}} }", "between 1 and 512");
  expectError("b_msLen", "{ \"sets\": {\"s\": {\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0],[1,0]],\"ms\":[100]}}}} }", "\"ms\" lists 1 times");
  expectError("b_msRange", "{ \"sets\": {\"s\": {\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]],\"ms\":5}}}} }", "\"ms\" must be between 10 and 10000");
  expectError("b_hit", "{ \"sets\": {\"s\": {\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]},"
                      "\"attack\": {\"sheet\":\"fighters\",\"frames\":[[0,1],[1,1]],\"hit\":2}}}} }",
              "\"hit\" is frame 2", "frames 0 to 1");
  expectError("b_faces", "{ \"sets\": {\"s\": {\"faces\":\"up\",\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}}}} }", "\"faces\" must be");
  expectError("b_anchor", "{ \"sets\": {\"s\": {\"anchor\":[40,10],\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}}}} }", "\"anchor\"", "outside the 32 x 32 frame");
  expectError("b_anchorShape", "{ \"sets\": {\"s\": {\"anchor\":[4],\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}}}} }", "\"anchor\" must be [x, y]");
  const std::string sets = std::string("\"sets\": {\"knight\": {\"animations\": {\"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]}}}}");
  expectError("a_set", "{ " + sets + ", \"assign\": [{\"set\":\"zzz\",\"type\":\"knight\"}] }", "battle assign 1", "no battle set \"zzz\"");
  expectError("a_type", "{ " + sets + ", \"assign\": [{\"set\":\"knight\",\"type\":\"dragon\"}] }", "no unit type with the id \"dragon\"");
  expectError("a_char", "{ " + sets + ", \"assign\": [{\"set\":\"knight\",\"character\":\"nobody\"}] }", "no character with the id \"nobody\"");
  expectError("a_group", "{ " + sets + ", \"assign\": [{\"set\":\"knight\",\"group\":\"wand\"}] }", "no weapon has the group \"wand\"");
  expectError("a_who", "{ " + sets + ", \"assign\": [{\"set\":\"knight\"}] }", "at least one of");
  expectError("a_dup", "{ " + sets + ", \"assign\": [{\"set\":\"knight\",\"type\":\"knight\"},{\"set\":\"knight\",\"type\":\"knight\"}] }", "already given");
  expectError("f_none", "{ \"effects\": [{\"sheet\":\"fx\",\"frames\":[[0,0]]}] }", "battle effect 1", "exactly one of");
  expectError("f_weapon", "{ \"effects\": [{\"weapon\":\"spear\",\"sheet\":\"fx\",\"frames\":[[0,0]]}] }", "no weapon with the id \"spear\"");
  expectError("f_mode", "{ \"effects\": [{\"weapon\":\"sword\",\"mode\":\"fly\",\"sheet\":\"fx\",\"frames\":[[0,0]]}] }", "\"mode\" must be");
  expectError("g_cell", "{ \"backgrounds\": {\"x\": {\"sheet\":\"bg\",\"cell\":[2,0]}} }", "battle background \"x\"", "outside sheet \"bg\"");
  expectError("g_terrain", "{ \"backgrounds\": {\"x\": {\"sheet\":\"bg\",\"cell\":[0,0]}}, \"terrainBackground\": {\"lava\":\"x\"} }", "no terrain with this id");
  expectError("g_ref", "{ \"backgrounds\": {\"x\": {\"sheet\":\"bg\",\"cell\":[0,0]}}, \"terrainBackground\": {\"wood\":\"y\"} }", "no background \"y\"");
  expectError("g_default", "{ \"backgrounds\": {\"x\": {\"sheet\":\"bg\",\"cell\":[0,0]}}, \"defaultBackground\": \"y\" }", "\"defaultBackground\" names unknown background \"y\"");
}

TEST(battle_art_map_background_refusal_and_no_art_games) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("mapbad", "{ \"backgrounds\": {\"x\": {\"sheet\":\"bg\",\"cell\":[0,0]}} }"), db, err));
  MapDef m;
  CHECK(loadMap(db, "m", m, err));
  CHECK_EQ(m.battleBackground, -1);  // no key: by terrain
  CHECK(loadGame(makeGame("mapbad2", "{ \"backgrounds\": {\"x\": {\"sheet\":\"bg\",\"cell\":[0,0]}} }", "", " \"battleBackground\": \"nope\","), db, err));
  CHECK(!loadMap(db, "m", m, err));
  CHECK(has(err, "m.json") && has(err, "\"battleBackground\" names unknown background \"nope\""));
  // A game without a "battle" key has no battle art at all.
  CHECK(loadGame(makeGame("nobattle", "{}"), db, err));
  CHECK(!db.graphics.battle.any());
}
