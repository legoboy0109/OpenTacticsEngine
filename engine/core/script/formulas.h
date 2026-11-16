// Lua host for data-defined formulas (damage, hit chance, XP...).
//
// One Lua state per loaded game. Game scripts are loaded first, then every
// formula expression is compiled once into a Lua function; eval() just
// calls it. The state is sandboxed: no file/OS access and no math.random
// (gameplay randomness must come from Battle::rng to stay deterministic).
#pragma once
#include <string>
#include <vector>

#include "core/base/types.h"

struct lua_State;

namespace ote {

struct Database;
class Battle;
struct Unit;
struct SideState;
struct WeaponDef;

// Inputs a formula can see. Null pointers become nil / empty tables.
struct FormulaCtx {
  const Unit* a = nullptr;
  const Unit* b = nullptr;
  const WeaponDef* w = nullptr;
  // The weapon the OTHER fighter fights with (`fw` in a formula, docs/SRPG.md "Foe's weapon"): its answering weapon
  // when `a` strikes, the attacker's weapon in a counter. Null = none (unarmed, cannot wield, out of ammunition) or
  // a context without a fight: the formula then sees the "empty" table (`fw.none`), never nil.
  const WeaponDef* fw = nullptr;
  // `fw.answers = 1`: the foe really answers with `fw` in this fight (in range, may counter). Only the strike formulas set it
  // (Battle::strikeForecast, from CombatSetup::counters); everywhere else `fw.answers` is absent. A prop of the weapon named
  // "answers" wins, like "id" and "group".
  bool fwAnswers = false;
  // Numbers of the square under `a` / `b` (Battle::numbersAt: terrain merged with structure).
  const Props* ta = nullptr;
  const Props* tb = nullptr;
  int dist = 0;
  double vs = 1;
  // The battle's sides: a unit's stats then include its side's research bonuses (null = items only).
  const std::vector<SideState>* sides = nullptr;
  // The battle the units are in: their stats are then the stats as the rules see them (Battle::stat: accessories,
  // research, standing skills, carry penalty / pair bonus; docs/SRPG.md). Every caller inside a battle sets it;
  // null = no battle (unitStat with `sides`, as before).
  const Battle* battle = nullptr;
  // Per stat index, added to the stats of `a` / `b` for this evaluation (the combat bonuses of skills, auras and
  // supports); null = nothing. A stat with a bonus does not go below 0.
  const std::vector<int>* bonusA = nullptr;
  const std::vector<int>* bonusB = nullptr;
};

class Formulas {
 public:
  enum Slot { Damage, Hit, Crit, Strikes, XpHit, XpKill, CaptureAmount, kSlotCount };

  Formulas();
  ~Formulas();
  Formulas(const Formulas&) = delete;
  Formulas& operator=(const Formulas&) = delete;

  // Loads db.scripts and compiles all formulas. Fails with a message naming
  // the script/formula on syntax errors. db must outlive this object.
  bool init(const Database& db, std::string& err);

  bool has(Slot slot) const { return refs_[slot] != 0; }
  // Evaluates a rules formula; returns def if absent or on runtime error
  // (errors are logged once per formula, not every call).
  double eval(Slot slot, const FormulaCtx& ctx, double def);
  // Damage using the weapon's own formula if it has one, else Slot Damage.
  double evalDamage(int weaponIndex, const FormulaCtx& ctx);
  // Evaluates expression `index` of Database::exprs (docs/SRPG.md "Formulas in this document"): a number, or
  // 1 / 0 for true / false; nil counts as 0 (so `b.tags.flier` works as a yes/no). Returns def for an index
  // out of range and on a run-time error (logged once per expression).
  double evalExpr(int index, const FormulaCtx& ctx, double def);
  int exprCount() const { return (int)exprRefs_.size(); }

 private:
  bool compile(const std::string& expr, const std::string& label, int& ref, std::string& err);
  double call(int ref, bool& warned, const char* label, const FormulaCtx& ctx, double def, bool nilIsZero = false);
  void pushFoeWeapon(const WeaponDef* fw, bool answers);
  void pushUnit(const Unit* u, const FormulaCtx& ctx, const std::vector<int>* bonus);

  const Database* db_ = nullptr;
  lua_State* L_ = nullptr;
  int emptyWeaponMeta_ = 0;     // registry ref: metatable of an empty `fw` (every missing number reads 0)
  int refs_[kSlotCount];        // 0 = not defined
  bool warned_[kSlotCount];
  std::vector<int> weaponRefs_; // per weapon; 0 = no override
  std::vector<char> weaponWarned_;
  std::vector<int> exprRefs_;   // per Database::exprs
  std::vector<char> exprWarned_;
};

}  // namespace ote
