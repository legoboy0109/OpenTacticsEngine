// A map's "backdrop" (docs/VIEWS.md "Backdrop"): the prerendered picture the 2D isometric view draws in place of its generated
// squares, and the over picture drawn over the units.
//   "backdrop": { "sheet": "snow_ground", "origin": [256, 256], "over": "snow_props",
//                 "pieces": [ { "rect": [96, 180, 32, 40], "square": [3, 1] } ], "styles": ["iso"] }
// Presentation only: the squares' terrain and heights still decide everything; nothing here is saved or hashed.
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {
std::string q(const std::string& s) { return "\"" + s + "\""; }
constexpr int kMaxPieces = 4096;
constexpr int kMaxOrigin = 8192;

bool wholeIn(const Json& v, int lo, int hi) { return LoadCtx::isWhole(v) && v.asInt() >= lo && v.asInt() <= hi; }

bool readSheetRef(const LoadCtx& c, const Database& db, const Json& j, const char* key, const std::string& where, bool required, int& out) {
  std::string id;
  if (!c.str(j, key, where, id, required)) return false;
  out = -1;
  if (id.empty() && !required) return true;
  out = db.graphics.findSheet(id);
  if (out < 0)
    return c.fail(where, q(key) + " names unknown sheet " + q(id) + (db.graphics.sheets.empty() ? " (data/graphics.json has no \"sheets\")" : ""));
  return true;
}

bool readPieces(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, const MapDef& m, MapBackdrop& out) {
  if (!j.has("pieces")) return true;
  if (out.over < 0) return c.fail(where, "\"pieces\" cut the \"over\" picture, so they need an \"over\" sheet");
  const Json* list;
  if (!c.array(j, "pieces", where, list, true)) return false;
  if (list->size() < 1 || list->size() > (size_t)kMaxPieces)
    return c.fail(where, "\"pieces\" must list between 1 and " + std::to_string(kMaxPieces) + " pieces");
  const SheetDef& over = db.graphics.sheets[(size_t)out.over];
  for (size_t i = 0; i < list->size(); ++i) {
    const std::string at = where + " pieces[" + std::to_string(i) + "]";
    const Json& p = (*list)[i];
    if (!p.isObject()) return c.fail(at, "must be { \"rect\": [x, y, width, height], \"square\": [x, y] }");
    if (!c.onlyKeys(p, {"rect", "square"}, at)) return false;
    if (!p.has("rect") || !p.has("square")) return c.fail(at, "needs both \"rect\" and \"square\"");
    const Json& r = p["rect"];
    if (!r.isArray() || r.size() != 4 || !wholeIn(r[0], 0, 4096) || !wholeIn(r[1], 0, 4096) || !wholeIn(r[2], 1, 4096) || !wholeIn(r[3], 1, 4096))
      return c.fail(at, "\"rect\" must be [x, y, width, height]: whole numbers, x and y from 0, width and height at least 1 (pixels of the over picture)");
    BackdropPiece bp;
    bp.x = r[0].asInt();
    bp.y = r[1].asInt();
    bp.w = r[2].asInt();
    bp.h = r[3].asInt();
    if (bp.x + bp.w > over.width || bp.y + bp.h > over.height)
      return c.fail(at, "\"rect\" [" + std::to_string(bp.x) + ", " + std::to_string(bp.y) + ", " + std::to_string(bp.w) + ", " + std::to_string(bp.h) +
                            "] reaches outside the over picture " + q(over.id) + " (" + std::to_string(over.width) + " x " + std::to_string(over.height) + " pixels)");
    const Json& s = p["square"];
    if (!s.isArray() || s.size() != 2 || !wholeIn(s[0], 0, 9999) || !wholeIn(s[1], 0, 9999))
      return c.fail(at, "\"square\" must be [x, y]: the square the piece stands on");
    bp.gx = s[0].asInt();
    bp.gy = s[1].asInt();
    if (bp.gx >= m.width || bp.gy >= m.height)
      return c.fail(at, "the square " + std::to_string(bp.gx) + "," + std::to_string(bp.gy) + " is outside the map (" + std::to_string(m.width) + " x " +
                            std::to_string(m.height) + ")");
    out.pieces.push_back(bp);
  }
  return true;
}
}  // namespace

bool readMapBackdrop(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  m.backdrop = MapBackdrop();
  if (!root.has("backdrop")) return true;
  const Json& j = root["backdrop"];
  const std::string where = "backdrop";
  if (!j.isObject())
    return c.fail(where, "must be an object: { \"sheet\": \"<sheet id>\", \"origin\": [x, y], \"over\": \"<sheet id>\", \"pieces\": [ ... ] }");
  if (!c.onlyKeys(j, {"sheet", "origin", "over", "pieces", "styles"}, where)) return false;
  if (db.gridType == GridType::Hex)
    return c.fail(where, "backdrops are not supported on hexagon games (a hexagon map is drawn from its terrain's looks)");
  MapBackdrop b;
  if (!readSheetRef(c, db, j, "sheet", where, true, b.sheet) || !readSheetRef(c, db, j, "over", where, false, b.over)) return false;
  if (j.has("origin")) {
    const Json& o = j["origin"];
    if (!o.isArray() || o.size() != 2 || !wholeIn(o[0], -kMaxOrigin, kMaxOrigin) || !wholeIn(o[1], -kMaxOrigin, kMaxOrigin))
      return c.fail(where, "\"origin\" must be [x, y]: whole numbers from -8192 to 8192 (the pixel of the picture where the top corner of square 0,0 is at height 0)");
    b.originX = o[0].asInt();
    b.originY = o[1].asInt();
  }
  // Which styles draw it: the listed ones (each of the 2D isometric kind), else every such style. A game without one would never show it.
  std::vector<std::string> ids;
  if (!c.stringList(j, "styles", where, ids)) return false;
  for (const std::string& id : ids) {
    const int s = db.findStyle(id);
    if (s < 0) return c.fail(where, "\"styles\" names unknown visual style " + q(id));
    if (db.styles[(size_t)s].view != StyleView::Isometric)
      return c.fail(where, "\"styles\" names " + q(id) + ", which is not of the 2D isometric kind (\"view\": \"isometric\"); only that kind draws a backdrop");
    b.styles.push_back(s);
  }
  if (j.has("styles") && ids.empty()) return c.fail(where, "\"styles\" must list at least one style (leave it out for every 2D isometric style)");
  bool anyIso = false;
  for (const StyleDef& st : db.styles) anyIso |= st.view == StyleView::Isometric;
  if (!anyIso)
    return c.fail(where, "only the 2D isometric kind of view draws a backdrop, and game.json has no style with \"view\": \"isometric\"");
  if (!readPieces(c, db, j, where, m, b)) return false;
  m.backdrop = b;
  return true;
}

}  // namespace ote
