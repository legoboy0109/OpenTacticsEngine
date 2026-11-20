// A table over a JSON array of objects: one row per element, one column per key (the union of keys in the order they
// first appear). It does not own data: it reads the document's tree directly and every edit is an undo command on the
// project's stack, so the table, the tree view and Undo always agree. Columns only ever grow while the model lives (a
// column does not vanish because the last row using it was deleted; undo would bring the key back anyway).
#pragma once
#include <QAbstractTableModel>
#include <QList>
#include <QPoint>
#include <vector>

#include "json_document.h"

namespace ote_native {

class Project;

struct CellEdit {
  int row = 0, col = 0;
  QString text;
};

class JsonTableModel : public QAbstractTableModel {
  Q_OBJECT
 public:
  // Data of the sort role: numbers as double, bools as int, text as QString, absent cells as an invalid QVariant.
  static constexpr int SortRole = Qt::UserRole + 1;

  JsonTableModel(Project* project, JsonDocument* doc, JsonPath arrayPath, QObject* parent = nullptr);

  int rowCount(const QModelIndex& parent = {}) const override;
  int columnCount(const QModelIndex& parent = {}) const override;
  QVariant data(const QModelIndex& index, int role) const override;
  bool setData(const QModelIndex& index, const QVariant& value, int role) override;
  QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
  Qt::ItemFlags flags(const QModelIndex& index) const override;

  const QString columnKey(int col) const { return QString::fromStdString(columns_[(size_t)col]); }
  // The cell's JSON value, nullptr when this row has no such key.
  const ote::Json* cellValue(int row, int col) const;
  bool isNestedCell(int row, int col) const;
  const ote::Json* rowValue(int row) const;

  // Replace a cell with an already-parsed value (the nested-JSON dialog uses this).
  void setCellValue(int row, int col, const ote::Json& value);
  // Validate every edit first; apply all as ONE undoable step, or none (returns false with `error` for the first bad one).
  bool applyTextEdits(const QList<CellEdit>& edits, QString* error);
  // Remove the keys of these cells from their rows (the Delete key).
  void clearCells(const QList<QPoint>& cells);  // x = column, y = row

  // Row operations, each one undo step. New rows get a default value per column (by the column's usual type).
  void addRow(int at);
  void duplicateRows(const QList<int>& rows);  // copies are inserted right after the last selected row
  void deleteRows(const QList<int>& rows);
  void moveRows(const QList<int>& rowsAscending, int delta);  // delta -1 up, +1 down; refuses to push past an end

  JsonDocument* document() const { return doc_; }
  const JsonPath& arrayPath() const { return arrayPath_; }

 signals:
  // A user edit was refused (wrong type for the cell, bad JSON ...): the UI shows this where the user can see it.
  void editRejected(const QString& message);

 private:
  const ote::Json* array() const;
  void rebuildColumns();
  bool growColumns();  // adds keys seen in the rows but not in columns_; true when something was added
  JsonPath rowPath(int row) const;
  void onAboutToChange(const JsonChange& c);
  void onChanged(const JsonChange& c);
  std::string uniqueId(const ote::Json& row, int afterRow) const;
  ote::Json::Type columnHint(int col) const { return hints_[(size_t)col]; }

  Project* project_;
  JsonDocument* doc_;
  JsonPath arrayPath_;
  std::vector<std::string> columns_;
  std::vector<ote::Json::Type> hints_;  // first non-null type seen in each column
  bool resetting_ = false;
};

}  // namespace ote_native
