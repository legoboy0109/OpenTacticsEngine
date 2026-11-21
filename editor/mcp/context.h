// The state of one ote-mcp process: its settings (roots, read-only, helper programs), the schema set and the games under the
// roots. The server keeps no other memory between calls except two small caches (what the model last saw of each file, for the
// changed-on-disk check, and the last project check per game).
#pragma once
#include <map>
#include <string>
#include <vector>

#include "common.h"
#include "schema_engine.h"
#include "util.h"

namespace ote_mcp {

struct Config {
  std::vector<fs::path> roots;  // game folders are only ever touched below these (absolute, canonical)
  bool readOnly = false;
  fs::path player;        // ote-player (render_map); empty = not found
  fs::path sim;           // ote-sim (simulate); empty = not found
  fs::path assets;        // assets/ folder the player should use; empty = let the player decide
  fs::path templatesDir;  // editor/templates
  std::string schemaDir;  // override folder for the schemas ("" = the embedded copy)
  std::string exeDir;
};

struct Game {
  std::string id;  // the folder name
  fs::path dir;    // absolute, canonical
  fs::path root;   // the root it lives under
};

class Context {
 public:
  explicit Context(Config cfg);

  Config cfg;
  ote_editor::SchemaSet schemas;
  std::vector<std::string> schemaErrors;  // problems of the schema files themselves (only with an override folder)
  std::string schemaSource;               // "embedded" or the override folder

  // Games directly below a root (a folder with a game.json), plus a root that is itself a game.
  std::vector<Game> listGames() const;
  // `id` may be empty when exactly one game exists. On failure `err` says what to call next.
  bool findGame(const std::string& id, Game& out, std::string& err) const;

  // A project-relative path inside the game, resolved (symlinks, "..") and kept inside it.
  bool resolveFile(const Game& g, const std::string& rel, fs::path& out, std::string& err) const;

  // What the model last saw of each file (absolute path -> hash of its bytes), for the changed-on-disk check.
  std::map<std::string, std::string> seen;
  void noteSeen(const fs::path& file, const std::string& bytes) { seen[file.string()] = hashHex(bytes); }

  // The project check of the game as it is on disk, cached by the folder's revision.
  std::vector<ote_editor::Problem> problemsOf(const Game& g);
  void rememberProblems(const Game& g, const std::vector<ote_editor::Problem>& p);

 private:
  struct Cached { std::string revision; std::vector<ote_editor::Problem> problems; };
  std::map<std::string, Cached> problemCache_;
};

// Reads and parses a project file. `bytes` gets the file as it is on disk; `hasComments` says whether it has // comments.
struct LoadedJson {
  Json value;
  std::string bytes;
  bool hasComments = false;
};
bool loadJsonFile(Context& ctx, const Game& g, const std::string& rel, LoadedJson& out, std::string& err, bool markSeen = true);

// The schema for a project file ("data/units.json" -> units.json), or nullptr.
const ote_editor::FileSchema* schemaFor(const Context& ctx, const std::string& rel);

}  // namespace ote_mcp
