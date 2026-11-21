// What every widget of a schema form needs: the document it edits, the id lists, and the ONE way a form changes the file.
// Each method is a single user action = a single undo step (several commands are wrapped in a macro). The form never
// edits the tree directly, so table, tree and form of the same document always agree.
#pragma once
#include <QObject>
#include <functional>

#include "id_source.h"
#include "json_document.h"
#include "schema.h"

namespace ote_native {

class Project;

class FormContext : public QObject {
  Q_OBJECT
 public:
  FormContext(Project* project, JsonDocument* doc, IdSource* ids, QObject* parent = nullptr);

  Project* project() const { return project_; }
  JsonDocument* doc() const { return doc_; }
  IdSource* ids() const { return ids_; }
  IdLookup lookup() const;
  // Ids of id sources: another file ("data/units.json", "game.json#stats") or, with a leading "@", a list of this very file.
  QStringList idsOf(const std::vector<std::string>& sources) const;

  // The node at `path` or nullptr (absent keys are nullptr, a stored null is a Null node).
  const ote::Json* at(const JsonPath& path) const;
  // Sets the value: replaces it, or inserts the key (creating the objects it lives in when they are absent).
  bool set(const JsonPath& path, const ote::Json& value, const QString& undoText = QString());
  // Like set(), for a field of a schema: keys the node `excludes` (turns / until: one of the two) are removed in the same
  // undo step.
  bool setField(const SchemaNode& node, const JsonPath& path, const ote::Json& value);
  // Removes the key or list item.
  bool erase(const JsonPath& path);
  // Appends an item to the list at `path`, creating the list when it is absent.
  bool appendItem(const JsonPath& list, const ote::Json& item);
  bool moveItem(const JsonPath& list, size_t from, size_t to);
  // Inserts `item` into the list at `index` (a duplicated command goes right after its original): one undo step.
  bool insertItem(const JsonPath& list, size_t index, const ote::Json& item);

  // The "which kind?" chooser of a list's Add button; tests replace it so no modal window ever opens. Returns the index into
  // `entries` (name, help), or -1 when the user cancels.
  using VariantPicker = std::function<int(const QString& title, const std::vector<std::pair<QString, QString>>& entries)>;
  void setVariantPicker(VariantPicker f) { variantPicker_ = std::move(f); }
  int pickVariant(const QString& title, const std::vector<std::pair<QString, QString>>& entries, QWidget* parent) const;

  // The JSON editing dialog; tests replace it so no modal window ever opens.
  using JsonEditor = std::function<bool(const QString& title, const ote::Json& initial, ote::Json* result)>;
  void setJsonEditor(JsonEditor f) { jsonEditor_ = std::move(f); }
  bool editJson(const QString& title, const ote::Json& initial, ote::Json* result, QWidget* parent) const;

 signals:
  // The edit could not be made (a parent that should be an object is something else); the form shows it.
  void refused(const QString& message);

 private:
  Project* project_;
  JsonDocument* doc_;
  IdSource* ids_;
  JsonEditor jsonEditor_;
  VariantPicker variantPicker_;
};

}  // namespace ote_native
