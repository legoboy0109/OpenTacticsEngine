// The "battle" part of data/graphics.json (docs/BATTLE_SCENE.md): fighter animation sets, weapon effects, which set a
// unit type / character / weapon group uses, and the scene's backgrounds. Presentation only.
#include <algorithm>

#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {
namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool readFrameCell(const LoadCtx& c, const SheetDef& sh, const Json& j, const std::string& where, SpriteCell& out) {
  if (!j.isArray() || j.size() != 2 || !LoadCtx::isWhole(j[0]) || !LoadCtx::isWhole(j[1]) || j[0].asInt() < 0 || j[1].asInt() < 0)
    return c.fail(where, "\"frames\" must be a list of [column, row] cells (whole numbers, counted from 0)");
  out.col = (int16_t)std::min(j[0].asInt(), 30000);
  out.row = (int16_t)std::min(j[1].asInt(), 30000);
  if (out.col >= sh.cols || out.row >= sh.rows)
    return c.fail(where, "frame [" + std::to_string(j[0].asInt()) + ", " + std::to_string(j[1].asInt()) + "] is outside sheet " + q(sh.id) +
                             " (" + std::to_string(sh.cols) + " x " + std::to_string(sh.rows) + " cells of " + std::to_string(sh.cellW) + " x " +
                             std::to_string(sh.cellH) + ")");
  return true;
}

// One animation object: sheet, frames, ms, loop, hit, spawn.
bool readAnim(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, bool loopDefault, BattleAnim& out) {
  out = BattleAnim();
  if (!j.isObject()) return c.fail(where, "must be an object with \"sheet\" and \"frames\"");
  std::string sheetId;
  if (!c.str(j, "sheet", where, sheetId, true)) return false;
  out.sheet = db.graphics.findSheet(sheetId);
  if (out.sheet < 0) return c.fail(where, "unknown sheet " + q(sheetId));
  const SheetDef& sh = db.graphics.sheets[(size_t)out.sheet];
  const Json* frames;
  if (!c.array(j, "frames", where, frames, true)) return false;
  if (frames->size() < 1 || frames->size() > 64) return c.fail(where, "\"frames\" must list between 1 and 64 cells");
  for (size_t i = 0; i < frames->size(); ++i) {
    SpriteCell cell;
    if (!readFrameCell(c, sh, (*frames)[i], where, cell)) return false;
    out.frames.push_back(cell);
  }
  // "ms": one number for every frame, or a list with one number per frame. Default 120.
  const int n = (int)out.frames.size();
  out.ms.assign((size_t)n, 120);
  if (j.has("ms")) {
    const Json& m = j["ms"];
    auto okMs = [](const Json& v) { return LoadCtx::isWhole(v) && v.asInt() >= 10 && v.asInt() <= 10000; };
    if (m.isNumber()) {
      if (!okMs(m)) return c.fail(where, "\"ms\" must be between 10 and 10000");
      out.ms.assign((size_t)n, m.asInt());
    } else if (m.isArray()) {
      if ((int)m.size() != n)
        return c.fail(where, "\"ms\" lists " + std::to_string(m.size()) + " times but \"frames\" has " + std::to_string(n) + " frames (one time per frame)");
      for (int i = 0; i < n; ++i) {
        if (!okMs(m[(size_t)i])) return c.fail(where, "each of \"ms\" must be a whole number between 10 and 10000");
        out.ms[(size_t)i] = m[(size_t)i].asInt();
      }
    } else {
      return c.fail(where, "\"ms\" must be a number or a list of numbers (milliseconds per frame)");
    }
  }
  out.loop = loopDefault;
  if (!c.boolean(j, "loop", where, out.loop)) return false;
  for (const char* key : {"hit", "spawn"}) {
    if (!j.has(key)) continue;
    int v = 0;
    if (!c.integer(j, key, where, v)) return false;
    if (v < 0 || v >= n) return c.fail(where, q(key) + " is frame " + std::to_string(v) + " but the animation has frames 0 to " + std::to_string(n - 1));
    (std::string(key) == "hit" ? out.hit : out.spawn) = v;
  }
  return true;
}

bool readAnchor(const LoadCtx& c, const Json& j, const std::string& where, int& x, int& y) {
  if (!j.has("anchor")) return true;
  const Json& a = j["anchor"];
  if (!a.isArray() || a.size() != 2 || !LoadCtx::isWhole(a[0]) || !LoadCtx::isWhole(a[1]) || a[0].asInt() < 0 || a[1].asInt() < 0 ||
      a[0].asInt() > 4096 || a[1].asInt() > 4096)
    return c.fail(where, "\"anchor\" must be [x, y] in pixels from the top-left of a frame");
  x = a[0].asInt();
  y = a[1].asInt();
  return true;
}

bool readSets(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* sets;
  if (!c.object(b, "sets", "battle", sets)) return false;
  for (const Json::Member& e : sets->members()) {
    const std::string where = "battle set " + q(e.first);
    if (e.first.empty()) return c.fail("battle", "a set's id must not be empty");
    if (!e.second.isObject()) return c.fail(where, "must be an object");
    BattleSet s;
    s.id = e.first;
    std::string faces = "right";
    if (!c.str(e.second, "faces", where, faces)) return false;
    if (faces != "right" && faces != "left") return c.fail(where, "\"faces\" must be \"right\" or \"left\"");
    s.facesLeft = faces == "left";
    if (!c.boolean(e.second, "mirror", where, s.mirror)) return false;
    if (!readAnchor(c, e.second, where, s.anchorX, s.anchorY)) return false;
    const Json* anims;
    if (!c.object(e.second, "animations", where, anims, true)) return false;
    for (const Json::Member& a : anims->members()) {
      BattleAnimId id;
      if (!battleAnimFromKey(a.first, id))
        return c.fail(where, "unknown animation " + q(a.first) + " (use stand, attack, crit, ranged, cast, dodge, hurt or defeat)");
      if (a.second.isNull()) continue;
      if (!readAnim(c, db, a.second, where + ": animation " + q(a.first), id == BaStand, s.anims[id])) return false;
    }
    if (!s.anims[BaStand].any()) return c.fail(where, "needs a \"stand\" animation (a set without one is not used; leave the unit out of \"assign\" to use its map picture)");
    // An anchor outside the frame would put the fighter's feet in the wrong place.
    const SheetDef& sh = db.graphics.sheets[(size_t)s.anims[BaStand].sheet];
    if (s.anchorX >= sh.cellW || s.anchorY >= sh.cellH)
      return c.fail(where, "\"anchor\" [" + std::to_string(s.anchorX) + ", " + std::to_string(s.anchorY) + "] is outside the " +
                               std::to_string(sh.cellW) + " x " + std::to_string(sh.cellH) + " frame");
    art.sets.push_back(s);
  }
  return true;
}

bool weaponGroupExists(const Database& db, const std::string& g) {
  for (const WeaponDef& w : db.weapons)
    if (w.group == g) return true;
  for (const StaffDef& st : db.staves)
    if (st.gear.group == g) return true;
  return false;
}

bool readEffects(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* list;
  if (!c.array(b, "effects", "battle", list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& j = (*list)[i];
    const std::string where = "battle effect " + std::to_string(i + 1);
    if (!j.isObject()) return c.fail(where, "must be an object");
    BattleEffect fx;
    std::string weapon, group;
    if (!c.str(j, "weapon", where, weapon) || !c.str(j, "group", where, group)) return false;
    if (weapon.empty() == group.empty()) return c.fail(where, "needs exactly one of \"weapon\" (a weapon id) or \"group\" (a weapon group)");
    if (!weapon.empty()) {
      fx.weapon = db.findWeapon(weapon);
      if (fx.weapon < 0) return c.fail(where, "there is no weapon with the id " + q(weapon));
    } else {
      if (!weaponGroupExists(db, group)) return c.fail(where, "no weapon has the group " + q(group));
      fx.group = group;
    }
    for (const BattleEffect& other : art.effects)
      if (other.weapon == fx.weapon && other.group == fx.group) return c.fail(where, "a second effect for " + q(weapon.empty() ? group : weapon));
    std::string mode = "target";
    if (!c.str(j, "mode", where, mode)) return false;
    if (mode != "target" && mode != "travel") return c.fail(where, "\"mode\" must be \"target\" (appears on the foe) or \"travel\" (flies to the foe)");
    fx.travel = mode == "travel";
    if (!c.integer(j, "travelMs", where, fx.travelMs)) return false;
    if (fx.travelMs < 20 || fx.travelMs > 5000) return c.fail(where, "\"travelMs\" must be between 20 and 5000");
    if (!readAnchor(c, j, where, fx.anchorX, fx.anchorY)) return false;
    if (!readAnim(c, db, j, where, false, fx.anim)) return false;
    art.effects.push_back(fx);
  }
  return true;
}

bool readAssign(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* list;
  if (!c.array(b, "assign", "battle", list)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& j = (*list)[i];
    const std::string where = "battle assign " + std::to_string(i + 1);
    if (!j.isObject()) return c.fail(where, "must be an object");
    BattleAssign a;
    std::string set, type, character, group;
    if (!c.str(j, "set", where, set, true) || !c.str(j, "type", where, type) || !c.str(j, "character", where, character) ||
        !c.str(j, "group", where, group))
      return false;
    a.set = art.findSet(set);
    if (a.set < 0) return c.fail(where, "there is no battle set " + q(set));
    if (!type.empty()) {
      a.type = db.findUnitType(type);
      if (a.type < 0) return c.fail(where, "there is no unit type with the id " + q(type));
    }
    if (!character.empty()) {
      a.character = db.findCharacter(character);
      if (a.character < 0) return c.fail(where, "there is no character with the id " + q(character));
    }
    if (!group.empty()) {
      if (!weaponGroupExists(db, group)) return c.fail(where, "no weapon has the group " + q(group));
      a.group = group;
    }
    if (type.empty() && character.empty() && group.empty())
      return c.fail(where, "needs at least one of \"type\", \"character\" or \"group\" (who uses the set)");
    for (const BattleAssign& o : art.assign)
      if (o.character == a.character && o.type == a.type && o.group == a.group)
        return c.fail(where, "the same \"type\" / \"character\" / \"group\" is already given to set " + q(art.sets[(size_t)o.set].id));
    art.assign.push_back(a);
  }
  return true;
}

bool readBackgrounds(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* m;
  if (!c.object(b, "backgrounds", "battle", m)) return false;
  for (const Json::Member& e : m->members()) {
    const std::string where = "battle background " + q(e.first);
    if (e.first.empty() || !e.second.isObject()) return c.fail(where, "must be an object with \"sheet\" and \"cell\"");
    BattleBackground bg;
    bg.id = e.first;
    std::string sheetId;
    if (!c.str(e.second, "sheet", where, sheetId, true)) return false;
    bg.sheet = db.graphics.findSheet(sheetId);
    if (bg.sheet < 0) return c.fail(where, "unknown sheet " + q(sheetId));
    const SheetDef& sh = db.graphics.sheets[(size_t)bg.sheet];
    const Json* cell;
    if (!c.array(e.second, "cell", where, cell, true)) return false;
    if (!readFrameCell(c, sh, *cell, where, bg.cell)) return false;
    art.backgrounds.push_back(bg);
  }
  art.terrainBackground.assign(db.terrains.size(), -1);
  if (!c.object(b, "terrainBackground", "battle", m)) return false;
  for (const Json::Member& e : m->members()) {
    const int t = db.findTerrain(e.first);
    const std::string where = "battle terrainBackground " + q(e.first);
    if (t < 0) return c.fail(where, "there is no terrain with this id");
    if (!e.second.isString()) return c.fail(where, "must be a background id");
    const int bg = art.findBackground(e.second.asString());
    if (bg < 0) return c.fail(where, "there is no background " + q(e.second.asString()));
    art.terrainBackground[(size_t)t] = bg;
  }
  std::string def;
  if (!c.str(b, "defaultBackground", "battle", def)) return false;
  if (!def.empty()) {
    art.defaultBackground = art.findBackground(def);
    if (art.defaultBackground < 0) return c.fail("battle", "\"defaultBackground\" names unknown background " + q(def));
  }
  return true;
}

}  // namespace

bool readBattleArt(const LoadCtx& c, Database& db, const Json& root) {
  db.graphics.battle = BattleArt();
  if (!root.has("battle")) return true;
  const Json& b = root["battle"];
  if (!b.isObject()) return c.fail("", "\"battle\" must be an object");
  BattleArt art;
  art.terrainBackground.assign(db.terrains.size(), -1);
  if (!readSets(c, db, b, art) || !readEffects(c, db, b, art) || !readAssign(c, db, b, art) || !readBackgrounds(c, db, b, art)) return false;
  db.graphics.battle = art;
  return true;
}

// A map's "battleBackground": the id of a background of data/graphics.json "battle" (docs/BATTLE_SCENE.md).
bool readMapBattleBackground(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  m.battleBackground = -1;
  if (!root.has("battleBackground")) return true;
  std::string id;
  if (!c.str(root, "battleBackground", "", id)) return false;
  if (id.empty()) return true;
  m.battleBackground = db.graphics.battle.findBackground(id);
  if (m.battleBackground < 0) return c.fail("", "\"battleBackground\" names unknown background " + q(id) + " (data/graphics.json \"battle\" > \"backgrounds\")");
  return true;
}

}  // namespace ote
