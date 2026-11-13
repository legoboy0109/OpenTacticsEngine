#include "core/script/formulas.h"

#include "core/base/json.h"
#include "core/base/log.h"
#include "core/battle/battle.h"
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
  std::string chunk = "local a, b, w, ta, tb, dist, vs = ...\nreturn (" + expr + "\n)";
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

void Formulas::pushUnit(const Unit* u) {
  if (!u) { lua_pushnil(L_); return; }
  const UnitTypeDef& type = db_->unitTypes[u->type];
  lua_createtable(L_, 0, (int)db_->stats.size() + 7);
  for (size_t i = 0; i < db_->stats.size() && i < u->stats.size(); ++i) {
    lua_pushinteger(L_, u->stats[i]);
    lua_setfield(L_, -2, db_->stats[i].id.c_str());
  }
  // Set after the stats on purpose: if the hp stat is literally named
  // "hp", a.hp must be the CURRENT value and a.maxhp the maximum.
  if (db_->rules.statHp >= 0) {
    lua_pushinteger(L_, u->stats[db_->rules.statHp]);
    lua_setfield(L_, -2, "maxhp");
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
  lua_setfield(L_, -2, "tags");
}

double Formulas::call(int ref, bool& warned, const char* label, const FormulaCtx& ctx,
                      double def) {
  if (!ref) return def;
  lua_rawgeti(L_, LUA_REGISTRYINDEX, ref);
  pushUnit(ctx.a);
  pushUnit(ctx.b);
  pushProps(L_, ctx.w ? &ctx.w->props : nullptr);
  pushProps(L_, ctx.ta ? &ctx.ta->props : nullptr);
  pushProps(L_, ctx.tb ? &ctx.tb->props : nullptr);
  lua_pushinteger(L_, ctx.dist);
  lua_pushnumber(L_, ctx.vs);
  double result = def;
  if (lua_pcall(L_, 7, 1, 0) != LUA_OK) {
    if (!warned) OTE_ERROR("%s failed: %s", label, lua_tostring(L_, -1));
    warned = true;
  } else if (lua_isnumber(L_, -1)) {
    result = lua_tonumber(L_, -1);
  } else if (lua_isboolean(L_, -1)) {
    result = lua_toboolean(L_, -1) ? 1 : 0;
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
