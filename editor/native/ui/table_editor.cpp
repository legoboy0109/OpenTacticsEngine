#include "table_editor.h"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QTableView>
#include <QVBoxLayout>
#include <algorithm>
#include <set>

#include <QRegularExpression>
#include <QSplitter>
#include <QToolButton>

#include "form_context.h"
#include "form_view.h"
#include "id_source.h"
#include "json_table_model.h"
#include "json_text_dialog.h"
#include "project.h"
#include "table_proxy.h"
#include "tsv.h"

namespace ote_native {

TableEditor::TableEditor(Project* project, JsonDocument* doc, JsonPath arrayPath, QWidget* parent, const FileSchema* schema,
                         IdSource* ids)
    : QWidget(parent), doc_(doc), schema_(schema && schema->rows ? schema : nullptr) {
  setObjectName("tableEditor");
  model_ = new JsonTableModel(project, doc, std::move(arrayPath), this);
  if (schema_) model_->setSchema(schema_->rows.get(), ids);
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
  filter_->setPlaceholderText(tr("Filter rows"));
  filter_->setToolTip(tr("Only changes what you see in the table, never the file"));
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
  // The table part lives in its own pane so a form can sit beside it; without a schema the pane is the whole editor.
  auto* pane = new QWidget(this);
  auto* layout = new QVBoxLayout(pane);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->addLayout(bar);
  layout->addWidget(message_);
  layout->addWidget(view_, 1);
  auto* root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);
  if (!schema_) {
    root->addWidget(pane);
  } else {
    formCtx_ = new FormContext(project, doc, ids, this);
    form_ = new FormView(formCtx_, schema_->rows.get(), this);
    formTitle_ = new QLabel(this);
    formTitle_->setObjectName("formTitle");
    QFont f = formTitle_->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.1);
    formTitle_->setFont(f);
    auto* formPane = new QWidget(this);
    formPane->setObjectName("formPane");
    auto* fl = new QVBoxLayout(formPane);
    fl->setContentsMargins(4, 4, 4, 4);
    // Title with previous / next row buttons: in the form-only view there is no table to pick a row from.
    auto* titleBar = new QHBoxLayout;
    prevRow_ = smallRowButton(QStringLiteral("‹"), tr("Previous row"), "formPrevRow");
    nextRow_ = smallRowButton(QStringLiteral("›"), tr("Next row"), "formNextRow");
    titleBar->addWidget(formTitle_, 1);
    titleBar->addWidget(prevRow_);
    titleBar->addWidget(nextRow_);
    fl->addLayout(titleBar);
    fl->addWidget(form_, 1);
    connect(prevRow_, &QToolButton::clicked, this, [this] { stepRow(-1); });
    connect(nextRow_, &QToolButton::clicked, this, [this] { stepRow(+1); });
    auto* split = new QSplitter(Qt::Horizontal, this);
    split->setObjectName("tableFormSplitter");
    split_ = split;
    formPane_ = formPane;
    tablePane_ = pane;
    split->setChildrenCollapsible(false);
    split->addWidget(pane);
    split->addWidget(formPane);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 5);
    split->setSizes({440, 520});
    pane->setMinimumWidth(260);
    formPane->setMinimumWidth(340);
    // View toggle: table | table + form | form. The choice is the caller's to remember (per kind of file).
    auto* modeBar = new QHBoxLayout;
    modeBar->setContentsMargins(4, 2, 4, 0);
    modeBar->addStretch(1);
    auto* group = new QButtonGroup(this);
    group->setExclusive(true);
    const QPair<QString, QString> modes[3] = {{tr("Table"), tr("Only the table")}, {tr("Table + form"), tr("Table with the form of the selected row")}, {tr("Form"), tr("Only the form of the selected row (use the arrows to change rows)")}};
    for (int i = 0; i < 3; ++i) {
      auto* b = new QToolButton(this);
      b->setObjectName(QStringLiteral("viewMode%1").arg(i));
      b->setText(modes[i].first);
      b->setToolTip(modes[i].second);
      b->setCheckable(true);
      b->setAutoRaise(false);
      group->addButton(b, i);
      modeBar->addWidget(b);
      modeButtons_[i] = b;
    }
    connect(group, &QButtonGroup::idClicked, this, [this](int id) { setViewMode(id); });
    root->addLayout(modeBar);
    root->addWidget(split);
    setViewMode(1);
    connect(form_, &FormView::showInTreeRequested, this, &TableEditor::showInTreeRequested);
    connect(view_->selectionModel(), &QItemSelectionModel::currentRowChanged, this, [this] { syncForm(); });
    // Rows added, removed, moved or the whole table reset: the row the form shows may now be another one.
    connect(model_, &QAbstractItemModel::rowsInserted, this, [this] { syncForm(); });
    connect(model_, &QAbstractItemModel::rowsRemoved, this, [this] { syncForm(); });
    connect(model_, &QAbstractItemModel::rowsMoved, this, [this] { syncForm(); });
    // A new column (the form just added a key no row had) resets the model, which would drop the selection and blank the
    // form under the user's hands: remember the current row across a reset.
    connect(model_, &QAbstractItemModel::modelAboutToBeReset, this, [this] { rowBeforeReset_ = currentSourceRow(); });
    connect(model_, &QAbstractItemModel::modelReset, this, [this] {
      if (rowBeforeReset_ >= 0 && rowBeforeReset_ < model_->rowCount()) {
        const QModelIndex pi = proxy_->mapFromSource(model_->index(rowBeforeReset_, 0));
        if (pi.isValid()) {
          view_->setCurrentIndex(pi);
          view_->selectRow(pi.row());
        }
      }
      rowBeforeReset_ = -1;
      syncForm();
    });
    syncForm();
  }

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

QToolButton* TableEditor::smallRowButton(const QString& text, const QString& tip, const char* name) {
  auto* b = new QToolButton(this);
  b->setObjectName(name);
  b->setText(text);
  b->setToolTip(tip);
  b->setAutoRaise(true);
  return b;
}

void TableEditor::setViewMode(int mode) {
  if (!split_ || mode < 0 || mode > 2) return;
  viewMode_ = mode;
  tablePane_->setVisible(mode != 2);
  formPane_->setVisible(mode != 0);
  for (int i = 0; i < 3; ++i) modeButtons_[i]->setChecked(i == mode);
  emit viewModeChanged(mode);
}

void TableEditor::stepRow(int delta) {
  const QModelIndex cur = view_->currentIndex();
  const int next = (cur.isValid() ? cur.row() : -1) + delta;
  if (next < 0 || next >= proxy_->rowCount()) return;
  view_->selectRow(next);
  view_->setCurrentIndex(proxy_->index(next, cur.isValid() ? cur.column() : 0));
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

int TableEditor::currentSourceRow() const {
  const QModelIndex cur = view_->currentIndex();
  return cur.isValid() ? proxy_->mapToSource(cur).row() : -1;
}

void TableEditor::syncForm() {
  if (!form_ || syncPending_) return;
  // Coalesced to the next event-loop turn: a paste or a delete of many rows signals once per row.
  syncPending_ = true;
  QTimer::singleShot(0, this, [this] {
    syncPending_ = false;
    applyFormTarget();
  });
}

void TableEditor::applyFormTarget() {
  const int row = currentSourceRow();
  if (row < 0 || row >= model_->rowCount()) {
    form_->clearTarget();
    formTitle_->setText(tr("No row selected"));
    return;
  }
  JsonPath p = model_->arrayPath();
  p.push_back(PathStep::at((size_t)row));
  form_->setTarget(p);
  const ote::Json* r = model_->rowValue(row);
  const QString id = r ? QString::fromStdString((*r)["id"].asString()) : QString();
  formTitle_->setText(id.isEmpty() ? tr("Row %1").arg(row + 1) : tr("Row %1: %2").arg(row + 1).arg(id));
}

void TableEditor::selectRow(int sourceRow) {
  if (sourceRow < 0 || sourceRow >= model_->rowCount()) return;
  QModelIndex pi = proxy_->mapFromSource(model_->index(sourceRow, 0));
  if (!pi.isValid()) {  // a filter hides it: show everything rather than fail
    filter_->clear();
    pi = proxy_->mapFromSource(model_->index(sourceRow, 0));
  }
  view_->setCurrentIndex(pi);
  view_->selectRow(pi.row());
  view_->scrollTo(pi);
  if (form_) applyFormTarget();
}

bool TableEditor::locateProblem(const QString& message) {
  // Loader messages quote the entry and the key: weapon "mortar": "apCost" ... or name it by position: technologies[2].
  QStringList quoted;
  static const QRegularExpression quoteRe("\"([^\"]+)\"");
  for (auto it = quoteRe.globalMatch(message); it.hasNext();) quoted << it.next().captured(1);
  int row = -1;
  static const QRegularExpression posRe("\\w+\\[(\\d+)\\]");
  if (auto m = posRe.match(message); m.hasMatch() && m.captured(1).toInt() < model_->rowCount()) row = m.captured(1).toInt();
  for (int r = 0; row < 0 && r < model_->rowCount(); ++r) {
    const ote::Json* rv = model_->rowValue(r);
    if (rv && (*rv)["id"].isString() && quoted.contains(QString::fromStdString((*rv)["id"].asString()))) row = r;
  }
  if (row < 0) return false;
  selectRow(row);
  // The field: the first quoted word that is a key the schema (or the table) knows.
  for (const QString& q : quoted) {
    if (form_ && form_->focusKey(q)) return true;
    const int col = model_->columnOfKey(q);
    if (col >= 0) {
      view_->setCurrentIndex(proxy_->mapFromSource(model_->index(row, col)));
      return true;
    }
  }
  return true;
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
