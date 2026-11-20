#include "problems_runner.h"

#include <QThread>

#include "validate.h"

namespace ote_native {

ProblemsRunner::ProblemsRunner(QObject* parent) : QObject(parent) {}

ProblemsRunner::~ProblemsRunner() {
  if (thread_) {
    thread_->wait();  // a check takes seconds at most; never leave a thread running against a dead object
    delete thread_;
  }
}

void ProblemsRunner::run(const QString& folder) {
  pendingFolder_ = folder;
  if (thread_) {
    rerun_ = true;
    return;
  }
  startThread();
}

void ProblemsRunner::startThread() {
  shared_ = std::make_shared<Shared>();
  shared_->folder = pendingFolder_;
  rerun_ = false;
  auto shared = shared_;
  thread_ = QThread::create([shared] {
    shared->result = ote_editor::validateProject(ote_editor::fs::path(shared->folder.toStdString()));
  });
  connect(thread_, &QThread::finished, this, &ProblemsRunner::onThreadDone);
  thread_->start();
  emit started();
}

void ProblemsRunner::onThreadDone() {
  thread_->deleteLater();
  thread_ = nullptr;
  checkedFolder_ = shared_->folder;
  problems_ = std::move(shared_->result);
  if (rerun_) {
    startThread();
    return;
  }
  emit finished();
}

}  // namespace ote_native
