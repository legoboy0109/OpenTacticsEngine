// The schema of one data file: which keys exist, their types, ranges, enums and which ids they point at. Schemas are
// hand-written JSON files (editor/schema/*.json, format in docs/NATIVE_EDITOR.md) that drive the forms and the table
// columns. They never decide what is allowed in a file: the engine's loader does. A schema that disagrees with the loader
// is caught by native_test_conformance.
#pragma once
#include <QString>
#include <QStringList>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "core/base/json.h"

namespace ote_native {

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
  std::string alt;                  // another JSON kind the loader also takes as is: "string", "number" or "bool"
  std::vector<std::string> values;  // Enum words; for a Ref, extra words allowed beside the ids ("choose")
  std::vector<std::string> refs;    // Ref: id sources; Map: where the keys come from ("file" or "file#key.path")
  NodePtr of;                       // List items, Map values
  int length = -1;                  // List: fixed number of items (a pair such as [min, max])
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
    std::string matchKey;   // the object has this key ...
    std::string matchKind;  // ... or the value is of this JSON kind ("string", "number", "bool", "array", "object") ...
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
  ote::Json enable;     // block: exactly what the Enable switch writes (Null = built from required keys and defaults)
  bool free = false;    // Map: any key (named sets, free-form numbers) instead of the ids of `ref`

  const SchemaNode* field(const std::string& key) const;
  bool isScalar() const;  // String, Int, Number, Bool, Enum, Ref
  static const char* kindName(Kind k);
};

struct FileSchema {
  std::string name;                 // from the file name
  std::vector<std::string> files;   // project-relative data files this schema describes
  NodePtr rows;                     // table files: one row
  NodePtr root;                     // other files: the whole document
  std::string title;                // tab title of the form page ("Form" when empty)
  QString source;                   // where it was loaded from (for messages)
};

// Resolves id sources ("data/units.json" or "game.json#damageTypes") to the ids they list right now.
using IdLookup = std::function<QStringList(const std::vector<std::string>& sources)>;

class SchemaSet {
 public:
  // Adds every *.json file of a folder (or of a Qt resource folder such as ":/schema"); problems are collected, never fatal.
  void loadFolder(const QString& dir);
  // A schema file may also hold "defs" (named nodes other nodes pull in with "use": "name" or "file#name"); a file with only
  // defs describes no data file.
  void addFromText(const QString& name, const QByteArray& text);
  const FileSchema* forFile(const QString& rel) const;
  const std::vector<std::shared_ptr<const FileSchema>>& all() const { return schemas_; }
  const QStringList& errors() const { return errors_; }

  // The folder the program reads: `overrideDir` (a folder the user named) > "<app dir>/schema" > the copy inside the program.
  static SchemaSet loadDefault(const QString& overrideDir, const QString& appDir);

 private:
  void registerDefs(const QString& name, const ote::Json& j);
  void build(const QString& name, const ote::Json& j);
  std::map<std::string, ote::Json> defs_;
  std::vector<std::shared_ptr<const FileSchema>> schemas_;
  QStringList errors_;
};

// --- values -----------------------------------------------------------------------------------------------------
// Does `v` have the kind of value the node describes? On a mismatch `why` says what is wrong in one short phrase.
// Shallow: a list is "fine" when it is an array, its items are checked by their own nodes. Unknown ids of a Ref are not
// known here (the UI asks the project).
bool nodeAccepts(const SchemaNode& n, const ote::Json& v, QString* why = nullptr);
// Is `v` the right KIND of value (text, whole number, list ...) for the node? Looser than nodeAccepts: a number outside its
// range or a word outside its enum still has the right kind. A value of the wrong kind is shown as it is, with a warning.
bool typeOk(const SchemaNode& n, const ote::Json& v);
// True when `v` is not of the node's own kind but of its `alt` kind (turnOrder: "speed" instead of { "mode": "speed" }).
// The form shows such a value as JSON without a warning.
bool isAlt(const SchemaNode& n, const ote::Json& v);
// Index of the variant that describes `v`, or -1 (the form then shows `v` as raw JSON with a warning).
int variantIndex(const SchemaNode& n, const ote::Json& v);
// Does the field apply to `obj`, an object of `parent`? (Always true without a `when`.)
bool fieldApplies(const SchemaNode& parent, const SchemaNode& field, const ote::Json* obj);
// The value a fresh list item / new block starts with: required fields filled, ids from `ids`.
ote::Json defaultValue(const SchemaNode& n, const IdLookup& ids);
// What the picker writes when the user switches a variant node to variant `v`.
ote::Json defaultValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids);
// A valid value of variant `v` for the conformance test.
ote::Json sampleValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids);
// A valid value for the conformance test (the node's "sample" when given).
ote::Json sampleValue(const SchemaNode& n, const IdLookup& ids);

}  // namespace ote_native
