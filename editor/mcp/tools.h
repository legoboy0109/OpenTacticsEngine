// The tools of ote-mcp, grouped by what they do; each group registers its tools with the registry. Shared helpers for the
// tool implementations live at the bottom.
#pragma once
#include <string>
#include <utility>
#include <vector>

#include "context.h"
#include "schema_walk.h"
#include "tool.h"
#include "txn.h"

namespace ote_mcp {

void registerDiscoveryTools(ToolRegistry& r, Context& ctx);  // list_games, get_overview, list_files, describe, get, search, validate
void registerEditTools(ToolRegistry& r, Context& ctx);       // set, add_entry, duplicate_entry, remove_entry, move_entry, enable_block, disable_block
void registerRenameTool(ToolRegistry& r, Context& ctx);      // rename_id
void registerMapTools(ToolRegistry& r, Context& ctx);        // get_map, set_tiles, new_map
void registerGameTools(ToolRegistry& r, Context& ctx);       // list_templates, create_game
void registerRunTools(ToolRegistry& r, Context& ctx);        // render_map, simulate

// --- shared by the tool implementations ---------------------------------------------------------------------------

using Props = std::vector<std::pair<std::string, Json>>;

inline Json anyProp(const std::string& description) {  // a property that may hold any JSON value
  Json o = Json::makeObject();
  o.set("description", Json::makeString(description));
  return o;
}
Json gameProp();                       // the "game" argument
void addWriteProps(Props& props);      // dryRun, force
void addPagingProps(Props& props, int defaultLimit);  // limit, offset

// Finds the game named by args["game"] (optional when exactly one game exists).
bool gameFromArgs(Context& ctx, const Json& args, Game& out, ToolResult& err);

// Commits the transaction as the arguments ask (dryRun / force) and merges `extra` (what the tool wants to say about the
// change) into the report. A refused or failed commit becomes an error result carrying the report.
ToolResult finishWrite(Txn& txn, const Json& args, Json extra);

// Resolves the project file argument to its working copy.
Json* fileFromArgs(Txn& txn, const Json& args, const char* key, std::string& rel, ToolResult& err);

// "units[archer].stats.hp" -> concrete path in `root` (create: make missing intermediate objects).
bool pathFromArgs(Json& root, const std::string& pathText, ConcretePath& out, ToolResult& err, bool create = false);

// The refusal result of a failed value check: every issue with its place and the allowed values.
ToolResult refuse(const std::vector<Issue>& issues, const std::string& file);

// A short preview of a value for lists: scalars as they are, containers as "{...N keys}" / "[N items]".
std::string previewOf(const Json& v, size_t maxChars = 80);

// The item node of the list at `listPath` (the row node for a table file); null when the schema does not describe it.
const ote_editor::SchemaNode* itemNodeAt(const ote_editor::FileSchema* fs, const Json& root, const ConcretePath& listPath);

// Every project JSON file, relative paths sorted.
std::vector<std::string> jsonFilesOf(const Game& g);

// All the references to an id found through the schemas (used by rename_id and remove_entry).
struct RefHit {
  std::string file;
  ConcretePath path;
  bool isKey = false;  // the id is a map key, not a value
};
// `listFile` / `listPath`: where the entry lives (its list). Returns false (with `why`) when the id has no reference domain.
std::vector<RefHit> findReferences(Txn& txn, const std::string& listFile, const ConcretePath& listPath, const std::string& id);

}  // namespace ote_mcp
