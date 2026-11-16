// Shared by the combat pipeline tests (docs/SRPG.md step F2): a duel on a strip of plain ground in the miniature
// game of battle_fixture.h, with Battle::testMods attached so a test can hand the fighters any modifier before a
// game has skills or supports.
//
// Numbers of the miniature game that the tests lean on: a soldier (hp 10, atk 5, def 2, sword at range 1) does
// 3 to another soldier; an archer (hp 8, atk 4, def 1, bow at range 2 only, never answers); a tank (hp 20, atk 8,
// def 5, cannon at range 1). Unit 0 (blue, at x = 0) attacks unit 1 (red, at x = `gap`).
#pragma once
#include "battle_fixture.h"

#include "core/battle/combat_mods.h"

namespace combattest {

using namespace fixture;

struct Duel {
  World w;
  CombatTestMods mods;

  explicit Duel(const char* attacker = "soldier", const char* defender = "soldier", int gap = 1)
      : w({"......"}, {{attacker, 0, 0, 0, false}, {defender, 1, gap, 0, false}}) {}

  bool begin(uint32_t seed = 1) {
    if (!w.begin(seed)) return false;
    w.battle.testMods = &mods;
    return true;
  }
  // The same modifiers for every forecast and strike of the fight.
  void combat(std::function<void(CombatMods& attacker, CombatMods& defender)> fn) {
    mods.combat = [fn](const Battle&, const CombatSetup&, CombatMods& a, CombatMods& d) { fn(a, d); };
  }
  // The same strike effects in the forecast (as blocks without a chance) and in the fight. `counter` = the
  // defender is the one striking.
  void strike(std::function<void(bool counter, StrikeMods&)> fn) {
    mods.passive = [fn](const Battle&, const CombatSetup&, bool counter, StrikeMods& s) { fn(counter, s); };
    mods.strike = [fn](Battle&, const CombatSetup&, bool counter, StrikeMods& s, std::vector<Event>&,
                       std::vector<Event>&) { fn(counter, s); };
  }
  int weaponOf(int unit) const { return w.db.unitTypes[(size_t)w.battle.units[(size_t)unit].type].weapons[0]; }
  Forecast forecast() {
    Forecast f;
    CHECK(w.battle.forecast(0, 1, weaponOf(0), w.battle.units[0].cell, f));
    return f;
  }
  bool attack() { return w.act(Action::attack(0, 1, weaponOf(0))); }
  int hp(int unit) const { return w.battle.units[(size_t)unit].hp; }
  // The fight as text, one letter per event: 'A' = the attacker struck, 'D' = the defender struck (lower case =
  // a miss), 'h' = someone healed, 's' = a SkillActivated event.
  std::string story() const {
    std::string s;
    for (const Event& e : w.events) {
      if (e.type == EventType::Attack) {
        char c = e.a == 0 ? 'A' : 'D';
        s += (e.flags & kHit) ? c : (char)(c + 32);
      } else if (e.type == EventType::UnitHealed) {
        s += 'h';
      } else if (e.type == EventType::SkillActivated) {
        s += 's';
      }
    }
    return s;
  }
};

}  // namespace combattest
