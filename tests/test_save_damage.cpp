// Restoring refuses damaged saves with a readable message; the data hash;
// and the "saving" game setting.
#include "battle_fixture.h"
#include "core/data/game_info.h"
#include "save_helpers.h"

using namespace ote;
using namespace savetest;

namespace {

struct Step {
  const char* key;  // object member, or
  int idx;          // array element when key is null
};

// Copy of `node` with the value at `path` replaced (or the member/element erased).
Json editAt(const Json& node, const std::vector<Step>& path, size_t i, const Json& value, bool erase) {
  if (i == path.size()) return value;
  const Step& s = path[i];
  bool last = i + 1 == path.size();
  if (s.key) {
    Json out = node;
    if (last && erase) out.erase(s.key);
    else out.set(s.key, editAt(node[s.key], path, i + 1, value, erase));
    return out;
  }
  Json arr = Json::makeArray();
  for (size_t k = 0; k < node.size(); ++k) {
    if ((int)k == s.idx) {
      if (!(last && erase)) arr.push(editAt(node[k], path, i + 1, value, erase));
    } else {
      arr.push(node[k]);
    }
  }
  return arr;
}

struct Damage {
  const char* what;
  std::vector<Step> path;
  Json value;
  bool erase;
  const char* mention;  // the message must contain this
};

Json N(double v) { return Json::makeNumber(v); }
Json S(const char* v) { return Json::makeString(v); }

}  // namespace

TEST(save_restore_refuses_every_kind_of_damage) {
  Rig live;
  CHECK(live.loadDemo("demo-economy", "twin_rivers", 9));
  live.start();
  std::vector<Event> ev;
  playAi(live.battle, 80, ev);  // mid-battle: some units dead, structures changed hands
  const Battle& lb = live.battle;
  CHECK(!lb.over);

  // Targets for the damage: two living units, a structure nobody is capturing,
  // and a living unit that is not on that structure.
  int u1 = -1, u2 = -1, structure = -1, farUnit = -1;
  for (const Unit& u : lb.units)
    if (u.alive) {
      if (u1 < 0) u1 = u.id;
      else if (u2 < 0) u2 = u.id;
    }
  for (size_t i = 0; i < lb.structures.size() && structure < 0; ++i)
    if (lb.structures[i].capturer < 0) structure = (int)i;
  CHECK(u1 >= 0 && u2 >= 0 && structure >= 0);
  for (const Unit& u : lb.units)
    if (u.alive && lb.structureAt[(size_t)u.cell] != structure) { farUnit = u.id; break; }
  CHECK(farUnit >= 0);
  Json good;
  saveBattleState(lb, good);
  Cell cell1 = lb.units[(size_t)u1].cell;
  const int cells = lb.grid.cellCount();
  Json tooMany = good["structures"];
  tooMany.push(tooMany[0]);

  const std::vector<Damage> table = {
      {"round missing", {{"round", 0}}, Json(), true, "round"},
      {"round is text", {{"round", 0}}, S("3"), false, "round"},
      {"round is zero", {{"round", 0}}, N(0), false, "round"},
      {"round is fractional", {{"round", 0}}, N(2.5), false, "round"},
      {"active side out of range", {{"activeSide", 0}}, N(9), false, "activeSide"},
      {"active side negative", {{"activeSide", 0}}, N(-1), false, "activeSide"},
      {"over is a number", {{"over", 0}}, N(1), false, "over"},
      {"winner team out of range", {{"winnerTeam", 0}}, N(50), false, "winnerTeam"},
      {"rng missing", {{"rng", 0}}, Json(), true, "rng"},
      {"rng state not hex", {{"rng", 0}, {"state", 0}}, S("not-hex-at-all!!"), false, "rng.state"},
      {"rng state too short", {{"rng", 0}, {"state", 0}}, S("abc"), false, "rng.state"},
      {"rng state is a number", {{"rng", 0}, {"state", 0}}, N(5), false, "rng.state"},
      {"rng inc even", {{"rng", 0}, {"inc", 0}}, S("0000000000000002"), false, "rng.inc"},
      {"sides not a list", {{"sides", 0}}, S("x"), false, "sides"},
      {"too few sides", {{"sides", 0}, {nullptr, 1}}, Json(), true, "sides"},
      {"side is not an object", {{"sides", 0}, {nullptr, 0}}, N(1), false, "sides[0]"},
      {"side resources wrong length", {{"sides", 0}, {nullptr, 0}, {"resources", 0}}, Json::makeArray(), false, "resources"},
      {"side resource is text", {{"sides", 0}, {nullptr, 0}, {"resources", 0}, {nullptr, 0}}, S("5"), false, "resources[0]"},
      {"side flag missing", {{"sides", 0}, {nullptr, 1}, {"defeated", 0}}, Json(), true, "defeated"},
      {"units not a list", {{"units", 0}}, N(3), false, "units"},
      {"unit not an object", {{"units", 0}, {nullptr, u1}}, S("unit"), false, "units["},
      {"unit type out of range", {{"units", 0}, {nullptr, u1}, {"type", 0}}, N(999), false, ".type"},
      {"unit type negative", {{"units", 0}, {nullptr, u1}, {"type", 0}}, N(-2), false, ".type"},
      {"unit side out of range", {{"units", 0}, {nullptr, u1}, {"side", 0}}, N(7), false, ".side"},
      {"unit cell out of range", {{"units", 0}, {nullptr, u1}, {"cell", 0}}, N(cells), false, ".cell"},
      {"unit cell fractional", {{"units", 0}, {nullptr, u1}, {"cell", 0}}, N(1.5), false, ".cell"},
      {"two units on one cell", {{"units", 0}, {nullptr, u2}, {"cell", 0}}, N(cell1), false, "already on cell"},
      {"living unit without a cell", {{"units", 0}, {nullptr, u1}, {"cell", 0}}, N(-1), false, "no cell"},
      {"dead unit with a cell", {{"units", 0}, {nullptr, u1}, {"alive", 0}}, Json::makeBool(false), false, "dead unit"},
      {"unit stats wrong length", {{"units", 0}, {nullptr, u1}, {"stats", 0}}, Json::makeArray(), false, ".stats"},
      {"unit stat is text", {{"units", 0}, {nullptr, u1}, {"stats", 0}, {nullptr, 0}}, S("9"), false, ".stats[0]"},
      {"unit hp above its maximum", {{"units", 0}, {nullptr, u1}, {"hp", 0}}, N(100000), false, ".hp"},
      {"unit hp negative", {{"units", 0}, {nullptr, u1}, {"hp", 0}}, N(-3), false, ".hp"},
      {"living unit with no hp", {{"units", 0}, {nullptr, u1}, {"hp", 0}}, N(0), false, "hit points"},
      {"unit level zero", {{"units", 0}, {nullptr, u1}, {"level", 0}}, N(0), false, ".level"},
      {"unit name is a number", {{"units", 0}, {nullptr, u1}, {"name", 0}}, N(4), false, ".name"},
      {"unit flag missing", {{"units", 0}, {nullptr, u1}, {"done", 0}}, Json(), true, ".done"},
      {"structures not a list", {{"structures", 0}}, Json::makeObject(), false, "structures"},
      {"too many structures", {{"structures", 0}}, tooMany, false, "structures"},
      {"structure owner out of range", {{"structures", 0}, {nullptr, structure}, {"owner", 0}}, N(99), false, ".owner"},
      {"structure capture too high", {{"structures", 0}, {nullptr, structure}, {"capture", 0}}, N(9999), false, ".capture"},
      {"structure capture negative", {{"structures", 0}, {nullptr, structure}, {"capture", 0}}, N(-1), false, ".capture"},
      {"capturer is no unit", {{"structures", 0}, {nullptr, structure}, {"capturer", 0}}, N(99999), false, ".capturer"},
      {"capturer stands elsewhere", {{"structures", 0}, {nullptr, structure}, {"capturer", 0}}, N(farUnit), false, "not standing"},
      {"progress without a capturer", {{"structures", 0}, {nullptr, structure}, {"capture", 0}}, N(1), false, "nobody"},
  };

  for (const Damage& d : table) {
    Json bad = editAt(good, d.path, 0, d.value, d.erase);
    Rig fresh;
    CHECK(fresh.load(live.gameDir, live.mapId, 1));
    std::string untouched = stateText(fresh.battle), err;
    bool ok = restoreBattleState(fresh.battle, bad, err);
    if (ok || err.find(d.mention) == std::string::npos)
      std::printf("  [%s] restore %s, message: \"%s\" (wanted \"%s\")\n", d.what, ok ? "succeeded" : "failed",
                  err.c_str(), d.mention);
    CHECK(!ok);
    CHECK(err.find(d.mention) != std::string::npos);
    CHECK_STR(stateText(fresh.battle), untouched);  // nothing half-restored
  }

  // The undamaged save is fine; a non-object is refused, not crashed on.
  Rig fresh;
  CHECK(fresh.load(live.gameDir, live.mapId, 1));
  std::string err;
  CHECK(restoreBattleState(fresh.battle, good, err));
  CHECK_STR(stateText(fresh.battle), stateText(lb));
  Rig fresh2;
  CHECK(fresh2.load(live.gameDir, live.mapId, 1));
  CHECK(!restoreBattleState(fresh2.battle, Json(), err));
  CHECK(!restoreBattleState(fresh2.battle, Json::makeArray(), err));
}

namespace {
// A copy of the files that define demo-srpg's rules (and graphics.json), so
// the hash can be computed after changing them one by one.
struct HashGame {
  Database db;
  std::string dir;
  bool make() {
    std::string src = ote_test::sourceDir() + "/games/demo-srpg";
    std::string err;
    if (!loadGame(src, db, err)) return false;
    dir = tempFolder("save_hash_game");
    tempFolder("save_hash_game/data");
    tempFolder("save_hash_game/maps");
    tempFolder("save_hash_game/scripts");
    const char* files[] = {"game.json", "data/weapons.json", "data/units.json", "data/terrain.json",
                           "data/structures.json", "data/items.json", "data/graphics.json", "data/campaign.json",
                           "maps/bridge.json", "maps/north_road.json", "scripts/formulas.lua"};
    for (const char* f : files)
      if (!copyFile(src + "/" + f, dir + "/" + f)) return false;
    db.rootPath = dir;
    return true;
  }
  std::string hash() {
    std::string h, err;
    if (!computeDataHash(db, "bridge", h, err)) std::printf("  hash: %s\n", err.c_str());
    return h;
  }
  std::string before;  // the file as it was before the last edit()
  // Replaces the first `from` in a file with `to`; false if it is not there.
  bool edit(const char* rel, const std::string& from, const std::string& to) {
    std::string text;
    if (!readTextFile(dir + "/" + rel, text)) return false;
    before = text;
    size_t at = text.find(from);
    if (at == std::string::npos) {
      std::printf("  [%s] does not contain: %s\n", rel, from.c_str());
      return false;
    }
    text.replace(at, from.size(), to);
    return writeTextFile(dir + "/" + rel, text);
  }
  // Puts back the file changed by the last edit(). (Replacing `to` with
  // `from` would not do: the new text may also appear earlier in the file.)
  bool undo(const char* rel) { return writeTextFile(dir + "/" + rel, before); }
};
}  // namespace

TEST(save_data_hash_follows_rules_and_map_but_not_art) {
  HashGame g;
  CHECK(g.make());
  std::string base = g.hash();
  CHECK_EQ((int)base.size(), 16);
  CHECK_STR(g.hash(), base);  // stable

  // The copy hashes like the original folder (same bytes, same names).
  Database orig;
  std::string err, origHash;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-srpg", orig, err));
  CHECK(computeDataHash(orig, "bridge", origHash, err));
  CHECK_STR(origHash, base);

  // Rules changes: each must change the hash, and undoing it must restore it.
  struct Edit { const char* rel; const char* from; const char* to; };
  const Edit rules[] = {
      {"game.json", "\"id\": \"spd\"", "\"id\": \"speed\""},
      {"data/weapons.json", "\"might\": 6", "\"might\": 9"},
      {"data/units.json", "\"hp\": 20", "\"hp\": 21"},
      {"data/terrain.json", "\"defense\": 0", "\"defense\": 1"},
      {"data/structures.json", "\"id\": \"fort\"", "\"id\": \"fort\", \"capturePoints\": 5"},
      {"data/items.json", "\"heal\": 10", "\"heal\": 12"},
      {"maps/bridge.json", "\"x\": 4, \"y\": 3", "\"x\": 5, \"y\": 3"},
      {"maps/bridge.json", "\"controller\": \"ai\"", "\"controller\": \"human\""},
      {"scripts/formulas.lua", "\n", "\n-- changed\n"},
  };
  for (const Edit& e : rules) {
    CHECK(g.edit(e.rel, e.from, e.to));
    if (g.hash() == base) std::printf("  hash did not change for %s: %s\n", e.rel, e.to);
    CHECK(g.hash() != base);
    CHECK(g.undo(e.rel));
    CHECK_STR(g.hash(), base);
  }

  // Cosmetic changes: names, descriptions, colours, letters, fonts, styles,
  // screen size, the saving setting, comments and layout. Saves stay valid.
  const Edit cosmetic[] = {
      {"game.json", "\"name\": \"Emblem Demo\"", "\"name\": \"Renamed\", \"author\": \"Me\", \"saving\": \"continue\""},
      {"game.json", "\"description\": \"A short", "\"font\": { \"name\": \"sans\" }, \"description\": \"A long"},
      {"game.json", "\"name\": \"Classic\"", "\"name\": \"Plain\", \"background\": \"#000000\""},
      {"game.json", "\"tileSize\": 24", "\"tileSize\": 32"},
      {"game.json", "\"name\": \"Strength\"", "\"name\": \"Power\""},
      {"data/weapons.json", "\"name\": \"Iron Sword\"", "\"name\": \"Old Sword\""},
      {"data/items.json", "\"name\": \"Vulnerary\"", "\"name\": \"Herb\""},
      {"data/units.json", "\"name\": \"Lord\", \"glyph\": \"L\"", "\"name\": \"Hero\", \"glyph\": \"H\""},
      {"data/terrain.json", "\"color\": \"#8fbf6a\", \"glyph\": \".\"", "\"color\": \"#123456\",   \"glyph\": \",\""},
      {"data/structures.json", "{ \"structures\"", "// another note\n{   \"structures\""},
      {"maps/bridge.json", "\"name\": \"Skirmish at the Ford\"", "\"name\": \"The Ford\""},
      {"maps/bridge.json", "\"name\": \"Blue Company\", \"color\": \"#3d85c6\"", "\"name\": \"Azure\", \"color\": \"#0000ff\""},
  };
  for (const Edit& e : cosmetic) {
    CHECK(g.edit(e.rel, e.from, e.to));
    if (g.hash() != base) std::printf("  hash changed for a cosmetic edit in %s: %s\n", e.rel, e.to);
    CHECK_STR(g.hash(), base);
  }
  CHECK(g.edit("data/graphics.json", "{", "{ "));  // art: pictures, not rules
  CHECK_STR(g.hash(), base);

  std::string h;
  CHECK(!computeDataHash(g.db, "no-such-map", h, err));
  CHECK(!err.empty());
  removeFile(g.dir + "/maps/bridge.json");  // a missing file is an error, not a quiet hash
  CHECK(!computeDataHash(g.db, "bridge", h, err));
  CHECK(!err.empty());
}

namespace {
// A tiny game copied from the loader fixture with a chosen "saving" line.
Database loadWithSaving(const char* tag, const char* line, std::string& err, bool& ok) {
  std::string src = ote_test::sourceDir() + "/tests/fixtures/base";
  std::string base = std::string("saving_") + tag;
  std::string dir = tempFolder(base);
  tempFolder(base + "/data");
  tempFolder(base + "/maps");
  const char* files[] = {"data/weapons.json", "data/units.json", "data/terrain.json", "maps/ok.json"};
  for (const char* f : files) copyFile(src + "/" + f, dir + "/" + f);
  std::string text;
  readTextFile(src + "/game.json", text);
  text.insert(text.find("\"name\""), std::string(line) + " ");
  writeTextFile(dir + "/game.json", text);
  Database db;
  ok = loadGame(dir, db, err);
  return db;
}
}  // namespace

TEST(loader_reads_the_saving_setting) {
  std::string err;
  bool ok = false;
  Database db = loadWithSaving("default", "", err, ok);
  CHECK(ok && db.saving == SavingMode::Any);  // missing = any
  db = loadWithSaving("any", "\"saving\": \"any\",", err, ok);
  CHECK(ok && db.saving == SavingMode::Any);
  db = loadWithSaving("continue", "\"saving\": \"continue\",", err, ok);
  CHECK(ok && db.saving == SavingMode::Continue);
  db = loadWithSaving("off", "\"saving\": \"off\",", err, ok);
  CHECK(ok && db.saving == SavingMode::Off);

  loadWithSaving("bogus", "\"saving\": \"sometimes\",", err, ok);
  CHECK(!ok);
  CHECK(err.find("\"saving\" must be") != std::string::npos);
  CHECK(err.find("sometimes") != std::string::npos);
  CHECK(err.find("game.json") != std::string::npos);
  loadWithSaving("number", "\"saving\": 3,", err, ok);
  CHECK(!ok);
  CHECK(err.find("saving") != std::string::npos);

  // The launcher's quick preview reads it too.
  GameInfo gi;
  CHECK(loadGameInfo(ote_test::tempDir() + "/saving_off", gi, err));
  CHECK(gi.saving == SavingMode::Off);
  CHECK(loadGameInfo(ote_test::tempDir() + "/saving_continue", gi, err));
  CHECK(gi.saving == SavingMode::Continue);
  CHECK(loadGameInfo(ote_test::sourceDir() + "/games/demo-srpg", gi, err));
  CHECK(gi.saving == SavingMode::Any);
}
