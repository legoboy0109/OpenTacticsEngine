#include "resources.h"

#include <cstdlib>

#include "embedded.h"
#include "paths.h"
#include "txn.h"

namespace ote_mcp {

namespace {

constexpr size_t kChunk = 100000;

Json entry(const std::string& uri, const std::string& name, const std::string& description, const char* mime) {
  Json o = jobj();
  o.set("uri", jstr(uri));
  o.set("name", jstr(name));
  o.set("description", jstr(description));
  o.set("mimeType", jstr(mime));
  return o;
}

std::string embeddedText(const Embedded* list, size_t count, const std::string& name, bool& found) {
  for (size_t i = 0; i < count; ++i)
    if (name == list[i].name) { found = true; return std::string(reinterpret_cast<const char*>(list[i].data), list[i].size); }
  found = false;
  return "";
}

Json textContents(const std::string& uri, const std::string& mime, const std::string& text) {
  Json c = jobj();
  c.set("uri", jstr(uri));
  c.set("mimeType", jstr(mime));
  c.set("text", jstr(sanitizeUtf8(text)));
  Json arr = jarr();
  arr.push(c);
  Json out = jobj();
  out.set("contents", arr);
  return out;
}

const char* docSummary(const std::string& name) {
  if (name == "GAME_FORMAT") return "The game folder format: game.json, data files, maps (reference).";
  if (name == "SRPG") return "Strategy-RPG rules: units, weapons, skills, experience, items, supports (very long).";
  if (name == "ECONOMY") return "Economy rules: funds, factories, capture, fog, weather, factions.";
  if (name == "EVENTS") return "Story events on maps: triggers, conditions and every command.";
  if (name == "JOBS") return "Jobs and classes.";
  if (name == "TURN_ORDER") return "Turn order modes (sides, speed ...).";
  if (name == "COMMANDERS") return "Commanders with meters and powers.";
  if (name == "MCP") return "How to use this server: tools, safety, a worked session.";
  return "Engine documentation.";
}

}  // namespace

Json listAllResources(Context& ctx) {
  Json arr = jarr();
  for (size_t i = 0; i < kSchemasCount; ++i)
    arr.push(entry(std::string("ote://schema/") + kSchemas[i].name, std::string("schema ") + kSchemas[i].name,
                   "The editor schema of this file (keys, types, ranges, references); describe answers the same per place.", "application/json"));
  for (size_t i = 0; i < kDocsCount; ++i)
    arr.push(entry(std::string("ote://docs/") + kDocs[i].name, std::string("docs ") + kDocs[i].name, docSummary(kDocs[i].name), "text/markdown"));
  for (const Game& g : ctx.listGames())
    arr.push(entry("ote://game/" + g.id + "/problems", "problems of " + g.id, "The engine's project check of this game as it is on disk.", "application/json"));
  return arr;
}

Json resourceTemplates() {
  Json arr = jarr();
  auto t = [&](const char* uri, const char* name, const char* desc, const char* mime) {
    Json o = jobj();
    o.set("uriTemplate", jstr(uri));
    o.set("name", jstr(name));
    o.set("description", jstr(desc));
    o.set("mimeType", jstr(mime));
    arr.push(o);
  };
  t("ote://schema/{file}", "schema", "An editor schema file, e.g. units.json.", "application/json");
  t("ote://docs/{name}", "docs", "An engine document, e.g. GAME_FORMAT. Long ones are cut: add ?offset=N to continue.", "text/markdown");
  t("ote://game/{game}/problems", "game problems", "The project check of a game.", "application/json");
  return arr;
}

bool readResourceUri(Context& ctx, const std::string& uriIn, Json& contents, std::string& message) {
  std::string uri = uriIn;
  size_t offset = 0;
  const size_t q = uri.find('?');
  if (q != std::string::npos) {
    const std::string query = uri.substr(q + 1);
    uri = uri.substr(0, q);
    if (startsWith(query, "offset=")) offset = (size_t)std::strtoull(query.c_str() + 7, nullptr, 10);
  }
  if (startsWith(uri, "ote://schema/")) {
    const std::string name = uri.substr(13);
    std::string text;
    if (!ctx.cfg.schemaDir.empty()) {
      // An override folder replaces the embedded copy; only plain file names inside it are served.
      if (name.find('/') != std::string::npos || name.find('\\') != std::string::npos || name.find("..") != std::string::npos) { message = "bad schema name"; return false; }
      if (!ote_editor::readWholeFile(fs::path(ctx.cfg.schemaDir) / name, text)) { message = "no schema file \"" + name + "\""; return false; }
    } else {
      bool found = false;
      text = embeddedText(kSchemas, kSchemasCount, name, found);
      if (!found) { message = "no schema file \"" + name + "\" (resources/list shows them)"; return false; }
    }
    contents = textContents(uriIn, "application/json", text);
    return true;
  }
  if (startsWith(uri, "ote://docs/")) {
    bool found = false;
    const std::string text = embeddedText(kDocs, kDocsCount, uri.substr(11), found);
    if (!found) { message = "no document \"" + uri.substr(11) + "\" (resources/list shows them)"; return false; }
    if (offset > text.size()) { message = "offset is beyond the end of the document"; return false; }
    size_t end = std::min(text.size(), offset + kChunk);
    if (end < text.size()) {
      const size_t nl = text.rfind('\n', end);
      if (nl != std::string::npos && nl > offset) end = nl + 1;  // cut at a line end
    }
    std::string part = text.substr(offset, end - offset);
    if (end < text.size())
      part += "\n\n[... cut here: " + std::to_string(text.size() - end) + " more bytes. Read " + uri + "?offset=" + std::to_string(end) + " to continue.]\n";
    contents = textContents(uriIn, "text/markdown", part);
    return true;
  }
  if (startsWith(uri, "ote://game/") && endsWith(uri, "/problems")) {
    const std::string id = uri.substr(11, uri.size() - 11 - 9);
    Game g;
    std::string why;
    if (!ctx.findGame(id, g, why)) { message = why; return false; }
    const std::vector<ote_editor::Problem> p = ctx.problemsOf(g);
    Json o = jobj();
    o.set("game", jstr(g.id));
    o.set("ok", jbool(p.empty()));
    o.set("problems", problemsJson(p, 200));
    contents = textContents(uriIn, "application/json", o.dump(0));
    return true;
  }
  message = "unknown resource \"" + uriIn + "\" (resources/list shows what exists)";
  return false;
}

Json listPromptDefs() {
  Json arr = jarr();
  {
    Json p = jobj();
    p.set("name", jstr("build_a_game"));
    p.set("description", jstr("Plan and build a small tactics game with the ote-mcp tools, checking after every step."));
    Json args = jarr();
    Json a = jobj();
    a.set("name", jstr("idea"));
    a.set("description", jstr("What the game should be like, in a sentence or two."));
    a.set("required", jbool(true));
    args.push(a);
    p.set("arguments", args);
    arr.push(p);
  }
  {
    Json p = jobj();
    p.set("name", jstr("review_game"));
    p.set("description", jstr("Check a game for problems and balance, and suggest fixes."));
    Json args = jarr();
    Json a = jobj();
    a.set("name", jstr("game"));
    a.set("description", jstr("Game id (see list_games)."));
    a.set("required", jbool(false));
    args.push(a);
    p.set("arguments", args);
    arr.push(p);
  }
  return arr;
}

bool getPromptByName(Context&, const std::string& name, const Json& arguments, Json& result, std::string& message) {
  std::string text;
  if (name == "build_a_game") {
    const std::string idea = arguments["idea"].asString();
    if (idea.empty()) { message = "the prompt build_a_game needs the argument \"idea\""; return false; }
    text = "Build this game with the ote-mcp tools: " + idea + "\n\n"
           "Work in small steps and check after each one:\n"
           "1. list_templates, then create_game from the closest template.\n"
           "2. get_overview, and describe the files you will change (data/units.json, data/weapons.json, data/terrain.json, game.json) before writing: describe lists the allowed keys, ranges and the ids that exist.\n"
           "3. Change data with add_entry and set (use dryRun=true first when unsure). Every write returns the project problems; fix new problems before moving on.\n"
           "4. Make a map with new_map, paint it with set_tiles, place units with add_entry on the map's units, then validate.\n"
           "5. render_map to look at the result and simulate to see whether the sides are balanced.\n"
           "Read ote://docs/GAME_FORMAT for the file formats if you need them.";
  } else if (name == "review_game") {
    const std::string game = arguments["game"].asString();
    text = "Review the game" + (game.empty() ? std::string() : " \"" + game + "\"") + " with the ote-mcp tools: get_overview, validate (list every problem and fix those found), "
           "get with mode=summary on the unit, weapon and skill tables to look for entries that stand out, simulate each map to see whether one side always wins, and render_map to check the maps look right. "
           "Report what you found and propose changes; make them only if asked.";
  } else {
    message = "unknown prompt \"" + name + "\" (prompts/list shows them)";
    return false;
  }
  Json content = jobj();
  content.set("type", jstr("text"));
  content.set("text", jstr(text));
  Json msg = jobj();
  msg.set("role", jstr("user"));
  msg.set("content", content);
  Json msgs = jarr();
  msgs.push(msg);
  result = jobj();
  result.set("description", jstr(name));
  result.set("messages", msgs);
  return true;
}

}  // namespace ote_mcp
