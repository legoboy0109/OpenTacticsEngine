// The table editor tab: filter box, row buttons, a message strip for refused edits, and the table itself.
// Keyboard: Ctrl+C / Ctrl+V copy and paste a cell range as tab-separated text, Delete clears the selected cells,
// Enter or double-click on a nested cell opens the JSON dialog. Sorting and filtering are view-only.
#pragma once
#include <QWidget>
#include <functional>

#include "json_document.h"
#include "schema.h"

class QLabel;
class QSplitter;
class QToolButton;
class QLineEdit;
class QPushButton;
class QTableView;

namespace ote_native {

class FormContext;
class FormView;
class IdSource;
class JsonTableModel;
class Project;
class TableProxy;

class TableEditor : public QWidget {
  Q_OBJECT
 public:
  // With a schema the table gets schema columns and a form for the selected row beside it; without one it is the
  // plain table of milestone 1.
  TableEditor(Project* project, JsonDocument* doc, JsonPath arrayPath, QWidget* parent = nullptr,
              const FileSchema* schema = nullptr, IdSource* ids = nullptr);

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

  // The three views of a table file with a schema: 0 table only, 1 table + form (default), 2 form only.
  void setViewMode(int mode);
  int viewMode() const { return viewMode_; }
  FormView* form() const { return form_; }
  FormContext* formContext() const { return formCtx_; }
  // Selects row `sourceRow` (file order) and shows it in the form.
  void selectRow(int sourceRow);
  int currentSourceRow() const;
  // Jumps to what a problem message names: the row (by its id or "name[3]" position) and the field. False when nothing
  // in the message could be placed (the file is still open).
  bool locateProblem(const QString& message);

 signals:
  void messageShown(const QString& text);
  void showInTreeRequested(const JsonPath& path);
  void viewModeChanged(int mode);

 protected:
  bool eventFilter(QObject* obj, QEvent* ev) override;

 private:
  QList<int> selectedSourceRows() const;
  void updateRowButtons();
  void stepRow(int delta);
  QToolButton* smallRowButton(const QString& text, const QString& tip, const char* name);
  void syncForm();         // deferred, coalesced
  void applyFormTarget();  // now

  JsonDocument* doc_;
  JsonTableModel* model_;
  TableProxy* proxy_;
  QTableView* view_;
  QLineEdit* filter_;
  QLabel* message_;
  QPushButton* upButton_ = nullptr;
  QPushButton* downButton_ = nullptr;
  NestedEditorHook nestedHook_;
  const FileSchema* schema_ = nullptr;
  FormContext* formCtx_ = nullptr;
  FormView* form_ = nullptr;
  QLabel* formTitle_ = nullptr;
  QSplitter* split_ = nullptr;
  QWidget* tablePane_ = nullptr;
  QWidget* formPane_ = nullptr;
  QToolButton* prevRow_ = nullptr;
  QToolButton* nextRow_ = nullptr;
  QToolButton* modeButtons_[3] = {nullptr, nullptr, nullptr};
  int viewMode_ = 1;
  bool syncPending_ = false;
  int rowBeforeReset_ = -1;
};

}  // namespace ote_native
