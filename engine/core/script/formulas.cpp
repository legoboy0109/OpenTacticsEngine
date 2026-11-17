#include "core/script/formulas.h"

#include "core/base/json.h"
#include "core/base/log.h"
#include "core/battle/battle.h"
#include "core/battle/unit_stats.h"
#include "lua.hpp"

namespace ote {

static const char* kSlotNames[Formulas::kSlotCount] = {
    "formulas.damage", "formulas.hit", "formulas.crit", "formulas.strikes",
    "progression.xpHit", "progression.xpKill", "capture.amount"};

Formulas::Formulas() {
  for (int i = 0; i < kSlotCount; ++i) { refs_[i] = 0; warned_[i] = false; }
}

Formulas::~Formulas() {
  if (L_) lua_close(L_);
}

static void openSandboxedLibs(lua_State* L) {
  static const luaL_Reg libs[] = {{LUA_GNAME, luaopen_base},
                                  {LUA_TABLIBNAME, luaopen_table},
                                  {LUA_STRLIBNAME, luaopen_string},
                                  {LUA_MATHLIBNAME, luaopen_math},
                                  {nullptr, nullptr}};
  for (const luaL_Reg* lib = libs; lib->func; ++lib) {
    luaL_requiref(L, lib->name, lib->func, 1);
    lua_pop(L, 1);
  }
  // Remove file access and non-deterministic randomness.
  static const char* removedGlobals[] = {"dofile", "loadfile", nullptr};
  for (const char** g = removedGlobals; *g; ++g) {
    lua_pushnil(L);
    lua_setglobal(L, *g);
  }
  lua_getglobal(L, "math");
  lua_pushnil(L); lua_setfield(L, -2, "random");
  lua_pushnil(L); lua_setfield(L, -2, "randomseed");
  lua_pop(L, 1);
}

bool Formulas::compile(const std::string& expr, const std::string& label, int& ref,
                       std::string& err) {
  ref = 0;
  if (expr.empty()) return true;
  // Wrapped as a vararg chunk so the expression sees its inputs as locals.
  std::string chunk = "local a, b, w, ta, tb, dist, vs, fw, turn = ...\nreturn (" + expr + "\n)";
  if (luaL_loadbuffer(L_, chunk.data(), chunk.size(), label.c_str()) != LUA_OK) {
    err = label + ": " + lua_tostring(L_, -1);
    lua_pop(L_, 1);
    return false;
  }
  ref = luaL_ref(L_, LUA_REGISTRYINDEX);
  return true;
}

bool Formulas::init(const Database& db, std::string& err) {
  db_ = &db;
  if (L_) lua_close(L_);
  L_ = luaL_newstate();
  if (!L_) { err = "out of memory creating Lua state"; return false; }
  openSandboxedLibs(L_);
  // Metatable of an empty `fw`: `fw.might + 1` must not raise, so a missing number reads 0. (A real weapon's table
  // has no metatable: a prop it lacks is nil, like in `w`.)
  // The one exception is `answers`: `fw.answers` must be nil (false), not a truthy 0, when there is no foe weapon.
  luaL_dostring(L_, "return { __index = function(_, k) if k == 'answers' then return nil end return 0 end }");
  emptyWeaponMeta_ = luaL_ref(L_, LUA_REGISTRYINDEX);

  for (const std::string& rel : db.scripts) {
    std::string path = db.rootPath + "/" + rel;
    std::string source;
    if (!readTextFile(path, source)) { err = path + ": cannot read script"; return false; }
    std::string chunkName = "@" + rel;
    if (luaL_loadbuffer(L_, source.data(), source.size(), chunkName.c_str()) != LUA_OK ||
        lua_pcall(L_, 0, 0, 0) != LUA_OK) {
      err = std::string(lua_tostring(L_, -1));
      lua_pop(L_, 1);
      return false;
    }
  }

  const RulesConfig& r = db.rules;
  const std::string* exprs[kSlotCount] = {&r.fDamage, &r.fHit, &r.fCrit, &r.fStrikes,
                                          &r.fXpHit, &r.fXpKill, &r.fCaptureAmount};
  for (int i = 0; i < kSlotCount; ++i) {
    warned_[i] = false;
    if (!compile(*exprs[i], kSlotNames[i], refs_[i], err)) return false;
  }
  weaponRefs_.assign(db.weapons.size(), 0);
  weaponWarned_.assign(db.weapons.size(), 0);
  for (size_t i = 0; i < db.weapons.size(); ++i) {
    std::string label = "weapon '" + db.weapons[i].id + "' damage";
    if (!compile(db.weapons[i].damageFormula, label, weaponRefs_[i], err)) return false;
  }
  // The expressions the strategy-RPG data registered (docs/SRPG.md): a syntax error names the place it was written.
  exprRefs_.assign(db.exprs.size(), 0);
  exprWarned_.assign(db.exprs.size(), 0);
  for (size_t i = 0; i < db.exprs.size(); ++i) {
    if (db.exprs[i].text.empty()) {
      err = db.exprs[i].label + ": the formula is empty";
      return false;
    }
    if (!compile(db.exprs[i].text, db.exprs[i].label, exprRefs_[i], err)) return false;
  }
  return true;
}

static void pushProps(lua_State* L, const Props* props) {
  lua_createtable(L, 0, props ? (int)props->size() : 0);
  if (!props) return;
  for (const NamedNumber& n : *props) {
    lua_pushnumber(L, n.value);
    lua_setfield(L, -2, n.key.c_str());
  }
}

// A weapon as formulas see it (`w`, `fw`): its props as numbers, plus `id` and `group` as text (group "" when the
// weapon has none), so a formula can ask `w.group == "sword"`. A prop the maker named "id" or "group" wins, as before.
// `answers` (only `fw` in a fight where the foe really answers) is set before the props, so a prop of that name wins.
static void pushWeapon(lua_State* L, const WeaponDef* w, bool answers = false) {
  if (!w) { pushProps(L, nullptr); return; }
  lua_createtable(L, 0, (int)w->props.size() + 3);
  lua_pushstring(L, w->id.c_str()); lua_setfield(L, -2, "id");
  lua_pushstring(L, w->group.c_str()); lua_setfield(L, -2, "group");
  lua_pushstring(L, w->damageKind.c_str()); lua_setfield(L, -2, "damageKind");  // "" when the weapon has none
  if (answers) { lua_pushinteger(L, 1); lua_setfield(L, -2, "answers"); }
  for (const NamedNumber& n : w->props) {
    lua_pushnumber(L, n.value);
    lua_setfield(L, -2, n.key.c_str());
  }
}

// `fw`: the props of the foe's weapon like `w`; without one an empty table marked `none = 1` whose missing numbers
// read 0 (see emptyWeaponMeta_), so a formula can test `fw.none` or just compute with it.
void Formulas::pushFoeWeapon(const WeaponDef* fw, bool answers) {
  if (fw) { pushWeapon(L_, fw, answers); return; }
  lua_createtable(L_, 0, 4);
  lua_pushinteger(L_, 1);
  lua_setfield(L_, -2, "none");
  lua_pushstring(L_, ""); lua_setfield(L_, -2, "damageKind");
  lua_pushstring(L_, ""); lua_setfield(L_, -2, "id");     // strings, so `fw.group == "sword"` is simply false
  lua_pushstring(L_, ""); lua_setfield(L_, -2, "group");
  lua_rawgeti(L_, LUA_REGISTRYINDEX, emptyWeaponMeta_);
  lua_setmetatable(L_, -2);
}

void Formulas::pushUnit(const Unit* u, const FormulaCtx& ctx, const std::vector<int>* bonus, int follow, int tri) {
  if (!u) { lua_pushnil(L_); return; }
  const UnitTypeDef& type = db_->unitTypes[u->type];
  // A stat as the rules see it: through the battle when there is one (skills, carrying), else its own value with
  // what its accessories and research add; plus this evaluation's bonus.
  auto statOf = [&](int index) {
    int v = ctx.battle ? ctx.battle->stat(*u, index) : unitStat(*db_, *u, index, ctx.sides);
    if (bonus && (size_t)index < bonus->size() && (*bonus)[(size_t)index] != 0) {
      v += (*bonus)[(size_t)index];
      if (v < 0) v = 0;
    }
    return v;
  };
  lua_createtable(L_, 0, (int)db_->stats.size() + 10);
  // Set before the stats, so a stat of that name wins (as with `character`).
  lua_pushinteger(L_, follow); lua_setfield(L_, -2, "followShift");
  lua_pushinteger(L_, tri); lua_setfield(L_, -2, "trianglePercent");
  // The sum of the unit's "carryBonus" skills (docs/SRPG.md "Carry capacity"); 0 when none. Only `canCarry` has a use for it.
  lua_pushinteger(L_, ctx.battle ? ctx.battle->carryBonusOf(*u) : 0); lua_setfield(L_, -2, "carryBonus");
  for (size_t i = 0; i < db_->stats.size() && i < u->stats.size(); ++i) {
    lua_pushinteger(L_, statOf((int)i));
    lua_setfield(L_, -2, db_->stats[i].id.c_str());
  }
  // Set after the stats on purpose: if the hp stat is literally named
  // "hp", a.hp must be the CURRENT value and a.maxhp the maximum.
  if (db_->rules.statHp >= 0) {
    lua_pushinteger(L_, statOf(db_->rules.statHp));
    lua_setfield(L_, -2, "maxhp");
  }
  // The character id (docs/SRPG.md); the field is simply absent (nil) for a unit without one.
  if (u->character >= 0 && (size_t)u->character < db_->characters.size()) {
    lua_pushstring(L_, db_->characters[(size_t)u->character].id.c_str());
    lua_setfield(L_, -2, "character");
  }
  // Weapon ranks (docs/SRPG.md section 7): a.rank.sword = the rank's index (0 = lowest); absent in a game without ranks.
  if (db_->ranks.on()) {
    lua_createtable(L_, 0, (int)db_->ranks.groups.size());
    for (size_t g = 0; g < db_->ranks.groups.size(); ++g) {
      const int xp = g < u->weaponXp.size() ? u->weaponXp[g] : 0;
      lua_pushinteger(L_, db_->ranks.rankAt(xp));
      lua_setfield(L_, -2, db_->ranks.groups[g].c_str());
    }
    lua_setfield(L_, -2, "rank");
  }
  lua_pushinteger(L_, u->hp); lua_setfield(L_, -2, "hp");
  lua_pushinteger(L_, u->level); lua_setfield(L_, -2, "level");
  lua_pushinteger(L_, u->xp); lua_setfield(L_, -2, "xp");
  lua_pushstring(L_, type.id.c_str()); lua_setfield(L_, -2, "type");
  lua_pushstring(L_, u->name.c_str()); lua_setfield(L_, -2, "name");
  lua_createtable(L_, 0, (int)type.tags.size());
  for (const std::string& tag : type.tags) {
    lua_pushboolean(L_, 1);
    lua_setfield(L_, -2, tag.c_str());
  }
  if (u->character >= 0 && (size_t)u->character < db_->characters.size())  // the character's own tags too
    for (const std::string& tag : db_->characters[(size_t)u->character].tags) {
      lua_pushboolean(L_, 1);
      lua_setfield(L_, -2, tag.c_str());
    }
  lua_setfield(L_, -2, "tags");
}

double Formulas::call(int ref, bool& warned, const char* label, const FormulaCtx& ctx,
                      double def, bool nilIsZero) {
  if (!ref) return def;
  lua_rawgeti(L_, LUA_REGISTRYINDEX, ref);
  pushUnit(ctx.a, ctx, ctx.bonusA, ctx.followA, ctx.triA);
  pushUnit(ctx.b, ctx, ctx.bonusB, ctx.followB, ctx.triB);
  pushWeapon(L_, ctx.w);
  pushProps(L_, ctx.ta);
  pushProps(L_, ctx.tb);
  lua_pushinteger(L_, ctx.dist);
  lua_pushnumber(L_, ctx.vs);
  pushFoeWeapon(ctx.fw, ctx.fwAnswers);
  lua_pushinteger(L_, ctx.battle ? ctx.battle->round : 0);  // `turn`: the battle's turn (round) number, 0 outside a battle
  double result = def;
  if (lua_pcall(L_, 9, 1, 0) != LUA_OK) {
    if (!warned) OTE_ERROR("%s failed: %s", label, lua_tostring(L_, -1));
    warned = true;
  } else if (lua_isnumber(L_, -1)) {
    result = lua_tonumber(L_, -1);
  } else if (lua_isboolean(L_, -1)) {
    result = lua_toboolean(L_, -1) ? 1 : 0;
  } else if (nilIsZero && lua_isnil(L_, -1)) {
    result = 0;  // a yes/no expression such as `a.level >= 10 and b.tags.flier`
  } else {
    if (!warned) OTE_ERROR("%s did not return a number (got %s)", label, luaL_typename(L_, -1));
    warned = true;
  }
  lua_pop(L_, 1);
  return result;
}

double Formulas::eval(Slot slot, const FormulaCtx& ctx, double def) {
  return call(refs_[slot], warned_[slot], kSlotNames[slot], ctx, def);
}

double Formulas::evalExpr(int index, const FormulaCtx& ctx, double def) {
  if (index < 0 || index >= (int)exprRefs_.size()) return def;
  bool warned = exprWarned_[(size_t)index] != 0;
  double v = call(exprRefs_[(size_t)index], warned, db_->exprs[(size_t)index].label.c_str(), ctx, def, true);
  exprWarned_[(size_t)index] = warned;
  return v;
}

double Formulas::evalDamage(int weaponIndex, const FormulaCtx& ctx) {
  if (weaponIndex >= 0 && weaponIndex < (int)weaponRefs_.size() && weaponRefs_[weaponIndex]) {
    bool warned = weaponWarned_[weaponIndex] != 0;
    double v = call(weaponRefs_[weaponIndex], warned, "weapon damage formula", ctx, 0);
    weaponWarned_[weaponIndex] = warned;
    return v;
  }
  return eval(Damage, ctx, 0);
}

}  // namespace ote
