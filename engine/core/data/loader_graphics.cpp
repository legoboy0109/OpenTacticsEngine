// Loads data/graphics.json (docs/GRAPHICS.md). The file is optional.
#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {
namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// [col, row] with whole numbers >= 0.
bool readCell(const LoadCtx& c, const Json& j, const std::string& where, const char* key, SpriteCell& out) {
  if (!j.isArray() || j.size() != 2 || !LoadCtx::isWhole(j[0]) || !LoadCtx::isWhole(j[1]) || j[0].asInt() < 0 ||
      j[1].asInt() < 0 || j[0].asInt() > 30000 || j[1].asInt() > 30000)
    return c.fail(where, q(key) + " must be [column, row] (whole numbers, counted from 0)");
  out.col = (int16_t)j[0].asInt();
  out.row = (int16_t)j[1].asInt();
  return true;
}

// The w x h block of cells starting at `at` must lie inside the sheet.
bool checkInside(const LoadCtx& c, const SheetDef& sh, SpriteCell at, int w, int h, const std::string& where) {
  if (at.col + w <= sh.cols && at.row + h <= sh.rows) return true;
  std::string what = (w == 1 && h == 1) ? "cell [" + std::to_string(at.col) + ", " + std::to_string(at.row) + "]"
                                        : "the " + std::to_string(w) + " x " + std::to_string(h) +
                                              " block at [" + std::to_string(at.col) + ", " +
                                              std::to_string(at.row) + "]";
  return c.fail(where, what + " is outside sheet " + q(sh.id) + " (" + std::to_string(sh.cols) + " x " +
                           std::to_string(sh.rows) + " cells)");
}

// A unit picture's "walk": frames while it moves (docs/GRAPHICS.md "Walking"). Keys: "sheet" (default: the picture's own), "down", "up",
// "side" (faces right; the left is it turned round), "left" / "right" (own frames instead), "frameMs". Each direction is a list of
// [column, row] cells of the walk sheet. At least one direction is needed; one that is not given is not drawn (the nearest given
// direction is used instead).
bool readWalk(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, int standSheet, WalkLook& out) {
  out = WalkLook();
  const std::string w = where + " walk";
  if (!j.isObject()) return c.fail(w, "must be an object with \"down\", \"up\", \"side\" (or \"left\" / \"right\") and \"frameMs\"");
  static const char* const kKnown[] = {"sheet", "down", "up", "side", "left", "right", "frameMs"};
  for (const Json::Member& m : j.members()) {
    bool known = false;
    for (const char* k : kKnown) known |= m.first == k;
    if (!known) return c.fail(w, "has an unknown key " + q(m.first) + " (use sheet, down, up, side, left, right and frameMs)" + didYouMean(m.first, {"sheet", "down", "up", "side", "left", "right", "frameMs"}));
  }
  std::string sheetId;
  if (!c.str(j, "sheet", w, sheetId)) return false;
  out.sheet = sheetId.empty() ? standSheet : db.graphics.findSheet(sheetId);
  if (out.sheet < 0) return c.fail(w, "unknown sheet " + q(sheetId));
  const SheetDef& sh = db.graphics.sheets[(size_t)out.sheet];
  std::vector<SpriteCell> given[kWalkDirs];
  bool side = false;
  static const struct { const char* key; int dir; } kDirs[] = {{"down", WalkDown}, {"up", WalkUp}, {"right", WalkRight}, {"left", WalkLeft}, {"side", -1}};
  std::vector<SpriteCell> sideFrames;
  for (const auto& d : kDirs) {
    if (!j.has(d.key)) continue;
    const Json* list;
    if (!c.array(j, d.key, w, list, true)) return false;
    if (list->size() < 1 || list->size() > 64) return c.fail(w, q(d.key) + " must list between 1 and 64 cells");
    std::vector<SpriteCell>& cells = d.dir >= 0 ? given[d.dir] : sideFrames;
    for (size_t i = 0; i < list->size(); ++i) {
      SpriteCell cell;
      if (!readCell(c, (*list)[i], w, d.key, cell) || !checkInside(c, sh, cell, 1, 1, w)) return false;
      cells.push_back(cell);
    }
    side |= d.dir < 0;
  }
  if (given[WalkDown].empty() && given[WalkUp].empty() && given[WalkLeft].empty() && given[WalkRight].empty() && !side)
    return c.fail(w, "needs at least one of \"down\", \"up\", \"side\", \"left\" or \"right\"");
  out.frames[WalkDown] = given[WalkDown];
  out.frames[WalkUp] = given[WalkUp];
  // "side" faces right; the left is the same turned round unless "left" is given. "right" / "left" given alone mirror each other.
  out.frames[WalkRight] = !given[WalkRight].empty() ? given[WalkRight] : side ? sideFrames : given[WalkLeft];
  out.mirrored[WalkRight] = given[WalkRight].empty() && !side && !given[WalkLeft].empty();
  out.frames[WalkLeft] = !given[WalkLeft].empty() ? given[WalkLeft] : out.frames[WalkRight];
  out.mirrored[WalkLeft] = given[WalkLeft].empty() && !out.frames[WalkRight].empty();
  if (!c.integer(j, "frameMs", w, out.frameMs)) return false;
  if (out.frameMs < 20 || out.frameMs > 60000) return c.fail(w, "\"frameMs\" must be between 20 and 60000");
  return true;
}

// One look object. `terrainLook` allows "variants" and "join".
bool readLook(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, bool terrainLook,
              int selfTerrain, Look& out, const char* subject = "units") {
  out = Look();
  if (!j.isObject()) return c.fail(where, "must be an object (or null for no art)");
  std::string sheetId;
  if (!c.str(j, "sheet", where, sheetId, true)) return false;
  out.sheet = db.graphics.findSheet(sheetId);
  if (out.sheet < 0) return c.fail(where, "unknown sheet " + q(sheetId));
  const SheetDef& sh = db.graphics.sheets[(size_t)out.sheet];

  int kinds = (j.has("cell") ? 1 : 0) + (j.has("variants") ? 1 : 0) + (j.has("frames") ? 1 : 0) +
              (j.has("join") ? 1 : 0);
  const char* allowed = terrainLook ? "\"cell\", \"variants\", \"frames\" or \"join\"" : "\"cell\" or \"frames\"";
  if (kinds != 1) return c.fail(where, std::string("needs exactly one of ") + allowed);
  if (!terrainLook && (j.has("variants") || j.has("join")))
    return c.fail(where, std::string(subject) + " can only use " + allowed);

  if (j.has("cell")) {
    SpriteCell cell;
    if (!readCell(c, j["cell"], where, "cell", cell) || !checkInside(c, sh, cell, 1, 1, where)) return false;
    out.kind = Look::Still;
    out.cells.push_back(cell);
  } else if (j.has("join")) {
    // Either one block, [col, row], or several blocks played as an animation,
    // [[col, row], ...]. Each is the top-left cell of a 4 x 4 set of pieces (8 x 8 on a hexagon game).
    const int side = db.gridType == GridType::Hex ? 8 : 4;
    const Json& jj = j["join"];
    bool list = jj.isArray() && jj.size() > 0 && jj[0].isArray();
    size_t blocks = list ? jj.size() : 1;
    if (blocks > 16) return c.fail(where, "\"join\" can list at most 16 animation frames");
    for (size_t i = 0; i < blocks; ++i) {
      SpriteCell cell;
      if (!readCell(c, list ? jj[i] : jj, where, "join", cell) || !checkInside(c, sh, cell, side, side, where)) return false;
      out.cells.push_back(cell);
    }
    out.kind = Look::Join;
    out.joinWith.assign(db.terrains.size(), 0);
    if (selfTerrain >= 0) out.joinWith[(size_t)selfTerrain] = 1;
    std::vector<std::string> ids;
    if (!c.stringList(j, "joinWith", where, ids)) return false;
    for (const std::string& id : ids) {
      int t = db.findTerrain(id);
      if (t < 0) return c.fail(where, "\"joinWith\" names unknown terrain " + q(id));
      out.joinWith[(size_t)t] = 1;
    }
    // "joinExcept": joins with every terrain EXCEPT the listed ones (a bridge
    // reaches every kind of land but not the water it crosses).
    if (j.has("joinExcept")) {
      if (j.has("joinWith")) return c.fail(where, "use either \"joinWith\" or \"joinExcept\", not both");
      std::vector<std::string> not_;
      if (!c.stringList(j, "joinExcept", where, not_)) return false;
      out.joinWith.assign(db.terrains.size(), 1);
      for (const std::string& id : not_) {
        int t = db.findTerrain(id);
        if (t < 0) return c.fail(where, "\"joinExcept\" names unknown terrain " + q(id));
        if (t == selfTerrain) return c.fail(where, "\"joinExcept\" cannot list the terrain itself");
        out.joinWith[(size_t)t] = 0;
      }
    }
    if (!c.boolean(j, "joinEdge", where, out.joinEdge)) return false;
  } else {
    const char* key = j.has("variants") ? "variants" : "frames";
    const Json* list;
    if (!c.array(j, key, where, list)) return false;
    if (list->size() < 1 || list->size() > 64)
      return c.fail(where, q(key) + " must list between 1 and 64 cells");
    for (size_t i = 0; i < list->size(); ++i) {
      SpriteCell cell;
      if (!readCell(c, (*list)[i], where, key, cell) || !checkInside(c, sh, cell, 1, 1, where)) return false;
      out.cells.push_back(cell);
    }
    out.kind = j.has("variants") ? Look::Variants : Look::Animated;
    if (out.cells.size() == 1) out.kind = Look::Still;
  }
  if (!c.integer(j, "frameMs", where, out.frameMs)) return false;
  if (out.frameMs < 20 || out.frameMs > 60000) return c.fail(where, "\"frameMs\" must be between 20 and 60000");
  if (j.has("walk")) {
    if (terrainLook || std::string(subject) != "units") return c.fail(where, "only a unit picture can have \"walk\" frames");
    if (!readWalk(c, db, j["walk"], where, out.sheet, out.walk)) return false;
  }
  return true;
}

// Model file names: inside the game folder, forward slashes, ".glb" or ".gltf".
bool goodModelPath(const std::string& p) {
  if (p.size() < 5 || p[0] == '/' || p.find("..") != std::string::npos || p.find('\\') != std::string::npos ||
      p.find(':') != std::string::npos)
    return false;
  std::string low = p;
  for (char& ch : low)
    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
  auto ends = [&](const char* e) { std::string s(e); return low.size() > s.size() && low.compare(low.size() - s.size(), s.size(), s) == 0; };
  return ends(".glb") || ends(".gltf");
}

// A unit's or structure's entry that names a 3D model instead of a sheet.
bool readModelLook(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, bool view3d,
                   ModelLook& out) {
  out = ModelLook();
  if (!view3d)
    return c.fail(where, "a \"model\" can only be used in a style whose kind of view is 3D "
                         "(\"isometric-3d\" or \"3d\"); give other styles a picture");
  for (const char* key : {"sheet", "cell", "frames", "frameMs", "sides", "walk"})
    if (j.has(key)) return c.fail(where, std::string("has a \"model\", so it cannot also have ") + q(key));
  if (!c.onlyKeys(j, {"model", "height", "turn", "sideMaterial", "animations"}, where)) return false;
  if (!c.str(j, "model", where, out.file, true)) return false;
  if (!goodModelPath(out.file))
    return c.fail(where, "\"model\" must be a .glb or .gltf file inside the game folder (for example \"models/knight.glb\")");
  if (!fileExists(db.rootPath + "/" + out.file)) return c.fail(where, "model file " + q(out.file) + " does not exist");
  double height = 0, turn = 0;
  if (!c.num(j, "height", where, height) || !c.num(j, "turn", where, turn)) return false;
  if (j.has("height") && (height < 0.1 || height > 16)) return c.fail(where, "\"height\" must be between 0.1 and 16 (squares)");
  if (turn < -360 || turn > 360) return c.fail(where, "\"turn\" must be between -360 and 360 (degrees)");
  out.height = (float)height;
  out.turn = (float)turn;
  if (!c.str(j, "sideMaterial", where, out.sideMaterial)) return false;
  const Json* anims;
  if (!c.object(j, "animations", where, anims)) return false;
  static const char* const kNames[kModelAnims] = {"idle", "move", "attack", "die"};
  for (const Json::Member& a : anims->members()) {
    int which = -1;
    for (int i = 0; i < kModelAnims; ++i)
      if (a.first == kNames[i]) which = i;
    if (which < 0) return c.fail(where, "\"animations\" knows \"idle\", \"move\", \"attack\" and \"die\", not " + q(a.first));
    if (!a.second.isString()) return c.fail(where, "\"animations\" " + q(a.first) + " must be the animation's name in the model file (text)");
    out.clips[which] = a.second.asString();
  }
  return true;
}

// A terrain's "stand": what stands on its ground in a 3D style, a picture or models (docs/GRAPHICS.md
// "Standing terrain"). Strict: every key is known, the picture keys and the model keys do not mix.
bool readStand(const LoadCtx& c, const Database& db, const Json& j, const std::string& terrainWhere, bool view3d,
               StandLook& out) {
  out = StandLook();
  const std::string where = terrainWhere + " stand";
  if (j.isNull()) return true;
  if (!view3d)
    return c.fail(where, "a \"stand\" can only be used in a style whose kind of view is 3D "
                         "(\"isometric-3d\" or \"3d\"); other styles draw a terrain as its picture only");
  if (!j.isObject()) return c.fail(where, "must be an object (or null for nothing standing on the ground)");
  static const char* const kPicture[] = {"sheet", "cell", "frames", "variants", "frameMs", "width"};
  static const char* const kModel[] = {"model", "variants", "height", "turn"};
  auto in = [](const char* const* list, size_t n, const std::string& key) {
    for (size_t i = 0; i < n; ++i)
      if (key == list[i]) return true;
    return false;
  };
  // Which kind: a "model", else a "sheet", else "variants" that name files (a list of models).
  const bool modelVariants = j.has("variants") && j["variants"].isArray() && j["variants"].size() > 0 && j["variants"][0].isString();
  const bool model = j.has("model") || (!j.has("sheet") && modelVariants);
  if (!model && !j.has("sheet")) return c.fail(where, "needs a \"sheet\" (an upright picture) or a \"model\" (a 3D model)");
  for (const Json::Member& m : j.members()) {
    if (m.first == "scatter") continue;
    const bool mine = model ? in(kModel, 4, m.first) : in(kPicture, 6, m.first);
    if (mine) continue;
    const bool other = model ? in(kPicture, 6, m.first) : in(kModel, 4, m.first);
    if (other)
      return c.fail(where, std::string(model ? "has a \"model\", so it cannot also have " : "is a picture, so it cannot have ") + q(m.first));
    return c.fail(where, "has an unknown key " + q(m.first) + didYouMean(m.first, {"sheet", "cell", "frames", "variants", "frameMs", "width", "model", "height", "turn", "scatter"}));
  }
  if (!c.boolean(j, "scatter", where, out.scatter)) return false;

  if (!model) {
    int kinds = (j.has("cell") ? 1 : 0) + (j.has("variants") ? 1 : 0) + (j.has("frames") ? 1 : 0);
    if (kinds != 1) return c.fail(where, "needs exactly one of \"cell\", \"variants\" or \"frames\"");
    if (!readLook(c, db, j, where, true, -1, out.picture)) return false;
    int width = 0;
    if (!c.integer(j, "width", where, width)) return false;
    if (j.has("width") && (width < 1 || width > 512)) return c.fail(where, "\"width\" must be between 1 and 512 (pixels; the tile size is one square)");
    out.width = (float)width;
    return true;
  }

  if (j.has("model") && j.has("variants"))
    return c.fail(where, "needs exactly one of \"model\" (one file) or \"variants\" (a list of model files)");
  std::vector<std::string> files;
  if (j.has("model")) {
    std::string f;
    if (!c.str(j, "model", where, f, true)) return false;
    files.push_back(f);
  } else {
    const Json* list;
    if (!c.array(j, "variants", where, list)) return false;
    if (list->size() < 1 || list->size() > 64) return c.fail(where, "\"variants\" must list between 1 and 64 model files");
    for (size_t i = 0; i < list->size(); ++i) {
      if (!(*list)[i].isString()) return c.fail(where, "\"variants\" must list model files (text)");
      files.push_back((*list)[i].asString());
    }
  }
  double height = 1.0, turn = 0;
  if (!c.num(j, "height", where, height) || !c.num(j, "turn", where, turn)) return false;
  if (j.has("height") && (height < 0.1 || height > 16)) return c.fail(where, "\"height\" must be between 0.1 and 16 (squares)");
  if (turn < -360 || turn > 360) return c.fail(where, "\"turn\" must be between -360 and 360 (degrees)");
  for (const std::string& f : files) {
    if (!goodModelPath(f))
      return c.fail(where, "a model must be a .glb or .gltf file inside the game folder (for example \"models/tree.glb\"), not " + q(f));
    if (!fileExists(db.rootPath + "/" + f)) return c.fail(where, "model file " + q(f) + " does not exist");
    ModelLook ml;
    ml.file = f;
    ml.height = (float)height;
    ml.turn = (float)turn;
    ml.sideMaterial.clear();  // terrain belongs to nobody: no side colour
    out.models.push_back(ml);
  }
  return true;
}

// The "units" or "structures" map of the file root or of one style: id -> look
// (+ optional per-side looks). `find` maps an id to an index into `out`, or -1.
template <class Find>
bool readThingLooks(const LoadCtx& c, const Database& db, const Json& o, const std::string& prefix,
                    const char* key, const char* word, const Find& find, bool view3d, std::vector<UnitLook>& out) {
  const Json* m;
  if (!c.object(o, key, prefix, m)) return false;
  const std::string subject = std::string(word) + "s";
  for (const Json::Member& e : m->members()) {
    int u = find(e.first);
    std::string where = prefix + (prefix.empty() ? "" : ": ") + word + " " + q(e.first);
    if (u < 0) return c.fail(where, std::string("there is no ") + word + " with this id");
    UnitLook& ul = out[(size_t)u];
    ul = UnitLook();
    if (e.second.isNull()) continue;
    if (e.second.isObject() && e.second.has("model")) {
      if (!readModelLook(c, db, e.second, where, view3d, ul.model)) return false;
      continue;
    }
    if (!readLook(c, db, e.second, where, false, -1, ul.base, subject.c_str())) return false;
    if (!c.onlyKeys(e.second, {"sheet", "cell", "frames", "frameMs", "sides", "walk"}, where)) return false;  // (after readLook: its message about a "join" wins)
    const Json* sides;
    if (!c.object(e.second, "sides", where, sides)) return false;
    for (const Json::Member& s : sides->members()) {
      if (s.first.empty()) return c.fail(where, "\"sides\" has an empty side id");
      Look l;
      if (!readLook(c, db, s.second, where + " side " + q(s.first), false, -1, l, subject.c_str())) return false;
      if (!c.onlyKeys(s.second, {"sheet", "cell", "frames", "frameMs", "walk"}, where + " side " + q(s.first))) return false;
      ul.sides.push_back(std::make_pair(s.first, l));
    }
  }
  return true;
}

// The "portraits" map of the file root or of one style: id -> { "sheet", "cell" }, applied on top of
// what `set` already has (replacing per id; null removes the id).
bool readPortraits(const LoadCtx& c, const Database& db, const Json& o, const std::string& prefix, GraphicsSet& set) {
  const Json* m;
  if (!c.object(o, "portraits", prefix, m)) return false;
  for (const Json::Member& e : m->members()) {
    std::string where = prefix + (prefix.empty() ? "" : ": ") + "portrait " + q(e.first);
    if (e.first.empty()) return c.fail(where, "the id must not be empty");
    auto& list = set.portraits;
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i].first == e.first) {
        list.erase(list.begin() + (long)i);
        break;
      }
    if (e.second.isNull()) continue;
    if (e.second.isObject() && (e.second.has("frames") || e.second.has("variants") || e.second.has("join")))
      return c.fail(where, "a portrait is one \"cell\" of a sheet");
    if (!c.onlyKeys(e.second, {"sheet", "cell", "frameMs", "faces", "talk", "blink"}, where)) return false;
    Look look;
    if (!readLook(c, db, e.second, where, false, -1, look, "portraits")) return false;
    if (look.kind != Look::Still) return c.fail(where, "a portrait is one \"cell\" of a sheet");
    if (e.second.has("faces")) {
      std::string f;
      if (!c.str(e.second, "faces", where, f)) return false;
      if (f != "left" && f != "right") return c.fail(where, "\"faces\" must be \"left\" or \"right\": the way the picture looks");
      look.faces = f == "left" ? 1 : 2;
    }
    for (const char* key : {"talk", "blink"}) {  // other frames of the face (docs/GRAPHICS.md "Portraits")
      if (!e.second.has(key)) continue;
      const Json* frames;
      if (!c.array(e.second, key, where, frames, true)) return false;
      if (frames->size() < 1 || frames->size() > 8) return c.fail(where, q(key) + " must list between 1 and 8 cells of the portrait's sheet");
      const SheetDef& sh = db.graphics.sheets[(size_t)look.sheet];
      for (size_t i = 0; i < frames->size(); ++i) {
        SpriteCell cell;
        if (!readCell(c, (*frames)[i], where, key, cell) || !checkInside(c, sh, cell, 1, 1, where)) return false;
        (std::string(key) == "talk" ? look.talk : look.blink).push_back(cell);
      }
    }
    list.push_back(std::make_pair(e.first, look));
  }
  return true;
}

// The "terrain" / "units" / "structures" maps of the file root or of one style, applied on
// top of `set`. A null value removes the art (placeholder look).
bool readSet(const LoadCtx& c, const Database& db, const Json& o, const std::string& prefix, bool view3d,
             GraphicsSet& set) {
  if (!readPortraits(c, db, o, prefix, set)) return false;
  const Json* m;
  if (!c.object(o, "terrain", prefix, m)) return false;
  for (const Json::Member& e : m->members()) {
    int t = db.findTerrain(e.first);
    std::string where = prefix + (prefix.empty() ? "" : ": ") + "terrain " + q(e.first);
    if (t < 0) return c.fail(where, "there is no terrain with this id");
    set.stands[(size_t)t] = StandLook();
    if (e.second.isNull()) {
      set.terrain[(size_t)t] = Look();
      continue;
    }
    if (!readLook(c, db, e.second, where, true, t, set.terrain[(size_t)t])) return false;
    if (!c.onlyKeys(e.second, {"sheet", "cell", "variants", "frames", "join", "joinWith", "joinExcept", "joinEdge", "frameMs", "stand"}, where)) return false;
    if (e.second.has("stand") && !readStand(c, db, e.second["stand"], where, view3d, set.stands[(size_t)t])) return false;
  }
  if (!readThingLooks(c, db, o, prefix, "units", "unit", [&](const std::string& id) { return db.findUnitType(id); },
                      view3d, set.units))
    return false;
  // Structures look like units: one cell drawn across the footprint, optional own art per side.
  return readThingLooks(c, db, o, prefix, "structures", "structure",
                        [&](const std::string& id) { return db.findStructure(id); }, view3d, set.structures);
}

// Art file names: inside the game folder, forward slashes, ".png".
bool goodArtPath(const std::string& p) {
  if (p.size() < 5 || p[0] == '/' || p.find("..") != std::string::npos || p.find('\\') != std::string::npos ||
      p.find(':') != std::string::npos)
    return false;
  std::string ext = p.substr(p.size() - 4);
  for (char& ch : ext)
    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
  return ext == ".png";
}

}  // namespace

bool loadGraphics(const std::string& path, Database& db, std::string& err) {
  db.graphics = Graphics();
  if (!fileExists(path)) return true;  // no art: placeholder look everywhere
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);

  if (!c.onlyKeys(root, {"sheets", "unitMarker", "backgrounds", "tilesets", "terrain", "units", "structures", "portraits", "styles", "battle"}, ""))
    return false;
  const Json* sheets;
  if (!c.array(root, "sheets", "", sheets)) return false;
  for (size_t i = 0; i < sheets->size(); ++i) {
    const Json& j = (*sheets)[i];
    std::string where = "sheets[" + std::to_string(i) + "]";
    if (!j.isObject()) return c.fail(where, "must be an object");
    SheetDef sh;
    if (!c.str(j, "id", where, sh.id, true)) return false;
    if (sh.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = "sheet " + q(sh.id);
    if (!c.onlyKeys(j, {"id", "file", "cellWidth", "cellHeight", "smooth"}, where)) return false;
    if (db.graphics.findSheet(sh.id) >= 0) return c.fail(where, "duplicate id");
    if (!c.str(j, "file", where, sh.file, true)) return false;
    if (!goodArtPath(sh.file))
      return c.fail(where, "\"file\" must be a .png file inside the game folder (for example \"art/tiles.png\")");
    if (!c.integer(j, "cellWidth", where, sh.cellW, true)) return false;
    if (!c.integer(j, "cellHeight", where, sh.cellH, true)) return false;
    if (sh.cellW < 1 || sh.cellH < 1) return c.fail(where, "\"cellWidth\" and \"cellHeight\" must be at least 1");
    if (!c.boolean(j, "smooth", where, sh.smooth)) return false;
    std::string why;
    if (!readPngSize(db.rootPath + "/" + sh.file, sh.width, sh.height, why))
      return c.fail(where, "image " + q(sh.file) + ": " + why);
    if (sh.width > kMaxSheetSize || sh.height > kMaxSheetSize)
      return c.fail(where, "image " + q(sh.file) + " is " + std::to_string(sh.width) + " x " +
                               std::to_string(sh.height) + " pixels; the largest allowed is " +
                               std::to_string(kMaxSheetSize) + " x " + std::to_string(kMaxSheetSize));
    sh.cols = sh.width / sh.cellW;
    sh.rows = sh.height / sh.cellH;
    if (sh.cols < 1 || sh.rows < 1)
      return c.fail(where, "the cell size (" + std::to_string(sh.cellW) + " x " + std::to_string(sh.cellH) +
                               ") is larger than image " + q(sh.file) + " (" + std::to_string(sh.width) + " x " +
                               std::to_string(sh.height) + " pixels)");
    db.graphics.sheets.push_back(sh);
  }

  if (!c.boolean(root, "unitMarker", "", db.graphics.unitMarker)) return false;
  if (!readSceneBackgrounds(c, db, root)) return false;
  if (!readTilesets(c, db, root)) return false;  // loader_tilesets.cpp

  GraphicsSet base;
  base.terrain.assign(db.terrains.size(), Look());
  base.stands.assign(db.terrains.size(), StandLook());
  base.units.assign(db.unitTypes.size(), UnitLook());
  base.structures.assign(db.structures.size(), UnitLook());
  if (!readSet(c, db, root, "", false, base)) return false;

  // A top-down style starts from the general art and replaces parts of it. A
  // style with another kind of view starts with no art at all: pictures made
  // for one kind of view are wrong in another (docs/VIEWS.md).
  GraphicsSet none = base;
  none.terrain.assign(db.terrains.size(), Look());
  none.stands.assign(db.terrains.size(), StandLook());
  none.units.assign(db.unitTypes.size(), UnitLook());
  none.structures.assign(db.structures.size(), UnitLook());
  std::vector<GraphicsSet> sets;
  for (const StyleDef& st : db.styles) sets.push_back(st.view == StyleView::TopDown ? base : none);
  const Json* styles;
  if (!c.object(root, "styles", "", styles)) return false;
  for (const Json::Member& e : styles->members()) {
    int s = db.findStyle(e.first);
    std::string where = "style " + q(e.first);
    if (s < 0) return c.fail(where, "there is no visual style with this id");
    if (!e.second.isObject()) return c.fail(where, "must be an object");
    if (!c.onlyKeys(e.second, {"terrain", "units", "structures", "portraits"}, where)) return false;
    StyleView view = db.styles[(size_t)s].view;
    if (!readSet(c, db, e.second, where, view == StyleView::Isometric3D || view == StyleView::Full3D, sets[(size_t)s])) return false;
  }
  bool models = false;
  for (const GraphicsSet& set : sets) {
    for (const UnitLook& u : set.units) models |= u.model.any();
    for (const UnitLook& u : set.structures) models |= u.model.any();
    for (const StandLook& s : set.stands) models |= s.isModel();
  }
  // A file with no sheets and no models cannot draw anything; keep "no art" cheap to test.
  if (!db.graphics.sheets.empty() || models) db.graphics.styles = sets;
  return readBattleArt(c, db, root);  // the battle scene's animation sets and backgrounds (docs/BATTLE_SCENE.md)
}

}  // namespace ote
