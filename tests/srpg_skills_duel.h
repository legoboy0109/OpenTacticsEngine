// Helper of the K2 skill tests (docs/SRPG.md step K2): a duel in a copy of tests/fixtures/srpg-skills with skills
// written by the test. Unit types (all hp 30, atk 6, def 2, skl 20, mov 4, so 4 damage to each other, no hit or crit
// formula: 100 % hit, 0 % crit, critical x3): "ua" and "ub" carry the skills the test names, "uc" has none; weapons
// "plain_sword" (range 1), "plain_bow" (range 2, never answers), "hexed_sword" (carries the skill "weapon_skill").
// The map "duel" is 8 x 3 plain ground: unit 0 = "ua" (blue) at x 0, unit 1 = "ub" (red) at x 1, then `extra`.
#pragma once
#include <string>

#include "srpg_skills_fixture.h"

namespace duel {

using namespace ote;
using skilltest::Copy;
using skilltest::World;

struct Options {
  std::string aSkills, dSkills;  // skill ids of the two duelists, as JSON list contents: "\"x\", \"y\""
  std::string extraUnits;        // more map units (JSON objects, each starting with a comma)
  std::string aWeapon = "plain_sword", dWeapon = "plain_sword";
  std::string aTags, dTags, cTags;  // "tags" of the three unit types, as JSON list contents: "\"scout\""
  std::string extraWeapons;         // more weapon objects, each followed by a comma
  int gap = 1;                   // the defender's x
  uint32_t seed = 7;
};

struct Duel {
  Copy c;
  World w;

  static std::string files(Copy& c, const std::string& skills, const Options& o) {
    // "hexed_sword" names "weapon_skill": every game must have it; a test that wants it to do something defines it.
    const bool defined = skills.find("\"weapon_skill\"") != std::string::npos;
    c.skills(skills + (defined ? std::string() : std::string(skills.empty() ? "" : ",") +
                                                   "{ \"id\": \"weapon_skill\", \"blocks\": [ { \"when\": \"combat\", "
                                                   "\"do\": [ { \"damage\": 0 } ] } ] }"));
    std::string types;
    const char* names[3] = {"ua", "ub", "uc"};
    for (int i = 0; i < 3; ++i) {
      const std::string& list = i == 0 ? o.aSkills : i == 1 ? o.dSkills : std::string();
      types += std::string("{ \"id\": \"") + names[i] + "\", \"name\": \"" + names[i] +
               "\", \"moveType\": \"foot\", \"stats\": { \"hp\": 30, \"atk\": 6, \"skl\": 20, \"def\": 2, \"mov\": 4 }, "
               "\"weapons\": [\"" + (i == 1 ? o.dWeapon : o.aWeapon) + "\"], \"skills\": [" + list + "], \"tags\": [" +
               (i == 0 ? o.aTags : i == 1 ? o.dTags : o.cTags) + "] },";
    }
    c.swap("data/units.json", "{ \"units\": [", "{ \"units\": [" + types);
    c.swap("data/weapons.json", "{ \"weapons\": [",
           "{ \"weapons\": [ " + o.extraWeapons + "{ \"id\": \"plain_sword\", \"name\": \"Plain Sword\" },"
           " { \"id\": \"plain_bow\", \"name\": \"Plain Bow\", \"range\": [2, 2], \"counter\": false },"
           " { \"id\": \"hexed_sword\", \"name\": \"Hexed Sword\", \"skills\": [\"weapon_skill\"] },");
    c.put("maps/duel.json",
          std::string("{ \"name\": \"Duel\", \"grid\": { \"type\": \"square\", \"width\": 8, \"height\": 3 }, "
                      "\"legend\": { \".\": \"plain\" }, \"tiles\": [\"........\", \"........\", \"........\"], "
                      "\"sides\": [ { \"id\": \"blue\", \"controller\": \"human\" }, { \"id\": \"red\", \"controller\": \"ai\" } ], "
                      "\"units\": [ { \"type\": \"ua\", \"side\": \"blue\", \"x\": 0, \"y\": 1, \"leader\": true }, "
                      "{ \"type\": \"ub\", \"side\": \"red\", \"x\": ") + std::to_string(o.gap) +
              ", \"y\": 1 }" + o.extraUnits + " ] }");
    c.swap("game.json", "\"maps\": [", "\"maps\": [ {\"id\":\"duel\",\"name\":\"Duel\",\"file\":\"maps/duel.json\"},");
    return c.dir;
  }

  // `skills` = more skill objects (JSON list body); the fixture's own are there too.
  Duel(const std::string& skills, const Options& o = Options())
      : c("k2duel"), w("duel", files(c, skills, o), false) {
    if (!w.b().db) return;  // the game did not load (World already said why)
    w.rig.battle.rng.seed(o.seed);
    w.b().start(w.events);
  }
  Battle& b() { return w.b(); }
  Unit& u(int id) { return b().units[(size_t)id]; }
  int weapon(int unit) { return b().typeOf(u(unit)).weapons[0]; }
  int skill(const char* id) { return w.skill(id); }
  Forecast forecast(int from = 0, int to = 1) {
    Forecast f;
    CHECK(b().forecast(from, to, weapon(from), u(from).cell, f));
    return f;
  }
  bool attack(int from = 0, int to = 1) { return w.apply(Action::attack(from, to, weapon(from))); }
  // The events of the last action as letters: 'A' / 'D' = the attacker / defender hit (lower case: a miss), 's' = a
  // SkillActivated, 'h' = UnitHealed, 'x' = UnitHurt.
  std::string story() const {
    std::string s;
    for (const Event& e : w.events) {
      if (e.type == EventType::Attack) {
        const char ch = e.a == 0 ? 'A' : 'D';
        s += (e.flags & kHit) ? ch : (char)(ch + 32);
      } else if (e.type == EventType::SkillActivated) s += 's';
      else if (e.type == EventType::UnitHealed) s += 'h';
      else if (e.type == EventType::UnitHurt) s += 'x';
    }
    return s;
  }
  int hp(int id) { return u(id).hp; }
};

// A skill object with one block.
inline std::string skill(const std::string& id, const std::string& block) {
  return "{ \"id\": \"" + id + "\", \"blocks\": [" + block + "] }";
}
// One block: when, optional if / chance (JSON fragments without a leading comma), do.
inline std::string block(const std::string& when, const std::string& dos, const std::string& cond = "",
                         const std::string& chance = "") {
  return "{ \"when\": \"" + when + "\"" + (cond.empty() ? "" : ", \"if\": [" + cond + "]") +
         (chance.empty() ? "" : ", \"chance\": " + chance) + ", \"do\": [" + dos + "] }";
}

}  // namespace duel
