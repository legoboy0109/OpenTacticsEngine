#include "file_list_dock.h"

#include <QFont>
#include <QTreeWidget>

namespace ote_native {

namespace {
constexpr int kRelRole = Qt::UserRole + 1;
}

FileListDock::FileListDock(QWidget* parent) : QDockWidget(tr("Files"), parent) {
  setObjectName("fileListDock");
  tree_ = new QTreeWidget(this);
  tree_->setObjectName("fileTree");
  tree_->setHeaderHidden(true);
  setWidget(tree_);
  connect(tree_, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* it) {
    const QString rel = it->data(0, kRelRole).toString();
    if (!rel.isEmpty()) emit openFileRequested(rel);
  });
}

void FileListDock::setFiles(const QStringList& relPaths) {
  tree_->clear();
  items_.clear();
  QHash<QString, QTreeWidgetItem*> folders;
  for (const QString& rel : relPaths) {
    QTreeWidgetItem* parent = nullptr;
    const QStringList parts = rel.split('/');
    QString acc;
    for (int i = 0; i + 1 < parts.size(); ++i) {
      acc += (acc.isEmpty() ? "" : "/") + parts[i];
      QTreeWidgetItem*& f = folders[acc];
      if (!f) {
        f = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
        f->setText(0, parts[i] + "/");
        f->setFlags(Qt::ItemIsEnabled);
        f->setExpanded(true);
      }
      parent = f;
    }
    auto* item = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree_);
    item->setData(0, kRelRole, rel);
    items_.insert(rel, item);
    refreshText(rel);
  }
}

void FileListDock::refreshText(const QString& rel) {
  QTreeWidgetItem* it = items_.value(rel);
  if (!it) return;
  QString text = rel.section('/', -1);
  QStringList notes;
  if (dirty_.value(rel)) text += " *";
  if (external_.value(rel)) notes << tr("changed on disk");
  if (problems_.value(rel) > 0) notes << tr("%1 problem(s)").arg(problems_.value(rel));
  if (!notes.isEmpty()) text += "  (" + notes.join(", ") + ")";
  it->setText(0, text);
  it->setToolTip(0, rel);
  QFont f = it->font(0);
  f.setBold(dirty_.value(rel));
  it->setFont(0, f);
}

void FileListDock::setFileState(const QString& rel, bool dirty, bool changedOnDisk) {
  dirty_[rel] = dirty;
  external_[rel] = changedOnDisk;
  refreshText(rel);
}

void FileListDock::setProblemCounts(const QHash<QString, int>& countsByFile) {
  problems_ = countsByFile;
  for (auto it = items_.constBegin(); it != items_.constEnd(); ++it) refreshText(it.key());
}

void FileListDock::selectFile(const QString& rel) {
  if (QTreeWidgetItem* it = items_.value(rel)) tree_->setCurrentItem(it);
}

QString FileListDock::textOf(const QString& rel) const {
  QTreeWidgetItem* it = items_.value(rel);
  return it ? it->text(0) : QString();
}

}  // namespace ote_native
