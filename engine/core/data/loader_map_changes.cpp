// A map's "changes" (docs/EVENTS.md "Changing the map"):
//
//   "changes": { "bridge": { "cells": [ { "x": 4, "y": 2, "terrain": "bridge", "tile": 57 }, ... ], "keepTile": false,
//                            "structures": [ { "id": "gate", "kind": "gate_open" } ] } }
//
// "cells" gives squares another terrain, "structures" turns structures the map placed (named by their "id") into another kind of the
// same size. Applied and reverted by the story commands applyChange / revertChange. Every refusal names the map file, the change and
// the entry.
#include <set>

#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool ok = false;
    for (const char* k : keys) ok = ok || m.first == k;
    if (!ok) return c.fail(where, "unknown key " + q(m.first));
  }
  return true;
}

// A change's "tile" (a tile number of the map's tileset, or -1 / null = no tile).
bool readChangeTile(const LoadCtx& c, const Database& db, const MapDef& m, const Json& v, const std::string& at, int& out) {
  if (m.tileset < 0) return c.fail(at, "\"tile\" needs a tile layer: the map has no \"tileLayer\"");
  if (v.isNull()) {
    out = -1;
    return true;
  }
  const int count = db.graphics.tilesets[(size_t)m.tileset].count();
  if (!LoadCtx::isWhole(v) || v.asInt() < -1 || v.asInt() >= count)
    return c.fail(at, "\"tile\" must be -1 (no tile) or a tile number from 0 to " + std::to_string(count - 1) + " of tileset " +
                          q(db.graphics.tilesets[(size_t)m.tileset].id));
  out = v.asInt();
  return true;
}

}  // namespace

bool readMapChanges(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  if (!root.has("changes")) return true;
  const Json& all = root["changes"];
  if (!all.isObject()) return c.fail("changes", "must be an object: { \"<change id>\": { \"cells\": [...] } }");
  if ((int)all.size() > kMaxMapChanges) return c.fail("changes", "has more than " + std::to_string(kMaxMapChanges) + " changes");
  for (const Json::Member& mem : all.members()) {
    MapChangeDef ch;
    ch.id = mem.first;
    const std::string where = "change " + q(ch.id);
    if (!isStoryId(ch.id)) return c.fail("changes", "the id " + q(ch.id) + " may only have letters, digits, \"_\" and \"-\"");
    const Json& j = mem.second;
    if (!j.isObject()) return c.fail(where, "must be an object such as { \"cells\": [ { \"x\": 3, \"y\": 4, \"terrain\": \"bridge\" } ] }");
    if (!onlyKeys(c, j, {"cells", "structures", "keepTile"}, where)) return false;
    bool keepTile = false;  // docs/GRAPHICS.md "Tile layer": squares whose terrain changes keep their tile
    if (!c.boolean(j, "keepTile", where, keepTile)) return false;
    std::set<std::pair<int, int>> seen;
    if (j.has("cells")) {
      if (!j["cells"].isArray()) return c.fail(where, "\"cells\" must be a list of { \"x\", \"y\", \"terrain\" }");
      if ((int)j["cells"].size() > kMaxChangeCells) return c.fail(where, "\"cells\" has more than " + std::to_string(kMaxChangeCells) + " squares");
      for (size_t i = 0; i < j["cells"].size(); ++i) {
        const Json& e = j["cells"][i];
        const std::string at = where + ": cell " + std::to_string(i + 1);
        if (!e.isObject()) return c.fail(at, "must be { \"x\": 3, \"y\": 4, \"terrain\": \"bridge\" }");
        if (!onlyKeys(c, e, {"x", "y", "terrain", "tile", "keepTile"}, at)) return false;
        MapChangeCell cell;
        cell.keepTile = keepTile;
        if (!c.boolean(e, "keepTile", at, cell.keepTile)) return false;
        if (e.has("tile") && !readChangeTile(c, db, m, e["tile"], at, cell.tile)) return false;
        std::string terrain;
        if (!c.integer(e, "x", at, cell.x, true) || !c.integer(e, "y", at, cell.y, true) || !c.str(e, "terrain", at, terrain, true)) return false;
        if (cell.x < 0 || cell.y < 0 || cell.x >= m.width || cell.y >= m.height)
          return c.fail(at, "the square " + std::to_string(cell.x) + "," + std::to_string(cell.y) + " is outside the map");
        cell.terrain = db.findTerrain(terrain);
        if (cell.terrain < 0) return c.fail(at, "\"terrain\" names unknown terrain " + q(terrain));
        if (!seen.insert({cell.x, cell.y}).second)
          return c.fail(at, "the square " + std::to_string(cell.x) + "," + std::to_string(cell.y) + " is in this change twice");
        ch.cells.push_back(cell);
      }
    }
    if (j.has("structures")) {
      if (!j["structures"].isArray()) return c.fail(where, "\"structures\" must be a list of { \"id\", \"kind\" }");
      for (size_t i = 0; i < j["structures"].size(); ++i) {
        const Json& e = j["structures"][i];
        const std::string at = where + ": structure " + std::to_string(i + 1);
        if (!e.isObject()) return c.fail(at, "must be { \"id\": \"gate\", \"kind\": \"gate_open\" }");
        if (!onlyKeys(c, e, {"id", "kind"}, at)) return false;
        MapChangeStructure s;
        std::string kind;
        if (!c.str(e, "id", at, s.id, true) || !c.str(e, "kind", at, kind, true)) return false;
        for (size_t k = 0; k < m.structures.size(); ++k)
          if (!s.id.empty() && m.structures[k].id == s.id) s.index = (int)k;
        if (s.index < 0) return c.fail(at, "\"id\" " + q(s.id) + " is not the \"id\" of a structure of this map");
        s.kind = db.findStructure(kind);
        if (s.kind < 0) return c.fail(at, "\"kind\" names unknown structure kind " + q(kind));
        const StructureDef& from = db.structures[(size_t)m.structures[(size_t)s.index].type];
        const StructureDef& to = db.structures[(size_t)s.kind];
        if (from.width != to.width || from.height != to.height || from.rings != to.rings)
          return c.fail(at, q(to.id) + " does not have the size of " + q(from.id) + ", the kind the map placed there");
        for (const MapChangeStructure& o : ch.structures)
          if (o.index == s.index) return c.fail(at, "the structure " + q(s.id) + " is in this change twice");
        ch.structures.push_back(s);
      }
    }
    if (ch.cells.empty() && ch.structures.empty()) return c.fail(where, "changes nothing: it needs \"cells\" and / or \"structures\"");
    m.changes.push_back(ch);
  }
  return true;
}

}  // namespace ote
