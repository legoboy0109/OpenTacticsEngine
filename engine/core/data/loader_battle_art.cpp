// The "battle" part of data/graphics.json (docs/BATTLE_SCENE.md): fighter animation sets, weapon effects, which set a
// unit type / character / weapon group uses, and the scene's backgrounds. Presentation only.
#include <algorithm>
#include <cstdlib>

#include "core/data/events.h"  // isStoryId
#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {
namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool readFrameCell(const LoadCtx& c, const SheetDef& sh, const Json& j, const std::string& where, SpriteCell& out, size_t n = 2) {
  if (!j.isArray() || j.size() != n || !LoadCtx::isWhole(j[0]) || !LoadCtx::isWhole(j[1]) || j[0].asInt() < 0 || j[1].asInt() < 0)
    return c.fail(where, "\"frames\" must be a list of [column, row] cells (whole numbers, counted from 0)");
  out.col = (int16_t)std::min(j[0].asInt(), 30000);
  out.row = (int16_t)std::min(j[1].asInt(), 30000);
  if (out.col >= sh.cols || out.row >= sh.rows)
    return c.fail(where, "frame [" + std::to_string(j[0].asInt()) + ", " + std::to_string(j[1].asInt()) + "] is outside sheet " + q(sh.id) +
                             " (" + std::to_string(sh.cols) + " x " + std::to_string(sh.rows) + " cells of " + std::to_string(sh.cellW) + " x " +
                             std::to_string(sh.cellH) + ")");
  return true;
}

// One animation object: sheet, frames, ms, loop, hit, spawn, sounds. `isEffect`: an effect's frames may name their own sheet as a third
// entry ([column, row, "sheet"]) and its "sounds" are not frame sounds (readEffects reads them, keyed by milliseconds).
bool readAnim(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, bool loopDefault, BattleAnim& out, bool isEffect = false) {
  out = BattleAnim();
  if (!j.isObject()) return c.fail(where, "must be an object with \"sheet\" and \"frames\"");
  std::string sheetId;
  if (!c.str(j, "sheet", where, sheetId, true)) return false;
  out.sheet = db.graphics.findSheet(sheetId);
  if (out.sheet < 0) return c.fail(where, "unknown sheet " + q(sheetId));
  const SheetDef& sh = db.graphics.sheets[(size_t)out.sheet];
  const Json* frames;
  if (!c.array(j, "frames", where, frames, true)) return false;
  if (frames->size() < 1 || frames->size() > (size_t)kMaxBattleFrames)
    return c.fail(where, "\"frames\" must list between 1 and " + std::to_string(kMaxBattleFrames) + " cells (all from one sheet)");
  bool several = false;
  for (size_t i = 0; i < frames->size(); ++i) {
    SpriteCell cell;
    const Json& f = (*frames)[i];
    if (isEffect && f.isArray() && f.size() == 3) {  // [column, row, "sheet"]: this frame is from another sheet of the same cell size
      if (!f[2].isString()) return c.fail(where, "a frame's third entry must be a sheet id: [column, row, \"sheet\"]");
      const int fs = db.graphics.findSheet(f[2].asString());
      if (fs < 0) return c.fail(where, "frame " + std::to_string(i) + " names unknown sheet " + q(f[2].asString()));
      const SheetDef& fsh = db.graphics.sheets[(size_t)fs];
      if (fsh.cellW != sh.cellW || fsh.cellH != sh.cellH)
        return c.fail(where, "frame " + std::to_string(i) + " is from sheet " + q(fsh.id) + " (cells of " + std::to_string(fsh.cellW) + " x " +
                                 std::to_string(fsh.cellH) + ") but the effect's sheet " + q(sh.id) + " has cells of " + std::to_string(sh.cellW) + " x " +
                                 std::to_string(sh.cellH) + " (all the sheets of one effect need the same cell size)");
      if (!readFrameCell(c, fsh, f, where, cell, 3)) return false;
      out.frameSheets.resize(i, out.sheet);
      out.frameSheets.push_back(fs);
      several = true;
    } else {
      if (!readFrameCell(c, sh, f, where, cell)) return false;
      if (several) out.frameSheets.push_back(out.sheet);
    }
    out.frames.push_back(cell);
  }
  if (several) out.frameSheets.resize(out.frames.size(), out.sheet);
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
  // Holding a pose (docs/BATTLE_SCENE.md "Holding a pose"): the slot-specific rules are checked by checkHolds once the slot is known.
  for (const char* key : {"holdFrame", "startFrame", "startDelayMs"}) {
    if (!j.has(key)) continue;
    if (isEffect) return c.fail(where, q(key) + " is only for an animation of a battle set (an effect has no pose to hold)");
  }
  if (!c.integerIn(j, "holdFrame", where, 0, n - 1, out.holdFrame) || !c.integerIn(j, "startFrame", where, 0, n - 1, out.startFrame) ||
      !c.integerIn(j, "startDelayMs", where, 0, 5000, out.startDelayMs))
    return false;
  // "sounds": { "<frame>": "<sound id>" }: a sound that plays when that frame begins (docs/BATTLE_SCENE.md "Sounds").
  if (j.has("sounds") && !isEffect) {
    const Json& s = j["sounds"];
    if (!s.isObject()) return c.fail(where, "\"sounds\" must be an object: a frame number, then a sound id, such as { \"1\": \"swing\" }");
    for (const Json::Member& m : s.members()) {
      char* end = nullptr;
      const long frame = std::strtol(m.first.c_str(), &end, 10);
      if (m.first.empty() || *end != 0 || frame < 0 || frame >= n)
        return c.fail(where, "\"sounds\" names frame " + q(m.first) + " but the animation has frames 0 to " + std::to_string(n - 1));
      if (!m.second.isString() || !isStoryId(m.second.asString()))
        return c.fail(where, "\"sounds\" frame " + m.first + " must name a sound of data/audio.json (letters, digits, \"_\" and \"-\")");
      out.sounds.push_back(std::make_pair((int)frame, m.second.asString()));
    }
    std::sort(out.sounds.begin(), out.sounds.end());
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

// "sides": { "<side id>": { "sheet": "<sheet id>" } } of a set or of one animation: another sheet with the same layout (the same
// cell size, every frame of the animation inside it) that the fighter of that side draws from. Returns the (side, sheet) pairs.
bool readSideSheets(const LoadCtx& c, const Database& db, const Json& owner, const std::string& where,
                    std::vector<std::pair<std::string, int>>& out) {
  if (!owner.has("sides")) return true;
  const Json& m = owner["sides"];
  if (!m.isObject()) return c.fail(where, "\"sides\" must be an object: a side id, then { \"sheet\": \"...\" }");
  for (const Json::Member& e : m.members()) {
    const std::string w = where + " side " + q(e.first);
    if (e.first.empty()) return c.fail(where, "\"sides\" has an empty side id");
    if (!e.second.isObject()) return c.fail(w, "must be an object with a \"sheet\"");
    if (!c.onlyKeys(e.second, {"sheet"}, w)) return false;
    std::string sheetId;
    if (!c.str(e.second, "sheet", w, sheetId, true)) return false;
    const int idx = db.graphics.findSheet(sheetId);
    if (idx < 0) return c.fail(w, "unknown sheet " + q(sheetId));
    out.push_back(std::make_pair(e.first, idx));
  }
  return true;
}

// The per-side sheet must be laid out like the animation's own sheet, or the frames would not match.
bool checkSideSheet(const LoadCtx& c, const Database& db, const BattleAnim& a, const std::string& side, int sheet, const std::string& where) {
  const SheetDef& base = db.graphics.sheets[(size_t)a.sheet];
  const SheetDef& sh = db.graphics.sheets[(size_t)sheet];
  const std::string w = where + " side " + q(side);
  if (sh.cellW != base.cellW || sh.cellH != base.cellH)
    return c.fail(w, "sheet " + q(sh.id) + " has cells of " + std::to_string(sh.cellW) + " x " + std::to_string(sh.cellH) + " but " + q(base.id) +
                         " has " + std::to_string(base.cellW) + " x " + std::to_string(base.cellH) + " (a side's sheet must have the same layout)");
  for (const SpriteCell& f : a.frames)
    if (f.col >= sh.cols || f.row >= sh.rows)
      return c.fail(w, "frame [" + std::to_string(f.col) + ", " + std::to_string(f.row) + "] is outside sheet " + q(sh.id) + " (" + std::to_string(sh.cols) +
                           " x " + std::to_string(sh.rows) + " cells; a side's sheet must have the same layout)");
  return true;
}

// The rules of "holdFrame" / "startFrame" / "startDelayMs" that depend on the slot (docs/BATTLE_SCENE.md "Holding a pose").
bool checkHolds(const LoadCtx& c, const BattleAnim& a, BattleAnimId id, const std::string& where) {
  if (!a.holds() && a.startDelayMs == 0) return true;
  const bool dodge = id == BaDodge || id == BaDodgeRanged;
  const bool striker = id == BaAttack || id == BaCrit || id == BaRanged || id == BaCast || id == BaCritRanged || id == BaMiss;
  if (!dodge && !striker && id != BaHurt)
    return c.fail(where, "\"holdFrame\" / \"startFrame\" are for the strike animations, \"dodge\", \"dodgeRanged\" and \"hurt\" (a standing animation or a fall never waits for a result)");
  if (a.startFrame >= 0 && !dodge) return c.fail(where, "\"startFrame\" is only for \"dodge\" and \"dodgeRanged\" (the frames before it are the brace before the blow)");
  if (a.startDelayMs > 0 && !dodge) return c.fail(where, "\"startDelayMs\" is only for \"dodge\" and \"dodgeRanged\"");
  if (a.startDelayMs > 0 && !a.holds()) return c.fail(where, "\"startDelayMs\" needs a \"startFrame\" or a \"holdFrame\" (it moves the start of the dodge from the miss)");
  if (a.holdFrame >= 0 && a.startFrame >= 0 && a.holdFrame < a.startFrame)
    return c.fail(where, "\"holdFrame\" " + std::to_string(a.holdFrame) + " comes before \"startFrame\" " + std::to_string(a.startFrame) + " (the hold is a frame of the dodge itself, not of the brace)");
  if (striker && a.holdFrame >= 0 && a.holdFrame < a.hitFrame())
    return c.fail(where, "\"holdFrame\" " + std::to_string(a.holdFrame) + " comes before the hit frame " + std::to_string(a.hitFrame()) + " (the animation waits after the blow, on the hit frame or a later one)");
  return true;
}

bool readSets(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* sets;
  if (!c.object(b, "sets", "battle", sets)) return false;
  for (const Json::Member& e : sets->members()) {
    const std::string where = "battle set " + q(e.first);
    if (e.first.empty()) return c.fail("battle", "a set's id must not be empty");
    if (!e.second.isObject()) return c.fail(where, "must be an object");
    if (!c.onlyKeys(e.second, {"faces", "mirror", "anchor", "animations", "sides"}, where)) return false;
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
        return c.fail(where, "unknown animation " + q(a.first) +
                                 " (use stand, attack, crit, ranged, cast, dodge, hurt, defeat, critRanged, dodgeRanged, standRanged or miss)");
      if (a.second.isNull()) continue;
      const std::string aw = where + ": animation " + q(a.first);
      if (!c.onlyKeys(a.second, {"sheet", "frames", "ms", "loop", "hit", "spawn", "holdFrame", "startFrame", "startDelayMs", "sounds", "sides"}, aw)) return false;
      if (!readAnim(c, db, a.second, aw, id == BaStand || id == BaStandRanged, s.anims[id])) return false;
      if (!checkHolds(c, s.anims[id], id, aw)) return false;
      if (!readSideSheets(c, db, a.second, aw, s.anims[id].sideSheets)) return false;
      for (const auto& ss : s.anims[id].sideSheets)
        if (!checkSideSheet(c, db, s.anims[id], ss.first, ss.second, aw)) return false;
    }
    // A set's own "sides": the sheet a side's fighter uses for every animation that has no sheet of its own for that side.
    std::vector<std::pair<std::string, int>> setSides;
    if (!readSideSheets(c, db, e.second, where, setSides)) return false;
    for (const auto& ss : setSides)
      for (int ai = 0; ai < kBattleAnims; ++ai) {
        BattleAnim& an = s.anims[ai];
        if (!an.any()) continue;
        bool own = false;
        for (const auto& o : an.sideSheets) own |= o.first == ss.first;
        if (own) continue;
        if (!checkSideSheet(c, db, an, ss.first, ss.second, where + ": animation " + q(battleAnimKey((BattleAnimId)ai)))) return false;
        an.sideSheets.push_back(ss);
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

// The newer keys of an effect (docs/BATTLE_SCENE.md "Effects"): sounds, hitAtMs, layer, mirror, flash, shake, at, returns, lingerMs.
bool readEffectExtras(const LoadCtx& c, const Json& j, const std::string& where, BattleEffect& fx) {
  auto given = [&](const char* k) { return j.has(k); };
  if (given("sounds")) {
    const Json& s = j["sounds"];
    if (!s.isObject()) return c.fail(where, "\"sounds\" must be an object: milliseconds after the effect starts, then a sound id, such as { \"0\": \"zap\" }");
    for (const Json::Member& m : s.members()) {
      char* end = nullptr;
      const long ms = std::strtol(m.first.c_str(), &end, 10);
      if (m.first.empty() || *end != 0 || ms < 0 || ms > 60000)
        return c.fail(where, "\"sounds\" key " + q(m.first) + " must be the milliseconds after the effect starts (0 to 60000)");
      if (!m.second.isString() || !isStoryId(m.second.asString()))
        return c.fail(where, "\"sounds\" at " + m.first + " must name a sound of data/audio.json (letters, digits, \"_\" and \"-\")");
      fx.sounds.push_back(std::make_pair((int)ms, m.second.asString()));
    }
    std::stable_sort(fx.sounds.begin(), fx.sounds.end(), [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) { return a.first < b.first; });
  }
  if (given("hitAtMs")) {
    if (!c.integer(j, "hitAtMs", where, fx.hitAtMs)) return false;
    if (fx.hitAtMs < 0 || fx.hitAtMs > 15000) return c.fail(where, "\"hitAtMs\" must be between 0 and 15000 (milliseconds the effect starts before the hit)");
    if (fx.travel) return c.fail(where, "\"hitAtMs\" is for an effect that appears on the target or the striker; a \"travel\" effect hits when it arrives");
  }
  if (given("fromHitFrame")) {  // docs/BATTLE_SCENE.md "Timing of effects"
    if (!c.boolean(j, "fromHitFrame", where, fx.fromHitFrame)) return false;
    if (fx.fromHitFrame && fx.travel) return c.fail(where, "\"fromHitFrame\" is for an effect that appears on the target or the striker; a \"travel\" effect leaves at the spawn frame (use \"hitAfterMs\")");
  }
  if (given("hitAfterMs")) {
    if (!c.integer(j, "hitAfterMs", where, fx.hitAfterMs)) return false;
    if (fx.hitAfterMs < 0 || fx.hitAfterMs > 15000) return c.fail(where, "\"hitAfterMs\" must be between 0 and 15000 (milliseconds from the shot leaving to the hit)");
    if (!fx.travel) return c.fail(where, "\"hitAfterMs\" is only for a \"travel\" effect (an effect in place times its hit with \"hitAtMs\")");
  }
  std::string layer = "front";
  if (given("layer")) {
    if (!c.str(j, "layer", where, layer)) return false;
    if (layer != "front" && layer != "behind" && layer != "screen") return c.fail(where, "\"layer\" must be \"front\", \"behind\" or \"screen\"");
    fx.layer = layer == "behind" ? FxLayer::Behind : layer == "screen" ? FxLayer::Screen : FxLayer::Front;
    if (fx.layer == FxLayer::Screen && fx.travel) return c.fail(where, "a \"screen\" effect covers the stage and cannot be a \"travel\" effect");
  }
  if (given("mirror")) {
    if (!c.boolean(j, "mirror", where, fx.mirror)) return false;
    if (fx.mirror && fx.layer != FxLayer::Screen) return c.fail(where, "\"mirror\" is only for \"layer\": \"screen\" (the other layers are turned round with the striker already)");
  }
  if (given("at")) {
    std::string at;
    if (!c.str(j, "at", where, at)) return false;
    if (at != "target" && at != "striker") return c.fail(where, "\"at\" must be \"target\" or \"striker\"");
    fx.atStriker = at == "striker";
    if (fx.atStriker && fx.travel) return c.fail(where, "\"at\": \"striker\" is for an effect that appears in place, not a \"travel\" effect");
    if (fx.atStriker && fx.layer == FxLayer::Screen) return c.fail(where, "a \"screen\" effect covers the stage and has no \"at\"");
  }
  if (given("returns")) {
    if (!c.boolean(j, "returns", where, fx.returns)) return false;
    if (fx.returns && !fx.travel) return c.fail(where, "\"returns\" is only for a \"travel\" effect (it flies back to the striker)");
  }
  if (given("lingerMs")) {
    if (!c.integer(j, "lingerMs", where, fx.lingerMs)) return false;
    if (fx.lingerMs < 0 || fx.lingerMs > 5000) return c.fail(where, "\"lingerMs\" must be between 0 and 5000");
    if (fx.lingerMs > 0 && !fx.travel) return c.fail(where, "\"lingerMs\" is only for a \"travel\" effect (it stays at the target after arriving)");
  }
  if (given("blend")) {  // docs/BATTLE_SCENE.md "Blending"
    std::string blend;
    if (!c.str(j, "blend", where, blend)) return false;
    if (blend != "normal" && blend != "add") return c.fail(where, "\"blend\" must be \"normal\" (the default) or \"add\" (its pixels brighten what is under them)");
    fx.additive = blend == "add";
  }
  if (given("order")) {  // docs/BATTLE_SCENE.md "Effect layers"
    std::string order;
    if (!c.str(j, "order", where, order)) return false;
    if (order != "backdrop" && order != "behind" && order != "front") return c.fail(where, "\"order\" must be \"backdrop\" (right after the backdrop, the default), \"behind\" or \"front\"");
    if (fx.layer != FxLayer::Screen) return c.fail(where, "\"order\" is only for \"layer\": \"screen\" (the other layers have their place already)");
    fx.order = order == "behind" ? FxOrder::Behind : order == "front" ? FxOrder::Front : FxOrder::Backdrop;
  }
  if (given("speed")) {  // docs/BATTLE_SCENE.md "Effects": a flight time that grows with the distance
    if (!c.integer(j, "speed", where, fx.speed)) return false;
    if (!fx.travel) return c.fail(where, "\"speed\" is only for a \"travel\" effect");
    if (given("travelMs")) return c.fail(where, "give a \"travelMs\" or a \"speed\", not both");
    if (fx.speed < 1 || fx.speed > 10000) return c.fail(where, "\"speed\" must be between 1 and 10000 (stage pixels per second)");
  }
  if (given("endOffset")) {  // docs/BATTLE_SCENE.md "Thrown weapons stopping short"
    if (!c.integerIn(j, "endOffset", where, -400, 400, fx.endOffset)) return false;  // (negative: the shot flies past the target)
    if (!fx.travel) return c.fail(where, "\"endOffset\" is only for a \"travel\" effect (it stops short of the target)");
  }
  if (given("panAtMs")) {  // docs/BATTLE_SCENE.md "Range bands": the camera's own start for strikes that use this effect
    if (!c.integerIn(j, "panAtMs", where, 0, 15000, fx.panAtMs)) return false;
  }
  if (given("atRangeFixed")) {  // docs/BATTLE_SCENE.md "Effect timing by distance"
    if (!c.boolean(j, "atRangeFixed", where, fx.rangeFixed)) return false;
    if (fx.rangeFixed && given("atRange")) return c.fail(where, "\"atRangeFixed\" is for an effect that is not delayed by distance; an effect with its own \"atRange\" is delayed by it (mark single rows of its fade / hide / flash / shake instead)");
  }
  if (given("atRange")) {  // docs/BATTLE_SCENE.md "Effect timing by distance"
    const Json& r = j["atRange"];
    if (!r.isArray() || r.size() < 1 || r.size() > 16)
      return c.fail(where, "\"atRange\" must be a list of 1 to 16 { \"from\": 2, \"delayMs\": 267 } (a travel effect's may also give \"endOffset\")");
    for (size_t i = 0; i < r.size(); ++i) {
      const std::string w = where + " atRange " + std::to_string(i + 1);
      if (!r[i].isObject()) return c.fail(w, "must be { \"from\": 2, \"delayMs\": 267 } (a travel effect's may also give an \"endOffset\")");
      for (const Json::Member& m : r[i].members())
        if (m.first != "from" && m.first != "delayMs" && m.first != "endOffset") return c.fail(w, unknownKeyText(m.first, {"from", "delayMs", "endOffset"}));
      FxRange fr;
      if (!c.integer(r[i], "from", w, fr.from, true)) return false;
      if (fr.from < 1 || fr.from > 99) return c.fail(w, "\"from\" must be between 1 and 99 (the fight distance in squares the step starts at)");
      if (!r[i].has("delayMs") && !r[i].has("endOffset")) return c.fail(w, "needs a \"delayMs\" (milliseconds the whole effect starts later) or an \"endOffset\" (pixels the flight stops short of the target)");
      if (r[i].has("delayMs")) {  // (milliseconds the whole effect starts later)
        if (!c.integer(r[i], "delayMs", w, fr.delayMs)) return false;
        if (fr.delayMs < 0 || fr.delayMs > 15000) return c.fail(w, "\"delayMs\" must be between 0 and 15000 (milliseconds the whole effect starts later)");
      }
      if (r[i].has("endOffset")) {
        if (!fx.travel) return c.fail(w, "\"endOffset\" is only for a \"travel\" effect (it stops short of the target)");
        if (!c.integerIn(r[i], "endOffset", w, -400, 400, fr.endOffset)) return false;
        fr.hasEndOffset = true;
      }
      for (const FxRange& o : fx.atRange)
        if (o.from == fr.from) return c.fail(w, "a second entry starts at " + std::to_string(fr.from) + " (each \"from\" can be used once)");
      fx.atRange.push_back(fr);
    }
    std::sort(fx.atRange.begin(), fx.atRange.end(), [](const FxRange& a, const FxRange& b) { return a.from < b.from; });
  }
  if (given("hide")) {  // docs/BATTLE_SCENE.md "Hiding and fading"
    const Json& h = j["hide"];
    if (!h.isArray() || h.size() > 32) return c.fail(where, "\"hide\" must be a list of up to 32 { \"atMs\": 0, \"ms\": 1500, \"what\": [\"backdrop\", \"platforms\"], \"color\": \"#000000\" }");
    for (size_t i = 0; i < h.size(); ++i) {
      const std::string w = where + " hide " + std::to_string(i + 1);
      if (!h[i].isObject()) return c.fail(w, "must be { \"atMs\": 0, \"ms\": 1500, \"what\": [\"backdrop\", \"platforms\"] }");
      for (const Json::Member& m : h[i].members())
        if (m.first != "atMs" && m.first != "ms" && m.first != "what" && m.first != "color" && m.first != "atRangeFixed")
          return c.fail(w, unknownKeyText(m.first, {"atMs", "ms", "what", "color", "atRangeFixed"}));
      FxHide hd;
      if (!c.boolean(h[i], "atRangeFixed", w, hd.fixed)) return false;
      if (!c.integer(h[i], "atMs", w, hd.atMs, true) || !c.integer(h[i], "ms", w, hd.ms, true)) return false;
      if (hd.atMs < 0 || hd.atMs > 60000) return c.fail(w, "\"atMs\" must be between 0 and 60000");
      if (hd.ms < 10 || hd.ms > 60000) return c.fail(w, "\"ms\" must be between 10 and 60000");
      const Json* what;
      if (!c.array(h[i], "what", w, what, true)) return false;
      if (what->size() < 1 || what->size() > 2) return c.fail(w, "\"what\" must list \"backdrop\" and / or \"platforms\"");
      for (size_t k = 0; k < what->size(); ++k) {
        const std::string name = (*what)[k].isString() ? (*what)[k].asString() : std::string();
        const unsigned bit = name == "backdrop" ? (unsigned)kHideBackdrop : name == "platforms" ? (unsigned)kHidePlatforms : 0u;
        if (!bit) return c.fail(w, "\"what\" can name \"backdrop\" and \"platforms\" only");
        if (hd.what & bit) return c.fail(w, "\"what\" names " + q(name) + " twice");
        hd.what |= bit;
      }
      if (h[i].has("color")) {
        std::string col;
        if (!c.str(h[i], "color", w, col)) return false;
        char* end = nullptr;
        const unsigned long v = col.size() == 7 && col[0] == '#' ? std::strtoul(col.c_str() + 1, &end, 16) : 0;
        if (col.size() != 7 || col[0] != '#' || *end != 0) return c.fail(w, "\"color\" must be a colour such as \"#102030\"");
        hd.hasColor = true;
        hd.rgb = (uint32_t)v;
      }
      fx.hide.push_back(hd);
    }
  }
  if (given("fade")) {
    const Json& f = j["fade"];
    if (!f.isArray() || f.size() > 32)
      return c.fail(where, "\"fade\" must be a list of up to 32 { \"atMs\": 0, \"ms\": 300, \"to\": \"#000000\", \"alpha\": 0.8, \"holdMs\": 400, \"over\": \"all\" }");
    for (size_t i = 0; i < f.size(); ++i) {
      const std::string w = where + " fade " + std::to_string(i + 1);
      if (!f[i].isObject()) return c.fail(w, "must be { \"atMs\": 0, \"ms\": 300, \"to\": \"#000000\", \"alpha\": 0.8, \"holdMs\": 400, \"over\": \"all\" }");
      for (const Json::Member& m : f[i].members())
        if (m.first != "atMs" && m.first != "ms" && m.first != "to" && m.first != "alpha" && m.first != "holdMs" && m.first != "over" && m.first != "atRangeFixed")
          return c.fail(w, unknownKeyText(m.first, {"atMs", "ms", "to", "alpha", "holdMs", "over", "atRangeFixed"}));
      FxFade fd;
      if (!c.boolean(f[i], "atRangeFixed", w, fd.fixed)) return false;
      if (!c.integer(f[i], "atMs", w, fd.atMs, true) || !c.integer(f[i], "ms", w, fd.ms, true) || !c.integer(f[i], "holdMs", w, fd.holdMs)) return false;
      if (fd.atMs < 0 || fd.atMs > 60000) return c.fail(w, "\"atMs\" must be between 0 and 60000");
      if (fd.ms < 10 || fd.ms > 5000) return c.fail(w, "\"ms\" must be between 10 and 5000 (the time to fade in, and again to fade out)");
      if (fd.holdMs < 0 || fd.holdMs > 60000) return c.fail(w, "\"holdMs\" must be between 0 and 60000");
      std::string col = "#000000", over = "all";
      if (!c.str(f[i], "to", w, col) || !c.str(f[i], "over", w, over)) return false;
      char* end = nullptr;
      const unsigned long v = col.size() == 7 && col[0] == '#' ? std::strtoul(col.c_str() + 1, &end, 16) : 0;
      if (col.size() != 7 || col[0] != '#' || *end != 0) return c.fail(w, "\"to\" must be a colour such as \"#000000\"");
      fd.rgb = (uint32_t)v;
      if (over != "all" && over != "backdrop") return c.fail(w, "\"over\" must be \"all\" (everything, the fighters too) or \"backdrop\"");
      fd.backdropOnly = over == "backdrop";
      if (f[i].has("alpha")) {
        double a = 1;
        if (!c.num(f[i], "alpha", w, a)) return false;
        if (!(a > 0 && a <= 1)) return c.fail(w, "\"alpha\" must be above 0 and at most 1 (how opaque the colour gets at its peak)");
        fd.alpha = (int)(a * 255 + 0.5);
      }
      fx.fade.push_back(fd);
    }
  }
  if (given("flash")) {
    const Json& f = j["flash"];
    if (!f.isArray() || f.size() > 32) return c.fail(where, "\"flash\" must be a list of up to 32 { \"atMs\": 100, \"ms\": 80, \"color\": \"#ffffff\" }");
    for (size_t i = 0; i < f.size(); ++i) {
      const std::string w = where + " flash " + std::to_string(i + 1);
      if (!f[i].isObject()) return c.fail(w, "must be { \"atMs\": 100, \"ms\": 80, \"color\": \"#ffffff\" }");
      FxFlash fl;
      if (!c.onlyKeys(f[i], {"atMs", "ms", "color", "atRangeFixed"}, w)) return false;
      if (!c.boolean(f[i], "atRangeFixed", w, fl.fixed)) return false;
      if (!c.integer(f[i], "atMs", w, fl.atMs, true) || !c.integer(f[i], "ms", w, fl.ms, true)) return false;
      if (fl.atMs < 0 || fl.atMs > 60000) return c.fail(w, "\"atMs\" must be between 0 and 60000");
      if (fl.ms < 10 || fl.ms > 5000) return c.fail(w, "\"ms\" must be between 10 and 5000");
      std::string col = "#ffffff";
      if (!c.str(f[i], "color", w, col)) return false;
      char* end = nullptr;
      const unsigned long v = col.size() == 7 && col[0] == '#' ? std::strtoul(col.c_str() + 1, &end, 16) : 0;
      if (col.size() != 7 || col[0] != '#' || *end != 0) return c.fail(w, "\"color\" must be a colour such as \"#ffffff\"");
      fl.rgb = (uint32_t)v;
      fx.flash.push_back(fl);
    }
  }
  if (given("shake")) {
    const Json& f = j["shake"];
    if (!f.isArray() || f.size() > 32) return c.fail(where, "\"shake\" must be a list of up to 32 { \"atMs\": 100, \"ms\": 200, \"strength\": 2 }");
    for (size_t i = 0; i < f.size(); ++i) {
      const std::string w = where + " shake " + std::to_string(i + 1);
      if (!f[i].isObject()) return c.fail(w, "must be { \"atMs\": 100, \"ms\": 200, \"strength\": 2 }");
      FxShake sh;
      if (!c.onlyKeys(f[i], {"atMs", "ms", "strength", "atRangeFixed"}, w)) return false;
      if (!c.boolean(f[i], "atRangeFixed", w, sh.fixed)) return false;
      if (!c.integer(f[i], "atMs", w, sh.atMs, true) || !c.integer(f[i], "ms", w, sh.ms, true) || !c.integer(f[i], "strength", w, sh.strength, true)) return false;
      if (sh.atMs < 0 || sh.atMs > 60000) return c.fail(w, "\"atMs\" must be between 0 and 60000");
      if (sh.ms < 10 || sh.ms > 5000) return c.fail(w, "\"ms\" must be between 10 and 5000");
      if (sh.strength < 1 || sh.strength > 64) return c.fail(w, "\"strength\" must be between 1 and 64 (pixels of the stage)");
      fx.shake.push_back(sh);
    }
  }
  for (const char* k : {"sounds", "hitAtMs", "fromHitFrame", "hitAfterMs", "layer", "mirror", "flash", "shake", "at", "returns", "lingerMs", "blend", "hide", "fade", "order", "speed", "endOffset", "atRange", "atRangeFixed", "panAtMs"})
    if (given(k)) fx.extended = true;
  if (fx.anim.frameSheets.size() > 0) fx.extended = true;
  return true;
}

// "effects": one id or a list of ids of library effects; the indices are filled in once every entry is read.
bool readEffectIds(const LoadCtx& c, const Json& v, const std::string& where, std::vector<std::string>& out) {
  if (v.isString()) {
    out.push_back(v.asString());
    return true;
  }
  if (!v.isArray() || v.size() > 8) return c.fail(where, "must be an effect id or a list of up to 8 effect ids");
  for (size_t i = 0; i < v.size(); ++i) {
    if (!v[i].isString()) return c.fail(where, "must be an effect id or a list of up to 8 effect ids");
    out.push_back(v[i].asString());
  }
  return true;
}

bool readEffects(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* list;
  if (!c.array(b, "effects", "battle", list)) return false;
  struct Pending {
    std::vector<std::string> ids;
    std::vector<std::pair<std::string, std::vector<std::string>>> byType;
    std::string where;
  };
  std::vector<Pending> pending;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& j = (*list)[i];
    const std::string where = "battle effect " + std::to_string(i + 1);
    if (!j.isObject()) return c.fail(where, "must be an object");
    // (holdFrame, startFrame and startDelayMs are accepted so that readAnim can say they are for the animations of a set only.)
    if (!c.onlyKeys(j, {"id", "weapon", "group", "effects", "byType", "mode", "travelMs", "anchor", "sheet", "frames", "ms", "loop", "hit", "spawn", "sounds",
                        "hitAtMs", "fromHitFrame", "hitAfterMs", "layer", "mirror", "flash", "shake", "at", "returns", "lingerMs", "blend", "hide",
                        "fade", "order", "speed", "endOffset", "atRange", "atRangeFixed", "panAtMs", "holdFrame", "startFrame", "startDelayMs"},
                    where))
      return false;
    BattleEffect fx;
    Pending pend;
    pend.where = where;
    std::string weapon, group;
    if (!c.str(j, "id", where, fx.id) || !c.str(j, "weapon", where, weapon) || !c.str(j, "group", where, group)) return false;
    if (j.has("id")) {
      if (!isStoryId(fx.id)) return c.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\"");
      if (art.findEffect(fx.id) >= 0) return c.fail(where, "a second effect with the id " + q(fx.id));
    }
    const bool library = weapon.empty() && group.empty() && !fx.id.empty();
    if (!library && weapon.empty() == group.empty())
      return c.fail(where, "needs exactly one of \"weapon\" (a weapon id) or \"group\" (a weapon group), or an \"id\" to be used in a list");
    if (!weapon.empty()) {
      fx.weapon = db.findWeapon(weapon);
      if (fx.weapon < 0) {  // a staff item is named here too (docs/BATTLE_SCENE.md "Staves in the scene")
        const int it = db.findItem(weapon);
        if (it >= 0 && db.items[(size_t)it].kind == ItemKind::Staff) fx.item = it;
        else return c.fail(where, "there is no weapon with the id " + q(weapon) + " (a staff item is named by its item id too)");
      }
    } else if (!library) {
      if (!weaponGroupExists(db, group)) return c.fail(where, "no weapon has the group " + q(group));
      fx.group = group;
    }
    if (!library)
      for (const BattleEffect& other : art.effects)
        if (!(other.weapon < 0 && other.item < 0 && other.group.empty()) && other.weapon == fx.weapon && other.item == fx.item && other.group == fx.group)
          return c.fail(where, "a second effect for " + q(weapon.empty() ? group : weapon));
    const bool hasLists = j.has("effects") || j.has("byType");
    if (hasLists && library) return c.fail(where, "\"effects\" and \"byType\" belong to an entry with a \"weapon\" or a \"group\"");
    if (j.has("effects") && !readEffectIds(c, j["effects"], where + " \"effects\"", pend.ids)) return false;
    if (j.has("byType")) {
      const Json& bt = j["byType"];
      if (!bt.isObject()) return c.fail(where, "\"byType\" must be { \"<unit type id>\": \"<effect id>\" or [\"<effect id>\", ...] }");
      for (const Json::Member& m : bt.members()) {
        if (db.findUnitType(m.first) < 0) return c.fail(where + " \"byType\"", "there is no unit type with the id " + q(m.first));
        std::vector<std::string> ids;
        if (!readEffectIds(c, m.second, where + " \"byType\" " + q(m.first), ids)) return false;
        pend.byType.push_back(std::make_pair(m.first, ids));
      }
    }
    if (!library && !j.has("sheet") && !j.has("frames") && hasLists) {
      // An assignment only: it names effects to play and has no pictures of its own.
      art.effects.push_back(fx);
      pending.push_back(pend);
      continue;
    }
    std::string mode = "target";
    if (!c.str(j, "mode", where, mode)) return false;
    if (mode != "target" && mode != "travel") return c.fail(where, "\"mode\" must be \"target\" (appears on the foe) or \"travel\" (flies to the foe)");
    fx.travel = mode == "travel";
    if (!c.integer(j, "travelMs", where, fx.travelMs)) return false;
    if (fx.travelMs < 20 || fx.travelMs > 5000) return c.fail(where, "\"travelMs\" must be between 20 and 5000");
    if (!readAnchor(c, j, where, fx.anchorX, fx.anchorY)) return false;
    if (!readAnim(c, db, j, where, false, fx.anim, true)) return false;
    if (!readEffectExtras(c, j, where, fx)) return false;
    art.effects.push_back(fx);
    pending.push_back(pend);
  }
  // Now every id is known: fill in the lists.
  auto resolve = [&](const std::vector<std::string>& ids, const std::string& where, std::vector<int>& out) {
    for (const std::string& id : ids) {
      const int e = art.findEffect(id);
      if (e < 0) return c.fail(where, "there is no effect with the id " + q(id) + " (give an effect an \"id\" to use it in a list)");
      if (art.effects[(size_t)e].assignsOnly()) return c.fail(where, "the effect " + q(id) + " has no pictures of its own (a list names effects with \"sheet\" and \"frames\")");
      out.push_back(e);
    }
    return true;
  };
  for (size_t i = 0; i < art.effects.size(); ++i) {
    BattleEffect& fx = art.effects[i];
    const Pending& p = pending[i];
    if (!resolve(p.ids, p.where + " \"effects\"", fx.list)) return false;
    for (const auto& bt : p.byType) {
      std::vector<int> v;
      if (!resolve(bt.second, p.where + " \"byType\" " + q(bt.first), v)) return false;
      fx.byType.push_back(std::make_pair(db.findUnitType(bt.first), v));
    }
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
    if (!c.onlyKeys(j, {"set", "type", "character", "group", "weapon", "side"}, where)) return false;
    BattleAssign a;
    std::string set, type, character, group, weapon, side;
    if (!c.str(j, "set", where, set, true) || !c.str(j, "type", where, type) || !c.str(j, "character", where, character) ||
        !c.str(j, "group", where, group) || !c.str(j, "weapon", where, weapon) || !c.str(j, "side", where, side))
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
    if (!weapon.empty()) {
      if (!group.empty()) return c.fail(where, "names a \"weapon\" and a \"group\"; use one of them (a weapon already belongs to its group)");
      a.weapon = db.findWeapon(weapon);
      if (a.weapon < 0) return c.fail(where, "there is no weapon with the id " + q(weapon));
    }
    a.side = side;
    if (type.empty() && character.empty() && group.empty() && weapon.empty() && side.empty())
      return c.fail(where, "needs at least one of \"type\", \"character\", \"group\", \"weapon\" or \"side\" (who uses the set)");
    for (const BattleAssign& o : art.assign)
      if (o.character == a.character && o.type == a.type && o.group == a.group && o.weapon == a.weapon && o.side == a.side)
        return c.fail(where, "the same \"type\" / \"character\" / \"group\" / \"weapon\" / \"side\" is already given to set " + q(art.sets[(size_t)o.set].id));
    art.assign.push_back(a);
  }
  return true;
}

// "platforms": { "<id>": { "sheet": id, "cell": [col, row], "far": [col, row] } }, "terrainPlatform": { "<terrain>": "<id>" },
// "defaultPlatform": "<id>" (docs/BATTLE_SCENE.md "Platforms").
bool readPlatforms(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* m;
  if (!c.object(b, "platforms", "battle", m)) return false;
  for (const Json::Member& e : m->members()) {
    const std::string where = "battle platform " + q(e.first);
    if (e.first.empty() || e.first == "default" || !e.second.isObject()) return c.fail(where, "must be an object with \"sheet\" and \"cell\" (and optionally \"far\"); the id cannot be \"default\"");
    for (const Json::Member& k : e.second.members())
      if (k.first != "sheet" && k.first != "cell" && k.first != "far" && k.first != "right" && k.first != "farRight")
        return c.fail(where, unknownKeyText(k.first, {"sheet", "cell", "far", "right", "farRight"}));
    BattlePlatform p;
    p.id = e.first;
    std::string sheetId;
    if (!c.str(e.second, "sheet", where, sheetId, true)) return false;
    p.sheet = db.graphics.findSheet(sheetId);
    if (p.sheet < 0) return c.fail(where, "unknown sheet " + q(sheetId));
    const SheetDef& sh = db.graphics.sheets[(size_t)p.sheet];
    const Json* cell;
    if (!c.array(e.second, "cell", where, cell, true)) return false;
    if (!readFrameCell(c, sh, *cell, where, p.cell)) return false;
    if (e.second.has("far")) {
      if (!readFrameCell(c, sh, e.second["far"], where + " \"far\"", p.farCell)) return false;
      p.hasFar = true;
    }
    // The right-hand fighter's own pictures (used with a fixed stage; else the left one drawn mirrored).
    if (e.second.has("right")) {
      if (!readFrameCell(c, sh, e.second["right"], where + " \"right\"", p.rightCell)) return false;
      p.hasRight = true;
    }
    if (e.second.has("farRight")) {
      if (!p.hasFar) return c.fail(where, "\"farRight\" needs a \"far\" picture");
      if (!readFrameCell(c, sh, e.second["farRight"], where + " \"farRight\"", p.farRightCell)) return false;
      p.hasFarRight = true;
    }
    art.platforms.push_back(p);
  }
  if (!c.object(b, "terrainPlatform", "battle", m)) return false;
  for (const Json::Member& e : m->members()) {
    const std::string where = "battle terrainPlatform " + q(e.first);
    const int t = db.findTerrain(e.first);
    if (t < 0) return c.fail(where, "there is no terrain with this id");
    if (!e.second.isString()) return c.fail(where, "must be a platform id");
    const int p = art.findPlatform(e.second.asString());
    if (p < 0) return c.fail(where, "there is no platform " + q(e.second.asString()));
    art.terrainPlatform[(size_t)t] = p;
  }
  std::string def;
  if (!c.str(b, "defaultPlatform", "battle", def)) return false;
  if (!def.empty()) {
    art.defaultPlatform = art.findPlatform(def);
    if (art.defaultPlatform < 0) return c.fail("battle", "\"defaultPlatform\" names unknown platform " + q(def));
  }
  return true;
}

bool readBackgrounds(const LoadCtx& c, Database& db, const Json& b, BattleArt& art) {
  const Json* m;
  if (!c.object(b, "backgrounds", "battle", m)) return false;
  for (const Json::Member& e : m->members()) {
    const std::string where = "battle background " + q(e.first);
    if (e.first.empty() || !e.second.isObject()) return c.fail(where, "must be an object with \"sheet\" and \"cell\"");
    if (!c.onlyKeys(e.second, {"sheet", "cell"}, where)) return false;
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
  return readPlatforms(c, db, b, art);
}

}  // namespace

bool readMapBattleTables(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  struct Table { const char* key; bool platform; };
  for (const Table t : {Table{"battleBackgrounds", false}, Table{"battlePlatforms", true}}) {
    std::vector<int>& byTerrain = t.platform ? m.platformByTerrain : m.bgByTerrain;
    int& dflt = t.platform ? m.platformDefault : m.bgDefault;
    byTerrain.clear();
    dflt = -1;
    if (!root.has(t.key)) continue;
    const Json& table = root[t.key];
    const std::string what = std::string("\"") + t.key + "\"";
    if (!table.isObject() || table.size() == 0)
      return c.fail("", what + " must be { \"<terrain id>\": \"<" + (t.platform ? "platform" : "background") + " id>\", \"default\": \"<id>\" } (data/graphics.json \"battle\")");
    byTerrain.assign(db.terrains.size(), -1);
    for (const Json::Member& e : table.members()) {
      const std::string where = what + " " + q(e.first);
      if (!e.second.isString()) return c.fail("", where + ": must be the id of a " + (t.platform ? "platform" : "background") + " of data/graphics.json \"battle\"");
      const int id = t.platform ? db.graphics.battle.findPlatform(e.second.asString()) : db.graphics.battle.findBackground(e.second.asString());
      if (id < 0) return c.fail("", where + ": there is no " + (t.platform ? "platform " : "background ") + q(e.second.asString()) + " (data/graphics.json \"battle\" > \"" + (t.platform ? "platforms" : "backgrounds") + "\")");
      if (e.first == "default") { dflt = id; continue; }
      const int terrain = db.findTerrain(e.first);
      if (terrain < 0) return c.fail("", where + ": there is no terrain with this id (use a terrain id, or \"default\")");
      byTerrain[(size_t)terrain] = id;
    }
  }
  return true;
}

// "scene": { "resultHoldMs": 533, "resultHoldAgainMs": 0 }: how long a held pose waits after the result (docs/BATTLE_SCENE.md "Holding a pose").
bool readSceneTiming(const LoadCtx& c, const Json& b, BattleSceneTiming& out) {
  if (!b.has("scene")) return true;
  const Json& sc = b["scene"];
  if (!sc.isObject()) return c.fail("battle scene", "must be an object such as { \"resultHoldMs\": 533, \"resultHoldAgainMs\": 0 }");
  for (const Json::Member& m : sc.members())
    if (m.first != "resultHoldMs" && m.first != "resultHoldAgainMs" && m.first != "hitHold" && m.first != "quote")
      return c.fail("battle scene", unknownKeyText(m.first, {"resultHoldMs", "resultHoldAgainMs", "hitHold", "quote"}));
  if (!c.integerIn(sc, "resultHoldMs", "battle scene", 0, 5000, out.resultHoldMs) || !c.integerIn(sc, "resultHoldAgainMs", "battle scene", 0, 5000, out.resultHoldAgainMs))
    return false;
  if (sc.has("quote")) {  // "quote": { "leadMs", "slideOutMs", "slideInMs" } (whole milliseconds, 0..5000)
    const Json& q = sc["quote"];
    const std::string at = "battle scene quote";
    if (!q.isObject()) return c.fail(at, "must be an object such as { \"leadMs\": 150, \"slideOutMs\": 300, \"slideInMs\": 300 }");
    for (const Json::Member& m : q.members())
      if (m.first != "leadMs" && m.first != "slideOutMs" && m.first != "slideInMs") return c.fail(at, unknownKeyText(m.first, {"leadMs", "slideOutMs", "slideInMs"}));
    if (!c.integerIn(q, "leadMs", at, 0, 5000, out.quote.leadMs) || !c.integerIn(q, "slideOutMs", at, 0, 5000, out.quote.slideOutMs) ||
        !c.integerIn(q, "slideInMs", at, 0, 5000, out.quote.slideInMs))
      return false;
  }
  if (!sc.has("hitHold")) return true;
  // "hitHold": { "minMs", "baseMs", "perHpMs", "killMs", "noChangeMs" } (all optional; decimals allowed).
  const Json& h = sc["hitHold"];
  const std::string where = "battle scene hitHold";
  if (!h.isObject()) return c.fail(where, "must be an object such as { \"minMs\": 567, \"perHpMs\": 33.333, \"killMs\": 817 }");
  static const char* const kKeys[] = {"minMs", "baseMs", "perHpMs", "killMs", "noChangeMs"};
  for (const Json::Member& m : h.members()) {
    bool known = false;
    for (const char* k : kKeys) known = known || m.first == k;
    if (!known) return c.fail(where, unknownKeyText(m.first, {"minMs", "baseMs", "perHpMs", "killMs", "noChangeMs"}));
  }
  HitHold& hh = out.hitHold;
  hh.on = true;
  hh.minMs = out.resultHoldMs;
  double* slots[] = {&hh.minMs, &hh.baseMs, &hh.perHpMs, &hh.killMs, &hh.noChangeMs};
  const double hi[] = {5000, 5000, 1000, 10000, 5000};
  bool noChangeGiven = h.has("noChangeMs");
  for (int i = 0; i < 5; ++i) {
    if (!c.num(h, kKeys[i], where, *slots[i])) return false;
    if (*slots[i] < 0 || *slots[i] > hi[i])
      return c.fail(where, std::string("\"") + kKeys[i] + "\" must be from 0 to " + std::to_string((int)hi[i]) + " (milliseconds; decimals allowed)");
  }
  if (!noChangeGiven) hh.noChangeMs = hh.minMs;  // no point changed: the plain minimum
  return true;
}

bool readBattleArt(const LoadCtx& c, Database& db, const Json& root) {
  db.graphics.battle = BattleArt();
  if (!root.has("battle")) return true;
  const Json& b = root["battle"];
  if (!b.isObject()) return c.fail("", "\"battle\" must be an object");
  if (!c.onlyKeys(b, {"sets", "effects", "assign", "backgrounds", "terrainBackground", "defaultBackground", "platforms", "terrainPlatform", "defaultPlatform",
                      "stage", "scene"},
                  "battle"))
    return false;
  BattleArt art;
  art.terrainBackground.assign(db.terrains.size(), -1);
  art.terrainPlatform.assign(db.terrains.size(), -1);
  if (!readSets(c, db, b, art) || !readEffects(c, db, b, art) || !readAssign(c, db, b, art) || !readBackgrounds(c, db, b, art)) return false;
  if (!readBattleStage(c, b, art.stage) || !readSceneTiming(c, b, art.timing)) return false;
  // A flight by "speed" is measured in stage pixels: it needs a fixed stage.
  for (size_t i = 0; i < art.effects.size(); ++i)
    if (art.effects[i].speed > 0 && !art.stage.on) return c.fail("battle effect " + std::to_string(i + 1), "\"speed\" is in stage pixels per second and needs a fixed \"stage\" (or use \"travelMs\")");
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
