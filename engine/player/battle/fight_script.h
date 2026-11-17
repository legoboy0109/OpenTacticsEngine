// The battle scene's script of one fight (docs/BATTLE_SCENE.md "What plays"): the rules' events of one fight turned into the
// beats the scene plays. Pure data in, pure data out: no Battle, no renderer, so it is unit-tested thoroughly.
// Presentation only: the events stay the single source of truth, this only reads them.
#pragma once
#include <map>
#include <string>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

// Is `e` one of the events of the fight between units `u0` and `u1`? A fight's run in the event queue is: its skill
// activations, the strikes (Attack), drain, recoil, conditions, a weapon that broke, the deaths. Anything else (dialogue,
// experience, level-up, a third unit) ends it.
bool belongsToFight(const Event& e, int u0, int u1);

struct FightUnitInfo {
  int unit = -1;
  std::string name;
  int level = 1;
  int hp = 0, maxHp = 1;  // the hit points shown when the fight starts
  int x = 0;              // the map column (the unit nearer the left edge stands on the left)
};

// What the builder needs to know that the events do not say.
struct FightInfo {
  std::map<int, FightUnitInfo> units;  // by unit id (at least the two fighters)
  std::vector<std::string> skillNames, conditionNames, itemNames;  // by index; an unknown index reads as a plain word
  int distance = 1;                    // squares between the fighters (more than 1 = a ranged fight)
};

struct FightTag {
  int side = 0;  // 0 = the left fighter, 1 = the right fighter
  std::string text;
};

struct FightHp {
  int side = 0;
  int delta = 0;  // + heals (drain), - hurts (a skill's recoil)
};

struct FightStrike {
  int actor = 0;  // side index of the striker
  int weapon = -1;  // index into Database::weapons
  bool counter = false, hit = false, crit = false, kill = false, survived = false;
  int damage = 0;
  std::vector<FightTag> tags;        // skills that fired and conditions put on, shown with this strike
  std::vector<FightHp> after;        // drain and recoil, after the hit
  bool dies[2] = {false, false};     // this fighter falls after this strike
  bool weaponBroke = false;
  std::string brokeText;             // "Iron Sword broke"
  int hpBefore[2] = {0, 0};          // both fighters' hit points when the strike begins ...
  int hpAfterHit[2] = {0, 0};        // ... after the hit ...
  int hpEnd[2] = {0, 0};             // ... and after drain and recoil
};

struct FightScript {
  bool valid = false;
  FightUnitInfo side[2];  // left, right
  int initiator = 0;      // side index of the unit that started the fight (the one that attacked)
  int distance = 1;
  std::vector<FightStrike> strikes;

  bool ranged() const { return distance > 1; }
  int sideOf(int unit) const { return side[0].unit == unit ? 0 : side[1].unit == unit ? 1 : -1; }
};

// The script of `run` (events for which belongsToFight held). Not valid when the run holds no strike.
FightScript buildFightScript(const std::vector<Event>& run, const FightInfo& info);

}  // namespace ote
