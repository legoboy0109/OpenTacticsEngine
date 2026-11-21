// Using the schema engine (editor/core/schema_engine.h) on a path of a real document: which schema node describes this place,
// is this value allowed there, what does a new entry start as, and what does a node look like to a model.
#pragma once
#include <string>
#include <vector>

#include "context.h"
#include "jpath.h"
#include "schema_engine.h"
#include "schema_ids.h"

namespace ote_mcp {

// What the checks need to know about the project: the other files (as the change would leave them) for id lists.
struct CheckEnv {
  const Json* doc = nullptr;                // the file being edited (for "@sides" style sources)
  ote_editor::DocLookup docs;               // every other file
  std::vector<std::string> ids(const std::vector<std::string>& sources) const {
    return ote_editor::resolveIdSources(doc ? *doc : Json(), docs, sources);
  }
  ote_editor::IdLookup lookup() const {
    return [this](const std::vector<std::string>& s) { return ids(s); };
  }
};

struct Issue {
  std::string path;
  std::string message;
  std::vector<std::string> allowed;  // valid words / ids, when there is a short list
};

// The schema node of a place. `node` is null when the schema says nothing there ("raw" parts, unknown keys).
struct NodeAt {
  const ote_editor::SchemaNode* node = nullptr;
  const ote_editor::SchemaNode* parent = nullptr;  // the object / map / list node that holds it
  const Json* parentValue = nullptr;
  std::string key;          // its key in the parent (object / map)
  bool unknownKey = false;  // the parent is an object that has no such field
  bool isTable = false;     // the place is the table array itself of a table file
};

// Walks the schema along `path` of `root`. `fs` may be null (file without a schema: every NodeAt has a null node).
NodeAt schemaAt(const ote_editor::FileSchema* fs, const Json& root, const ConcretePath& path);

// Is `v` allowed at that place? Fills `issues` (at most ~8). Unknown keys in described objects are issues unless
// `allowUnknown`; so are missing required keys, wrong types, ranges, enum words and ids that do not exist.
void checkValue(const ote_editor::SchemaNode* node, const Json& v, const CheckEnv& env, const std::string& path,
                bool allowUnknown, std::vector<Issue>& issues);

// The value a new list item / block of `node` starts with (variant kind by name; "" = the first).
bool newValueFor(const ote_editor::SchemaNode& node, const std::string& kind, const CheckEnv& env, Json& out, std::string& err);

// What a node looks like to a model (type, help, range, values, references with their CURRENT ids, children).
Json describeNode(const ote_editor::SchemaNode& node, const CheckEnv& env, int depth, const Json* value = nullptr);

// One-line text of a list item using the schema's summary templates (variants and objects); "" when there is none.
std::string itemSummary(const ote_editor::SchemaNode* itemNode, const Json& item);

// The names a variant node offers, with their help lines.
Json variantList(const ote_editor::SchemaNode& node);

}  // namespace ote_mcp
