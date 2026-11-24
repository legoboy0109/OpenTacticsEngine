// Resizing a map (editor/core/map_grid.h): the grid pieces are rebuilt, everything placed on the map moves with the anchor.
#include <string>

#include "core/grid/grid.h"
#include "map_grid.h"

namespace ote_editor {

using ote::Json;

void resizeOffset(const MapGrid& g, const ResizeSpec& spec, int& dx, int& dy) {
  dx = spec.anchorX == 0 ? 0 : (spec.anchorX == 1 ? (spec.width - g.width) / 2 : spec.width - g.width);
  dy = spec.anchorY == 0 ? 0 : (spec.anchorY == 1 ? (spec.height - g.height) / 2 : spec.height - g.height);
  // Odd rows (pointy) / odd columns (flat) are drawn half a step over; moving by an odd number would swap which are which.
  if (g.hex) {
    if (g.flatTop) dx -= dx % 2;
    else dy -= dy % 2;
  }
}

namespace {

struct Shifter {
  const ote::Grid& grid;  // the NEW grid
  int dx, dy;
  std::vector<LostThing>& lost;

  bool fits(int x, int y, const Footprint& fp) const {
    if (grid.isHex()) {
      const ote::Cell mid = grid.at(x, y);
      if (mid == ote::kNoCell) return false;
      if (fp.rings < 1) return true;
      std::vector<ote::Cell> cells;
      return grid.ringCells(mid, fp.rings, cells);
    }
    return x >= 0 && y >= 0 && x + fp.w <= grid.width() && y + fp.h <= grid.height();
  }

  // Moves every entry of `list` (objects with x, y); entries that no longer fit are dropped and recorded.
  Json shift(const Json& list, const std::string& kind, const std::map<std::string, Footprint>* footprints) const {
    Json out = Json::makeArray();
    for (const Json& item : list.items()) {
      if (!item.isObject() || !item["x"].isNumber() || !item["y"].isNumber()) {
        out.push(item);
        continue;
      }
      const int ox = item["x"].asInt(), oy = item["y"].asInt();
      Footprint fp;
      if (footprints) {
        const auto it = footprints->find(item["type"].asString());
        if (it != footprints->end()) fp = it->second;
      }
      if (!fits(ox + dx, oy + dy, fp)) {
        std::string label = item["type"].asString();  // a unit may be placed as a story character or by its job instead
        if (label.empty()) label = item["character"].asString();
        if (label.empty()) label = item["job"].asString();
        if (item["id"].isString()) label += " (" + item["id"].asString() + ")";
        lost.push_back({kind, label, ox, oy});
        continue;
      }
      Json moved = item;
      moved.set("x", Json::makeNumber(ox + dx));
      moved.set("y", Json::makeNumber(oy + dy));
      out.push(moved);
    }
    return out;
  }
};

}  // namespace

bool resizeMap(const Json& map, const MapGrid& g, const ResizeSpec& spec, const std::map<std::string, Footprint>& footprints,
               ResizeResult& out, std::string& err) {
  out = ResizeResult();
  if (spec.width < 1 || spec.height < 1 || spec.width > 256 || spec.height > 256) {
    err = "width and height must be between 1 and 256";
    return false;
  }
  if (g.terrainOf(spec.fill).empty()) {
    err = "the fill symbol is not in the legend";
    return false;
  }
  if (spec.width == g.width && spec.height == g.height) return true;
  resizeOffset(g, spec, out.dx, out.dy);

  MapGrid ng = g;
  ng.width = spec.width;
  ng.height = spec.height;
  ng.rows.assign((size_t)ng.height, std::string((size_t)ng.width, spec.fill));
  if (g.hasHeights) ng.heights.assign((size_t)ng.width * (size_t)ng.height, 0);
  if (g.hasLayer) ng.layer.assign((size_t)ng.width * (size_t)ng.height, -1);
  for (int y = 0; y < ng.height; ++y)
    for (int x = 0; x < ng.width; ++x) {
      const int ox = x - out.dx, oy = y - out.dy;
      if (!g.inside(ox, oy)) continue;
      ng.rows[(size_t)y][(size_t)x] = g.symbolAt(ox, oy);
      if (g.hasHeights) ng.heights[(size_t)y * (size_t)ng.width + (size_t)x] = g.heightAt(ox, oy);
      if (g.hasLayer) ng.layer[(size_t)y * (size_t)ng.width + (size_t)x] = g.layerAt(ox, oy);
    }

  Json copy = map;
  Json grid = map["grid"];
  grid.set("width", Json::makeNumber(ng.width));
  grid.set("height", Json::makeNumber(ng.height));
  copy.set("grid", grid);
  copy.set("tiles", tilesJson(ng));
  if (g.hasHeights) copy.set("heights", heightsJson(ng));
  if (g.hasLayer) {
    Json layer = map["tileLayer"];
    layer.set("cells", layerCellsJson(ng));
    copy.set("tileLayer", layer);
  }

  const ote::Grid newGrid(g.hex ? ote::GridType::Hex : ote::GridType::Square, ng.width, ng.height,
                          g.flatTop ? ote::HexLayout::Flat : ote::HexLayout::Pointy);
  const Shifter sh{newGrid, out.dx, out.dy, out.lost};
  if (map["units"].isArray()) copy.set("units", sh.shift(map["units"], "unit", nullptr));
  if (map["structures"].isArray()) copy.set("structures", sh.shift(map["structures"], "structure", &footprints));
  if (map["prep"].isObject() && map["prep"]["slots"].isArray()) {
    Json prep = map["prep"];
    prep.set("slots", sh.shift(map["prep"]["slots"], "preparation slot", nullptr));
    copy.set("prep", prep);
  }
  if (map["changes"].isObject()) {
    Json changes = map["changes"];
    for (const Json::Member& m : map["changes"].members()) {
      if (!m.second.isObject() || !m.second["cells"].isArray()) continue;
      Json one = m.second;
      one.set("cells", sh.shift(m.second["cells"], "map change square", nullptr));
      changes.set(m.first, one);
    }
    copy.set("changes", changes);
  }
  // Backdrop pieces (docs/VIEWS.md "Backdrop") name their square as [x, y]; they move with the squares, and those off the new
  // map are dropped (the loader refuses a piece outside the map). The picture and its origin are left as they are: the origin
  // is in the style's own pixels, which this file does not know; the maker re-aligns it.
  if (map["backdrop"].isObject() && map["backdrop"]["pieces"].isArray()) {
    Json backdrop = map["backdrop"];
    Json pieces = Json::makeArray();
    for (const Json& p : map["backdrop"]["pieces"].items()) {
      const Json& sq = p["square"];
      if (!sq.isArray() || sq.size() != 2 || !sq[0].isNumber() || !sq[1].isNumber()) {
        pieces.push(p);
        continue;
      }
      const int ox = sq[0].asInt(), oy = sq[1].asInt();
      if (!sh.fits(ox + out.dx, oy + out.dy, Footprint())) {
        out.lost.push_back({"backdrop piece", "", ox, oy});
        continue;
      }
      Json moved = p, s = Json::makeArray();
      s.push(Json::makeNumber(ox + out.dx));
      s.push(Json::makeNumber(oy + out.dy));
      moved.set("square", s);
      pieces.push(moved);
    }
    // An empty list is refused: no pieces left = the over picture drawn whole, so the key goes.
    if (pieces.size() == 0) backdrop.erase("pieces");
    else backdrop.set("pieces", pieces);
    copy.set("backdrop", backdrop);
  }

  for (const Json::Member& m : copy.members())
    if (!map.has(m.first.c_str()) || map[m.first].dump(0) != m.second.dump(0)) out.set.emplace_back(m.first, m.second);
  err.clear();
  return true;
}

}  // namespace ote_editor
