#include "table_editor.h"

#include <QApplication>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

#include "json_table_model.h"
#include "json_text_dialog.h"
#include "project.h"
#include "table_proxy.h"
#include "tsv.h"

namespace ote_native {

TableEditor::TableEditor(Project* project, JsonDocument* doc, JsonPath arrayPath, QWidget* parent)
    : QWidget(parent), doc_(doc) {
  setObjectName("tableEditor");
  model_ = new JsonTableModel(project, doc, std::move(arrayPath), this);
  proxy_ = new TableProxy(this);
  proxy_->setSourceModel(model_);

  view_ = new QTableView(this);
  view_->setObjectName("tableView");
  view_->setModel(proxy_);
  view_->setSortingEnabled(true);
  view_->horizontalHeader()->setSortIndicatorClearable(true);
  view_->horizontalHeader()->setSortIndicator(-1, Qt::AscendingOrder);  // start in file order
  view_->setSelectionMode(QAbstractItemView::ExtendedSelection);
  view_->setAlternatingRowColors(true);
  view_->setWordWrap(false);
  view_->setTextElideMode(Qt::ElideRight);
  view_->horizontalHeader()->setStretchLastSection(false);
  view_->horizontalHeader()->setHighlightSections(false);
  // Fixed row height: with 5000 rows, asking every row for its size is the slow part of a table view.
  view_->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
  view_->verticalHeader()->setDefaultSectionSize(view_->fontMetrics().height() + 10);
  view_->installEventFilter(this);

  filter_ = new QLineEdit(this);
  filter_->setObjectName("tableFilter");
  filter_->setPlaceholderText(tr("Filter rows (only changes what you see, never the file)"));
  filter_->setClearButtonEnabled(true);

  auto button = [this](const QString& text, const QString& name, const QString& tip) {
    auto* b = new QPushButton(text, this);
    b->setObjectName(name);
    b->setToolTip(tip);
    return b;
  };
  auto* addB = button(tr("Add row"), "addRowButton", tr("Add a row after the selected one"));
  auto* dupB = button(tr("Duplicate"), "duplicateRowButton", tr("Copy the selected rows"));
  auto* delB = button(tr("Delete"), "deleteRowButton", tr("Delete the selected rows"));
  upButton_ = button(tr("Move up"), "moveUpButton", tr("Move the selected rows up in the file"));
  downButton_ = button(tr("Move down"), "moveDownButton", tr("Move the selected rows down in the file"));

  message_ = new QLabel(this);
  message_->setObjectName("tableMessage");
  message_->setWordWrap(true);
  message_->hide();

  auto* bar = new QHBoxLayout;
  bar->addWidget(filter_, 1);
  for (QPushButton* b : {addB, dupB, delB, upButton_, downButton_}) bar->addWidget(b);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->addLayout(bar);
  layout->addWidget(message_);
  layout->addWidget(view_, 1);

  connect(addB, &QPushButton::clicked, this, &TableEditor::addRow);
  connect(dupB, &QPushButton::clicked, this, &TableEditor::duplicateSelectedRows);
  connect(delB, &QPushButton::clicked, this, &TableEditor::deleteSelectedRows);
  connect(upButton_, &QPushButton::clicked, this, [this] { moveSelectedRows(-1); });
  connect(downButton_, &QPushButton::clicked, this, [this] { moveSelectedRows(+1); });
  connect(filter_, &QLineEdit::textChanged, this, &TableEditor::setFilterText);
  connect(model_, &JsonTableModel::editRejected, this, [this](const QString& m) { showMessage(m, true); });
  connect(view_, &QTableView::doubleClicked, this, &TableEditor::editNestedCell);
  connect(view_->horizontalHeader(), &QHeaderView::sortIndicatorChanged, this, [this] { updateRowButtons(); });
  connect(model_, &QAbstractItemModel::modelReset, this, &TableEditor::autoSizeColumns);
  connect(model_, &QAbstractItemModel::columnsInserted, this, &TableEditor::autoSizeColumns);
  autoSizeColumns();
  updateRowButtons();
}

void TableEditor::autoSizeColumns() {
  // Width from the header and the first rows only: measuring all 5000 would cost more than showing them.
  const QFontMetrics fm = view_->fontMetrics();
  const int rows = std::min(model_->rowCount(), 200);
  for (int c = 0; c < model_->columnCount(); ++c) {
    int w = fm.horizontalAdvance(model_->columnKey(c)) + 36;  // room for the sort arrow
    for (int r = 0; r < rows; ++r)
      w = std::max(w, fm.horizontalAdvance(model_->index(r, c).data().toString()) + 20);
    view_->setColumnWidth(c, std::clamp(w, 60, 320));
  }
}

void TableEditor::showMessage(const QString& text, bool isError) {
  message_->setText(text);
  message_->setVisible(!text.isEmpty());
  // Fixed red for errors reads on light and dark themes; ordinary notes use the theme's own text colour.
  message_->setStyleSheet(isError ? "color: #e5534b; font-weight: bold;" : QString());
  emit messageShown(text);
}

void TableEditor::setFilterText(const QString& text) {
  proxy_->setFilterText(text);
  updateRowButtons();
}

void TableEditor::updateRowButtons() {
  const bool fileOrder = !proxy_->isViewOnlyOrder();
  for (QPushButton* b : {upButton_, downButton_}) {
    b->setEnabled(fileOrder);
    b->setToolTip(fileOrder ? (b == upButton_ ? tr("Move the selected rows up in the file") : tr("Move the selected rows down in the file"))
                            : tr("Clear the sort and the filter first: moving rows changes the order in the file"));
  }
}

QList<int> TableEditor::selectedSourceRows() const {
  std::set<int> rows;
  for (const QModelIndex& i : view_->selectionModel()->selectedIndexes()) rows.insert(proxy_->mapToSource(i).row());
  return QList<int>(rows.begin(), rows.end());
}

void TableEditor::addRow() {
  const QList<int> sel = selectedSourceRows();
  model_->addRow(sel.isEmpty() ? model_->rowCount() : sel.last() + 1);
  showMessage(QString(), false);
}

void TableEditor::duplicateSelectedRows() { model_->duplicateRows(selectedSourceRows()); }
void TableEditor::deleteSelectedRows() { model_->deleteRows(selectedSourceRows()); }

void TableEditor::moveSelectedRows(int delta) {
  if (proxy_->isViewOnlyOrder()) return;
  model_->moveRows(selectedSourceRows(), delta);
}

void TableEditor::clearSelectedCells() {
  QList<QPoint> cells;
  for (const QModelIndex& i : view_->selectionModel()->selectedIndexes()) {
    const QModelIndex s = proxy_->mapToSource(i);
    cells << QPoint(s.column(), s.row());
  }
  model_->clearCells(cells);
}

QString TableEditor::selectionAsTsv() const {
  const QModelIndexList sel = view_->selectionModel()->selectedIndexes();
  if (sel.isEmpty()) return {};
  int r0 = INT_MAX, r1 = -1, c0 = INT_MAX, c1 = -1;
  for (const QModelIndex& i : sel) {
    r0 = std::min(r0, i.row()); r1 = std::max(r1, i.row());
    c0 = std::min(c0, i.column()); c1 = std::max(c1, i.column());
  }
  QList<QStringList> rows;
  for (int r = r0; r <= r1; ++r) {
    QStringList cells;
    for (int c = c0; c <= c1; ++c) {
      const QModelIndex i = proxy_->index(r, c);
      cells << (view_->selectionModel()->isSelected(i) ? i.data(Qt::DisplayRole).toString() : QString());
    }
    rows << cells;
  }
  return toTsv(rows);
}

bool TableEditor::pasteTsv(const QString& text) {
  const QList<QStringList> grid = parseTsv(text);
  if (grid.isEmpty()) return false;
  QModelIndex start = view_->currentIndex();
  if (!start.isValid()) {
    const QModelIndexList sel = view_->selectionModel()->selectedIndexes();
    if (sel.isEmpty()) { showMessage(tr("Click a cell first: the pasted text starts there."), true); return false; }
    start = sel.first();
  }
  QList<CellEdit> edits;
  int skipped = 0;
  for (int i = 0; i < grid.size(); ++i)
    for (int j = 0; j < grid[i].size(); ++j) {
      const int pr = start.row() + i, pc = start.column() + j;
      if (pr >= proxy_->rowCount() || pc >= proxy_->columnCount()) { ++skipped; continue; }
      const QModelIndex s = proxy_->mapToSource(proxy_->index(pr, pc));
      edits.append({s.row(), s.column(), grid[i][j]});
    }
  QString err;
  if (!model_->applyTextEdits(edits, &err)) {
    showMessage(tr("Nothing was pasted. %1").arg(err), true);
    return false;
  }
  showMessage(skipped ? tr("Pasted %1 cells; %2 did not fit in the table and were left out.").arg(edits.size()).arg(skipped)
                      : tr("Pasted %1 cells.").arg(edits.size()), false);
  return true;
}

void TableEditor::copySelection() { QApplication::clipboard()->setText(selectionAsTsv()); }
void TableEditor::pasteFromClipboard() { pasteTsv(QApplication::clipboard()->text()); }

void TableEditor::editNestedCell(const QModelIndex& viewIndex) {
  const QModelIndex s = proxy_->mapToSource(viewIndex);
  if (!s.isValid() || !model_->isNestedCell(s.row(), s.column())) return;
  const ote::Json& current = *model_->cellValue(s.row(), s.column());
  const QString title = tr("Edit %1 of row %2").arg(model_->columnKey(s.column())).arg(s.row() + 1);
  ote::Json result;
  bool accepted;
  if (nestedHook_) {
    accepted = nestedHook_(title, current, &result);
  } else {
    JsonTextDialog dlg(title, current, this);
    accepted = dlg.exec() == QDialog::Accepted;
    if (accepted) result = dlg.value();
  }
  if (accepted) model_->setCellValue(s.row(), s.column(), result);
}

bool TableEditor::eventFilter(QObject* obj, QEvent* ev) {
  if (obj == view_ && ev->type() == QEvent::KeyPress) {
    auto* k = static_cast<QKeyEvent*>(ev);
    if (k->matches(QKeySequence::Copy)) { copySelection(); return true; }
    if (k->matches(QKeySequence::Paste)) { pasteFromClipboard(); return true; }
    if (k->key() == Qt::Key_Delete) { clearSelectedCells(); return true; }
    if ((k->key() == Qt::Key_Return || k->key() == Qt::Key_Enter || k->key() == Qt::Key_F2) &&
        view_->currentIndex().isValid() &&
        model_->isNestedCell(proxy_->mapToSource(view_->currentIndex()).row(), proxy_->mapToSource(view_->currentIndex()).column())) {
      editNestedCell(view_->currentIndex());
      return true;
    }
  }
  return QWidget::eventFilter(obj, ev);
}

}  // namespace ote_native
