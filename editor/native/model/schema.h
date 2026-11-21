// The schema of one data file, Qt face. The schema engine itself (types, loader, value checks, defaults, summaries) lives in
// editor/core/schema_engine.h, shared with ote-mcp; this header re-exports its types into ote_native and adds the QString-typed
// entry points the forms use. Format of the schema files: docs/NATIVE_EDITOR.md.
#pragma once
#include <QByteArray>
#include <QString>
#include <QStringList>
#include <functional>
#include <string>
#include <vector>

#include "core/base/json.h"
#include "schema_engine.h"  // editor/core

namespace ote_native {

using ote_editor::FileSchema;
using ote_editor::NodePtr;
using ote_editor::SchemaNode;
using ote_editor::fieldApplies;
using ote_editor::isAlt;
using ote_editor::typeOk;
using ote_editor::variantIndex;

// Resolves id sources ("data/units.json" or "game.json#damageTypes") to the ids they list right now.
using IdLookup = std::function<QStringList(const std::vector<std::string>& sources)>;

class SchemaSet {
 public:
  // Adds every *.json file of a folder (or of a Qt resource folder such as ":/schema"); problems are collected, never fatal.
  void loadFolder(const QString& dir);
  // A schema file may also hold "defs" (named nodes other nodes pull in with "use": "name" or "file#name"); a file with only
  // defs describes no data file.
  void addFromText(const QString& name, const QByteArray& text);
  const FileSchema* forFile(const QString& rel) const { return core_.forFile(rel.toStdString()); }
  const std::vector<std::shared_ptr<const FileSchema>>& all() const { return core_.all(); }
  QStringList errors() const;

  // The folder the program reads: `overrideDir` (a folder the user named) > "<app dir>/schema" > the copy inside the program.
  static SchemaSet loadDefault(const QString& overrideDir, const QString& appDir);

 private:
  ote_editor::SchemaSet core_;
  QStringList readErrors_;  // resource files that could not be read
};

// Does `v` have the kind of value the node describes? On a mismatch `why` says what is wrong in one short phrase.
bool nodeAccepts(const SchemaNode& n, const ote::Json& v, QString* why = nullptr);
// The value a fresh list item / new block starts with: required fields filled, ids from `ids`.
ote::Json defaultValue(const SchemaNode& n, const IdLookup& ids);
// The one-line text of a variant value from its `summary` template (ote_editor::schemaSummary).
QString summaryText(const QString& tmpl, const ote::Json& value);
// What the picker writes when the user switches a variant node to variant `v`.
ote::Json defaultValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids);
// A valid value of variant `v` for the conformance test.
ote::Json sampleValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids);
// A valid value for the conformance test (the node's "sample" when given).
ote::Json sampleValue(const SchemaNode& n, const IdLookup& ids);

}  // namespace ote_native
