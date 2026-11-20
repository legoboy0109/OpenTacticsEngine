#include "tree_editor.h"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QToolButton>
#include <QTreeView>
#include <QVBoxLayout>

#include "cell_text.h"
#include "json_tree_model.h"
#include "project.h"

namespace ote_native {

namespace {
struct TypeChoice { const char* label; ote::Json::Type type; };
const TypeChoice kTypes[] = {
    {"Text", ote::Json::Type::String}, {"Number", ote::Json::Type::Number}, {"Yes/No", ote::Json::Type::Bool},
    {"Object { }", ote::Json::Type::Object}, {"List [ ]", ote::Json::Type::Array}, {"Empty (null)", ote::Json::Type::Null}};
}  // namespace

TreeEditor::TreeEditor(Project* project, JsonDocument* doc, QWidget* parent) : QWidget(parent), doc_(doc) {
  setObjectName("treeEditor");
  model_ = new JsonTreeModel(project, doc, this);
  view_ = new QTreeView(this);
  view_->setObjectName("treeView");
  view_->setModel(model_);
  view_->setAlternatingRowColors(true);
  view_->setUniformRowHeights(true);
  view_->setContextMenuPolicy(Qt::CustomContextMenu);
  view_->setSelectionBehavior(QAbstractItemView::SelectRows);
  view_->header()->setStretchLastSection(false);
  view_->header()->setSectionResizeMode(JsonTreeModel::KeyColumn, QHeaderView::Interactive);
  view_->header()->setSectionResizeMode(JsonTreeModel::ValueColumn, QHeaderView::Stretch);
  view_->header()->setSectionResizeMode(JsonTreeModel::TypeColumn, QHeaderView::ResizeToContents);
  view_->setColumnWidth(JsonTreeModel::KeyColumn, 260);
  view_->expandToDepth(0);

  auto* addButton = new QToolButton(this);
  addButton->setObjectName("addChildButton");
  addButton->setText(tr("Add"));
  addButton->setPopupMode(QToolButton::InstantPopup);
  addButton->setToolTip(tr("Add a value inside the selected object or list"));
  auto* addMenu = new QMenu(addButton);
  for (const TypeChoice& t : kTypes) {
    const ote::Json::Type type = t.type;
    addMenu->addAction(tr(t.label), this, [this, type] { addChild(type); });
  }
  addButton->setMenu(addMenu);
  auto* delButton = new QPushButton(tr("Delete"), this);
  delButton->setObjectName("removeNodeButton");
  auto* expandButton = new QPushButton(tr("Expand all"), this);
  auto* collapseButton = new QPushButton(tr("Collapse all"), this);

  message_ = new QLabel(this);
  message_->setObjectName("treeMessage");
  message_->setWordWrap(true);
  message_->hide();

  auto* bar = new QHBoxLayout;
  bar->addWidget(addButton);
  bar->addWidget(delButton);
  bar->addStretch(1);
  bar->addWidget(expandButton);
  bar->addWidget(collapseButton);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(4, 4, 4, 4);
  layout->addLayout(bar);
  layout->addWidget(message_);
  layout->addWidget(view_, 1);

  connect(delButton, &QPushButton::clicked, this, [this] { removeCurrent(); });
  connect(expandButton, &QPushButton::clicked, view_, &QTreeView::expandAll);
  connect(collapseButton, &QPushButton::clicked, view_, &QTreeView::collapseAll);
  connect(view_, &QTreeView::customContextMenuRequested, this, &TreeEditor::showContextMenu);
  connect(model_, &JsonTreeModel::editRejected, this, [this](const QString& m) { showMessage(m, true); });
}

void TreeEditor::showMessage(const QString& text, bool isError) {
  message_->setText(text);
  message_->setVisible(!text.isEmpty());
  message_->setStyleSheet(isError ? "color: #e5534b; font-weight: bold;" : QString());
  emit messageShown(text);
}

QModelIndex TreeEditor::containerForAdd() const {
  QModelIndex cur = view_->currentIndex();
  if (!cur.isValid()) return {};  // the document root
  const ote::Json* j = model_->jsonAt(cur);
  if (j && (j->isObject() || j->isArray())) return cur.siblingAtColumn(0);
  return model_->parent(cur);  // beside a scalar
}

bool TreeEditor::addChild(ote::Json::Type type) {
  const QModelIndex container = containerForAdd();
  const ote::Json* c = model_->jsonAt(container);
  if (!c) return false;
  const ote::Json value = defaultValueFor(type);
  showMessage(QString(), false);
  bool ok;
  if (c->isObject()) {
    const QString title = tr("Name of the new key in %1").arg(pathToString(model_->pathOf(container)));
    const QString key = keyPrompt_ ? keyPrompt_(title)
                                   : QInputDialog::getText(this, tr("Add key"), title, QLineEdit::Normal, QString(), nullptr);
    if (key.isEmpty()) return false;
    ok = model_->addMember(container, key, value);
  } else {
    ok = model_->addItem(container, value);
  }
  if (ok && container.isValid()) view_->expand(container);
  return ok;
}

bool TreeEditor::removeCurrent() {
  const QModelIndex cur = view_->currentIndex();
  if (!cur.isValid()) return false;
  showMessage(QString(), false);
  return model_->removeNode(cur.siblingAtColumn(0));
}

bool TreeEditor::changeCurrentType(ote::Json::Type type) {
  const QModelIndex cur = view_->currentIndex();
  if (!cur.isValid()) return false;
  return model_->setValue(cur.siblingAtColumn(0), defaultValueFor(type));
}

void TreeEditor::selectPath(const JsonPath& path) {
  const QModelIndex i = model_->indexForPath(path);
  if (!i.isValid()) return;
  for (QModelIndex p = model_->parent(i); p.isValid(); p = model_->parent(p)) view_->expand(p);
  view_->setCurrentIndex(i);
  view_->scrollTo(i);
}

void TreeEditor::showContextMenu(const QPoint& pos) {
  const QModelIndex i = view_->indexAt(pos);
  if (i.isValid()) view_->setCurrentIndex(i.siblingAtColumn(0));
  QMenu menu(this);
  QMenu* add = menu.addMenu(tr("Add"));
  QMenu* type = menu.addMenu(tr("Change type to"));
  for (const TypeChoice& t : kTypes) {
    const ote::Json::Type tt = t.type;
    add->addAction(tr(t.label), this, [this, tt] { addChild(tt); });
    type->addAction(tr(t.label), this, [this, tt] { changeCurrentType(tt); });
  }
  type->setEnabled(i.isValid());
  QAction* del = menu.addAction(tr("Delete"), this, [this] { removeCurrent(); });
  del->setEnabled(i.isValid());
  menu.exec(view_->viewport()->mapToGlobal(pos));
}

}  // namespace ote_native
