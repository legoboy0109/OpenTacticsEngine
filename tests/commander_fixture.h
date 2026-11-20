// Shared by the commander tests (docs/COMMANDERS.md): the fixture game tests/fixtures/commanders and a few helpers.
//
// Numbers of the fixture: damage = attacker str - defender def. Soldier hp 20 str 7 def 2 mov 3, knight 28 / 9 / 4 / 4, golem 36 / 8 / 5 / 2,
// archer 16 / 6 / 1 / 3. Skills: bulwark def +2, keen str +1, fleet mov +1, frenzy str +3, brittle def -2.
// Commanders: ada (passive bulwark for infantry; meter 100, 2 per damage dealt, 1 per damage taken; powers rally 40 = frenzy 1 turn + heal 5, and
// surge 80 = fleet 2 turns + refresh, +50% per use), bram (passive fleet, brittle for heavy; meter 60, 1 per damage dealt, 10 per turn; storm 30 =
// 4 damage to every enemy + 25 gold, instant), cato (no passive; meter 50, 3 per damage taken; wall 20 = bulwark 3 turns, mend 25 = heal 8 for heavy).
// Factions: order (keen for infantry; commanders ada, cato), horde (bram), free (any). Map "field": blue (human, order, ada) and red (ai, horde,
// bram); "pick": everything "choose"; "duel": two computer sides choosing; "plain": no factions, no commanders.
#pragma once
#include <string>

#include "core/ai/ai.h"
#include "json_edit.h"
#include "srpg_skills_fixture.h"

namespace cmdtest {

using namespace ote;
using skilltest::Copy;
using skilltest::saysIn;
using skilltest::World;

inline std::string fixtureDir() { return ote_test::sourceDir() + "/tests/fixtures/commanders"; }

inline int commanderIndex(World& w, const char* id) { return w.db().findCommander(id); }
inline int powerIndex(World& w, const char* commander, const char* power) {
  const CommanderDef& d = w.db().commanders[(size_t)commanderIndex(w, commander)];
  for (size_t i = 0; i < d.powers.size(); ++i)
    if (d.powers[i].id == power) return (int)i;
  return -1;
}
inline int gold(World& w, int side) { return w.b().sides[(size_t)side].resources[(size_t)w.db().findResource("gold")]; }

// The sum of the damage the events say one side's units dealt (Attack events, counters included).
inline int dealtBy(const Battle& b, const std::vector<Event>& events, int side) {
  int sum = 0;
  for (const Event& e : events)
    if (e.type == EventType::Attack && b.units[(size_t)e.a].side == side) sum += e.d;
  return sum;
}

// Blue's soldier walks next to the red leader and hits it (the foe counters): returns false when anything was refused.
inline bool soldierHitsFoe(World& w) {
  const bool moved = w.apply(Action::move(w.id("sol"), w.at(4, 1)));
  return moved && w.apply(Action::attack(w.id("sol"), w.id("foe"), w.db().findWeapon("sword")));
}

// The game's load message after one text of data/commanders.json was replaced ("" = it loaded).
inline std::string commandersError(const char* name, const std::string& from, const std::string& to) {
  Copy c(name, "commanders");
  c.swap("data/commanders.json", from, to);
  return c.gameError();
}

// A fresh battle on the same game and map as `w` with the saved state of `w` put on it.
inline bool restoreInto(World& w, Battle& r, std::string& err) {
  Json j;
  saveBattleState(w.b(), j);
  Json parsed;
  if (!Json::parse(j.dump(0), parsed, err)) return false;
  return r.init(w.rig.db, w.rig.map, w.rig.formulas, 7, err) && restoreBattleState(r, parsed, err);
}

}  // namespace cmdtest
