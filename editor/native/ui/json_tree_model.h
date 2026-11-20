// A tree over any JSON document: object members and array items as rows with Key / Value / Type columns. Like the
// table model it reads the document's tree directly and edits through undo commands. It keeps a lazily built mirror of
// the nodes that were shown, so expanded branches stay expanded when something else in the file changes.
#pragma once
#include <QAbstractItemModel>
#include <memory>
#include <vector>

#include "json_document.h"

namespace ote_native {

class Project;

class JsonTreeModel : public QAbstractItemModel {
  Q_OBJECT
 public:
  enum Column { KeyColumn = 0, ValueColumn, TypeColumn, ColumnCount };

  JsonTreeModel(Project* project, JsonDocument* doc, QObject* parent = nullptr);
  ~JsonTreeModel() override;

  QModelIndex index(int row, int column, const QModelIndex& parent = {}) const override;
  QModelIndex parent(const QModelIndex& child) const override;
  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& parent = {}) const override { return parent.isValid() && parent.column() != 0 ? 0 : ColumnCount; }
  bool hasChildren(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  JsonPath pathOf(const QModelIndex& index) const;  // empty for the invisible root
  const ote::Json* jsonAt(const QModelIndex& index) const;
  QModelIndex indexForPath(const JsonPath& path) const;  // column 0; invalid when the path does not exist

  // Edits (each one undo step). `parent` is a container index (invalid = the document root).
  // Returns false and emits editRejected when the edit is refused.
  bool addMember(const QModelIndex& parent, const QString& key, const ote::Json& value);  // objects: appended
  bool addItem(const QModelIndex& parent, const ote::Json& value);                         // arrays: appended
  bool removeNode(const QModelIndex& index);
  bool renameKey(const QModelIndex& index, const QString& newKey);
  bool setValue(const QModelIndex& index, const ote::Json& value);  // replace a node by a value of any type

  static QString typeName(const ote::Json& j);

 signals:
  void editRejected(const QString& message);

 private:
  struct Node {
    Node* parent = nullptr;
    int row = 0;
    bool inArray = false;
    std::string key;  // objects
    std::vector<std::unique_ptr<Node>> kids;
    bool built = false;
  };

  Node* nodeFor(const QModelIndex& index) const { return index.isValid() ? static_cast<Node*>(index.internalPointer()) : root_.get(); }
  QModelIndex indexOf(Node* n, int column = 0) const;
  JsonPath pathOfNode(const Node* n) const;
  void ensureKids(Node* n) const;
  Node* findNode(const JsonPath& path) const;  // only through nodes that were built
  void renumber(Node* n, int from) const;
  void onAboutToChange(const JsonChange& c);
  void onChanged(const JsonChange& c);
  std::unique_ptr<Node> makeNode(Node* parent, const ote::Json& parentJson, int row) const;

  Project* project_;
  JsonDocument* doc_;
  std::unique_ptr<Node> root_;
  bool resetting_ = false;
};

}  // namespace ote_native
