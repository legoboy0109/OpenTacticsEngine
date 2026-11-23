// data/graphics.json "badges": a small picture drawn on the map sprite of the units that have a tag (docs/GRAPHICS.md "Unit badges").
// Presentation only. [ { "id": "boss", "sheet": "marks", "cell": [0, 0], "tag": "boss", "sides": "computer",
//                        "anchor": "bottomRight", "offset": [1, -1] } ]
// `sheet` alone = the sheet's whole image, with `cell` = one cell (as a story background). The sheets, unit types and characters are loaded already.
#include <algorithm>

#include "core/data/events.h"
#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

bool tagExists(const Database& db, const std::string& tag) {
  for (const UnitTypeDef& t : db.unitTypes)
    if (std::find(t.tags.begin(), t.tags.end(), tag) != t.tags.end()) return true;
  for (const CharacterDef& ch : db.characters)
    if (std::find(ch.tags.begin(), ch.tags.end(), tag) != ch.tags.end()) return true;
  return false;
}

}  // namespace

bool readBadges(const LoadCtx& c, Database& db, const Json& root) {
  db.graphics.badges.clear();
  if (!root.has("badges")) return true;
  const Json& list = root["badges"];
  if (!list.isArray()) return c.fail("", "\"badges\" must be a list of badges: [ { \"id\": \"boss\", \"sheet\": \"marks\", \"tag\": \"boss\" } ]");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    std::string where = "badge " + std::to_string(i + 1);
    if (!e.isObject()) return c.fail(where, "must be an object such as { \"id\": \"boss\", \"sheet\": \"marks\", \"tag\": \"boss\" }");
    BadgeDef b;
    if (!c.str(e, "id", where, b.id, true)) return false;
    where = "badge \"" + b.id + "\"";
    if (b.id.empty() || !isStoryId(b.id)) return c.fail(where, "the id must be letters, digits, \"_\" and \"-\" (and not empty)");
    for (const BadgeDef& o : db.graphics.badges)
      if (o.id == b.id) return c.fail(where, "duplicate id");
    for (const Json::Member& k : e.members())
      if (k.first != "id" && k.first != "sheet" && k.first != "cell" && k.first != "tag" && k.first != "sides" && k.first != "anchor" && k.first != "offset")
        return c.fail(where, unknownKeyText(k.first, {"id", "sheet", "cell", "tag", "sides", "anchor", "offset"}));
    std::string sheetId;
    if (!c.str(e, "sheet", where, sheetId, true)) return false;
    b.sheet = db.graphics.findSheet(sheetId);
    if (b.sheet < 0) return c.fail(where, "unknown sheet \"" + sheetId + "\"");
    const SheetDef& sh = db.graphics.sheets[(size_t)b.sheet];
    if (e.has("cell")) {
      const Json& j = e["cell"];
      if (!j.isArray() || j.size() != 2 || !LoadCtx::isWhole(j[0]) || !LoadCtx::isWhole(j[1]) || j[0].asInt() < 0 || j[1].asInt() < 0 || j[0].asInt() > 30000 ||
          j[1].asInt() > 30000)
        return c.fail(where, "\"cell\" must be [column, row] (whole numbers, counted from 0)");
      b.whole = false;
      b.cell.col = (int16_t)j[0].asInt();
      b.cell.row = (int16_t)j[1].asInt();
      if (b.cell.col >= sh.cols || b.cell.row >= sh.rows)
        return c.fail(where, "cell [" + std::to_string(b.cell.col) + ", " + std::to_string(b.cell.row) + "] is outside sheet \"" + sh.id + "\" (" +
                                 std::to_string(sh.cols) + " x " + std::to_string(sh.rows) + " cells)");
    }
    if (!c.str(e, "tag", where, b.tag, true)) return false;
    if (b.tag.empty()) return c.fail(where, "\"tag\" must not be empty");
    if (!tagExists(db, b.tag))
      return c.fail(where, "\"tag\" names \"" + b.tag + "\", which no unit type (units.json \"tags\") and no character (characters.json \"tags\") has");
    if (e.has("sides")) {
      const Json& s = e["sides"];
      if (s.isString()) {
        const std::string w = s.asString();
        if (w == "all") b.sides = BadgeDef::Sides::All;
        else if (w == "computer") b.sides = BadgeDef::Sides::Computer;
        else if (w == "player") b.sides = BadgeDef::Sides::Player;
        else return c.fail(where, "\"sides\" must be \"all\", \"computer\", \"player\" or a list of side ids, got \"" + w + "\"");
      } else if (s.isArray()) {
        b.sides = BadgeDef::Sides::List;
        if (s.size() == 0) return c.fail(where, "\"sides\" is an empty list: name at least one side id, or use \"all\"");
        for (const Json& id : s.items()) {
          if (!id.isString() || id.asString().empty()) return c.fail(where, "\"sides\" must be a list of side ids (text)");
          b.sideIds.push_back(id.asString());
        }
      } else {
        return c.fail(where, "\"sides\" must be \"all\", \"computer\", \"player\" or a list of side ids");
      }
    }
    if (e.has("anchor")) {
      std::string a;
      if (!c.str(e, "anchor", where, a)) return false;
      static const char* const kNames[] = {"topLeft", "topRight", "bottomLeft", "bottomRight", "center"};
      int found = -1;
      for (int k = 0; k < 5; ++k)
        if (a == kNames[k]) found = k;
      if (found < 0) return c.fail(where, "\"anchor\" must be \"topLeft\", \"topRight\", \"bottomLeft\", \"bottomRight\" or \"center\", got \"" + a + "\"");
      b.anchor = (BadgeDef::Anchor)found;
    }
    if (e.has("offset")) {
      const Json& o = e["offset"];
      if (!o.isArray() || o.size() != 2 || !LoadCtx::isWhole(o[0]) || !LoadCtx::isWhole(o[1]) || o[0].asInt() < -256 || o[0].asInt() > 256 || o[1].asInt() < -256 ||
          o[1].asInt() > 256)
        return c.fail(where, "\"offset\" must be [x, y]: whole numbers from -256 to 256, in the pixels of one square of the game's tile size");
      b.dx = o[0].asInt();
      b.dy = o[1].asInt();
    }
    db.graphics.badges.push_back(b);
  }
  return true;
}

// "markers": [ { "id": "interest", "sheet": "marks", "cell": [0, 0], "anchor": "center", "offset": [0, 0] } ]
bool readMarkers(const LoadCtx& c, Database& db, const Json& root) {
  db.graphics.markers.clear();
  if (!root.has("markers")) return true;
  const Json& list = root["markers"];
  if (!list.isArray()) return c.fail("", "\"markers\" must be a list of markers: [ { \"id\": \"interest\", \"sheet\": \"marks\" } ]");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    std::string where = "marker " + std::to_string(i + 1);
    if (!e.isObject()) return c.fail(where, "must be an object such as { \"id\": \"interest\", \"sheet\": \"marks\" }");
    MarkerDef m;
    if (!c.str(e, "id", where, m.id, true)) return false;
    where = "marker \"" + m.id + "\"";
    if (m.id.empty() || !isStoryId(m.id)) return c.fail(where, "the id must be letters, digits, \"_\" and \"-\" (and not empty)");
    if (db.graphics.findMarker(m.id) >= 0) return c.fail(where, "duplicate id");
    for (const Json::Member& k : e.members())
      if (k.first != "id" && k.first != "sheet" && k.first != "cell" && k.first != "anchor" && k.first != "offset")
        return c.fail(where, unknownKeyText(k.first, {"id", "sheet", "cell", "anchor", "offset"}));
    std::string sheetId;
    if (!c.str(e, "sheet", where, sheetId, true)) return false;
    m.sheet = db.graphics.findSheet(sheetId);
    if (m.sheet < 0) return c.fail(where, "unknown sheet \"" + sheetId + "\"");
    const SheetDef& sh = db.graphics.sheets[(size_t)m.sheet];
    if (e.has("cell")) {
      const Json& j = e["cell"];
      if (!j.isArray() || j.size() != 2 || !LoadCtx::isWhole(j[0]) || !LoadCtx::isWhole(j[1]) || j[0].asInt() < 0 || j[1].asInt() < 0 || j[0].asInt() > 30000 ||
          j[1].asInt() > 30000)
        return c.fail(where, "\"cell\" must be [column, row] (whole numbers, counted from 0)");
      m.whole = false;
      m.cell.col = (int16_t)j[0].asInt();
      m.cell.row = (int16_t)j[1].asInt();
      if (m.cell.col >= sh.cols || m.cell.row >= sh.rows)
        return c.fail(where, "cell [" + std::to_string(m.cell.col) + ", " + std::to_string(m.cell.row) + "] is outside sheet \"" + sh.id + "\" (" +
                                 std::to_string(sh.cols) + " x " + std::to_string(sh.rows) + " cells)");
    }
    if (e.has("anchor")) {
      std::string a;
      if (!c.str(e, "anchor", where, a)) return false;
      static const char* const kNames[] = {"topLeft", "topRight", "bottomLeft", "bottomRight", "center"};
      int found = -1;
      for (int k = 0; k < 5; ++k)
        if (a == kNames[k]) found = k;
      if (found < 0) return c.fail(where, "\"anchor\" must be \"topLeft\", \"topRight\", \"bottomLeft\", \"bottomRight\" or \"center\", got \"" + a + "\"");
      m.anchor = (BadgeDef::Anchor)found;
    }
    if (e.has("offset")) {
      const Json& o = e["offset"];
      if (!o.isArray() || o.size() != 2 || !LoadCtx::isWhole(o[0]) || !LoadCtx::isWhole(o[1]) || o[0].asInt() < -256 || o[0].asInt() > 256 || o[1].asInt() < -256 ||
          o[1].asInt() > 256)
        return c.fail(where, "\"offset\" must be [x, y]: whole numbers from -256 to 256, in the pixels of one square of the game's tile size");
      m.dx = o[0].asInt();
      m.dy = o[1].asInt();
    }
    db.graphics.markers.push_back(m);
  }
  return true;
}

}  // namespace ote
