// Map tools: reading a map as a picture of symbols, painting terrain and heights without touching the raw grid encoding, and
// making a new map. The grid itself ("tiles": one text row per map row, one legend symbol per square) is raw JSON to the
// schema; these tools are what makes it editable for a model.
#include <algorithm>
#include <cctype>

#include "paths.h"
#include "tools.h"

namespace ote_mcp {

namespace {

constexpr int kDefaultCrop = 80;  // squares per side shown when a map is bigger and no rect is given

// map argument: a map id of game.json, a file name below maps/, or the file path itself.
bool mapFile(Txn& txn, const std::string& arg, std::string& rel, std::string& id, ToolResult& err) {
  const Json* game = txn.peek("game.json");
  if (game)
    for (const Json& m : (*game)["maps"].items())
      if (m["id"].isString() && m["id"].asString() == arg && m["file"].isString()) { rel = m["file"].asString(); id = arg; return true; }
  std::vector<std::string> known;
  for (const std::string& f : txn.knownFiles())
    if (startsWith(f, "maps/")) known.push_back(f);
  for (const std::string& cand : {arg, "maps/" + arg, "maps/" + arg + ".json"})
    if (std::find(known.begin(), known.end(), cand) != known.end()) { rel = cand; id = arg; return true; }
  std::string ids;
  if (game) for (const Json& m : (*game)["maps"].items()) ids += (ids.empty() ? "" : ", ") + m["id"].asString();
  err = toolFail("no map \"" + arg + "\"" + (ids.empty() ? "" : " (the game's maps: " + ids + ")"), "get_overview lists the maps; new_map makes one");
  return false;
}

struct Grid {
  int w = 0, h = 0;
};

bool gridOf(const Json& map, Grid& g, std::string& why) {
  g.w = map["grid"]["width"].asInt(0);
  g.h = map["grid"]["height"].asInt(0);
  if (g.w < 1 || g.h < 1) { why = "the map has no grid width / height"; return false; }
  if (!map["tiles"].isArray() || (int)map["tiles"].size() != g.h) { why = "the map's tiles are not " + std::to_string(g.h) + " rows (the file is inconsistent; validate shows the loader's view)"; return false; }
  for (const Json& row : map["tiles"].items())
    if (!row.isString() || (int)row.asString().size() != g.w) { why = "a tiles row is not a text of " + std::to_string(g.w) + " characters"; return false; }
  return true;
}

std::vector<std::string> terrainIds(Txn& txn) {
  std::vector<std::string> ids;
  if (const Json* t = txn.peek("data/terrain.json"))
    for (const Json& r : (*t)["terrain"].items()) if (r["id"].isString()) ids.push_back(r["id"].asString());
  return ids;
}

ToolResult getMap(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel, id;
  if (!mapFile(txn, args["map"].asString(), rel, id, err)) return err;
  LoadedJson l;
  std::string why;
  if (!loadJsonFile(ctx, g, rel, l, why)) return toolFail(why);
  const Json& m = l.value;
  Grid grid;
  Json out = jobj();
  out.set("map", jstr(id));
  out.set("file", jstr(rel));
  out.set("name", m["name"]);
  out.set("grid", m["grid"]);
  out.set("legend", m["legend"]);
  const bool gridOk = gridOf(m, grid, why);
  if (!gridOk) out.set("tilesProblem", jstr(why));
  if (gridOk && args["tiles"].asBool(true)) {
    int x0 = 0, y0 = 0, w = grid.w, h = grid.h;
    if (args["rect"].isObject()) {
      x0 = args["rect"]["x"].asInt(0); y0 = args["rect"]["y"].asInt(0);
      w = args["rect"]["w"].asInt(grid.w); h = args["rect"]["h"].asInt(grid.h);
    } else if (grid.w > kDefaultCrop || grid.h > kDefaultCrop) {
      w = std::min(w, kDefaultCrop); h = std::min(h, kDefaultCrop);
      out.set("cropped", jstr("the map is bigger than " + std::to_string(kDefaultCrop) + " squares a side: this shows the top-left part; pass rect {x,y,w,h} for another part"));
    }
    x0 = std::max(0, std::min(x0, grid.w)); y0 = std::max(0, std::min(y0, grid.h));
    w = std::max(0, std::min(w, grid.w - x0)); h = std::max(0, std::min(h, grid.h - y0));
    Json rows = jarr();
    for (int y = y0; y < y0 + h; ++y) rows.push(jstr(m["tiles"][(size_t)y].asString().substr((size_t)x0, (size_t)w)));
    out.set("tiles", rows);
    out.set("tilesRect", jstr("x=" + std::to_string(x0) + " y=" + std::to_string(y0) + " w=" + std::to_string(w) + " h=" + std::to_string(h)));
    if (m["heights"].isArray()) {
      Json hs = jarr();
      for (int y = y0; y < y0 + h; ++y) {
        std::string line;
        for (int x = x0; x < x0 + w; ++x) line += (x > x0 ? " " : "") + std::to_string(m["heights"][(size_t)y][(size_t)x].asInt(0));
        hs.push(jstr(line));
      }
      out.set("heights", hs);
    }
  }
  Json sides = jarr();
  for (const Json& s : m["sides"].items()) {
    Json o = jobj();
    for (const char* k : {"id", "name", "team", "controller"}) if (s.has(k)) o.set(k, s[k]);
    sides.push(o);
  }
  out.set("sides", sides);
  Json units = jarr();
  for (const Json& u : m["units"].items()) {
    if (units.size() >= 200) break;
    Json o = jobj();
    for (const char* k : {"id", "type", "character", "side", "x", "y", "name", "level", "leader"}) if (u.has(k)) o.set(k, u[k]);
    units.push(o);
  }
  out.set("units", units);
  if (m["units"].size() > 200) out.set("unitsNote", jstr("showing 200 of " + std::to_string(m["units"].size()) + "; use get with path units"));
  Json structs = jarr();
  for (const Json& s : m["structures"].items()) {
    if (structs.size() >= 200) break;
    Json o = jobj();
    for (const char* k : {"id", "type", "x", "y", "side"}) if (s.has(k)) o.set(k, s[k]);
    structs.push(o);
  }
  out.set("structures", structs);
  out.set("events", jnum((double)m["events"].size()));
  if (m.has("tileLayer")) out.set("note", jstr("this map also has a tileLayer (hand-painted picture per square); set_tiles changes terrain only"));
  out.set("next", jstr("set_tiles paints terrain and heights; render_map shows the picture"));
  return toolOk(out);
}

// The symbol of a terrain in the legend, adding one when it has none.
bool symbolFor(Json& map, Txn& txn, const std::string& terrain, std::string& symbol, bool& added) {
  added = false;
  for (const auto& m : map["legend"].members())
    if (m.second.isString() && m.second.asString() == terrain && m.first.size() == 1) { symbol = m.first; return true; }
  std::vector<std::string> candidates;
  if (const Json* t = txn.peek("data/terrain.json"))
    for (const Json& r : (*t)["terrain"].items())
      if (r["id"].isString() && r["id"].asString() == terrain && r["glyph"].isString() && r["glyph"].asString().size() == 1) candidates.push_back(r["glyph"].asString());
  if (!terrain.empty()) { candidates.push_back(terrain.substr(0, 1)); std::string up = terrain.substr(0, 1); up[0] = (char)std::toupper((unsigned char)up[0]); candidates.push_back(up); }
  for (const char* c = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789~=#%&*+-^:;,'"; *c; ++c) candidates.push_back(std::string(1, *c));
  if (!map["legend"].isObject()) map.set("legend", jobj());
  Json* legend = nullptr;
  for (auto& m : map.mutableMembers()) if (m.first == "legend") legend = &m.second;
  for (const std::string& cand : candidates)
    if (!legend->has(cand.c_str()) && cand != "\"" && cand != "\\") {
      legend->set(cand, jstr(terrain));
      symbol = cand;
      added = true;
      return true;
    }
  return false;
}

ToolResult setTiles(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel, id;
  if (!mapFile(txn, args["map"].asString(), rel, id, err)) return err;
  std::string why;
  Json* map = txn.file(rel, why);
  if (!map) return toolFail(why);
  Grid grid;
  if (!gridOf(*map, grid, why)) return toolFail(why, "validate shows the loader's view of the map");
  const std::vector<std::string> terrains = terrainIds(txn);
  std::vector<std::string> rows;
  for (const Json& r : (*map)["tiles"].items()) rows.push_back(r.asString());
  std::vector<std::vector<int>> heights;
  bool heightsTouched = false;
  if ((*map)["heights"].isArray())
    for (int y = 0; y < grid.h; ++y) {
      std::vector<int> line;
      for (int x = 0; x < grid.w; ++x) line.push_back((*map)["heights"][(size_t)y][(size_t)x].asInt(0));
      heights.push_back(line);
    }
  size_t cells = 0;
  Json addedLegend = jobj();
  const Json& paint = args["paint"];
  if (paint.size() == 0) return toolFail("paint is empty: give at least one {x, y, terrain} (w and h make it a rectangle)");
  for (size_t i = 0; i < paint.size(); ++i) {
    const Json& op = paint[i];
    const int x = op["x"].asInt(0), y = op["y"].asInt(0), w = op["w"].asInt(1), h = op["h"].asInt(1);
    const std::string where = "paint[" + std::to_string(i) + "]";
    if (x < 0 || y < 0 || w < 1 || h < 1 || x + w > grid.w || y + h > grid.h)
      return toolFail(where + ": the rectangle x=" + std::to_string(x) + " y=" + std::to_string(y) + " w=" + std::to_string(w) + " h=" + std::to_string(h) + " is outside the map (" + std::to_string(grid.w) + " x " + std::to_string(grid.h) + " squares, x and y start at 0)");
    if (!op.has("terrain") && !op.has("height")) return toolFail(where + ": give terrain and / or height");
    std::string symbol;
    if (op.has("terrain")) {
      const std::string t = op["terrain"].asString();
      if (std::find(terrains.begin(), terrains.end(), t) == terrains.end()) {
        std::string all;
        for (const std::string& s : terrains) all += (all.empty() ? "" : ", ") + s;
        return toolFail(where + ": no terrain \"" + t + "\" (the game's terrain: " + all + ")", "add_entry to data/terrain.json makes a new terrain");
      }
      bool added = false;
      if (!symbolFor(*map, txn, t, symbol, added)) return toolFail("the legend has no free symbol left for \"" + t + "\"");
      if (added) addedLegend.set(symbol, jstr(t));
    }
    if (op.has("height")) {
      const int hv = op["height"].asInt(0);
      if (hv < 0 || hv > 99 || !op["height"].isNumber()) return toolFail(where + ": height must be a whole number from 0 to 99");
      if (heights.empty()) heights.assign((size_t)grid.h, std::vector<int>((size_t)grid.w, 0));
      heightsTouched = true;
      for (int yy = y; yy < y + h; ++yy) for (int xx = x; xx < x + w; ++xx) heights[(size_t)yy][(size_t)xx] = hv;
    }
    for (int yy = y; yy < y + h; ++yy)
      for (int xx = x; xx < x + w; ++xx) {
        if (!symbol.empty()) rows[(size_t)yy][(size_t)xx] = symbol[0];
        ++cells;
      }
  }
  Json newRows = jarr();
  for (const std::string& r : rows) newRows.push(jstr(r));
  map->set("tiles", newRows);
  if (heightsTouched) {
    Json hs = jarr();
    for (const auto& line : heights) {
      Json row = jarr();
      for (int v : line) row.push(jnum(v));
      hs.push(row);
    }
    map->set("heights", hs);
  }
  Json extra = jobj();
  extra.set("map", jstr(id));
  extra.set("squaresPainted", jnum((double)cells));
  if (addedLegend.size() > 0) extra.set("legendAdded", addedLegend);
  return finishWrite(txn, args, extra);
}

ToolResult newMap(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  const std::string id = args["map"].asString();
  if (id.empty() || id.size() > 40) return toolFail("map must be an id of 1 to 40 characters");
  for (char c : id) if (!(std::isalnum((unsigned char)c) || c == '_' || c == '-')) return toolFail("map ids use letters, digits, _ and - only");
  const int w = args["width"].asInt(0), h = args["height"].asInt(0);
  if (w < 1 || h < 1 || w > 256 || h > 256) return toolFail("width and height must be 1 to 256");
  std::string why;
  Json* game = txn.file("game.json", why);
  if (!game) return toolFail(why);
  for (const Json& m : (*game)["maps"].items())
    if (m["id"].isString() && m["id"].asString() == id) return toolFail("the game already has a map \"" + id + "\"", "pick another id, or use set_tiles to change it");
  const std::vector<std::string> terrains = terrainIds(txn);
  if (terrains.empty()) return toolFail("the game has no terrain yet (data/terrain.json): add one first");
  const std::string fill = args["fill"].asString(terrains[0]);
  if (std::find(terrains.begin(), terrains.end(), fill) == terrains.end()) {
    std::string all;
    for (const std::string& s : terrains) all += (all.empty() ? "" : ", ") + s;
    return toolFail("no terrain \"" + fill + "\" (the game's terrain: " + all + ")");
  }
  Json map = jobj();
  map.set("name", jstr(args["name"].asString(id)));
  Json grid = jobj();
  grid.set("type", jstr((*game)["grid"]["type"].asString("square")));
  grid.set("width", jnum(w));
  grid.set("height", jnum(h));
  map.set("grid", grid);
  Json holder = jobj();
  holder.set("legend", jobj());
  std::string symbol;
  bool added = false;
  symbolFor(holder, txn, fill, symbol, added);
  map.set("legend", holder["legend"]);
  Json rows = jarr();
  for (int y = 0; y < h; ++y) rows.push(jstr(std::string((size_t)w, symbol[0])));
  map.set("tiles", rows);
  Json sides = jarr();
  if (args["sides"].isArray() && args["sides"].size() > 0) {
    for (const Json& s : args["sides"].items()) {
      Json o = jobj();
      o.set("id", s["id"]);
      o.set("name", s["name"].isString() ? s["name"] : s["id"]);
      if (s["color"].isString()) o.set("color", s["color"]);
      if (s["team"].isString()) o.set("team", s["team"]);
      o.set("controller", jstr(s["controller"].asString("human")));
      sides.push(o);
    }
  } else {
    Json blue = jobj(), red = jobj();
    blue.set("id", jstr("blue")); blue.set("name", jstr("Blue")); blue.set("color", jstr("#3d85c6")); blue.set("controller", jstr("human"));
    red.set("id", jstr("red")); red.set("name", jstr("Red")); red.set("color", jstr("#cc0000")); red.set("controller", jstr("ai"));
    sides.push(blue);
    sides.push(red);
  }
  map.set("sides", sides);
  map.set("units", jarr());
  const std::string rel = "maps/" + id + ".json";
  if (!txn.create(rel, map, why)) return toolFail(why);
  Json entry = jobj();
  entry.set("id", jstr(id));
  entry.set("name", map["name"]);
  entry.set("file", jstr(rel));
  if (!(*game)["maps"].isArray()) game->set("maps", jarr());
  game->set("maps", [&] { Json l = (*game)["maps"]; l.push(entry); return l; }());
  Json extra = jobj();
  extra.set("map", jstr(id));
  extra.set("file", jstr(rel));
  extra.set("next", jstr("place units with add_entry (file " + rel + ", path units), paint with set_tiles; a map needs units on its sides to be a playable battle"));
  return finishWrite(txn, args, extra);
}

}  // namespace

void registerMapTools(ToolRegistry& r, Context& ctx) {
  Context* c = &ctx;
  {
    Tool t;
    t.name = "get_map";
    t.title = "Read a map";
    t.description = "Shows a battle map in a form you can read: size and grid kind, the legend (which symbol means which terrain), the tile grid as rows of legend symbols (top row first, x counts from the left, y from the top, both starting at 0), heights if the map has them, the sides, the placed units (with x and y), structures and the number of events. Big maps are cropped to 80 x 80 squares unless you pass rect. Use set_tiles to paint and add_entry on the map's units to place units.";
    Props p{{"game", gameProp()},
            {"map", sType("string", "Map id from game.json (see get_overview), or the file such as maps/start.json.")},
            {"tiles", sType("boolean", "false = leave the tile grid out (default true).")},
            {"rect", sObject({{"x", sInt("", 0, 255)}, {"y", sInt("", 0, 255)}, {"w", sInt("", 1, 256)}, {"h", sInt("", 1, 256)}}, {}, false)}};
    t.inputSchema = sObject(p, {"map"});
    t.run = [c](const Json& a) { return getMap(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "set_tiles";
    t.title = "Paint terrain and heights";
    t.description = "Paints a map without you touching the raw tile grid. paint is a list of operations applied in order; each is a rectangle x, y (top-left square, 0-based), w and h (default 1 each, so a single square needs only x and y) with terrain (a terrain id of the game, e.g. forest) and / or height (0 to 99; only matters if the game uses the height rule). The legend is kept in step: a terrain that has no symbol yet gets one added (its glyph from data/terrain.json when free). Squares outside the map are refused with the map's size. The tileLayer (hand-painted pictures) is not touched. The usual write options apply (dryRun shows the diff).";
    Props p{{"game", gameProp()},
            {"map", sType("string", "Map id (or file) as in get_map.")},
            {"paint", sArray(sObject({{"x", sInt("Column of the top-left square.", 0, 255)}, {"y", sInt("Row of the top-left square.", 0, 255)},
                                      {"w", sInt("Width in squares (default 1).", 1, 256)}, {"h", sInt("Height in squares (default 1).", 1, 256)},
                                      {"terrain", sType("string", "Terrain id to put on these squares.")}, {"height", sInt("Height to set on these squares.", 0, 99)}}, {"x", "y"}),
                             "Rectangles to paint, in order.")}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"map", "paint"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return setTiles(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "new_map";
    t.title = "Create a map";
    t.description = "Creates a new battle map file (maps/<id>.json) filled with one terrain, with the given sides (default: blue controlled by a person, red by the computer), and registers it in game.json's list of maps, in one atomic change. The grid kind (square or hex) follows the game. The map has no units yet: a battle needs units on its sides, so add them with add_entry (file maps/<id>.json, path units, e.g. fields {\"type\":\"soldier\",\"side\":\"blue\",\"x\":1,\"y\":1}), then paint with set_tiles and check with validate.";
    Props p{{"game", gameProp()},
            {"map", sType("string", "Id of the new map: letters, digits, _ and -, up to 40 characters. Also the file name.")},
            {"name", sType("string", "Display name (default: the id).")},
            {"width", sInt("Columns.", 1, 256)},
            {"height", sInt("Rows.", 1, 256)},
            {"fill", sType("string", "Terrain id the map starts as (default: the game's first terrain).")},
            {"sides", sArray(sObject({{"id", sType("string", "Side id, e.g. blue.")}, {"name", sType("string", "")}, {"color", sType("string", "#rrggbb")},
                                      {"team", sType("string", "")}, {"controller", sEnum({"human", "ai"}, "Who plays the side.")}}, {"id"}),
                             "The sides (default: blue = human, red = ai).")}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"map", "width", "height"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return newMap(*c, a); };
    r.add(t);
  }
}

}  // namespace ote_mcp
