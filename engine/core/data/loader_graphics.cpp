// Loads data/graphics.json (docs/GRAPHICS.md). The file is optional.
#include "core/data/loader_content.h"
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
    // [[col, row], ...]. Each is the top-left cell of a 4 x 4 set of pieces.
    const Json& jj = j["join"];
    bool list = jj.isArray() && jj.size() > 0 && jj[0].isArray();
    size_t blocks = list ? jj.size() : 1;
    if (blocks > 16) return c.fail(where, "\"join\" can list at most 16 animation frames");
    for (size_t i = 0; i < blocks; ++i) {
      SpriteCell cell;
      if (!readCell(c, list ? jj[i] : jj, where, "join", cell) || !checkInside(c, sh, cell, 4, 4, where)) return false;
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
  return true;
}

// The "units" or "structures" map of the file root or of one style: id -> look
// (+ optional per-side looks). `find` maps an id to an index into `out`, or -1.
template <class Find>
bool readThingLooks(const LoadCtx& c, const Database& db, const Json& o, const std::string& prefix,
                    const char* key, const char* word, const Find& find, std::vector<UnitLook>& out) {
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
    if (!readLook(c, db, e.second, where, false, -1, ul.base, subject.c_str())) return false;
    const Json* sides;
    if (!c.object(e.second, "sides", where, sides)) return false;
    for (const Json::Member& s : sides->members()) {
      if (s.first.empty()) return c.fail(where, "\"sides\" has an empty side id");
      Look l;
      if (!readLook(c, db, s.second, where + " side " + q(s.first), false, -1, l, subject.c_str())) return false;
      ul.sides.push_back(std::make_pair(s.first, l));
    }
  }
  return true;
}

// The "terrain" / "units" / "structures" maps of the file root or of one style, applied on
// top of `set`. A null value removes the art (placeholder look).
bool readSet(const LoadCtx& c, const Database& db, const Json& o, const std::string& prefix, GraphicsSet& set) {
  const Json* m;
  if (!c.object(o, "terrain", prefix, m)) return false;
  for (const Json::Member& e : m->members()) {
    int t = db.findTerrain(e.first);
    std::string where = prefix + (prefix.empty() ? "" : ": ") + "terrain " + q(e.first);
    if (t < 0) return c.fail(where, "there is no terrain with this id");
    if (e.second.isNull()) {
      set.terrain[(size_t)t] = Look();
      continue;
    }
    if (!readLook(c, db, e.second, where, true, t, set.terrain[(size_t)t])) return false;
  }
  if (!readThingLooks(c, db, o, prefix, "units", "unit", [&](const std::string& id) { return db.findUnitType(id); },
                      set.units))
    return false;
  // Structures look like units: one cell drawn across the footprint, optional own art per side.
  return readThingLooks(c, db, o, prefix, "structures", "structure",
                        [&](const std::string& id) { return db.findStructure(id); }, set.structures);
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
    if (db.graphics.findSheet(sh.id) >= 0) return c.fail(where, "duplicate id");
    if (!c.str(j, "file", where, sh.file, true)) return false;
    if (!goodArtPath(sh.file))
      return c.fail(where, "\"file\" must be a .png file inside the game folder (for example \"art/tiles.png\")");
    if (!c.integer(j, "cellWidth", where, sh.cellW, true)) return false;
    if (!c.integer(j, "cellHeight", where, sh.cellH, true)) return false;
    if (sh.cellW < 1 || sh.cellH < 1) return c.fail(where, "\"cellWidth\" and \"cellHeight\" must be at least 1");
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

  GraphicsSet base;
  base.terrain.assign(db.terrains.size(), Look());
  base.units.assign(db.unitTypes.size(), UnitLook());
  base.structures.assign(db.structures.size(), UnitLook());
  if (!readSet(c, db, root, "", base)) return false;

  std::vector<GraphicsSet> sets(db.styles.size(), base);
  const Json* styles;
  if (!c.object(root, "styles", "", styles)) return false;
  for (const Json::Member& e : styles->members()) {
    int s = db.findStyle(e.first);
    std::string where = "style " + q(e.first);
    if (s < 0) return c.fail(where, "there is no visual style with this id");
    if (!e.second.isObject()) return c.fail(where, "must be an object");
    if (!readSet(c, db, e.second, where, sets[(size_t)s])) return false;
  }
  // A file with no sheets cannot draw anything; keep "no art" cheap to test.
  if (!db.graphics.sheets.empty()) db.graphics.styles = sets;
  return true;
}

}  // namespace ote
