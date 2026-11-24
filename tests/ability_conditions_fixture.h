// A small game written by code for the ability-condition tests (docs/SRPG.md "Ability conditions"): a chance on an ability's "inflict",
// harmful / helpful conditions, cure lists by kind, cures on enemies, formulas that read conditions, and the computer's use of them.
//
// Map "field" (9 x 3, all plain): blue "hero" (a mage: mag 5, res 0, 30 HP, carries a sword) at 1,1 and "pal" (a knight) at 0,1; red "bandit"
// (a brigand: res 0, 20 HP) at 3,1. Damage = a.atk - b.def, hit 100, crit 0, so only the conditions' own chances are random. Opts change the units,
// the conditions, the abilities, and which side the computer plays.
#pragma once
#include "posix_compat.h"

#include <cstdio>
#include <string>
#include <vector>

#include "core/ai/ai.h"
#include "core/battle/battle.h"
#include "core/data/loader.h"
#include "save_helpers.h"
#include "test.h"

namespace acf {

using namespace ote;

struct Opts {
  std::string conditions =
      "{\"id\":\"sleep\",\"name\":\"Sleep\",\"turns\":3,\"blocks\":[{\"when\":\"always\",\"do\":[{\"cannotAct\":true},{\"cannotMove\":true}]}]},"
      "{\"id\":\"burn\",\"name\":\"Burn\",\"kind\":\"harmful\",\"turns\":3,\"blocks\":[{\"when\":\"turnStart\",\"do\":[{\"hurt\":1,\"to\":\"self\"}]}]},"
      "{\"id\":\"haste\",\"name\":\"Haste\",\"kind\":\"helpful\",\"turns\":3,\"blocks\":[{\"when\":\"always\",\"do\":[{\"stats\":{\"mov\":1}}]}]},"
      "{\"id\":\"protect\",\"name\":\"Protect\",\"kind\":\"helpful\",\"turns\":3,\"blocks\":[{\"when\":\"combat\",\"if\":[{\"weaponKind\":\"physical\",\"who\":\"foe\"}],"
      "\"do\":[{\"damageTakenPercent\":50}]}]},"
      "{\"id\":\"doom\",\"name\":\"Doom\",\"turns\":3,\"blocks\":[{\"when\":\"turnStart\",\"if\":[{\"formula\":\"a.conditions.doom == 0\"}],"
      "\"do\":[{\"hurt\":{\"percent\":100,\"canKill\":true},\"to\":\"self\"}]}]},"
      "{\"id\":\"ward\",\"name\":\"Ward\",\"kind\":\"helpful\",\"until\":\"cured\",\"blocks\":[{\"when\":\"always\",\"do\":[{\"immune\":[\"sleep\"]}]}]}";
  std::string abilities =
      "{\"id\":\"hex\",\"name\":\"Hex\",\"target\":\"enemy\",\"range\":[1,3],\"inflict\":{\"condition\":\"sleep\",\"chance\":50}},"
      "{\"id\":\"lull\",\"name\":\"Lull\",\"target\":\"enemy\",\"range\":[1,3],\"inflict\":{\"condition\":\"sleep\",\"chance\":\"a.mag * 10 - b.res * 10\"}},"
      "{\"id\":\"sure\",\"name\":\"Sure\",\"target\":\"enemy\",\"range\":[1,3],\"inflict\":{\"condition\":\"sleep\",\"chance\":\"b.conditions.burn and 100 or 0\"}},"
      "{\"id\":\"plain\",\"name\":\"Plain\",\"target\":\"enemy\",\"range\":[1,3],\"inflict\":{\"condition\":\"sleep\"}},"
      "{\"id\":\"quicken\",\"name\":\"Quicken\",\"target\":\"unit\",\"range\":[1,3],\"inflict\":{\"condition\":\"haste\"}},"
      "{\"id\":\"esuna\",\"name\":\"Esuna\",\"target\":\"unit\",\"range\":[1,3],\"effect\":{\"cure\":\"harmful\"}},"
      "{\"id\":\"dispel\",\"name\":\"Dispel\",\"target\":\"unit\",\"range\":[1,3],\"effect\":{\"cure\":\"helpful\"}},"
      "{\"id\":\"scorch\",\"name\":\"Scorch\",\"effect\":{\"strike\":\"fire_spell\"},\"inflict\":{\"condition\":\"burn\",\"chance\":50}}";
  // Abilities of each unit type (JSON lists).
  std::string mageAbilities = "[\"hex\",\"lull\",\"sure\",\"plain\",\"quicken\",\"esuna\",\"dispel\",\"scorch\"]";
  // More items of data/items.json (each starting with ","), what a mage wields and carries (JSON lists): the downed-unit tests add a revive item
  // and a staff (test_downed*.cpp).
  std::string moreItems;
  std::string mageWields = "[\"blade\"]";
  std::string mageItems = "[\"iron_sword\"]";
  std::string brigandAbilities = "[]";
  std::string units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
                      "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
                      "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1}";
  bool blueAi = false;  // the computer plays blue too
  std::string rules;     // more members of game.json "rules", each ending with "," (e.g. "\"progression\":{\"enabled\":true},")
  std::string mapExtra;  // more keys of the map, each ending with "," (e.g. "\"heights\":[...],")
};

inline std::string make(const char* name, const Opts& o = Opts()) {
  const std::string root = savetest::tempFolder(std::string("acf_") + name);
  makeDir(root + "/data");
  makeDir(root + "/maps");
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"Acf\", \"view\": {\"width\":480,\"height\":270,\"tileSize\":24},"
                      " \"stats\": [{\"id\":\"hp\"},{\"id\":\"atk\"},{\"id\":\"def\"},{\"id\":\"mag\"},{\"id\":\"res\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"rules\": { " + o.rules + "\"defeat\": [\"noUnits\"], \"counterAttacks\": true, \"items\": {\"weapons\":\"carried\",\"inventorySize\":4} },"
                      " \"formulas\": {\"damage\":\"a.atk - b.def\",\"hit\":\"100\",\"crit\":\"0\"},"
                      " \"maps\": [{\"id\":\"field\",\"name\":\"Field\",\"file\":\"maps/field.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json",
                      "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\",\"damageKind\":\"physical\"},"
                      "{\"id\":\"fire_spell\",\"name\":\"Fire\",\"group\":\"spell\",\"damageKind\":\"magic\",\"range\":[1,3],\"damage\":\"a.mag - b.res\"}] }"));
  CHECK(writeTextFile(root + "/data/items.json",
                      "{ \"items\": [{\"id\":\"iron_sword\",\"name\":\"Iron Sword\",\"kind\":\"weapon\",\"weapon\":\"sword\"}" + o.moreItems + "] }"));
  CHECK(writeTextFile(root + "/data/conditions.json", "{ \"conditions\": [" + o.conditions + "] }"));
  CHECK(writeTextFile(root + "/data/abilities.json", "{ \"abilities\": [" + o.abilities + "] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"mage\",\"name\":\"Mage\",\"moveType\":\"foot\",\"stats\":{\"hp\":30,\"atk\":6,\"def\":1,\"mag\":5,\"res\":0,\"mov\":4},"
                      "\"wields\":" + o.mageWields + ",\"items\":" + o.mageItems + ",\"abilities\":" + o.mageAbilities + "},"
                      "{\"id\":\"knight\",\"name\":\"Knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":30,\"atk\":8,\"def\":2,\"mag\":0,\"res\":0,\"mov\":4},"
                      "\"wields\":[\"blade\"],\"items\":[\"iron_sword\"]},"
                      "{\"id\":\"brigand\",\"name\":\"Brigand\",\"moveType\":\"foot\",\"stats\":{\"hp\":20,\"atk\":5,\"def\":1,\"mag\":0,\"res\":0,\"mov\":3},"
                      "\"wields\":[\"blade\"],\"items\":[\"iron_sword\"],\"abilities\":" + o.brigandAbilities + "}] }"));
  CHECK(writeTextFile(root + "/maps/field.json",
                      std::string("{ \"name\": \"Field\", \"grid\": {\"type\":\"square\",\"width\":9,\"height\":3}, \"legend\": {\".\":\"plain\"},"
                                  " \"tiles\": [\".........\",\".........\",\".........\"]," + o.mapExtra +
                                  " \"sides\": [{\"id\":\"blue\",\"controller\":\"") + (o.blueAi ? "ai" : "human") + "\"},{\"id\":\"red\",\"controller\":\"ai\"}],"
                      " \"units\": [" + o.units + "] }"));
  return root;
}

// The game's load message ("" = it loaded).
inline std::string error(const std::string& root) {
  Database db;
  std::string err;
  if (!loadGame(root, db, err)) return err;
  Formulas f;  // a formula that does not compile is refused here
  if (!f.init(db, err)) return err;
  MapDef m;
  return loadMap(db, "field", m, err) ? "" : err;
}

inline bool says(const std::string& err, const char* part) {
  const bool ok = err.find(part) != std::string::npos;
  if (!ok) std::printf("  wanted \"%s\" in: %s\n", part, err.c_str());
  return ok;
}

struct World {
  savetest::Rig rig;
  std::vector<Event> events;
  explicit World(const std::string& root, unsigned seed = 11) {
    CHECK(rig.load(root, "field", seed));
    rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
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
  const Event* first(EventType t) const {
    for (const Event& e : events)
      if (e.type == t) return &e;
    return nullptr;
  }
  // Blue ends its turn and the computer plays red's whole turn (its events are kept in `events`).
  void redTurn() {
    CHECK(run(Action::endTurn()));
    int guard = 0;
    while (!b().over && aiTakeStep(b(), events) && ++guard < 200) {
    }
  }
  bool sameRng(const Rng& before) const { return rig.battle.rng.state == before.state && rig.battle.rng.inc == before.inc; }
  int ability(const char* id) const { return rig.db.findAbility(id); }
  int cond(const char* id) const { return rig.db.findCondition(id); }
  bool has(const char* story, const char* condition) { return b().hasCondition(unit(story), cond(condition)); }
};

}  // namespace acf
