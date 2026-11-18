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
  bool human = false;     // the unit's side is controlled by a person (game.json "battleSceneSides")
};

// Which fighter stands on the left of the scene (docs/BATTLE_SCENE.md "Which side each fighter stands on"). `mode`: 0 = "map" (the unit
// nearer the left edge of the map; on the same column the one that started the fight), 1 = "playerRight", 2 = "playerLeft" (a person's
// fighter on the right / left, the computer's opposite). When both or neither fighter is a person's, the map rule decides.
bool initiatorStandsLeft(int mode, bool initiatorHuman, bool otherHuman, int initiatorX, int otherX);

// A staff use in the scene (docs/BATTLE_SCENE.md "Staves in the scene"): is `e` one of the entries of the use that `StaffUsed` (user, target) began?
// Its results on the target; the user's own entries after it (broken staff, experience) are not part of it.
bool belongsToStaffUse(const Event& e, int user, int target);
// Does the use `used` (a StaffUsed entry) play as a scene? It needs one picked unit that is not the user and exactly one unit affected, and
// none of the entries that follow it (`following`, in order; only the staff's own are looked at) may be a unit moved (a warp, a pull, a swap stay on the map).
bool staffUseInScene(const Event& used, const std::vector<Event>& following);

// What the builder needs to know that the events do not say.
struct FightInfo {
  std::map<int, FightUnitInfo> units;  // by unit id (at least the two fighters)
  std::vector<std::string> skillNames, conditionNames, itemNames;  // by index; an unknown index reads as a plain word
  int distance = 1;                    // squares between the fighters (more than 1 = a ranged fight)
  int sidesMode = 0;                   // game.json "battleSceneSides": 0 = map, 1 = playerRight, 2 = playerLeft
};

struct FightTag {
  int side = 0;  // 0 = the left fighter, 1 = the right fighter
  std::string text;
  int kind = 0;  // a staff use's notes (FightStrike::notes): 0 = a condition, 1 = an item repaired
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
  // A staff used on one other unit (docs/BATTLE_SCENE.md "Staves in the scene"): the one strike of such a script. `actor` is the user,
  // `item` the staff (index into Database::items). It lands (`hit`) unless `missed`; `healed` hit points came back to the target (hpAfterHit
  // already holds them); `notes` are the labels that show on the target after the effect (a condition put on, resisted or cured; an item mended).
  bool staff = false, missed = false;
  int item = -1, healed = 0;
  std::vector<FightTag> notes;
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
  bool staff() const { return !strikes.empty() && strikes[0].staff; }  // a staff use, not a fight

  bool ranged() const { return distance > 1; }
  int sideOf(int unit) const { return side[0].unit == unit ? 0 : side[1].unit == unit ? 1 : -1; }
};

// The script of `run` (events for which belongsToFight held). Not valid when the run holds no strike.
FightScript buildFightScript(const std::vector<Event>& run, const FightInfo& info);

}  // namespace ote
