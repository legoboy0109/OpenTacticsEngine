#include "project.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QSaveFile>
#include <algorithm>

namespace ote_native {

Project::Project(const QString& folder, QObject* parent) : QObject(parent), folder_(QDir(folder).absolutePath()) {
  // Edits that arrive in a burst (an editor writing a temp file, renaming, touching) are looked at once.
  debounce_.setSingleShot(true);
  debounce_.setInterval(150);
  connect(&debounce_, &QTimer::timeout, this, &Project::flushWatcherEvents);
  connect(&watcher_, &QFileSystemWatcher::fileChanged, this, [this](const QString& p) { pendingPaths_.insert(p); debounce_.start(); });
  connect(&watcher_, &QFileSystemWatcher::directoryChanged, this, [this](const QString& p) { pendingPaths_.insert(p); debounce_.start(); });
  rescanFiles();
}

QString Project::absolutePath(const QString& rel) const { return QDir(folder_).filePath(rel); }

void Project::rescanFiles() {
  QStringList found;
  QDir root(folder_);
  QDirIterator it(folder_, {QStringLiteral("*.json")}, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) found << root.relativeFilePath(it.next());
  found.sort();
  const bool changed = found != files_;
  files_ = found;
  watchAll();
  if (changed) emit fileListChanged();
}

void Project::watchAll() {
  // The folder and its sub-folders catch files appearing and vanishing; each file catches content changes. An atomic
  // replace (temp + rename, as our own save and most editors do) drops the file watch, so this is repeated after every event.
  QStringList want;
  QDirIterator dirs(folder_, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
  want << folder_;
  while (dirs.hasNext()) want << dirs.next();
  for (const QString& rel : files_) want << absolutePath(rel);
  QStringList have = watcher_.files() + watcher_.directories();
  QStringList add;
  for (const QString& w : want)
    if (!have.contains(w)) add << w;
  if (!add.isEmpty()) watcher_.addPaths(add);
}

JsonDocument* Project::document(const QString& rel, QString* error) {
  if (JsonDocument* d = docs_.value(rel, nullptr)) return d;
  auto* doc = new JsonDocument(rel, absolutePath(rel));
  if (!doc->load(error)) {
    delete doc;
    return nullptr;
  }
  doc->setParent(this);
  docs_.insert(rel, doc);
  connect(doc, &JsonDocument::stateChanged, this, [this, rel] { emit fileStateChanged(rel); });
  return doc;
}

void Project::push(JsonDocument* doc, QUndoCommand* cmd) {
  doc->forgetUnreachableSavedState();
  stack_.push(cmd);
}

QStringList Project::dirtyFiles() const {
  QStringList out;
  for (auto it = docs_.constBegin(); it != docs_.constEnd(); ++it)
    if (it.value()->isDirty()) out << it.key();
  out.sort();
  return out;
}

SaveResult Project::save(const QString& rel, bool overwrite) {
  JsonDocument* doc = docs_.value(rel, nullptr);
  if (!doc) return {SaveResult::Status::Failed, QStringLiteral("%1 is not open").arg(rel)};
  if (!doc->isDirty() && !overwrite) return {SaveResult::Status::Unchanged, QString()};

  if (!overwrite) {
    // The file must still be the one we read. (Compared by content, not by project revision: a change to some other
    // file of the game is none of this file's business.)
    QFile disk(doc->absPath());
    if (!disk.exists())
      return {SaveResult::Status::Conflict, QStringLiteral("%1 was deleted on disk since it was loaded").arg(rel)};
    if (disk.open(QIODevice::ReadOnly)) {
      if (JsonDocument::hashOf(disk.readAll()) != doc->diskHash())
        return {SaveResult::Status::Conflict, QStringLiteral("%1 was changed on disk since it was loaded").arg(rel)};
      disk.close();
    }
  }

  const QByteArray bytes = doc->serialize();
  // QSaveFile writes to a temporary file next to the target and renames it over the target on commit(): readers never
  // see a half-written file, and a failed write leaves the old file untouched.
  QSaveFile out(doc->absPath());
  if (!out.open(QIODevice::WriteOnly) || out.write(bytes) != bytes.size() || !out.commit())
    return {SaveResult::Status::Failed, QStringLiteral("%1: cannot write (%2)").arg(rel, out.errorString())};
  doc->markSaved(bytes);
  watchAll();
  emit saved(rel);
  return {SaveResult::Status::Saved, QString()};
}

bool Project::reload(const QString& rel, QString* error) {
  JsonDocument* doc = docs_.value(rel, nullptr);
  if (!doc) return document(rel, error) != nullptr;
  if (!doc->load(error)) return false;
  emit documentReloaded(rel);
  return true;
}

void Project::checkExternalChange(const QString& rel) {
  JsonDocument* doc = docs_.value(rel, nullptr);
  if (!doc) return;
  QFile f(doc->absPath());
  bool differs = true;
  if (f.open(QIODevice::ReadOnly)) differs = JsonDocument::hashOf(f.readAll()) != doc->diskHash();
  doc->setExternallyChanged(differs);
}

void Project::flushWatcherEvents() {
  const QSet<QString> paths = pendingPaths_;
  pendingPaths_.clear();
  rescanFiles();
  for (const QString& p : paths) {
    const QString rel = QDir(folder_).relativeFilePath(p);
    checkExternalChange(rel);
  }
  watchAll();
}

}  // namespace ote_native
