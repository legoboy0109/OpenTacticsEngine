// The table editor tab: filter box, row buttons, a message strip for refused edits, and the table itself.
// Keyboard: Ctrl+C / Ctrl+V copy and paste a cell range as tab-separated text, Delete clears the selected cells,
// Enter or double-click on a nested cell opens the JSON dialog. Sorting and filtering are view-only.
#pragma once
#include <QWidget>
#include <functional>

#include "json_document.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;

namespace ote_native {

class JsonTableModel;
class Project;
class TableProxy;

class TableEditor : public QWidget {
  Q_OBJECT
 public:
  TableEditor(Project* project, JsonDocument* doc, JsonPath arrayPath, QWidget* parent = nullptr);

  QTableView* view() const { return view_; }
  JsonTableModel* model() const { return model_; }
  TableProxy* proxy() const { return proxy_; }
  JsonDocument* document() const { return doc_; }
  QLabel* messageLabel() const { return message_; }

  void addRow();
  void duplicateSelectedRows();
  void deleteSelectedRows();
  void moveSelectedRows(int delta);  // -1 up, +1 down; only while the view shows file order
  void clearSelectedCells();
  void setFilterText(const QString& text);

  // Selected cells as tab-separated text (the bounding rectangle; cells outside the selection are blank).
  QString selectionAsTsv() const;
  // Pastes tab-separated text with its top-left cell at the current cell. All or nothing; a refusal is shown in the strip.
  bool pasteTsv(const QString& text);
  void copySelection();
  void pasteFromClipboard();

  // Opens the nested JSON dialog for a cell (given in view coordinates). The test hook below replaces the modal dialog.
  void editNestedCell(const QModelIndex& viewIndex);
  using NestedEditorHook = std::function<bool(const QString& title, const ote::Json& initial, ote::Json* result)>;
  void setNestedEditorHook(NestedEditorHook hook) { nestedHook_ = std::move(hook); }

  void showMessage(const QString& text, bool isError);
  void autoSizeColumns();

 signals:
  void messageShown(const QString& text);

 protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;

 private:
  QList<int> selectedSourceRows() const;
  void updateRowButtons();

  JsonDocument* doc_;
  JsonTableModel* model_;
  TableProxy* proxy_;
  QTableView* view_;
  QLineEdit* filter_;
  QLabel* message_;
  QPushButton* upButton_ = nullptr;
  QPushButton* downButton_ = nullptr;
  NestedEditorHook nestedHook_;
};

}  // namespace ote_native
