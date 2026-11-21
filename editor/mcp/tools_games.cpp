// Game tools: the templates the browser editor's "new game" dialog offers, and making a game from one.
#include "paths.h"
#include "revision.h"
#include "templates.h"
#include "tools.h"
#include "validate.h"

namespace ote_mcp {

namespace {

ToolResult listTemplates(Context& ctx, const Json&) {
  std::error_code ec;
  if (ctx.cfg.templatesDir.empty() || !fs::is_directory(ctx.cfg.templatesDir, ec))
    return toolFail("the templates folder was not found" + (ctx.cfg.templatesDir.empty() ? std::string() : " (" + ctx.cfg.templatesDir.string() + ")"),
                    "start ote-mcp with --templates <path to editor/templates> (or set OTE_TEMPLATES_DIR)");
  Json arr = jarr();
  for (const ote_editor::TemplateInfo& t : ote_editor::listTemplates(ctx.cfg.templatesDir)) {
    Json o = jobj();
    o.set("id", jstr(t.id));
    o.set("name", jstr(t.name));
    o.set("description", jstr(t.description));
    o.set("grid", jstr(t.gridType + (t.gridLayout.empty() ? "" : " (" + t.gridLayout + ")")));
    arr.push(o);
  }
  Json out = jobj();
  out.set("templates", arr);
  out.set("next", jstr("create_game makes a game from one"));
  return toolOk(out);
}

ToolResult createGame(Context& ctx, const Json& args) {
  const std::string folder = args["folder"].asString(), tpl = args["template"].asString();
  if (!validName(folder)) return toolFail("folder must be 1 to 40 characters: lowercase letters, digits and '-', starting with a letter or digit", "e.g. \"my-first-game\"");
  if (!validName(tpl)) return toolFail("unknown template \"" + tpl + "\"", "list_templates shows the ids");
  std::error_code ec;
  if (ctx.cfg.templatesDir.empty() || !fs::is_directory(ctx.cfg.templatesDir, ec))
    return toolFail("the templates folder was not found", "start ote-mcp with --templates <path to editor/templates> (or set OTE_TEMPLATES_DIR)");
  if (ctx.cfg.roots.empty()) return toolFail("no root folder is configured");
  fs::path root = ctx.cfg.roots[0];
  if (args["root"].isString() && !args["root"].asString().empty()) {
    bool found = false;
    std::error_code e2;
    const fs::path want = fs::weakly_canonical(fs::path(args["root"].asString()), e2);
    for (const fs::path& r : ctx.cfg.roots) if (!e2 && r == want) { root = r; found = true; }
    if (!found) {
      std::string all;
      for (const fs::path& r : ctx.cfg.roots) all += (all.empty() ? "" : ", ") + r.generic_string();
      return toolFail("root is not one of the folders this server may use (" + all + ")", "leave root out to use the first one");
    }
  }
  const std::string title = args["title"].asString(folder).empty() ? folder : args["title"].asString(folder);
  const fs::path dst = root / folder;
  if (fs::exists(fs::symlink_status(dst, ec))) return toolFail("a game or folder \"" + folder + "\" already exists under " + root.generic_string(), "choose another folder name");
  bool known = false;
  for (const ote_editor::TemplateInfo& t : ote_editor::listTemplates(ctx.cfg.templatesDir)) known = known || t.id == tpl;
  if (!known) return toolFail("unknown template \"" + tpl + "\"", "list_templates shows the ids");

  Json out = jobj();
  out.set("game", jstr(folder));
  out.set("template", jstr(tpl));
  out.set("title", jstr(title));
  if (args["dryRun"].asBool(false)) {
    size_t files = 0, bytes = 0;
    for (const auto& f : ote_editor::listProjectFiles(ctx.cfg.templatesDir / tpl)) {
      ++files;
      std::error_code e3;
      bytes += (size_t)fs::file_size(f.second, e3);
    }
    out.set("dryRun", jbool(true));
    out.set("wouldCreate", jstr(dst.generic_string()));
    out.set("files", jnum((double)files));
    out.set("bytes", jnum((double)bytes));
    return toolOk(out);
  }
  const ote_editor::CreateResult made = ote_editor::createFromTemplate(ctx.cfg.templatesDir, root, folder, tpl, title);
  if (made.status != ote_editor::CreateStatus::Created) return toolFail("could not create the game: " + made.message);
  Game g{folder, fs::weakly_canonical(dst, ec), root};
  const std::vector<ote_editor::Problem> problems = ote_editor::validateProject(g.dir);
  ctx.rememberProblems(g, problems);
  out.set("dryRun", jbool(false));
  out.set("created", jstr(g.dir.generic_string()));
  out.set("problems", jnum((double)problems.size()));
  if (!problems.empty()) out.set("firstProblems", problemsJson(problems, 5));
  out.set("next", jstr("get_overview shows what the game contains; describe and the editing tools change it"));
  return toolOk(out);
}

}  // namespace

void registerGameTools(ToolRegistry& r, Context& ctx) {
  Context* c = &ctx;
  {
    Tool t;
    t.name = "list_templates";
    t.title = "List game templates";
    t.description = "Lists the starting points for a new game: small complete games (a blank one with a single unit type, a strategy RPG with characters and story, an economy game with factories and funds, hexagon-grid variants) with a name, description and grid kind. These are the same templates the browser editor offers. Pass one id to create_game.";
    t.inputSchema = sObject({});
    t.run = [c](const Json& a) { return listTemplates(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "create_game";
    t.title = "Create a game";
    t.description = "Creates a new game folder from a template, below the first allowed root folder (or the root you name). The folder name is lowercase letters, digits and '-'. The title becomes the game's name. The new game is checked with the engine's project check before this returns (a template always starts clean). It never overwrites or deletes anything: an existing folder of that name is refused. After this, use get_overview, describe, set, add_entry and the map tools to build the game. dryRun=true only reports what would be created.";
    Props p{{"folder", sType("string", "Folder name of the game, e.g. my-first-game (lowercase letters, digits, '-').")},
            {"template", sType("string", "Template id from list_templates.")},
            {"title", sType("string", "The game's name as players see it (default: the folder name).")},
            {"root", sType("string", "Which allowed root folder to create it in (default: the first).")},
            {"dryRun", sType("boolean", "true = create nothing; report what would be created.")}};
    t.inputSchema = sObject(p, {"folder", "template"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return createGame(*c, a); };
    r.add(t);
  }
}

}  // namespace ote_mcp
