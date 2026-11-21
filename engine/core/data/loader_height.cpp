// Loader of the height rules. Every refusal names the file, the entry and the key.
#include "core/data/loader_height.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyThese(const LoadCtx& c, const Json& block, const std::string& at, std::initializer_list<const char*> keys) {
  return c.onlyKeys(block, keys, at);
}

constexpr int kMaxLevels = 999;  // the most any height number may be (maps hold far fewer levels)

// A jump written as n or { "up": n, "down": m }: `up` / `down` keep their value for a missing half.
bool readJumpValue(const LoadCtx& c, const Json& holder, const char* key, const std::string& where, int& up, int& down) {
  const Json& v = holder[key];
  if (v.isObject()) {
    const std::string at = where + ", " + q(key);
    if (!onlyThese(c, v, at, {"up", "down"})) return false;
    return c.integerIn(v, "up", at, 0, kMaxLevels, up) && c.integerIn(v, "down", at, 0, kMaxLevels, down);
  }
  if (!LoadCtx::isWhole(v) || v.asInt() < 0 || v.asInt() > kMaxLevels)
    return c.fail(where, q(key) + " must be a whole number from 0 to " + std::to_string(kMaxLevels) + " or { \"up\": n, \"down\": m }");
  up = down = v.asInt();
  return true;
}

}  // namespace

bool loadHeightRules(const LoadCtx& c, const Json& rules, Database& db) {
  HeightRules& h = db.rules.height;
  h = HeightRules();
  if (!rules.has("height")) return true;
  const Json* blk;
  if (!c.object(rules, "height", "rules", blk, true)) return false;
  const std::string at = "rules.height";
  if (!onlyThese(c, *blk, at, {"jump", "flyingMoveTypes", "rangeBonus", "lineOfSight"})) return false;
  if (blk->has("jump")) {
    const Json* j;
    if (!c.object(*blk, "jump", at, j, true)) return false;
    if (!onlyThese(c, *j, at + ", \"jump\"", {"default"})) return false;
    if (j->has("default") && !readJumpValue(c, *j, "default", at + ", \"jump\"", h.jumpUp, h.jumpDown)) return false;
  }
  if (blk->has("flyingMoveTypes")) {
    std::vector<std::string> ids;
    if (!c.stringList(*blk, "flyingMoveTypes", at, ids)) return false;
    for (const std::string& id : ids) {
      const int mt = db.findMoveType(id);
      if (mt < 0) return c.fail(at, "\"flyingMoveTypes\" names unknown move type " + q(id) + " (list it under \"moveTypes\" in game.json)");
      if (!h.flies(mt)) h.fliers.push_back(mt);
    }
  }
  if (blk->has("rangeBonus")) {
    const Json* r;
    if (!c.object(*blk, "rangeBonus", at, r, true)) return false;
    const std::string w = at + ", \"rangeBonus\"";
    if (!onlyThese(c, *r, w, {"perLevels", "max"})) return false;
    if (!r->has("perLevels")) return c.fail(w, "needs \"perLevels\" (the levels of height that give one more square of range)");
    if (!r->has("max")) return c.fail(w, "needs \"max\" (the most extra squares of range)");
    if (!c.integerIn(*r, "perLevels", w, 1, kMaxLevels, h.rangeEvery) || !c.integerIn(*r, "max", w, 0, 8, h.rangeMax)) return false;
    if (h.rangeMax == 0) h.rangeEvery = 0;  // a bonus of at most 0 is no bonus
  }
  if (!c.boolean(*blk, "lineOfSight", at, h.lineOfSight)) return false;
  h.on = true;
  return true;
}

bool readWeaponHeight(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w) {
  w.heightRange = HeightRange();
  w.area.vertical = -1;
  if (e.has("heightRange")) {
    if (!db.rules.height.on) return c.fail(where, "\"heightRange\" needs \"rules\": { \"height\": ... } in game.json");
    const Json* r;
    if (!c.object(e, "heightRange", where, r, true)) return false;
    const std::string at = where + ", \"heightRange\"";
    if (!onlyThese(c, *r, at, {"up", "down"})) return false;
    if (r->size() == 0) return c.fail(at, "needs \"up\" and / or \"down\" (how many levels higher / lower the target may stand)");
    if (!c.integerIn(*r, "up", at, 0, kMaxLevels, w.heightRange.up) || !c.integerIn(*r, "down", at, 0, kMaxLevels, w.heightRange.down)) return false;
  }
  if (e.has("area") && e["area"].isObject() && e["area"].has("vertical")) {
    if (!db.rules.height.on) return c.fail(where, "\"area\".\"vertical\" needs \"rules\": { \"height\": ... } in game.json");
    if (!c.integerIn(e["area"], "vertical", where + ", \"area\"", 0, kMaxLevels, w.area.vertical)) return false;
  }
  return true;
}

bool readUnitJump(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u) {
  u.jumpUp = u.jumpDown = -1;
  if (!e.has("jump")) return true;
  if (!db.rules.height.on) return c.fail(where, "\"jump\" needs \"rules\": { \"height\": ... } in game.json");
  int up = db.rules.height.jumpUp, down = db.rules.height.jumpDown;  // a missing half keeps the game's default
  if (!readJumpValue(c, e, "jump", where, up, down)) return false;
  u.jumpUp = up;
  u.jumpDown = down;
  return true;
}

}  // namespace ote
