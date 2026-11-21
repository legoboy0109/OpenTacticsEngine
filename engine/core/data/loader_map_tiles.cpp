// A map's "tileLayer" (docs/GRAPHICS.md "Tile layer (hand-painted maps)", docs/GAME_FORMAT.md):
//   "tileLayer": { "tileset": "world", "cells": [ "0000000100020003....", "..." ] }
// One string per map row, four hex digits per square (column x is characters 4x..4x+3), "...." = no tile (the terrain's own look shows).
// A row may also be a list of numbers (-1 or null = no tile); the editor writes the strings. Presentation only: the terrain underneath
// still decides everything the rules look at.
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {
std::string q(const std::string& s) { return "\"" + s + "\""; }

int hexDigit(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}
}  // namespace

bool readMapTileLayer(const LoadCtx& c, const Database& db, const Json& root, MapDef& m) {
  m.tileset = -1;
  m.tileCells.clear();
  if (!root.has("tileLayer")) return true;
  const Json& j = root["tileLayer"];
  const std::string where = "tileLayer";
  if (!j.isObject()) return c.fail(where, "must be an object: { \"tileset\": \"<tileset id>\", \"cells\": [ one row per map row ] }");
  for (const Json::Member& k : j.members())
    if (k.first != "tileset" && k.first != "cells") return c.fail(where, "unknown key " + q(k.first) + " (use tileset and cells)" + didYouMean(k.first, {"tileset", "cells"}));
  if (db.gridType == GridType::Hex)
    return c.fail(where, "tile layers are not supported on hexagon games (a hexagon map is drawn from its terrain's looks)");
  std::string id;
  if (!c.str(j, "tileset", where, id, true)) return false;
  const int ts = db.graphics.findTileset(id);
  if (ts < 0)
    return c.fail(where, "unknown tileset " + q(id) + (db.graphics.tilesets.empty() ? " (data/graphics.json has no \"tilesets\")" : ""));
  const TilesetDef& def = db.graphics.tilesets[(size_t)ts];
  const Json* rows;
  if (!c.array(j, "cells", where, rows, true)) return false;
  if ((int)rows->size() != m.height)
    return c.fail(where, "\"cells\" has " + std::to_string(rows->size()) + " rows but the grid height is " + std::to_string(m.height));
  std::vector<int> out((size_t)m.width * (size_t)m.height, kNoTile);
  for (int y = 0; y < m.height; ++y) {
    const Json& row = (*rows)[(size_t)y];
    const std::string at = "cells row " + std::to_string(y);
    if (row.isString()) {
      const std::string& s = row.asString();
      if ((int)s.size() != m.width * 4)
        return c.fail(where, at + " has " + std::to_string(s.size()) + " characters but " + std::to_string(m.width * 4) + " are needed (" +
                                 std::to_string(m.width) + " squares of four hex digits, or \"....\" for no tile)");
      for (int x = 0; x < m.width; ++x) {
        const std::string tok = s.substr((size_t)x * 4, 4);
        if (tok == "....") continue;
        int v = 0;
        for (char ch : tok) {
          const int d = hexDigit(ch);
          if (d < 0) return c.fail(where, at + ", column " + std::to_string(x) + ": " + q(tok) + " is not four hex digits or \"....\"");
          v = v * 16 + d;
        }
        if (v >= def.count())
          return c.fail(where, at + ", column " + std::to_string(x) + ": tile " + std::to_string(v) + " is outside tileset " + q(id) + " (" +
                                   std::to_string(def.count()) + " tiles)");
        out[(size_t)y * (size_t)m.width + (size_t)x] = v;
      }
    } else if (row.isArray()) {
      if ((int)row.size() != m.width)
        return c.fail(where, at + " has " + std::to_string(row.size()) + " squares but the grid width is " + std::to_string(m.width));
      for (int x = 0; x < m.width; ++x) {
        const Json& e = row[(size_t)x];
        if (e.isNull()) continue;
        if (!LoadCtx::isWhole(e)) return c.fail(where, at + ", column " + std::to_string(x) + ": a tile must be a whole number (-1 or null = no tile)");
        const int v = e.asInt();
        if (v == -1) continue;
        if (v < 0 || v >= def.count())
          return c.fail(where, at + ", column " + std::to_string(x) + ": tile " + std::to_string(v) + " is outside tileset " + q(id) + " (" +
                                   std::to_string(def.count()) + " tiles)");
        out[(size_t)y * (size_t)m.width + (size_t)x] = v;
      }
    } else {
      return c.fail(where, at + " must be a string of four-digit hex tiles (or a list of numbers)");
    }
  }
  m.tileset = ts;
  m.tileCells = out;
  return true;
}

}  // namespace ote
