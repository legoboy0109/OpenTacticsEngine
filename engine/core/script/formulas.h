// Lua host for data-defined formulas (damage, hit chance, XP...).
//
// One Lua state per loaded game. Game scripts are loaded first, then every
// formula expression is compiled once into a Lua function; eval() just
// calls it. The state is sandboxed: no file/OS access and no math.random
// (gameplay randomness must come from Battle::rng to stay deterministic).
#pragma once
#include <string>
#include <vector>

struct lua_State;

namespace ote {

struct Database;
struct Unit;
struct WeaponDef;
struct TerrainDef;

// Inputs a formula can see. Null pointers become nil / empty tables.
struct FormulaCtx {
  const Unit* a = nullptr;
  const Unit* b = nullptr;
  const WeaponDef* w = nullptr;
  const TerrainDef* ta = nullptr;
  const TerrainDef* tb = nullptr;
  int dist = 0;
  double vs = 1;
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

 private:
  bool compile(const std::string& expr, const std::string& label, int& ref, std::string& err);
  double call(int ref, bool& warned, const char* label, const FormulaCtx& ctx, double def);
  void pushUnit(const Unit* u);

  const Database* db_ = nullptr;
  lua_State* L_ = nullptr;
  int refs_[kSlotCount];        // 0 = not defined
  bool warned_[kSlotCount];
  std::vector<int> weaponRefs_; // per weapon; 0 = no override
  std::vector<char> weaponWarned_;
};

}  // namespace ote
