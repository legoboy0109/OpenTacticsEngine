#include "problems_dock.h"

#include <QListWidget>

namespace ote_native {

namespace {
constexpr int kFileRole = Qt::UserRole + 1;
constexpr int kMessageRole = Qt::UserRole + 2;
}

ProblemsDock::ProblemsDock(QWidget* parent) : QDockWidget(tr("Problems"), parent) {
  setObjectName("problemsDock");  // QMainWindow::saveState needs a name per dock
  list_ = new QListWidget(this);
  list_->setObjectName("problemsList");
  list_->setWordWrap(true);
  list_->setUniformItemSizes(false);
  setWidget(list_);
  connect(list_, &QListWidget::itemActivated, this, [this](QListWidgetItem* it) {
    const QString file = it->data(kFileRole).toString();
    if (file.isEmpty()) return;
    emit openFileRequested(file);
    emit locateRequested(file, it->data(kMessageRole).toString());
  });
}

void ProblemsDock::setChecking() {
  list_->clear();
  list_->addItem(tr("Checking..."));
  list_->item(0)->setFlags(Qt::NoItemFlags);
  setWindowTitle(tr("Problems"));
}

void ProblemsDock::setProblems(const std::vector<ote_editor::Problem>& problems) {
  list_->clear();
  count_ = (int)problems.size();
  if (problems.empty()) {
    list_->addItem(tr("No problems found in the saved files."));
    list_->item(0)->setFlags(Qt::NoItemFlags);
    setWindowTitle(tr("Problems"));
    return;
  }
  for (const ote_editor::Problem& p : problems) {
    const QString file = QString::fromStdString(p.file);
    const QString msg = QString::fromStdString(p.message);
    auto* item = new QListWidgetItem(file.isEmpty() ? msg : QStringLiteral("%1: %2").arg(file, msg), list_);
    item->setData(kFileRole, file);
    item->setData(kMessageRole, msg);
    item->setToolTip(msg);
    if (file.isEmpty()) item->setFlags(item->flags() & ~Qt::ItemIsSelectable);
  }
  setWindowTitle(tr("Problems (%1)").arg(count_));
}

}  // namespace ote_native
