// A small game written by code for the "fidelity 2" tests (ability with only inflict, staged fights, battle backdrops and platforms per map,
// weather and movement cost, a range of condition damage, summon follow-ups, the scene's sides). Each test changes a piece of text and loads
// the game with the real loader.
//
// Map "arena" (9 x 3, "s" = swamp at 3,1 and 4,1): blue hero (knight, 30 HP, atk 8, def 2, carries an iron sword with 3 uses) at 1,1, blue pal
// (a knight) at 0,1; red bandit (20 HP, atk 5, def 1) at 2,1. Damage = a.atk - b.def, hit 60, crit 30.
#pragma once
#include "posix_compat.h"

#include <cstdio>
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "core/battle/save.h"
#include "core/data/loader.h"
#include "save_helpers.h"
#include "test.h"

namespace f2 {

using namespace ote;

struct Opts {
  std::string gameExtra;       // more keys of game.json (before "maps"), each ending with a comma
  std::string terrain = "{\"id\":\"plain\"},{\"id\":\"swamp\",\"moveCost\":{\"foot\":2,\"wheel\":3}}";
  std::string conditions = "{\"id\":\"boost\",\"name\":\"Boost\",\"turns\":4,\"blocks\":[{\"when\":\"always\",\"do\":[{\"stats\":{\"atk\":2}}]}]},"
                           "{\"id\":\"poison\",\"name\":\"Poison\",\"turns\":50,\"blocks\":[{\"when\":\"turnStart\",\"do\":[{\"hurt\":{\"amount\":[1,3]},\"to\":\"self\"}]}]},"
                           "{\"id\":\"fixed\",\"name\":\"Fixed\",\"turns\":50,\"blocks\":[{\"when\":\"turnStart\",\"do\":[{\"hurt\":2,\"to\":\"self\"}]}]},"
                           "{\"id\":\"deadly\",\"name\":\"Deadly\",\"turns\":50,\"blocks\":[{\"when\":\"turnStart\",\"do\":[{\"hurt\":{\"amount\":[5,6],\"lethal\":true},\"to\":\"self\"}]}]},"
                           "{\"id\":\"gentle\",\"name\":\"Gentle\",\"turns\":50,\"blocks\":[{\"when\":\"turnStart\",\"do\":[{\"hurt\":{\"amount\":[5,6]},\"to\":\"self\"}]}]}";
  std::string abilities =
      "{\"id\":\"inspire\",\"name\":\"Inspire\",\"target\":\"ally\",\"inflict\":{\"condition\":\"boost\",\"turns\":4},\"xp\":20},"
      "{\"id\":\"call_pet\",\"name\":\"Call Pet\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"hound\",\"limit\":1,\"replace\":true,\"canAct\":true}}},"
      "{\"id\":\"call_high\",\"name\":\"Call High\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"hound\",\"limit\":3,\"canAct\":true,"
      "\"items\":[{\"item\":\"fang\",\"minLevel\":1},{\"item\":\"big_fang\",\"minLevel\":5}],\"pick\":\"highest\"}}},"
      "{\"id\":\"call_rand\",\"name\":\"Call Random\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"hound\",\"limit\":3,\"canAct\":true,"
      "\"items\":[{\"item\":\"fang\",\"minLevel\":1},{\"item\":\"big_fang\",\"minLevel\":2}],\"pick\":\"random\"}}},"
      "{\"id\":\"call_one\",\"name\":\"Call One\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"hound\",\"limit\":3,\"canAct\":true,"
      "\"items\":[{\"item\":\"fang\",\"minLevel\":1}],\"pick\":\"random\"}}},"
      "{\"id\":\"call_plain\",\"name\":\"Call Plain\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"summon\":{\"type\":\"hound\",\"canAct\":true}}}";
  std::string extraAbility;    // one more ability (a JSON object), nobody has it
  std::string events;          // the map's "events" list body
  std::string mapExtra;        // more keys of the map (each ending with a comma)
  std::string graphics;        // data/graphics.json "battle" object ("" = no graphics.json)
};

inline std::string path(const std::string& dir, const char* rel) { return dir + "/" + rel; }

inline void png(const std::string& file, int w, int h) {
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, (unsigned char)(w >> 8),
                         (unsigned char)w, 0, 0, (unsigned char)(h >> 8), (unsigned char)h, 8, 6};
  FILE* f = std::fopen(file.c_str(), "wb");
  CHECK(f != nullptr);
  if (f) {
    std::fwrite(b, 1, sizeof b, f);
    std::fclose(f);
  }
}

// Writes the game; returns its folder.
inline std::string make(const char* name, const Opts& o = Opts()) {
  const std::string root = savetest::tempFolder(std::string("fid2_") + name);
  makeDir(root + "/data");
  makeDir(root + "/maps");
  makeDir(root + "/art");
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"Fid2\", \"view\": {\"width\":480,\"height\":270,\"tileSize\":24},"
                      " \"stats\": [{\"id\":\"hp\"},{\"id\":\"atk\"},{\"id\":\"def\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"},{\"id\":\"wheel\"}],"
                      " \"rules\": { \"defeat\": [\"noUnits\"], \"counterAttacks\": true, \"items\": {\"weapons\":\"carried\",\"inventorySize\":4},"
                      "   \"progression\": {\"enabled\":true,\"xpPerLevel\":100,\"maxLevel\":20,\"xpHit\":\"30\",\"xpKill\":\"60\"} },"
                      " \"formulas\": {\"damage\":\"a.atk - b.def\",\"hit\":\"60\",\"crit\":\"30\"}," + o.gameExtra +
                      " \"maps\": [{\"id\":\"arena\",\"name\":\"Arena\",\"file\":\"maps/arena.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [" + o.terrain + "] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"},{\"id\":\"axe\",\"group\":\"blade\"}] }"));
  CHECK(writeTextFile(root + "/data/items.json",
                      "{ \"items\": [{\"id\":\"iron_sword\",\"name\":\"Iron Sword\",\"kind\":\"weapon\",\"weapon\":\"sword\",\"uses\":3},"
                      "{\"id\":\"old_axe\",\"name\":\"Old Axe\",\"kind\":\"weapon\",\"weapon\":\"axe\"},"
                      "{\"id\":\"fang\",\"name\":\"Fang\",\"kind\":\"weapon\",\"weapon\":\"sword\"},"
                      "{\"id\":\"big_fang\",\"name\":\"Big Fang\",\"kind\":\"weapon\",\"weapon\":\"axe\"}] }"));
  CHECK(writeTextFile(root + "/data/conditions.json", "{ \"conditions\": [" + o.conditions + "] }"));
  CHECK(writeTextFile(root + "/data/abilities.json", "{ \"abilities\": [" + o.abilities + (o.extraAbility.empty() ? "" : "," + o.extraAbility) + "] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"knight\",\"name\":\"Knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":30,\"atk\":8,\"def\":2,\"mov\":4},"
                      "\"growths\":{\"hp\":50},\"wields\":[\"blade\"],\"items\":[\"iron_sword\"],\"abilities\":[\"inspire\",\"call_pet\",\"call_high\",\"call_rand\",\"call_one\",\"call_plain\"]},"
                      "{\"id\":\"brigand\",\"name\":\"Brigand\",\"moveType\":\"foot\",\"stats\":{\"hp\":20,\"atk\":5,\"def\":1,\"mov\":3},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]},"
                      "{\"id\":\"hound\",\"name\":\"Hound\",\"moveType\":\"foot\",\"stats\":{\"hp\":8,\"atk\":3,\"def\":1,\"mov\":4},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]}] }"));
  CHECK(writeTextFile(root + "/maps/arena.json",
                      "{ \"name\": \"Arena\", \"grid\": {\"type\":\"square\",\"width\":9,\"height\":3}, \"legend\": {\".\":\"plain\",\"s\":\"swamp\"},"
                      " \"tiles\": [\".........\",\"...ss....\",\".........\"]," + o.mapExtra +
                      " \"sides\": [{\"id\":\"blue\",\"controller\":\"human\"},{\"id\":\"red\",\"controller\":\"ai\"}],"
                      " \"units\": [{\"type\":\"knight\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1,\"leader\":true},"
                      "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
                      "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":2,\"y\":1}]" +
                      (o.events.empty() ? "" : ", \"events\": [" + o.events + "]") + " }"));
  if (!o.graphics.empty()) {
    png(root + "/art/bgs.png", 240, 68);
    png(root + "/art/plat.png", 96, 16);
    CHECK(writeTextFile(root + "/data/graphics.json",
                        "{ \"sheets\": [ {\"id\":\"bgs\",\"file\":\"art/bgs.png\",\"cellWidth\":120,\"cellHeight\":68},"
                        "                {\"id\":\"plat\",\"file\":\"art/plat.png\",\"cellWidth\":48,\"cellHeight\":16} ],"
                        " \"battle\": " + o.graphics + " }"));
  }
  return root;
}

// The game's load message ("" = it loaded), then the map's.
inline std::string error(const std::string& root) {
  Database db;
  std::string err;
  if (!loadGame(root, db, err)) return err;
  MapDef m;
  return loadMap(db, "arena", m, err) ? "" : err;
}

inline bool says(const std::string& err, const char* part) {
  const bool ok = err.find(part) != std::string::npos;
  if (!ok) std::printf("  wanted \"%s\" in: %s\n", part, err.c_str());
  return ok;
}

// A started battle on the game.
struct World {
  savetest::Rig rig;
  std::vector<Event> events;
  explicit World(const std::string& root, bool start = true) {
    CHECK(rig.load(root, "arena", 11));
    if (start) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  const Database& db() const { return rig.db; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  bool run(const Action& a) {
    events.clear();
    std::string why;
    const bool ok = b().apply(a, events, &why);
    if (!ok) std::printf("  refused: %s\n", why.c_str());
    return ok;
  }
  std::string why(const Action& a) {
    std::string w;
    return b().validate(a, &w) ? "" : (w.empty() ? "(no reason)" : w);
  }
  int count(EventType t) const {
    int n = 0;
    for (const Event& e : events) n += e.type == t;
    return n;
  }
  std::vector<Event> of(EventType t) const {
    std::vector<Event> out;
    for (const Event& e : events)
      if (e.type == t) out.push_back(e);
    return out;
  }
  // Both sides end their turn: blue's turn starts again.
  void round() {
    CHECK(run(Action::endTurn()));
    std::vector<Event> keep;
    CHECK(run(Action::endTurn()));
  }
  bool sameRng(const Rng& before) const { return rig.battle.rng.state == before.state && rig.battle.rng.inc == before.inc; }
  int ability(const char* id) const { return rig.db.findAbility(id); }
  int cond(const char* id) const { return rig.db.findCondition(id); }
};

}  // namespace f2
