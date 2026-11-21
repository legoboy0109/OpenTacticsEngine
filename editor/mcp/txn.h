// One writing tool call = one transaction: the files it touches are loaded into working copies, the tool edits them, then
// commit() either reports what would change (dry run, on a temporary copy of the game) or writes every changed file
// atomically. Either way the project check runs on the result and the report says what got better or worse.
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

#include "context.h"
#include "schema_ids.h"

namespace ote_mcp {

struct CommitOptions {
  bool dryRun = false;
  bool force = false;  // write even when a file changed on disk since the model last read it
};

class Txn {
 public:
  Txn(Context& ctx, const Game& game) : ctx_(ctx), game_(game) {}

  // The working copy of a project JSON file (loaded on first use). nullptr with `err` when it cannot be read.
  Json* file(const std::string& rel, std::string& err);
  // A file that must not exist yet.
  bool create(const std::string& rel, const Json& content, std::string& err);
  // Removes a file of the game (never a folder).
  bool remove(const std::string& rel, std::string& err);

  // Id sources resolve against the working copies (so a change that adds a unit and uses it is judged as a whole).
  ote_editor::DocLookup docLookup();
  // The working copy if the file is loaded, else read-only from disk (cached); nullptr when it does not exist.
  const Json* peek(const std::string& rel);

  // Builds the report. When !dryRun and nothing is refused, the files are written. `failed` is set when nothing was written
  // because of a conflict or a write error (the report then carries "error").
  Json commit(const CommitOptions& opt, bool& failed);

  // Every JSON file of the game as the call would leave it (sorted), and the schema of one.
  std::vector<std::string> knownFiles();
  const ote_editor::FileSchema* schemaOf(const std::string& rel) const { return ctx_.schemas.forFile(rel); }

  bool touched() const { return !files_.empty(); }

 private:
  struct Entry {
    std::string rel;
    fs::path path;
    Json working;
    std::string originalBytes;
    bool existed = false;
    bool hadComments = false;
    bool removed = false;
  };
  Entry* entry(const std::string& rel) {
    auto it = files_.find(rel);
    return it == files_.end() ? nullptr : &it->second;
  }

  Context& ctx_;
  Game game_;
  std::map<std::string, Entry> files_;
  std::map<std::string, Json> peeked_;
  std::set<std::string> missing_;
};

// Compact JSON of a problem list for results: [{file, message}], at most `max`.
Json problemsJson(const std::vector<ote_editor::Problem>& p, size_t max = 25);

}  // namespace ote_mcp
