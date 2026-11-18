// The third batch of battle scene keys (docs/BATTLE_SCENE.md): the loader and its refusals for every key (game.json "battleSceneStaves", an effect's
// "blend", "hide", "fade", "order", "speed", a staff item as the effect's "weapon", the stage's "ranges"), the stage's range bands as pure geometry,
// and the timeline's windows, flight time and camera.
#include <sys/stat.h>

#include <cmath>
#include <cstdio>

#include "battle/fight_timeline.h"
#include "core/base/json.h"
#include "core/data/battle_stage.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

const char* kStage = "{ \"width\": 240, \"height\": 160, \"feet\": {\"left\":[88,104],\"right\":[152,104]}, \"feetFar\": {\"left\":[56,104],\"right\":[184,104]} }";

// Unit types knight and archer, weapons sword (group blade) and longbow (group bow), a staff item "mend" (group staff) and a weapon item. Sheets "fx" (16 x 16
// cells) and "bg" (240 x 68). `effectsJson`: the "effects" list; `stageJson`: the "stage" object ("" = none); `gameExtra`: more game.json keys.
std::string makeGame(const char* name, const std::string& effectsJson, const std::string& stageJson = std::string(), const std::string& gameExtra = std::string()) {
  std::string root = ote_test::tempDir() + "/scene3_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"},{\"id\":\"mag\"}]," + gameExtra +
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}], \"rules\": {\"items\": {\"weapons\": \"carried\"}},"
                      " \"formulas\": {\"damage\":\"1\"},"
                      " \"maps\": [{\"id\":\"m\",\"name\":\"M\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json",
                      "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"},{\"id\":\"longbow\",\"group\":\"bow\",\"range\":[2,2]}] }"));
  CHECK(writeTextFile(root + "/data/items.json",
                      "{ \"items\": [ { \"id\": \"mend\", \"name\": \"Mend Staff\", \"kind\": \"staff\", \"group\": \"staff\", \"uses\": 3, \"range\": 2, \"target\": \"ally\","
                      "                 \"effects\": [ { \"heal\": 5 } ] },"
                      "               { \"id\": \"sword_item\", \"name\": \"Sword\", \"kind\": \"weapon\", \"weapon\": \"sword\" } ] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3,\"mag\":1},\"weapons\":[\"sword\",\"longbow\"]},"
                      "{\"id\":\"archer\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3,\"mag\":1},\"weapons\":[\"longbow\"]}] }"));
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
  png("fighters.png", 64, 32);
  png("fx.png", 64, 16);
  png("bg.png", 240, 68);
  CHECK(writeTextFile(root + "/data/graphics.json",
                      "{ \"sheets\": [ {\"id\":\"fighters\",\"file\":\"art/fighters.png\",\"cellWidth\":32,\"cellHeight\":32},"
                      "                {\"id\":\"fx\",\"file\":\"art/fx.png\",\"cellWidth\":16,\"cellHeight\":16},"
                      "                {\"id\":\"bg\",\"file\":\"art/bg.png\",\"cellWidth\":240,\"cellHeight\":68} ],"
                      " \"battle\": { \"sets\": { \"knight\": { \"animations\": { \"stand\": {\"sheet\":\"fighters\",\"frames\":[[0,0]]} } } },"
                      "   \"effects\": " + effectsJson + (stageJson.empty() ? std::string() : ", \"stage\": " + stageJson) + " } }"));
  return root;
}

// One target effect on the sword with the given extra keys; a screen effect; a travel effect.
std::string one(const std::string& extra) { return "[ {\"weapon\":\"sword\",\"sheet\":\"fx\",\"frames\":[[0,0],[1,0]]," + extra + "} ]"; }
std::string screen(const std::string& extra) { return "[ {\"weapon\":\"sword\",\"sheet\":\"bg\",\"frames\":[[0,0]],\"layer\":\"screen\"," + extra + "} ]"; }
std::string travel(const std::string& extra) { return "[ {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"mode\":\"travel\"," + extra + "} ]"; }

void expectError(const char* name, const std::string& effects, const char* a, const char* b = "", const std::string& stage = std::string(kStage)) {
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame(name, effects, stage), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json") && has(err, a) && has(err, b));
}

void expectStageError(const char* name, const std::string& stage, const char* a, const char* b = "") { expectError(name, "[]", a, b, stage); }

bool load(const char* name, const std::string& effects, Database& db, const std::string& stage = std::string(kStage), const std::string& gameExtra = std::string()) {
  std::string err;
  const bool ok = loadGame(makeGame(name, effects, stage, gameExtra), db, err);
  if (!ok) std::printf("  [%s] %s\n", name, err.c_str());
  CHECK(ok);
  return ok;
}

// A stage with range bands: one that shows both fighters from 3 squares, one that pans from 5.
std::string bandStage(const std::string& ranges) {
  return "{ \"width\": 240, \"height\": 160, \"feet\": {\"left\":[88,104],\"right\":[152,104]}, \"feetFar\": {\"left\":[56,104],\"right\":[184,104]},"
         " \"platform\": {\"left\":[56,96],\"right\":[120,96]}, \"ranges\": " + ranges + " }";
}
const char* kBands =
    "[ { \"from\": 5, \"feet\": {\"left\":[40,100],\"right\":[400,100]}, \"platform\": {\"left\":[8,92],\"right\":[368,92]}, \"view\": \"each\","
    "    \"pan\": { \"panMs\": 250, \"leftView\": 0, \"rightView\": 200 } },"
    "  { \"from\": 3, \"feet\": {\"left\":[30,110],\"right\":[210,110]} } ]";

Event ev(EventType t, int a, int b = -1, int c = -1, int d = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  return e;
}
FightInfo info(int dist = 1) {
  FightInfo f;
  f.units[1] = {1, "Eirik", 3, 20, 24, 2, true};
  f.units[2] = {2, "Gorm", 5, 10, 30, 3, true};
  f.distance = dist;
  return f;
}
FighterTimes animated() {
  FighterTimes t;
  t.has[BaAttack] = t.has[BaHurt] = t.has[BaDefeat] = t.has[BaDodge] = true;
  t.total[BaAttack] = 430;
  t.hitAt[BaAttack] = 200;
  t.total[BaHurt] = 180;
  t.total[BaDefeat] = 500;
  t.total[BaDodge] = 250;
  for (int& v : t.spawnAt) v = -1;
  return t;
}
Event attack(int a, int b, int flags, int damage) {
  Event e = ev(EventType::Attack, a, b, 0, damage);
  e.flags = flags;
  return e;
}
const FightBeat* beat(const FightTimeline& tl, BeatKind k, int strike = -1) {
  for (const FightBeat& b : tl.beats)
    if (b.kind == k && (strike < 0 || b.strike == strike)) return &b;
  return nullptr;
}

}  // namespace

// ---- game.json and the effect keys ----

TEST(scene3_battle_scene_staves_setting) {
  Database db;
  if (!load("staves_default", "[]", db)) return;
  CHECK(!db.battleSceneStaves);  // off by default: a staff stays on the map
  Database on;
  if (!load("staves_on", "[]", on, kStage, "\"battleSceneStaves\": true,")) return;
  CHECK(on.battleSceneStaves);
  Database bad;
  std::string err;
  CHECK(!loadGame(makeGame("staves_bad", "[]", kStage, "\"battleSceneStaves\": \"yes\","), bad, err));
  CHECK(has(err, "battleSceneStaves") && has(err, "true or false"));
}

TEST(scene3_staff_item_effects) {
  // A staff item is named in "weapon" like a weapon; its entry beats the entry of its group.
  Database db;
  if (!load("staff_fx", "[ {\"weapon\":\"mend\",\"sheet\":\"fx\",\"frames\":[[0,0]]}, {\"group\":\"staff\",\"sheet\":\"fx\",\"frames\":[[1,0]]} ]", db)) return;
  const BattleArt& a = db.graphics.battle;
  const int mend = db.findItem("mend"), sword = db.findWeapon("sword");
  CHECK(a.effects[0].item == mend && a.effects[0].weapon < 0 && a.effects[0].group.empty());
  CHECK_EQ(a.resolveEffect(-1, "staff", mend), 0);   // the item's own entry
  CHECK_EQ(a.resolveEffect(-1, "staff", -1), 1);     // the group's, for another staff
  CHECK_EQ(a.resolveEffect(sword, "blade", mend), 0);
  CHECK(a.resolveEffects(-1, "staff", -1, mend) == std::vector<int>({0}));
  // A second entry for the same staff item is refused; an unknown id is refused as for a weapon; a weapon item's id is not a staff.
  expectError("staff_dup", "[ {\"weapon\":\"mend\",\"sheet\":\"fx\",\"frames\":[[0,0]]}, {\"weapon\":\"mend\",\"sheet\":\"fx\",\"frames\":[[1,0]]} ]", "a second effect for \"mend\"");
  expectError("staff_unknown", "[ {\"weapon\":\"nothing\",\"sheet\":\"fx\",\"frames\":[[0,0]]} ]", "there is no weapon with the id \"nothing\"");
  expectError("staff_notstaff", "[ {\"weapon\":\"sword_item\",\"sheet\":\"fx\",\"frames\":[[0,0]]} ]", "there is no weapon with the id \"sword_item\"");
}

TEST(scene3_blend_key) {
  Database db;
  if (!load("blend", "[ {\"weapon\":\"sword\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"blend\":\"add\"}, {\"weapon\":\"longbow\",\"sheet\":\"fx\",\"frames\":[[0,0]],\"blend\":\"normal\"} ]", db)) return;
  CHECK(db.graphics.battle.effects[0].additive && db.graphics.battle.effects[0].extended);
  CHECK(!db.graphics.battle.effects[1].additive);
  Database plain;
  if (!load("blend_plain", one("\"mode\":\"target\""), plain)) return;
  CHECK(!plain.graphics.battle.effects[0].additive && !plain.graphics.battle.effects[0].extended);  // an effect without it plays and logs as before
  expectError("blend1", one("\"blend\":\"multiply\""), "\"blend\" must be \"normal\"", "\"add\"");
  expectError("blend2", one("\"blend\":true"), "\"blend\"", "a string");
}

TEST(scene3_hide_and_fade_keys) {
  Database db;
  const std::string fx = one("\"hide\":[{\"atMs\":0,\"ms\":1500,\"what\":[\"backdrop\",\"platforms\"],\"color\":\"#102030\"},{\"atMs\":200,\"ms\":50,\"what\":[\"platforms\"]}],"
                             "\"fade\":[{\"atMs\":0,\"ms\":300,\"to\":\"#ff0000\",\"alpha\":0.5,\"holdMs\":400,\"over\":\"backdrop\"},{\"atMs\":10,\"ms\":20}]");
  if (!load("hidefade", fx, db)) return;
  const BattleEffect& e = db.graphics.battle.effects[0];
  CHECK(e.extended && e.hide.size() == 2 && e.fade.size() == 2);
  CHECK(e.hide[0].what == (kHideBackdrop | kHidePlatforms) && e.hide[0].hasColor && e.hide[0].rgb == 0x102030 && e.hide[0].atMs == 0 && e.hide[0].ms == 1500);
  CHECK(e.hide[1].what == kHidePlatforms && !e.hide[1].hasColor);
  CHECK(e.fade[0].rgb == 0xff0000 && e.fade[0].alpha == 128 && e.fade[0].holdMs == 400 && e.fade[0].backdropOnly && e.fade[0].ms == 300);
  CHECK(e.fade[1].rgb == 0 && e.fade[1].alpha == 255 && e.fade[1].holdMs == 0 && !e.fade[1].backdropOnly);  // the defaults: black, opaque, over everything
  expectError("hide1", one("\"hide\":{}"), "\"hide\" must be a list");
  expectError("hide2", one("\"hide\":[3]"), "hide 1", "must be");
  expectError("hide3", one("\"hide\":[{\"atMs\":0,\"ms\":10}]"), "hide 1", "\"what\"");
  expectError("hide4", one("\"hide\":[{\"atMs\":0,\"ms\":10,\"what\":[]}]"), "\"what\" must list");
  expectError("hide5", one("\"hide\":[{\"atMs\":0,\"ms\":10,\"what\":[\"fighters\"]}]"), "\"what\" can name \"backdrop\" and \"platforms\" only");
  expectError("hide6", one("\"hide\":[{\"atMs\":0,\"ms\":10,\"what\":[\"backdrop\",\"backdrop\"]}]"), "names \"backdrop\" twice");
  expectError("hide7", one("\"hide\":[{\"atMs\":-1,\"ms\":10,\"what\":[\"backdrop\"]}]"), "\"atMs\" must be between 0 and 60000");
  expectError("hide8", one("\"hide\":[{\"atMs\":0,\"ms\":5,\"what\":[\"backdrop\"]}]"), "\"ms\" must be between 10 and 60000");
  expectError("hide9", one("\"hide\":[{\"atMs\":0,\"ms\":10,\"what\":[\"backdrop\"],\"color\":\"red\"}]"), "\"color\" must be a colour");
  expectError("hide10", one("\"hide\":[{\"atMs\":0,\"ms\":10,\"what\":[\"backdrop\"],\"speed\":2}]"), "unknown key \"speed\"");
  expectError("hide11", one("\"hide\":[{\"ms\":10,\"what\":[\"backdrop\"]}]"), "atMs");
  expectError("fade1", one("\"fade\":\"black\""), "\"fade\" must be a list");
  expectError("fade2", one("\"fade\":[{\"atMs\":0,\"ms\":9}]"), "\"ms\" must be between 10 and 5000");
  expectError("fade3", one("\"fade\":[{\"atMs\":0,\"ms\":50,\"alpha\":0}]"), "\"alpha\" must be above 0 and at most 1");
  expectError("fade4", one("\"fade\":[{\"atMs\":0,\"ms\":50,\"alpha\":1.5}]"), "\"alpha\" must be above 0 and at most 1");
  expectError("fade5", one("\"fade\":[{\"atMs\":0,\"ms\":50,\"over\":\"fighters\"}]"), "\"over\" must be \"all\"");
  expectError("fade6", one("\"fade\":[{\"atMs\":0,\"ms\":50,\"to\":\"#12\"}]"), "\"to\" must be a colour");
  expectError("fade7", one("\"fade\":[{\"atMs\":0,\"ms\":50,\"holdMs\":-1}]"), "\"holdMs\" must be between 0 and 60000");
  expectError("fade8", one("\"fade\":[{\"atMs\":0,\"ms\":50,\"hold\":1}]"), "unknown key \"hold\"");
  expectError("fade9", one("\"fade\":[{\"atMs\":0,\"ms\":50,\"alpha\":\"half\"}]"), "alpha", "a number");
}

TEST(scene3_order_key) {
  Database db;
  if (!load("order", "[ {\"weapon\":\"sword\",\"sheet\":\"bg\",\"frames\":[[0,0]],\"layer\":\"screen\",\"order\":\"front\"},"
                     "   {\"weapon\":\"longbow\",\"sheet\":\"bg\",\"frames\":[[0,0]],\"layer\":\"screen\",\"order\":\"behind\"},"
                     "   {\"group\":\"staff\",\"sheet\":\"bg\",\"frames\":[[0,0]],\"layer\":\"screen\"} ]", db)) return;
  const BattleArt& a = db.graphics.battle;
  CHECK(a.effects[0].order == FxOrder::Front && a.effects[1].order == FxOrder::Behind && a.effects[2].order == FxOrder::Backdrop);
  expectError("order1", screen("\"order\":\"top\""), "\"order\" must be \"backdrop\"");
  expectError("order2", one("\"order\":\"front\""), "\"order\" is only for \"layer\": \"screen\"");
  expectError("order3", one("\"layer\":\"behind\",\"order\":\"front\""), "\"order\" is only for \"layer\": \"screen\"");
}

TEST(scene3_speed_key) {
  Database db;
  if (!load("speed", travel("\"speed\":240"), db)) return;
  CHECK(db.graphics.battle.effects[0].speed == 240 && db.graphics.battle.effects[0].extended);
  expectError("speed1", travel("\"speed\":0"), "\"speed\" must be between 1 and 10000");
  expectError("speed2", travel("\"speed\":20000"), "\"speed\" must be between 1 and 10000");
  expectError("speed3", travel("\"speed\":240,\"travelMs\":300"), "\"travelMs\" or a \"speed\", not both");
  expectError("speed4", one("\"speed\":240"), "\"speed\" is only for a \"travel\" effect");
  expectError("speed5", travel("\"speed\":2.5"), "\"speed\"", "whole number");
  expectError("speed6", travel("\"speed\":240"), "\"speed\" is in stage pixels", "fixed \"stage\"", "");  // no stage: refused (use travelMs)
}

// ---- range bands of the stage ----

TEST(scene3_stage_ranges_load) {
  Database db;
  if (!load("ranges", "[]", db, bandStage(kBands))) return;
  const BattleStage& st = db.graphics.battle.stage;
  CHECK_EQ((int)st.ranges.size(), 2);
  CHECK_EQ(st.ranges[0].from, 3);  // sorted by "from"
  CHECK(!st.ranges[0].each && !st.ranges[0].hasPlatform && st.ranges[0].feet[0][0] == 30 && st.ranges[0].feet[1][1] == 110);
  CHECK_EQ(st.ranges[1].from, 5);
  const StageBand& b = st.ranges[1];
  CHECK(b.each && b.hasPlatform && b.panMs == 250 && b.leftView == 0 && b.rightView == 200 && b.feet[1][0] == 400 && b.platform[1][0] == 368);
  // Without "ranges" nothing changes.
  Database plain;
  if (!load("ranges_plain", "[]", plain)) return;
  CHECK(plain.graphics.battle.stage.ranges.empty());
}

TEST(scene3_stage_ranges_refusals) {
  const std::string feet = "\"feet\": {\"left\":[30,110],\"right\":[210,110]}";
  expectStageError("rg1", bandStage("[]"), "\"ranges\" must be a list of 1 to 16");
  expectStageError("rg2", bandStage("{}"), "\"ranges\" must be a list");
  expectStageError("rg3", bandStage("[ 3 ]"), "range 1", "must be an object");
  expectStageError("rg4", bandStage("[ { \"from\": 1, " + feet + " } ]"), "\"from\" must be between 2 and 99");
  expectStageError("rg5", bandStage("[ { " + feet + " } ]"), "from");
  expectStageError("rg6", bandStage("[ { \"from\": 3, " + feet + " }, { \"from\": 3, " + feet + " } ]"), "a second band starts at 3");
  expectStageError("rg7", bandStage("[ { \"from\": 3 } ]"), "needs \"feet\"");
  expectStageError("rg8", bandStage("[ { \"from\": 3, \"feet\": {\"left\":[30,110],\"right\":[260,110]} } ]"), "\"right\" [260, 110] is outside the stage (240 x 160)");
  expectStageError("rg9", bandStage("[ { \"from\": 3, " + feet + ", \"view\": \"one\" } ]"), "\"view\" must be \"both\"");
  expectStageError("rg10", bandStage("[ { \"from\": 3, " + feet + ", \"view\": \"each\" } ]"), "needs a \"pan\"");
  expectStageError("rg11", bandStage("[ { \"from\": 3, " + feet + ", \"pan\": { \"leftView\": 0, \"rightView\": 0 } } ]"), "\"pan\" belongs to \"view\": \"each\"");
  expectStageError("rg12", bandStage("[ { \"from\": 3, " + feet + ", \"speed\": 1 } ]"), "unknown key \"speed\"");
  const std::string wide = "\"feet\": {\"left\":[40,100],\"right\":[400,100]}, \"view\": \"each\", ";
  expectStageError("rg13", bandStage("[ { \"from\": 3, " + wide + "\"pan\": { \"leftView\": 0 } } ]"), "pan", "rightView");
  expectStageError("rg14", bandStage("[ { \"from\": 3, " + wide + "\"pan\": { \"panMs\": 6000, \"leftView\": 0, \"rightView\": 200 } } ]"), "\"panMs\" must be between 0 and 5000");
  expectStageError("rg15", bandStage("[ { \"from\": 3, " + wide + "\"pan\": { \"leftView\": 0, \"rightView\": 9000 } } ]"), "\"leftView\" and \"rightView\" must be between 0 and 8191");
  expectStageError("rg16", bandStage("[ { \"from\": 3, " + wide + "\"pan\": { \"leftView\": 0, \"rightView\": 100 } } ]"), "right fighter's foot point x 400 is not inside the picture", "\"rightView\" 100");
  expectStageError("rg17", bandStage("[ { \"from\": 3, " + wide + "\"pan\": { \"leftView\": 100, \"rightView\": 200 } } ]"), "left fighter's foot point x 40 is not inside the picture");
  expectStageError("rg18", bandStage("[ { \"from\": 3, " + wide + "\"pan\": { \"leftView\": 0, \"rightView\": 200, \"zoom\": 2 } } ]"), "unknown key \"zoom\"");
  expectStageError("rg19", bandStage("[ { \"from\": 3, \"feet\": {\"left\":[40,100],\"right\":[9000,100]}, \"view\": \"each\", \"pan\": { \"leftView\": 0, \"rightView\": 200 } } ]"), "is outside the stage");
}

TEST(scene3_stage_layout_by_distance) {
  Database db;
  if (!load("layout", "[]", db, bandStage(kBands))) return;
  const BattleStage& st = db.graphics.battle.stage;
  // Range 1: feet; 2: feetFar; 3 and 4: the band from 3; 5 and more: the panning band.
  StageLayout l1 = stageLayoutFor(st, 1), l2 = stageLayoutFor(st, 2), l3 = stageLayoutFor(st, 3), l4 = stageLayoutFor(st, 4), l5 = stageLayoutFor(st, 5), l9 = stageLayoutFor(st, 9);
  CHECK(l1.band == -1 && l1.feet[0][0] == 88 && l1.feet[1][0] == 152 && !l1.each);
  CHECK(l2.band == -1 && l2.feet[0][0] == 56 && l2.feet[1][0] == 184);
  CHECK(l3.band == 0 && l3.feet[0][0] == 30 && l3.feet[1][0] == 210 && !l3.each);
  CHECK(l4.band == 0 && l4.feet[0][0] == 30);
  CHECK(l5.band == 1 && l5.each && l5.feet[1][0] == 400 && l5.panMs == 250 && l5.hasPlatform && l5.platform[1][0] == 368);
  CHECK(l9.band == 1);
  CHECK(!l3.hasPlatform);  // a band without "platform": the picture is centred on its foot point (not the base layout's points)
  CHECK(l1.hasPlatform && l1.platform[0][0] == 56);
  CHECK(l5.viewX(0) == 0 && l5.viewX(1) == 200 && l5.viewX(0.5) == 100);
}

TEST(scene3_stage_geometry_of_a_band_and_its_camera) {
  Database db;
  if (!load("geo", "[]", db, bandStage(kBands))) return;
  const BattleStage& st = db.graphics.battle.stage;
  const int pw[2] = {64, 64}, ph[2] = {32, 32};
  // 480 x 270, the boxes end at row 46 and the hint starts at row 257: scale 2 (240 * 2 = 480); the stage is taller than the area.
  const StageGeo base = stageGeometryAt(480, 270, 46, 257, st, 2, pw, ph);
  const StageGeo old = stageGeometry(480, 270, 46, 257, st, true, pw, ph);
  CHECK(base.valid && base.scale == 2 && base.x == 0);
  CHECK(base.feet[0][0] == old.feet[0][0] && base.feet[1][1] == old.feet[1][1] && base.y == old.y);  // the old call is distance 2
  // The band from 3 shows both fighters: its own feet, scaled; the platform is centred on each foot point.
  const StageGeo b3 = stageGeometryAt(480, 270, 46, 257, st, 3, pw, ph);
  CHECK(b3.feet[0][0] == 60 && b3.feet[1][0] == 420);
  CHECK(b3.platform[0][0] == b3.feet[0][0] - 64 && b3.platform[0][1] == b3.feet[0][1] - 32);
  // The panning band: the camera on the left fighter (view 0) shows it at x 80 and the right fighter far off the picture; on the right (view 200) the other way.
  const StageGeo left = stageGeometryAt(480, 270, 46, 257, st, 6, pw, ph, 0), right = stageGeometryAt(480, 270, 46, 257, st, 6, pw, ph, 200);
  CHECK(left.feet[0][0] == 80 && left.feet[1][0] == 800);
  CHECK(right.feet[0][0] == -320 && right.feet[1][0] == 400);
  CHECK(left.platform[0][0] == 16 && right.platform[1][0] == 336);  // platforms move with the picture
  CHECK(left.scale == right.scale && left.y == right.y && left.feet[0][1] == right.feet[0][1]);  // the camera only pans sideways
  // Half way (view 100): both feet are halfway.
  const StageGeo mid = stageGeometryAt(480, 270, 46, 257, st, 6, pw, ph, 100);
  CHECK(mid.feet[0][0] == -120 && mid.feet[1][0] == 600);
  // The old stageGeometry without any "ranges" is unchanged for a plain stage.
  BattleStage plain = st;
  plain.ranges.clear();
  CHECK(stageGeometryAt(480, 270, 46, 257, plain, 4, pw, ph).feet[0][0] == stageGeometry(480, 270, 46, 257, plain, true, pw, ph).feet[0][0]);
}

// ---- the timeline: windows, the flight time, the camera ----

TEST(scene3_timeline_hide_fade_windows) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 4)}, info());
  FighterTimes times[2] = {animated(), animated()};
  EffectTimes e;
  e.has = true;
  e.total = 100;
  e.hitAtMs = 100;  // starts 100 ms before the hit
  e.extended = true;
  FxHide h1;
  h1.atMs = 50;
  h1.ms = 500;
  h1.what = kHideBackdrop | kHidePlatforms;
  h1.hasColor = true;
  h1.rgb = 0x102030;
  FxHide h2;
  h2.atMs = 100;
  h2.ms = 100;
  h2.what = kHideBackdrop;  // a later window without a colour
  e.hide = {h1, h2};
  FxFade f;
  f.atMs = 0;
  f.ms = 200;
  f.holdMs = 300;
  f.alpha = 204;
  f.rgb = 0x000000;
  e.fade = {f};
  FightTimeline tl = buildTimeline(s, times, {e});
  CHECK(tl.fx.size() == 1 && tl.hides.size() == 2 && tl.fades.size() == 1);
  const double t0 = tl.fx[0].t0;
  CHECK(std::fabs(tl.hides[0].t0 - (t0 + 0.05)) < 1e-9 && std::fabs(tl.hides[0].dur - 0.5) < 1e-9);
  uint32_t rgb = 7;
  CHECK_EQ((int)tl.hiddenAt(t0, rgb), 0);                                    // before the window
  CHECK_EQ((int)tl.hiddenAt(t0 + 0.06, rgb), (int)(kHideBackdrop | kHidePlatforms));
  CHECK_EQ((int)rgb, 0x102030);
  tl.hiddenAt(t0 + 0.15, rgb);                                               // two windows: the one that began last and gives a colour decides
  CHECK_EQ((int)rgb, 0x102030);                                              // (the later one gives none)
  CHECK_EQ((int)tl.hiddenAt(t0 + 0.56, rgb), 0);                             // after both
  // The fade: in over 200 ms, held 300, out over 200.
  const FightFadeWin& fw = tl.fades[0];
  CHECK(std::fabs(fw.alpha - 0.8) < 1e-9 && !fw.backdropOnly);
  CHECK(fw.opacityAt(t0 - 0.1) == 0);
  CHECK(std::fabs(fw.opacityAt(t0 + 0.1) - 0.4) < 1e-9);   // half way in
  CHECK(std::fabs(fw.opacityAt(t0 + 0.3) - 0.8) < 1e-9);   // held
  CHECK(std::fabs(fw.opacityAt(t0 + 0.6) - 0.4) < 1e-9);   // half way out
  CHECK(fw.opacityAt(t0 + 0.8) == 0);
}

TEST(scene3_timeline_flight_beat_for_speed) {
  FightScript s = buildFightScript({attack(1, 2, kHit, 4)}, info(2));
  FighterTimes times[2] = {animated(), animated()};
  EffectTimes e;
  e.has = true;
  e.travel = true;
  e.travelMs = 640;  // worked out by the scene from the distance and "speed"
  e.bySpeed = true;
  e.extended = true;
  e.name = "bolt";
  e.total = 100;
  FightTimeline tl = buildTimeline(s, times, {e});
  const FightBeat* fl = beat(tl, BeatKind::FxFlight);
  CHECK(fl && fl->value == 640 && fl->label == "bolt");
  CHECK(tl.fx.size() == 1 && tl.fx[0].travelMs == 640);
  EffectTimes plain = e;
  plain.bySpeed = false;
  CHECK(beat(buildTimeline(s, times, {plain}), BeatKind::FxFlight) == nullptr);  // a plain travelMs logs no flight line
  // The hit lands when the shot arrives: a longer flight, a later hit.
  EffectTimes far = e;
  far.travelMs = 900;
  FightTimeline tl2 = buildTimeline(s, times, {far});
  CHECK(std::fabs((beat(tl2, BeatKind::Hit)->t - beat(tl, BeatKind::Hit)->t) - 0.26) < 1e-9);
}

TEST(scene3_timeline_camera) {
  // Eirik strikes Gorm twice (a double attack), then Gorm counters: the camera follows the actor and moves to the target for each hit.
  FightScript s = buildFightScript({attack(1, 2, kHit, 3), attack(1, 2, kHit, 3), attack(2, 1, kHit | kCounter, 3)}, info(5));
  FighterTimes times[2] = {animated(), animated()};
  // No camera: nothing is added and nothing waits.
  FightTimeline none = buildTimelineList(s, times, {}, -1);
  CHECK(none.cam.empty());
  FightTimeline tl = buildTimelineList(s, times, {}, 250);
  const int ini = s.initiator, other = 1 - ini;
  CHECK_EQ(tl.camStart, ini);
  // Strike 1: the camera is on the actor already; it moves to the target ending at the hit. Strike 2 (the same actor again): back to the actor
  // before it starts, then to the target again for the hit. Strike 3 (the foe): the camera is on it already; it moves to the initiator for the hit.
  CHECK_EQ((int)tl.cam.size(), 4);
  if (tl.cam.size() != 4) return;
  const double hit1 = beat(tl, BeatKind::Hit, 0)->t, hit2 = beat(tl, BeatKind::Hit, 1)->t, hit3 = beat(tl, BeatKind::Hit, 2)->t;
  const double start2 = beat(tl, BeatKind::StrikeStart, 1)->t, start3 = beat(tl, BeatKind::StrikeStart, 2)->t;
  CHECK(tl.cam[0].side == other && std::fabs(tl.cam[0].t + tl.cam[0].dur - hit1) < 1e-9);
  CHECK(tl.cam[1].side == ini && std::fabs(tl.cam[1].t + tl.cam[1].dur - start2) < 1e-9);  // the strike waits for the camera
  CHECK(tl.cam[2].side == other && std::fabs(tl.cam[2].t + tl.cam[2].dur - hit2) < 1e-9);
  CHECK(tl.cam[3].side == ini && std::fabs(tl.cam[3].t + tl.cam[3].dur - hit3) < 1e-9);
  // The camera is on the acting fighter when its strike starts and on the struck one when the hit lands.
  CHECK(tl.cameraAt(0) == (double)ini);
  CHECK(tl.cameraAt(start2) == (double)ini);
  CHECK(tl.cameraAt(start3) == (double)other);
  CHECK(tl.cameraAt(hit1) == (double)other && tl.cameraAt(hit2) == (double)other && tl.cameraAt(hit3) == (double)ini);
  // Between: it is moving (strictly between the two sides) and never leaves 0..1.
  const double m = tl.cameraAt(tl.cam[0].t + tl.cam[0].dur / 2);
  CHECK(m > 0.0 && m < 1.0);
  for (double t = 0; t < tl.total; t += 0.01) CHECK(tl.cameraAt(t) >= 0.0 && tl.cameraAt(t) <= 1.0);
  // Every hit waits for the camera: no hit lands sooner than panMs after its strike started.
  for (int i = 0; i < 3; ++i) CHECK(beat(tl, BeatKind::Hit, i)->t - beat(tl, BeatKind::StrikeStart, i)->t >= 0.25 - 1e-9);
  // The pan makes the whole fight longer than without a camera, and a cut (0 ms) adds nothing to the hits.
  CHECK(tl.total > none.total);
  FightTimeline cut = buildTimelineList(s, times, {}, 0);
  CHECK(cut.cam.size() == 4 && cut.cam[0].dur == 0);
  CHECK(std::fabs(beat(cut, BeatKind::Hit, 0)->t - beat(none, BeatKind::Hit, 0)->t) < 1e-9);
  // A staff use pans like a strike: the camera starts on the user and moves to the target for the result.
  FightScript staff = buildFightScript({ev(EventType::StaffUsed, 1, 0, 2, 1), ev(EventType::UnitHealed, 2, 4)}, info(5));
  FightTimeline st = buildTimelineList(staff, times, {}, 250);
  const FightBeat* heal = beat(st, BeatKind::StaffHeal);
  CHECK(heal && st.cam.size() == 1 && st.cam[0].side == 1 - staff.initiator && std::fabs(st.cam[0].t + st.cam[0].dur - heal->t) < 1e-9);
  CHECK_EQ(st.camStart, staff.initiator);
}
