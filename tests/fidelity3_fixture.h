// A small game written by code for the "fidelity 3" tests (an ability that spends its item, summon weights and level ceilings, promotion level
// per character, weather that closes a way, several preparation shops). It starts from the "fidelity 2" game (tests/fidelity2_fixture.h) and
// writes over the files a test wants different: an empty override keeps the fidelity 2 file.
//
// Map "arena" (9 x 3, "s" = swamp at 3,1 and 4,1): blue hero (knight) at 1,1, blue pal (a knight) at 0,1; red bandit at 2,1.
// The default skills / items / abilities below give the knight "zap" through a wand (an accessory with 3 uses, carried by the hero).
#pragma once
#include "fidelity2_fixture.h"

namespace f3 {

using namespace ote;

struct Opts {
  f2::Opts base;
  std::string skills;      // data/skills.json body (the list's items)
  std::string items;       // data/items.json list items
  std::string abilities;   // data/abilities.json list items
  std::string units;       // data/units.json list items
  std::string characters;  // data/characters.json list items ("" = no file)
  std::string weapons;     // data/weapons.json list items
};

inline const char* kSkills =
    "{\"id\":\"zapper\",\"name\":\"Zapper\",\"blocks\":[{\"when\":\"always\",\"do\":[{\"ability\":\"zap\"}]}]}";
inline const char* kItems =
    "{\"id\":\"iron_sword\",\"name\":\"Iron Sword\",\"kind\":\"weapon\",\"weapon\":\"sword\",\"uses\":3},"
    "{\"id\":\"old_axe\",\"name\":\"Old Axe\",\"kind\":\"weapon\",\"weapon\":\"axe\"},"
    "{\"id\":\"fang\",\"name\":\"Fang\",\"kind\":\"weapon\",\"weapon\":\"sword\"},"
    "{\"id\":\"big_fang\",\"name\":\"Big Fang\",\"kind\":\"weapon\",\"weapon\":\"axe\"},"
    "{\"id\":\"wand\",\"name\":\"Wand\",\"kind\":\"accessory\",\"uses\":3,\"skills\":[\"zapper\"]}";

// The default abilities are the fidelity 2 ones plus zap (an ability that spends its item; heals an ally for 5).
inline std::string abilities(const char* extra = "") {
  f2::Opts d;
  return d.abilities + ",{\"id\":\"zap\",\"name\":\"Zap\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"spendsItem\":true" + extra + "}";
}

// Writes the game; returns its folder.
inline std::string make(const char* name, const Opts& o = Opts()) {
  const std::string root = f2::make((std::string("f3_") + name).c_str(), o.base);
  const auto put = [&](const char* file, const std::string& text) { CHECK(writeTextFile(root + "/data/" + file, text)); };
  put("skills.json", "{ \"skills\": [" + (o.skills.empty() ? std::string(kSkills) : o.skills) + "] }");
  put("items.json", "{ \"items\": [" + (o.items.empty() ? std::string(kItems) : o.items) + "] }");
  put("abilities.json", "{ \"abilities\": [" + (o.abilities.empty() ? abilities() : o.abilities) + "] }");
  if (!o.weapons.empty()) put("weapons.json", "{ \"weapons\": [" + o.weapons + "] }");
  if (!o.characters.empty()) put("characters.json", "{ \"characters\": [" + o.characters + "] }");
  if (!o.units.empty()) {
    put("units.json", "{ \"units\": [" + o.units + "] }");
  } else {  // the hero carries the wand; nobody lists zap itself
    put("units.json",
        "{ \"units\": [{\"id\":\"knight\",\"name\":\"Knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":30,\"atk\":8,\"def\":2,\"mov\":4},"
        "\"growths\":{\"hp\":50},\"wields\":[\"blade\"],\"items\":[\"iron_sword\",\"wand\"],\"abilities\":[\"inspire\",\"call_pet\",\"call_high\",\"call_rand\",\"call_one\",\"call_plain\"]},"
        "{\"id\":\"brigand\",\"name\":\"Brigand\",\"moveType\":\"foot\",\"stats\":{\"hp\":20,\"atk\":5,\"def\":1,\"mov\":3},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]},"
        "{\"id\":\"hound\",\"name\":\"Hound\",\"moveType\":\"foot\",\"stats\":{\"hp\":8,\"atk\":3,\"def\":1,\"mov\":4},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]}] }");
  }
  return root;
}

using f2::error;
using f2::says;
using f2::World;

}  // namespace f3
