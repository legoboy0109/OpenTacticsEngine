// Read-only tools: finding games, getting oriented, looking at files, asking what a place accepts, searching, checking.
#include <algorithm>
#include <cctype>

#include "paths.h"
#include "revision.h"
#include "tools.h"

namespace ote_mcp {

using ote_editor::SchemaNode;
using K = SchemaNode::Kind;

namespace {

constexpr size_t kBudgetChars = 30000;  // one result never carries more than about this much JSON text

std::string jsonTypeName(const Json& v) {
  if (v.isObject()) return "object";
  if (v.isArray()) return "array";
  if (v.isString()) return "string";
  if (v.isNumber()) return "number";
  if (v.isBool()) return "boolean";
  return "null";
}

CheckEnv envFor(Txn& t, const Json* doc) {
  CheckEnv e;
  e.doc = doc;
  e.docs = t.docLookup();
  return e;
}

ToolResult listGames(Context& ctx, const Json&) {
  Json arr = jarr();
  for (const Game& g : ctx.listGames()) {
    Json o = jobj();
    o.set("id", jstr(g.id));
    Json game;
    std::string err;
    if (ote::loadJsonFile((g.dir / "game.json").generic_string(), game, err)) {
      o.set("name", jstr(game["name"].asString(g.id)));
      if (game["description"].isString()) o.set("description", game["description"]);
      o.set("grid", jstr(game["grid"]["type"].asString("square")));
      o.set("maps", jnum((double)game["maps"].size()));
    } else {
      o.set("problem", jstr("game.json cannot be read: " + err));
    }
    o.set("folder", jstr(g.dir.generic_string()));
    arr.push(o);
  }
  Json out = jobj();
  out.set("games", arr);
  if (arr.size() == 0) out.set("next", jstr("no games yet: call list_templates, then create_game"));
  return toolOk(out);
}

ToolResult getOverview(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn t(ctx, g);
  std::string why;
  const Json* game = t.peek("game.json");
  if (!game) return toolFail("game.json cannot be read", "validate shows what is wrong");
  Json o = jobj();
  o.set("game", jstr(g.id));
  o.set("name", game->has("name") ? (*game)["name"] : jstr(g.id));
  if ((*game)["description"].isString()) o.set("description", (*game)["description"]);
  Json grid = jobj();
  grid.set("type", jstr((*game)["grid"]["type"].asString("square")));
  if ((*game)["grid"]["layout"].isString()) grid.set("layout", (*game)["grid"]["layout"]);
  o.set("grid", grid);
  if ((*game)["view"].isObject()) o.set("view", (*game)["view"]);

  Json maps = jarr();
  for (const Json& m : (*game)["maps"].items()) {
    Json mo = jobj();
    mo.set("id", m["id"]);
    mo.set("name", m["name"]);
    mo.set("file", m["file"]);
    maps.push(mo);
  }
  o.set("maps", maps);

  // Counts of the entries of every data file that is a table.
  Json counts = jobj();
  for (const std::string& rel : t.knownFiles()) {
    if (!startsWith(rel, "data/")) continue;
    const Json* d = t.peek(rel);
    if (!d) continue;
    const Json* rows = ote_editor::tableArray(*d);
    if (rows) counts.set(baseNameOf(rel).substr(0, baseNameOf(rel).size() - 5), jnum((double)rows->size()));
    else if (d->isObject()) counts.set(baseNameOf(rel).substr(0, baseNameOf(rel).size() - 5), jstr("present"));
  }
  o.set("data", counts);
  Json ids = jobj();
  for (const char* k : {"stats", "resources", "moveTypes", "damageTypes", "roles"}) {
    const Json& v = (*game)[k];
    if (v.isArray()) {
      Json l = jarr();
      for (const Json& it : v.items()) l.push(it.isString() ? it : it["id"]);
      ids.set(k, l);
    }
  }
  o.set("ids", ids);

  // Which optional systems of "rules" are switched on, from the schema's list of blocks.
  Json systems = jobj();
  const ote_editor::FileSchema* fs = schemaFor(ctx, "game.json");
  const SchemaNode* rules = fs && fs->root ? fs->root->field("rules") : nullptr;
  if (rules) {
    const SchemaNode& rn = rules->resolved();
    Json on = jarr(), off = jarr();
    for (const auto& f : rn.fields) {
      const SchemaNode& fn = f.second->resolved();
      if (!fn.block && fn.kind != K::Bool && fn.kind != K::Enum) continue;
      ((*game)["rules"].has(f.first.c_str()) ? on : off).push(jstr(f.first));
    }
    systems.set("on", on);
    systems.set("off", off);
    systems.set("how", jstr("enable_block / disable_block switch a block; describe rules shows what each does"));
  }
  o.set("rules", systems);

  Json maxCounts = jobj();
  size_t units = 0, structures = 0, events = 0;
  for (const Json& m : (*game)["maps"].items()) {
    const Json* md = t.peek(m["file"].asString());
    if (!md) continue;
    units += (*md)["units"].size();
    structures += (*md)["structures"].size();
    events += (*md)["events"].size();
  }
  maxCounts.set("unitsPlacedOnMaps", jnum((double)units));
  maxCounts.set("structuresPlacedOnMaps", jnum((double)structures));
  maxCounts.set("events", jnum((double)events));
  o.set("onMaps", maxCounts);

  const std::vector<ote_editor::Problem> problems = ctx.problemsOf(g);
  o.set("problems", jnum((double)problems.size()));
  if (!problems.empty()) o.set("firstProblems", problemsJson(problems, 5));
  o.set("next", jstr(problems.empty() ? "describe (no arguments) lists the editable files; get reads a file" : "validate lists every problem"));
  return toolOk(o);
}

ToolResult listFiles(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  const std::string dir = args["dir"].asString();
  const size_t limit = (size_t)args["limit"].asInt(200), offset = (size_t)args["offset"].asInt(0);
  Json arr = jarr();
  size_t total = 0, shown = 0;
  for (const auto& f : ote_editor::listProjectFiles(g.dir)) {
    if (!dir.empty() && !startsWith(f.first, dir.back() == '/' ? dir : dir + "/")) continue;
    if (total++ < offset || shown >= limit) continue;
    ++shown;
    Json o = jobj();
    o.set("file", jstr(f.first));
    std::error_code ec;
    o.set("bytes", jnum((double)fs::file_size(f.second, ec)));
    const std::string ext = f.first.substr(f.first.find_last_of('.') == std::string::npos ? f.first.size() : f.first.find_last_of('.'));
    const char* kind = ext == ".json" ? "json" : ext == ".lua" ? "lua" : (ext == ".png" || ext == ".jpg" || ext == ".bmp") ? "image" : (ext == ".ogg" || ext == ".wav") ? "audio" : "other";
    o.set("kind", jstr(kind));
    if (ext == ".json")
      if (const ote_editor::FileSchema* fs = schemaFor(ctx, f.first)) o.set("schema", jstr(fs->title.empty() ? fs->name : fs->title));
    arr.push(o);
  }
  Json out = jobj();
  out.set("files", arr);
  out.set("total", jnum((double)total));
  if (offset + shown < total) out.set("nextOffset", jnum((double)(offset + shown)));
  return toolOk(out);
}

ToolResult describe(Context& ctx, const Json& args) {
  const std::string file = args["file"].asString();
  const int depth = args["depth"].asInt(1);
  if (file.empty()) {
    // The map of what can be edited: every schema, and the files of the game each one describes.
    Json arr = jarr();
    Game g;
    std::string why;
    const bool haveGame = ctx.findGame(args["game"].asString(), g, why);
    for (const auto& s : ctx.schemas.all()) {
      if (s->files.empty()) continue;
      Json o = jobj();
      o.set("schema", jstr(s->name));
      o.set("title", jstr(s->title.empty() ? s->name : s->title));
      o.set("files", jstrs(s->files));
      o.set("shape", jstr(s->rows ? "table (a list of rows)" : "document"));
      arr.push(o);
    }
    Json out = jobj();
    out.set("schemas", arr);
    if (haveGame) {
      Json have = jarr();
      for (const std::string& rel : jsonFilesOf(g))
        if (schemaFor(ctx, rel)) have.push(jstr(rel));
      out.set("filesOfThisGameWithASchema", have);
    }
    out.set("next", jstr("describe with file (and path) shows the keys of one place, e.g. file=data/units.json"));
    return toolOk(out);
  }
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  LoadedJson l;
  std::string why;
  if (!loadJsonFile(ctx, g, file, l, why)) return toolFail(why, "list_files");
  Txn t(ctx, g);
  const ote_editor::FileSchema* fs = schemaFor(ctx, file);
  Json out = jobj();
  out.set("file", jstr(file));
  if (!fs) {
    out.set("schema", Json());
    out.set("note", jstr("this file has no schema: it is plain JSON. get reads it; writing needs allowUnknown=true"));
    return toolOk(out);
  }
  ConcretePath cp;
  const std::string pathText = args["path"].asString();
  if (!pathText.empty() && !pathFromArgs(l.value, pathText, cp, err)) return err;
  out.set("path", jstr(pathText.empty() ? "" : pathLabel(l.value, cp)));
  const NodeAt na = schemaAt(fs, l.value, cp);
  const CheckEnv env = envFor(t, &l.value);
  const Json* value = valueAt(l.value, cp);
  if (na.isTable || (pathText.empty() && fs->rows)) {
    const Json* rows = ote_editor::tableArray(l.value);
    out.set("shape", jstr("table: a list of rows; each row is an object"));
    out.set("rows", jnum(rows ? (double)rows->size() : 0));
    if (rows) {
      Json ids = jarr();
      for (const Json& r : rows->items()) if (r["id"].isString() && ids.size() < 60) ids.push(r["id"]);
      out.set("rowIds", ids);
    }
    out.set("row", describeNode(*fs->rows, env, depth));
    out.set("next", jstr("add_entry adds a row; set changes a value, e.g. path=[id].key"));
    return toolOk(out);
  }
  if (!na.node) {
    if (na.unknownKey) out.set("note", jstr("the schema does not know the key \"" + na.key + "\" here; it is kept as raw JSON (writing it needs allowUnknown=true)"));
    else out.set("note", jstr("the schema does not describe this place; it is raw JSON"));
    if (na.parent) out.set("section", describeNode(*na.parent, env, 0));
    if (value) out.set("current", value->isObject() || value->isArray() ? Json::makeString(previewOf(*value)) : *value);
    return toolOk(out);
  }
  out.set("node", describeNode(*na.node, env, depth, value));
  if (value) {
    if (value->isObject() || value->isArray()) out.set("current", jstr(previewOf(*value)));
    else out.set("current", *value);
  }
  if (na.node->kind == K::List && na.node->of) {
    const SchemaNode& item = na.node->of->resolved();
    if (item.kind == K::Variant) out.set("itemKinds", variantList(item));
  }
  if (na.node->kind == K::Variant) out.set("kinds", variantList(*na.node));
  return toolOk(out);
}

// --- get --------------------------------------------------------------------------------------------------------------

ToolResult get(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  const std::string file = args["file"].asString();
  LoadedJson l;
  std::string why;
  if (!loadJsonFile(ctx, g, file, l, why)) return toolFail(why, "list_files");
  ConcretePath cp;
  const std::string pathText = args["path"].asString();
  if (!pathText.empty() && !pathFromArgs(l.value, pathText, cp, err)) return err;
  const Json* v = valueAt(l.value, cp);
  if (!v) return toolFail("nothing at that path");
  const bool summary = args["mode"].asString("value") == "summary";
  const size_t limit = (size_t)args["limit"].asInt(50), offset = (size_t)args["offset"].asInt(0);
  const ote_editor::FileSchema* fs = schemaFor(ctx, file);
  Json out = jobj();
  out.set("file", jstr(file));
  out.set("path", jstr(pathLabel(l.value, cp)));
  out.set("type", jstr(jsonTypeName(*v)));
  if (l.hasComments) out.set("note", jstr("this file has // comments; they are not shown and saving it with a write tool drops them"));

  size_t used = 0;
  if (v->isArray()) {
    const SchemaNode* itemNode = itemNodeAt(fs, l.value, cp);
    out.set("total", jnum((double)v->size()));
    Json items = jarr();
    size_t i = offset;
    for (; i < v->items().size() && items.size() < limit; ++i) {
      const Json& item = v->items()[i];
      ConcretePath ip = cp;
      Step s;
      s.isIndex = true;
      s.index = i;
      ip.push_back(s);
      Json e = jobj();
      e.set("path", jstr(pathLabel(l.value, ip)));
      if (summary) {
        std::string text = itemSummary(itemNode, item);
        if (text.empty()) {
          if (item.isObject()) {
            // No summary template: the id (and name), then the other keys in short form, so an event reads "opening when={..} do=[5]".
            text = item["id"].asString();
            if (item["name"].isString() && item["name"].asString() != text) text += (text.empty() ? "" : " - ") + item["name"].asString();
            for (const auto& m : item.members()) {
              if (m.first == "id" || m.first == "name" || text.size() > 120) continue;
              std::string shown = m.second.isArray() ? "[" + std::to_string(m.second.size()) + "]" : m.second.isObject() ? m.second.dump(0) : previewOf(m.second, 30);
              if (shown.size() > 40) shown = shown.substr(0, 37) + "...";
              text += (text.empty() ? "" : " ") + m.first + "=" + shown;
            }
          }
          if (text.empty()) text = previewOf(item);
        }
        e.set("summary", jstr(text));
      } else {
        e.set("value", item);
      }
      const size_t cost = e.dump(0).size();
      if (used + cost > kBudgetChars && items.size() > 0) break;
      used += cost;
      items.push(e);
    }
    out.set("items", items);
    if (i < v->items().size()) {
      out.set("nextOffset", jnum((double)i));
      out.set("more", jstr("this was part of the list: call again with offset=" + std::to_string(i) + (summary ? "" : " (or mode=summary for one line per entry)")));
    }
  } else if (v->isObject()) {
    out.set("total", jnum((double)v->size()));
    Json members = jobj();
    size_t i = 0, shown = 0;
    for (const auto& m : v->members()) {
      if (i++ < offset) continue;
      if (shown >= limit) break;
      Json mv = summary ? jstr(previewOf(m.second)) : m.second;
      const size_t cost = m.first.size() + mv.dump(0).size();
      if (used + cost > kBudgetChars && shown > 0) { --i; break; }
      used += cost;
      members.set(m.first, mv);
      ++shown;
    }
    out.set("keys", members);
    if (offset + shown < v->size()) {
      out.set("nextOffset", jnum((double)(offset + shown)));
      out.set("more", jstr("more keys exist: call again with offset=" + std::to_string(offset + shown) + (summary ? "" : " or use mode=summary")));
    }
  } else {
    out.set("value", *v);
  }
  return toolOk(out);
}

// --- search -----------------------------------------------------------------------------------------------------------

std::string lowered(std::string s) {
  for (char& c : s) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  return s;
}

struct SearchState {
  const std::string& q;
  const std::string ql;
  bool exact;
  const std::string& file;
  const Json& root;
  std::vector<Json>& hits;
  size_t cap;
  int budget = 400000;

  bool matches(const std::string& s) const { return exact ? s == q : lowered(s).find(ql) != std::string::npos; }

  void walk(const Json& v, ConcretePath& path) {
    if (--budget < 0 || hits.size() >= cap) return;
    if (v.isString() && matches(v.asString())) {
      Json h = jobj();
      h.set("file", jstr(file));
      h.set("path", jstr(pathLabel(root, path)));
      h.set("value", jstr(previewOf(v, 120)));
      hits.push_back(h);
    } else if (v.isObject()) {
      for (const auto& m : v.members()) {
        Step s;
        s.key = m.first;
        path.push_back(s);
        if (matches(m.first)) {
          Json h = jobj();
          h.set("file", jstr(file));
          h.set("path", jstr(pathLabel(root, path)));
          h.set("key", jstr(m.first));
          hits.push_back(h);
        }
        walk(m.second, path);
        path.pop_back();
        if (hits.size() >= cap) return;
      }
    } else if (v.isArray()) {
      for (size_t i = 0; i < v.items().size(); ++i) {
        Step s;
        s.isIndex = true;
        s.index = i;
        path.push_back(s);
        walk(v.items()[i], path);
        path.pop_back();
        if (hits.size() >= cap) return;
      }
    }
  }
};

ToolResult search(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  const std::string q = args["query"].asString();
  if (q.empty()) return toolFail("query must not be empty");
  const bool exact = args["mode"].asString("usages") == "usages";
  const size_t limit = (size_t)args["limit"].asInt(50), offset = (size_t)args["offset"].asInt(0);
  const std::string prefix = args["fileFilter"].asString();
  Txn t(ctx, g);
  std::vector<Json> hits;
  const size_t cap = offset + limit + 1;  // one more than asked, to know whether there is a next page
  for (const std::string& rel : t.knownFiles()) {
    if (!prefix.empty() && !startsWith(rel, prefix)) continue;
    const Json* doc = t.peek(rel);
    if (!doc) continue;
    SearchState s{q, lowered(q), exact, rel, *doc, hits, cap};
    ConcretePath p;
    s.walk(*doc, p);
    if (hits.size() >= cap) break;
  }
  // Lua scripts and formulas name ids in plain text: look there too (whole lines, in text mode a substring, in usages mode a whole word).
  if (hits.size() < cap)
    for (const auto& f : ote_editor::listProjectFiles(g.dir)) {
      if (!ote_editor::hasExtension(f.first, ".lua") || (!prefix.empty() && !startsWith(f.first, prefix))) continue;
      std::string text;
      if (!ote_editor::readWholeFile(f.second, text)) continue;
      size_t lineNo = 0;
      for (const std::string& line : splitOn(text, '\n')) {
        ++lineNo;
        bool hit = false;
        if (exact) {
          for (size_t pos = line.find(q); pos != std::string::npos && !hit; pos = line.find(q, pos + 1)) {
            auto word = [](char c) { return std::isalnum((unsigned char)c) || c == '_'; };
            const bool before = pos == 0 || !word(line[pos - 1]), after = pos + q.size() >= line.size() || !word(line[pos + q.size()]);
            hit = before && after;
          }
        } else {
          hit = lowered(line).find(lowered(q)) != std::string::npos;
        }
        if (!hit) continue;
        Json h = jobj();
        h.set("file", jstr(f.first));
        h.set("line", jnum((double)lineNo));
        h.set("text", jstr(previewOf(jstr(line), 160)));
        hits.push_back(h);
        if (hits.size() >= cap) break;
      }
      if (hits.size() >= cap) break;
    }
  Json arr = jarr();
  for (size_t i = offset; i < hits.size() && arr.size() < limit; ++i) arr.push(hits[i]);
  Json out = jobj();
  out.set("query", jstr(q));
  out.set("mode", jstr(exact ? "usages" : "text"));
  out.set("hits", arr);
  if (hits.size() > offset + arr.size()) out.set("nextOffset", jnum((double)(offset + arr.size())));
  if (arr.size() == 0) out.set("note", jstr("no matches; usages mode needs the exact text of an id (case-sensitive), text mode finds substrings"));
  return toolOk(out);
}

ToolResult validate(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  const std::vector<ote_editor::Problem> p = ctx.problemsOf(g);
  const size_t limit = (size_t)args["limit"].asInt(50), offset = (size_t)args["offset"].asInt(0);
  Json arr = jarr();
  for (size_t i = offset; i < p.size() && arr.size() < limit; ++i) {
    Json o = jobj();
    o.set("file", jstr(p[i].file));
    o.set("message", jstr(p[i].message));
    arr.push(o);
  }
  Json out = jobj();
  out.set("ok", jbool(p.empty()));
  out.set("problems", jnum((double)p.size()));
  out.set("list", arr);
  if (offset + arr.size() < p.size()) out.set("nextOffset", jnum((double)(offset + arr.size())));
  out.set("note", jstr("this is the engine's own project check (the loader on every file, every map, the formulas and a battle start): a game with no problems loads and starts in the player. It reports at most one problem per stage or map, so fixing one can reveal the next."));
  return toolOk(out);
}

}  // namespace

void registerDiscoveryTools(ToolRegistry& r, Context& ctx) {
  Context* c = &ctx;
  {
    Tool t;
    t.name = "list_games";
    t.title = "List games";
    t.description = "Lists the games this server can work on: every folder that holds a game.json directly under one of the allowed roots. A game is a folder of JSON data files (units, weapons, terrain, maps ...) that the OpenTacticsEngine player runs as a turn-based tactics game. Start here to find the id you pass as \"game\" to the other tools. If the list is empty, call list_templates and create_game.";
    t.inputSchema = sObject({});
    t.run = [c](const Json& a) { return listGames(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "get_overview";
    t.title = "Overview of a game";
    t.description = "One call that orients you in a game: its name, grid kind (square or hex), the maps, how many entries each data file has (units, weapons, terrain, items ...), the ids of stats / resources / move types, which optional rule systems are switched on in game.json 'rules' and which are off, how many units and events the maps hold, and how many problems the engine's project check finds (with the first few). Call it first after list_games; then use describe, get and the editing tools.";
    t.inputSchema = sObject({{"game", gameProp()}});
    t.run = [c](const Json& a) { return getOverview(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "list_files";
    t.title = "List files of a game";
    t.description = "Lists the files inside a game folder with their size and kind (json, lua, image, audio). JSON files that have a schema (so describe and the editing tools know their keys) show its name. Use dir to look into one folder such as data or maps. Results are paged: when nextOffset is present, call again with it.";
    Props p{{"game", gameProp()}, {"dir", sType("string", "Only files below this folder of the game, e.g. \"data\" or \"maps\".")}};
    addPagingProps(p, 200);
    t.inputSchema = sObject(p);
    t.run = [c](const Json& a) { return listFiles(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "describe";
    t.title = "Describe what a file or place accepts";
    t.description = "Asks the editor's schema what settings exist. With no arguments it lists every schema and the game files it covers. With file (e.g. data/units.json, game.json, maps/start.json) it describes the whole file; add path (e.g. rules.actionPoints, [archer].stats, events[0].do) for one place. The answer gives each key's type, whether it is required, its default, number ranges, the allowed words of an enum, which other ids a reference points at together with the CURRENT valid ids in this game, help text, which optional blocks exist and whether they are enabled, and for lists of several shapes (story commands, conditions, skill effects) the list of kinds with a one-line help each. depth (1-3) says how many levels of keys to unfold. Read this before writing so values pass the editor's checks.";
    Props p{{"game", gameProp()},
            {"file", sType("string", "Project file, e.g. data/units.json. Omit to list the schemas.")},
            {"path", sType("string", "Place inside the file. Syntax: keys joined by dots, list entries as [index] or [id], e.g. units[archer].stats.hp or [archer].stats (the table's own key may be left out in table files like data/units.json). Omit for the whole file.")},
            {"depth", sInt("Levels of keys to unfold (default 1).", 0, 3)}};
    t.inputSchema = sObject(p);
    t.run = [c](const Json& a) { return describe(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "get";
    t.title = "Read a value, row or section";
    t.description = "Reads JSON from a game file. file is project-relative (data/units.json, game.json, maps/start.json); path picks a place (same syntax as describe: keys joined by dots, list entries as [index] or [id], e.g. units[archer], rules.turnOrder, events[2].do). Without path you get the top of the file. Long lists and big objects are paged: the result carries total and, when there is more, nextOffset; use limit/offset to walk through. mode=summary returns one short line per list entry (using the schema's summaries, e.g. the story command kind and what it says) or a preview per key, which is the way to survey big lists such as events and commands before reading single entries. Each entry carries its path so you can edit it with set.";
    Props p{{"game", gameProp()},
            {"file", sType("string", "Project file, e.g. data/units.json.")},
            {"path", sType("string", "Place inside the file (see describe). Omit for the whole file.")},
            {"mode", sEnum({"value", "summary"}, "value (default): the JSON itself; summary: one line per entry or key.")}};
    addPagingProps(p, 50);
    t.inputSchema = sObject(p, {"file"});
    t.run = [c](const Json& a) { return get(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "search";
    t.title = "Search a game";
    t.description = "Finds text across all JSON files of a game and its Lua scripts. mode=usages (default) finds an exact id or word: every value equal to it and every key named so, which answers \"where is the weapon iron_sword used?\" before you rename or remove it. mode=text finds a case-insensitive substring in text values and keys (dialogue lines, names). Each hit has file and path (usable with get / set); Lua hits give file and line. Paged with limit/offset.";
    Props p{{"game", gameProp()},
            {"query", sType("string", "The id or text to look for.")},
            {"mode", sEnum({"usages", "text"}, "usages (default): exact match; text: case-insensitive substring.")},
            {"fileFilter", sType("string", "Only files whose path starts with this, e.g. \"maps/\".")}};
    addPagingProps(p, 50);
    t.inputSchema = sObject(p, {"query"});
    t.run = [c](const Json& a) { return search(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "validate";
    t.title = "Check a game";
    t.description = "Runs the engine's own project check on the saved files of a game: the loader reads every data file and every map, the formulas are compiled and a battle is started on each map. It returns ok and the list of problems as file plus message. The check reports at most one problem per stage or map, so after fixing one, call validate again. A game with no problems loads and starts in the player. The writing tools already return the problems after each write; use this to re-check at any time.";
    Props p{{"game", gameProp()}};
    addPagingProps(p, 50);
    t.inputSchema = sObject(p);
    t.run = [c](const Json& a) { return validate(*c, a); };
    r.add(t);
  }
}

}  // namespace ote_mcp
