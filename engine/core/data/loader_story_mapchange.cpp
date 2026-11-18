// Event loader, changing the map (docs/EVENTS.md "Changing the map"): the commands setTerrain, applyChange, revertChange, setFog and
// setVision. Self-contained so the shared story loader files change by a line each.
#include "core/data/loader_events.h"

namespace ote {

namespace {
std::string q(const std::string& s) { return "\"" + s + "\""; }
}  // namespace

bool StoryLoader::mapChangeCommand(const Json& e, const std::string& kind, const std::string& at, Command& out) {
  using K = Command::Kind;
  const Json& v = e[kind.c_str()];
  if (kind == "setTerrain") {
    out.kind = K::SetTerrain;
    if (!onlyKeys(e, {"setTerrain", "area", "cells", "tile", "keepTile"}, at)) return false;
    // docs/GRAPHICS.md "Tile layer": the squares' new tile (-1 / null = none), or "keepTile": true to leave their tiles alone.
    out.value = kTileUnset;
    out.on = false;
    if (e.has("keepTile")) {
      if (!e["keepTile"].isBool()) return fail(at, "\"keepTile\" must be true or false");
      out.on = e["keepTile"].asBool();
    }
    if (e.has("tile")) {
      if (map_.tileset < 0) return fail(at, "\"tile\" needs a tile layer: the map has no \"tileLayer\"");
      const int count = db_.graphics.tilesets[(size_t)map_.tileset].count();
      const Json& t = e["tile"];
      if (t.isNull()) out.value = -1;
      else if (!LoadCtx::isWhole(t) || t.asInt() < -1 || t.asInt() >= count)
        return fail(at, "\"tile\" must be -1 (no tile) or a tile number from 0 to " + std::to_string(count - 1) + " of the map's tileset");
      else out.value = t.asInt();
    }
    if (!v.isString()) return fail(at, "\"setTerrain\" must be the id of a terrain");
    out.index = db_.findTerrain(v.asString());
    if (out.index < 0) return fail(at, "\"setTerrain\" names unknown terrain " + q(v.asString()));
    if (e.has("area") == e.has("cells")) return fail(at, "give the squares as \"area\": { \"x\", \"y\", \"w\", \"h\" } or as \"cells\": [[x, y], ...] (one of them)");
    if (e.has("area")) {
      Rect r;
      if (!rect(e["area"], at + ": \"area\"", r)) return false;
      out.rects.push_back(r);
      return true;
    }
    const Json& list = e["cells"];
    if (!list.isArray() || list.size() == 0 || (int)list.size() > kMaxChangeCells)
      return fail(at, "\"cells\" must be a list of squares [[x, y], ...] (1 to " + std::to_string(kMaxChangeCells) + ")");
    for (size_t i = 0; i < list.size(); ++i) {
      const Json& p = list[i];
      if (!p.isArray() || p.size() != 2 || !LoadCtx::isWhole(p[0]) || !LoadCtx::isWhole(p[1]))
        return fail(at, "\"cells\" entry " + std::to_string(i + 1) + " must be a square [x, y]");
      Rect r;
      r.x = p[0].asInt();
      r.y = p[1].asInt();
      r.w = r.h = 1;
      if (r.x < 0 || r.y < 0 || r.x >= map_.width || r.y >= map_.height)
        return fail(at, "the square " + std::to_string(r.x) + "," + std::to_string(r.y) + " is outside the map");
      out.rects.push_back(r);
    }
    return true;
  }
  if (kind == "applyChange" || kind == "revertChange") {
    out.kind = kind == "applyChange" ? K::ApplyChange : K::RevertChange;
    if (!onlyKeys(e, {kind.c_str()}, at)) return false;
    if (!v.isString()) return fail(at, q(kind) + " must be the id of one of the map's \"changes\"");
    out.index = -1;
    for (size_t i = 0; i < map_.changes.size(); ++i)
      if (map_.changes[i].id == v.asString()) out.index = (int)i;
    if (out.index < 0)
      return fail(at, q(kind) + " names " + q(v.asString()) + ", which is not one of this map's \"changes\"" +
                          (map_.changes.empty() ? " (the map has none)" : ""));
    return true;
  }
  if (kind == "setFog") {
    out.kind = K::SetFog;
    if (!onlyKeys(e, {"setFog"}, at)) return false;
    if (!v.isBool()) return fail(at, "write { \"setFog\": true } or { \"setFog\": false }");
    out.on = v.asBool();
    return true;
  }
  // setVision
  out.kind = K::SetVision;
  if (!onlyKeys(e, {"setVision"}, at)) return false;
  if (!LoadCtx::isWhole(v) || v.asInt() < -kMaxFogVision || v.asInt() > kMaxFogVision)
    return fail(at, "\"setVision\" must be a whole number from " + std::to_string(-kMaxFogVision) + " to " + std::to_string(kMaxFogVision) +
                        " (added to every unit's vision; 0 = as the unit types say)");
  out.value = v.asInt();
  return true;
}

}  // namespace ote
