#include "map_grid.h"

#include <algorithm>
#include <cctype>
#include <cmath>

#include "core/grid/grid.h"

namespace ote_editor {

using ote::Json;

std::string MapGrid::terrainOf(char symbol) const {
  for (const auto& e : legend)
    if (e.first == symbol) return e.second;
  return std::string();
}

char MapGrid::symbolFor(const std::string& terrain) const {
  for (const auto& e : legend)
    if (e.second == terrain) return e.first;
  return 0;
}

namespace {

bool wholeNumber(const Json& j) { return j.isNumber() && j.asNumber() == std::floor(j.asNumber()); }

int hexDigit(char ch) {
  if (ch >= '0' && ch <= '9') return ch - '0';
  if (ch >= 'a' && ch <= 'f') return ch - 'a' + 10;
  if (ch >= 'A' && ch <= 'F') return ch - 'A' + 10;
  return -1;
}

bool readLayer(const Json& map, MapGrid& g, std::string& err) {
  if (!map.has("tileLayer")) return true;
  const Json& t = map["tileLayer"];
  if (!t.isObject() || !t["tileset"].isString() || !t["cells"].isArray() || (int)t["cells"].size() != g.height) {
    err = "tileLayer is not { \"tileset\": id, \"cells\": one row per map row }";
    return false;
  }
  g.hasLayer = true;
  g.layerTileset = t["tileset"].asString();
  g.layer.assign((size_t)g.width * (size_t)g.height, -1);
  for (int y = 0; y < g.height; ++y) {
    const Json& row = t["cells"][(size_t)y];
    if (row.isString()) {
      const std::string& s = row.asString();
      if ((int)s.size() != g.width * 4) {
        err = "tileLayer row " + std::to_string(y) + " has the wrong length";
        return false;
      }
      for (int x = 0; x < g.width; ++x) {
        const std::string tok = s.substr((size_t)x * 4, 4);
        if (tok == "....") continue;
        int v = 0;
        for (char ch : tok) {
          const int d = hexDigit(ch);
          if (d < 0) {
            err = "tileLayer row " + std::to_string(y) + ": \"" + tok + "\" is not four hex digits";
            return false;
          }
          v = v * 16 + d;
        }
        g.layer[(size_t)y * (size_t)g.width + (size_t)x] = v;
      }
    } else if (row.isArray() && (int)row.size() == g.width) {
      for (int x = 0; x < g.width; ++x) {
        const Json& e = row[(size_t)x];
        if (e.isNull()) continue;
        if (!wholeNumber(e)) {
          err = "tileLayer row " + std::to_string(y) + " has a tile that is not a whole number";
          return false;
        }
        g.layer[(size_t)y * (size_t)g.width + (size_t)x] = e.asInt() < 0 ? -1 : e.asInt();
      }
    } else {
      err = "tileLayer row " + std::to_string(y) + " is not a string of tiles";
      return false;
    }
  }
  return true;
}

}  // namespace

bool readMapGrid(const Json& map, bool flatTop, MapGrid& out, std::string& err) {
  MapGrid g;
  g.flatTop = flatTop;
  const Json& grid = map["grid"];
  if (!grid.isObject() || !wholeNumber(grid["width"]) || !wholeNumber(grid["height"])) {
    err = "the map has no grid { type, width, height }";
    return false;
  }
  g.width = grid["width"].asInt();
  g.height = grid["height"].asInt();
  if (g.width < 1 || g.height < 1 || g.width > 256 || g.height > 256) {
    err = "grid width and height must be between 1 and 256";
    return false;
  }
  g.hex = grid["type"].asString() == "hex";
  if (!map["legend"].isObject()) {
    err = "the map has no legend";
    return false;
  }
  for (const Json::Member& m : map["legend"].members()) {
    if (m.first.size() != 1 || !m.second.isString()) {
      err = "the legend must map single characters to terrain ids";
      return false;
    }
    g.legend.emplace_back(m.first[0], m.second.asString());
  }
  if (!map["tiles"].isArray() || (int)map["tiles"].size() != g.height) {
    err = "tiles must have one row per grid row";
    return false;
  }
  for (int y = 0; y < g.height; ++y) {
    const Json& r = map["tiles"][(size_t)y];
    if (!r.isString() || (int)r.asString().size() != g.width) {
      err = "tiles row " + std::to_string(y) + " is not " + std::to_string(g.width) + " characters long";
      return false;
    }
    g.rows.push_back(r.asString());
  }
  if (map.has("heights")) {
    const Json& h = map["heights"];
    if (!h.isArray() || (int)h.size() != g.height) {
      err = "heights must have one row per grid row";
      return false;
    }
    g.hasHeights = true;
    g.heights.assign((size_t)g.width * (size_t)g.height, 0);
    for (int y = 0; y < g.height; ++y) {
      const Json& row = h[(size_t)y];
      if (!row.isArray() || (int)row.size() != g.width) {
        err = "heights row " + std::to_string(y) + " must list " + std::to_string(g.width) + " numbers";
        return false;
      }
      for (int x = 0; x < g.width; ++x) {
        if (!wholeNumber(row[(size_t)x])) {
          err = "heights row " + std::to_string(y) + " has a value that is not a whole number";
          return false;
        }
        g.heights[(size_t)y * (size_t)g.width + (size_t)x] = row[(size_t)x].asInt();
      }
    }
  }
  if (!readLayer(map, g, err)) return false;
  out = std::move(g);
  err.clear();
  return true;
}

Json tilesRowJson(const MapGrid& g, int y) { return Json::makeString(g.rows[(size_t)y]); }

Json tilesJson(const MapGrid& g) {
  Json a = Json::makeArray();
  for (int y = 0; y < g.height; ++y) a.push(tilesRowJson(g, y));
  return a;
}

Json heightsRowJson(const MapGrid& g, int y) {
  Json row = Json::makeArray();
  for (int x = 0; x < g.width; ++x) row.push(Json::makeNumber(g.heightAt(x, y)));
  return row;
}

Json heightsJson(const MapGrid& g) {
  Json a = Json::makeArray();
  for (int y = 0; y < g.height; ++y) a.push(heightsRowJson(g, y));
  return a;
}

Json layerRowJson(const MapGrid& g, int y) {
  static const char* digits = "0123456789abcdef";
  std::string s;
  for (int x = 0; x < g.width; ++x) {
    const int v = g.layerAt(x, y);
    if (v < 0) {
      s += "....";
    } else {
      for (int shift = 12; shift >= 0; shift -= 4) s += digits[(v >> shift) & 15];
    }
  }
  return Json::makeString(s);
}

Json layerCellsJson(const MapGrid& g) {
  Json a = Json::makeArray();
  for (int y = 0; y < g.height; ++y) a.push(layerRowJson(g, y));
  return a;
}

namespace {
enum class Style { Lower, Upper, Digit, Symbol };

Style styleOf(char c) {
  if (std::islower((unsigned char)c)) return Style::Lower;
  if (std::isupper((unsigned char)c)) return Style::Upper;
  if (std::isdigit((unsigned char)c)) return Style::Digit;
  return Style::Symbol;
}

bool usable(char c) { return c > ' ' && c < 127 && c != '"' && c != '\\'; }
}  // namespace

char legendSymbolFor(const MapGrid& g, const std::string& terrain, char glyphHint, bool& isNew) {
  isNew = false;
  if (const char have = g.symbolFor(terrain)) return have;
  isNew = true;
  auto isFree = [&](char c) {
    return usable(c) && std::none_of(g.legend.begin(), g.legend.end(), [c](const std::pair<char, std::string>& e) { return e.first == c; });
  };
  // The map's style: the kind of symbol most legend entries use. Punctuation does not vote (nearly every legend has "." or "~"
  // as a filler); it is the style only when the legend has no letter or digit at all. Ties: lower case, upper case, digits.
  int count[4] = {0, 0, 0, 0};
  for (const auto& e : g.legend) ++count[(int)styleOf(e.first)];
  Style style = Style::Symbol;
  int most = 0;
  for (Style st : {Style::Lower, Style::Upper, Style::Digit})
    if (count[(int)st] > most) {
      most = count[(int)st];
      style = st;
    }

  std::string order;
  if (glyphHint) order += glyphHint;
  for (char c : terrain)  // letters of the id only in a map whose legend uses letters; digits / punctuation maps stay in their kind
    if ((style == Style::Lower || style == Style::Upper) && std::isalpha((unsigned char)c)) {
      order += style == Style::Upper ? (char)std::toupper((unsigned char)c) : (char)std::tolower((unsigned char)c);
    }
  const std::string classes[4] = {"abcdefghijklmnopqrstuvwxyz", "ABCDEFGHIJKLMNOPQRSTUVWXYZ", "0123456789", "~=#%&*+-^:;,'./!?@$()[]{}<>|_`"};
  if (style == Style::Digit) {  // continue after the highest digit in use: 1 2 3 -> 4
    int top = -1;
    for (const auto& e : g.legend)
      if (std::isdigit((unsigned char)e.first)) top = std::max(top, e.first - '0');
    for (int i = 1; i <= 10; ++i) order += (char)('0' + (top + i) % 10);
  }
  order += classes[(int)style];
  for (int i = 0; i < 4; ++i) order += classes[i];
  for (char c : order)
    if (isFree(c)) return c;
  return 0;
}

std::vector<int> floodRegion(const MapGrid& g, int x, int y) {
  std::vector<int> out;
  if (!g.inside(x, y)) return out;
  const ote::Grid grid(g.hex ? ote::GridType::Hex : ote::GridType::Square, g.width, g.height,
                       g.flatTop ? ote::HexLayout::Flat : ote::HexLayout::Pointy);
  const char want = g.symbolAt(x, y);
  std::vector<char> seen((size_t)g.width * (size_t)g.height, 0);
  std::vector<int> stack{grid.at(x, y)};
  seen[(size_t)stack[0]] = 1;
  while (!stack.empty()) {
    const int c = stack.back();
    stack.pop_back();
    out.push_back(c);
    ote::Cell next[ote::Grid::kMaxNeighbors];
    const int n = grid.neighbors(c, next);
    for (int i = 0; i < n; ++i) {
      const int nc = next[i];
      if (seen[(size_t)nc] || g.symbolAt(grid.x(nc), grid.y(nc)) != want) continue;
      seen[(size_t)nc] = 1;
      stack.push_back(nc);
    }
  }
  return out;
}

std::vector<int> brushCells(const MapGrid& g, int x, int y, int size) {
  std::vector<int> out;
  if (!g.inside(x, y)) return out;
  const int r = std::max(0, (size - 1) / 2);
  if (!g.hex) {
    for (int yy = std::max(0, y - r); yy <= std::min(g.height - 1, y + r); ++yy)
      for (int xx = std::max(0, x - r); xx <= std::min(g.width - 1, x + r); ++xx) out.push_back(yy * g.width + xx);
    return out;
  }
  // Hexagons: breadth-first over the engine's own neighbour rule, so odd rows / columns need no special case here.
  const ote::Grid grid(ote::GridType::Hex, g.width, g.height, g.flatTop ? ote::HexLayout::Flat : ote::HexLayout::Pointy);
  std::vector<int> dist((size_t)g.width * (size_t)g.height, -1);
  const int start = grid.at(x, y);
  dist[(size_t)start] = 0;
  std::vector<int> queue{start};
  for (size_t i = 0; i < queue.size(); ++i) {
    const int c = queue[i];
    out.push_back(c);
    if (dist[(size_t)c] >= r) continue;
    ote::Cell next[ote::Grid::kMaxNeighbors];
    const int n = grid.neighbors(c, next);
    for (int k = 0; k < n; ++k)
      if (dist[(size_t)next[k]] < 0) {
        dist[(size_t)next[k]] = dist[(size_t)c] + 1;
        queue.push_back(next[k]);
      }
  }
  return out;
}

}  // namespace ote_editor
