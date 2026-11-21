#include "context.h"

#include <algorithm>

#include "embedded.h"
#include "json_comments.h"
#include "paths.h"
#include "revision.h"
#include "validate.h"

namespace ote_mcp {

Context::Context(Config c) : cfg(std::move(c)) {
  if (!cfg.schemaDir.empty()) {
    schemas.loadFolder(cfg.schemaDir);
    schemaSource = cfg.schemaDir;
  } else {
    std::vector<ote_editor::SchemaSet::Text> texts;
    for (size_t i = 0; i < kSchemasCount; ++i)
      texts.emplace_back(kSchemas[i].name, std::string(reinterpret_cast<const char*>(kSchemas[i].data), kSchemas[i].size));
    schemas.addTexts(texts);
    schemaSource = "embedded";
  }
  schemaErrors = schemas.errors();
  for (const std::string& e : schemaErrors) logWarn("schema: " + e);
}

namespace {
bool isGameDir(const fs::path& p) {
  std::error_code ec;
  return fs::is_regular_file(p / "game.json", ec);
}
}  // namespace

std::vector<Game> Context::listGames() const {
  std::vector<Game> out;
  for (const fs::path& root : cfg.roots) {
    std::error_code ec;
    if (isGameDir(root)) out.push_back({root.filename().string(), root, root});
    std::vector<fs::path> dirs;
    for (fs::directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
      std::error_code e2;
      if (!it->is_directory(e2)) continue;
      const std::string name = it->path().filename().string();
      if (name.empty() || name[0] == '.') continue;  // hidden folders (half-made games, temp copies)
      // A folder that is a link leading out of the root is not a game of this root.
      const fs::path canon = fs::weakly_canonical(it->path(), e2);
      if (e2) continue;
      auto mm = std::mismatch(root.begin(), root.end(), canon.begin(), canon.end());
      if (mm.first != root.end()) continue;
      if (isGameDir(canon)) dirs.push_back(canon);
    }
    std::sort(dirs.begin(), dirs.end());
    for (const fs::path& d : dirs) out.push_back({d.filename().string(), d, root});
  }
  return out;
}

bool Context::findGame(const std::string& id, Game& out, std::string& err) const {
  const std::vector<Game> all = listGames();
  if (id.empty()) {
    if (all.size() == 1) { out = all[0]; return true; }
    err = all.empty() ? "no game found under the roots: create one with create_game, or start ote-mcp with --root <folder that holds games>"
                      : "several games exist: pass \"game\" (an id from list_games)";
    return false;
  }
  for (const Game& g : all)
    if (g.id == id) { out = g; return true; }
  std::string names;
  for (const Game& g : all) names += (names.empty() ? "" : ", ") + g.id;
  err = "no game \"" + id + "\" under the roots (call list_games; known: " + (names.empty() ? "none" : names) + ")";
  return false;
}

bool Context::resolveFile(const Game& g, const std::string& rel, fs::path& out, std::string& err) const {
  if (!ote_editor::resolveInside(g.dir, rel, out, err)) {
    err = "file \"" + rel + "\": " + err + " (use a path relative to the game folder, like data/units.json)";
    return false;
  }
  return true;
}

std::vector<ote_editor::Problem> Context::problemsOf(const Game& g) {
  const std::string rev = ote_editor::computeRevision(g.dir);
  auto it = problemCache_.find(g.dir.string());
  if (it != problemCache_.end() && it->second.revision == rev) return it->second.problems;
  std::vector<ote_editor::Problem> p = ote_editor::validateProject(g.dir);
  problemCache_[g.dir.string()] = {rev, p};
  return p;
}

void Context::rememberProblems(const Game& g, const std::vector<ote_editor::Problem>& p) {
  problemCache_[g.dir.string()] = {ote_editor::computeRevision(g.dir), p};
}

bool loadJsonFile(Context& ctx, const Game& g, const std::string& rel, LoadedJson& out, std::string& err, bool markSeen) {
  fs::path full;
  if (!ctx.resolveFile(g, rel, full, err)) return false;
  if (!ote_editor::hasExtension(rel, ".json")) { err = "\"" + rel + "\" is not a .json file (this server edits the game's JSON data files)"; return false; }
  std::error_code ec;
  if (!fs::is_regular_file(full, ec)) { err = "no such file: " + rel + " (call list_files to see what the game has)"; return false; }
  if (!ote_editor::readWholeFile(full, out.bytes)) { err = "cannot read " + rel; return false; }
  std::string perr;
  if (!Json::parse(out.bytes, out.value, perr)) { err = rel + " is not valid JSON: " + perr; return false; }
  out.hasComments = ote_editor::jsonTextHasComments(out.bytes);
  if (markSeen) ctx.noteSeen(full, out.bytes);
  return true;
}

const ote_editor::FileSchema* schemaFor(const Context& ctx, const std::string& rel) { return ctx.schemas.forFile(rel); }

}  // namespace ote_mcp
