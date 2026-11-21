// The schema of one data file: which keys exist, their types, ranges, enums and which ids they point at. Schemas are
// hand-written JSON files (editor/schema/*.json, format in docs/NATIVE_EDITOR.md) that drive the native editor's forms and
// the MCP server's checks. They never decide what is allowed in a file: the engine's loader does. A schema that disagrees
// with the loader is caught by native_test_conformance.
//
// This is the one schema engine: Qt-free (ote::Json and std types only), shared by the native editor (which wraps it in
// QString types, editor/native/model/schema.h) and by ote-mcp.
#pragma once
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/base/json.h"

namespace ote_editor {

struct SchemaNode;
using NodePtr = std::shared_ptr<const SchemaNode>;

struct SchemaNode {
  enum class Kind { String, Int, Number, Bool, Enum, Ref, List, Object, Map, Json, Variant };
  Kind kind = Kind::Json;
  std::string help;  // one line, developer wording; lives in the schema file, never in C++
  bool required = false;
  bool block = false;   // Object that may be absent as a whole (shown with an Enable switch)
  bool expand = false;  // table: one column per member of this object / per id of this map
  std::optional<double> min, max;
  ote::Json deflt;   // value the engine assumes when the key is absent (Null = none stated)
  ote::Json sample;  // a valid value for the conformance test when the generated one would not do (Null = generate)
  std::string alt;                  // another JSON kind the loader also takes as is: "string", "number", "bool" or "null"
  std::vector<std::string> values;  // Enum words; for a Ref, extra words allowed beside the ids ("choose")
  std::vector<std::string> refs;    // Ref: id sources; Map: where the keys come from ("file" or "file#key.path")
  NodePtr of;                       // List items, Map values
  int length = -1;                  // List: fixed number of items (a pair such as [min, max])
  // List with "tuple": a fixed list whose items differ by position ([column, row, "sheet"]); `length` is its size and `of` stays
  // null. Everything that reads the item node of a list goes through itemNode().
  std::vector<NodePtr> tuple;
  std::vector<std::pair<std::string, NodePtr>> fields;  // Object, in display order

  // A field that applies only while a sibling key has one of some words (items: `heal` only when kind is usable or staff).
  struct When {
    std::string key;
    std::vector<std::string> values;
  };
  std::optional<When> when;
  // Variant: one of several shapes, picked by what the value looks like (the sub-languages of skills and events: a condition is
  // `{ "hp": {...} }` or `{ "near": ... }`; a skill grant is a plain id or `{ skill, filter }`).
  struct Variant {
    std::string name;       // shown in the picker
    std::string help;       // one line shown next to the name in the picker ("what this command does")
    std::string summary;    // one-line template of a collapsed list item: "{speaker}: {say}" (see schemaSummary)
    std::string matchKey;   // the object has this key ...
    std::vector<std::string> matchWith;    // ... and also all of these ("with") ...
    std::vector<std::string> matchUnless;  // ... and none of these ("unless"): setVariable with a number, or reading a unit
    std::string matchKind;  // ... or the value is of this JSON kind ("string", "number", "bool", "array", "object") ...
    int matchLength = -1;   // ... an "array" of exactly this many items (a cell [c, r] against [c, r, "sheet"]) ...
    std::string matchFirst; // ... and whose first item is of this JSON kind (one cell [0, 0] against a list of cells [[0, 0]])
    ote::Json matchValue;   // ... or equals this value exactly (matchIsValue)
    bool matchIsValue = false;
    NodePtr node;           // the shape (null for a constant `matchValue`)
    ote::Json sample;       // value written when the picker switches to it, or the conformance sample (Null = generate)
  };
  std::vector<Variant> variants;
  // A block that can only be switched on while some id lists are not empty (`rules.actionPoints` needs a resource).
  struct Needs {
    std::vector<std::string> refs;
    std::string hint;
  };
  std::optional<Needs> needs;
  ote::Json enable;     // exactly what a new value of this node starts with: a block's Enable switch, a new list item, a required
                        // list that must not start empty (Null = built from required keys and defaults)
  bool free = false;    // Map: any key (named sets, free-form numbers) instead of the ids of `ref`
  std::vector<std::string> excludes;  // setting this key removes these sibling keys (turns / until: one of the two)
  std::string summary;     // Object: one-line template of a collapsed list item ("{x},{y}"), see schemaSummary
  bool collapsed = false;  // List: the items start collapsed (a long command list: one line each)
  // A definition that contains itself (a command list inside the `if` command) is expanded twice when the schema is loaded; the
  // third level is this placeholder, expanded the first time somebody looks at it (`resolved()`), so scripts can nest as deep
  // as they like without the schema tree growing without end. Tools that enumerate keys skip such nodes.
  std::function<std::shared_ptr<const SchemaNode>()> lazy;
  mutable std::shared_ptr<const SchemaNode> lazyCache;
  const SchemaNode& resolved() const;

  const SchemaNode* field(const std::string& key) const;
  // The node of item `index` of a List: its `tuple` entry, else `of` (null when the position has none).
  const SchemaNode* itemNode(size_t index) const;
  bool isScalar() const;  // String, Int, Number, Bool, Enum, Ref
  static const char* kindName(Kind k);
};

struct FileSchema {
  std::string name;                 // from the file name
  std::vector<std::string> files;   // project-relative data files this schema describes
  NodePtr rows;                     // table files: one row
  NodePtr root;                     // other files: the whole document
  std::string title;                // tab title of the form page ("Form" when empty)
  std::string source;               // where it was loaded from (for messages)
};

// Resolves id sources ("data/units.json" or "game.json#damageTypes") to the ids they list right now.
using IdLookup = std::function<std::vector<std::string>(const std::vector<std::string>& sources)>;

class SchemaSet {
 public:
  using Text = std::pair<std::string, std::string>;  // file name ("units.json"), file text

  // Adds every *.json file of a folder; problems are collected in errors(), never fatal.
  void loadFolder(const std::string& dir);
  // Several schema files at once. Two passes: every file's "defs" (named nodes other nodes pull in with "use": "name" or
  // "file#name") must be known before any file's nodes ask for them. A file with only defs describes no data file.
  void addTexts(const std::vector<Text>& texts);
  void addFromText(const std::string& name, const std::string& text) { addTexts({{name, text}}); }
  const FileSchema* forFile(const std::string& rel) const;
  const std::vector<std::shared_ptr<const FileSchema>>& all() const { return schemas_; }
  const std::vector<std::string>& errors() const { return errors_; }

 private:
  void registerDefs(const std::string& name, const ote::Json& j);
  void build(const std::string& name, const ote::Json& j);
  std::shared_ptr<std::map<std::string, ote::Json>> defs_ = std::make_shared<std::map<std::string, ote::Json>>();
  std::vector<std::shared_ptr<const FileSchema>> schemas_;
  std::vector<std::string> errors_;
};

// --- values (schema_values.cpp) -------------------------------------------------------------------------------------
// Does `v` have the kind of value the node describes? On a mismatch `why` says what is wrong in one short phrase.
// Shallow: a list is "fine" when it is an array, its items are checked by their own nodes. Unknown ids of a Ref are not
// known here (the caller asks the project).
bool schemaAccepts(const SchemaNode& n, const ote::Json& v, std::string* why = nullptr);
// Is `v` the right KIND of value (text, whole number, list ...) for the node? Looser than schemaAccepts: a number outside its
// range or a word outside its enum still has the right kind.
bool typeOk(const SchemaNode& n, const ote::Json& v);
// True when `v` is not of the node's own kind but of its `alt` kind (turnOrder: "speed" instead of { "mode": "speed" }).
bool isAlt(const SchemaNode& n, const ote::Json& v);
// Index of the variant that describes `v`, or -1.
int variantIndex(const SchemaNode& n, const ote::Json& v);
// Does the field apply to `obj`, an object of `parent`? (Always true without a `when`.)
bool fieldApplies(const SchemaNode& parent, const SchemaNode& field, const ote::Json* obj);
// The value a fresh list item / new block starts with: required fields filled, ids from `ids`.
ote::Json schemaDefault(const SchemaNode& n, const IdLookup& ids);
// The one-line text of a variant value from its `summary` template: "{say}" is the value at that key, "{a.b}" a nested one,
// "{a|b}" the first of two that has a text, "[ ... ]" a part that is left out when a placeholder in it has no text.
// Texts are shortened, a one-key object such as { "unit": "gorm" } reads as "gorm", {} as "any unit", a list as its items.
std::string schemaSummary(const std::string& tmpl, const ote::Json& value);
// What the picker writes when the user switches a variant node to variant `v`.
ote::Json schemaVariantDefault(const SchemaNode::Variant& v, const IdLookup& ids);
// A valid value of variant `v` for the conformance test.
ote::Json schemaVariantSample(const SchemaNode::Variant& v, const IdLookup& ids);
// A valid value for the conformance test (the node's "sample" when given).
ote::Json schemaSample(const SchemaNode& n, const IdLookup& ids);

}  // namespace ote_editor
