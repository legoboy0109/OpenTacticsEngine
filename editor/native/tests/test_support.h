// Helpers shared by the native editor's tests: scratch folders under the build's test-tmp (never the source tree),
// copies of the demo game, small file read/write helpers, and the settings file the UI tests point QSettings at.
#pragma once
#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QString>
#include <QTemporaryDir>

#include "core/base/json.h"

namespace ote_test {

// A fresh folder below $OTE_TEST_TMP (or the system temp folder when run by hand); removed when the object dies.
class Scratch {
 public:
  Scratch() : dir_(baseDir() + "/native-XXXXXX") {}
  bool ok() const { return dir_.isValid(); }
  QString path() const { return dir_.path(); }
  QString file(const QString& rel) const { return QDir(dir_.path()).filePath(rel); }
  void keep() { dir_.setAutoRemove(false); }

 private:
  static QString baseDir() {
    QString b = qEnvironmentVariable("OTE_TEST_TMP");
    if (b.isEmpty()) b = QDir::tempPath();
    QDir().mkpath(b);
    return b;
  }
  QTemporaryDir dir_;
};

inline void writeFile(const QString& path, const QByteArray& bytes) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile f(path);
  if (f.open(QIODevice::WriteOnly)) f.write(bytes);
}

inline QByteArray readFile(const QString& path) {
  QFile f(path);
  return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}

// Copies a folder tree (a game) without following the original afterwards.
inline bool copyTree(const QString& from, const QString& to) {
  QDir src(from);
  QDir().mkpath(to);
  for (const QFileInfo& e : src.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
    const QString dst = QDir(to).filePath(e.fileName());
    if (e.isDir()) {
      if (!copyTree(e.absoluteFilePath(), dst)) return false;
    } else if (!QFile::copy(e.absoluteFilePath(), dst)) {
      return false;
    }
  }
  return true;
}

// A copy of tests/fixtures/<name> in `scratch` (the original is never touched); returns its path.
inline QString copyFixture(const Scratch& scratch, const QString& name) {
  const QString dst = scratch.file(name);
  copyTree(QStringLiteral(OTE_SOURCE_DIR "/tests/fixtures/") + name, dst);
  return dst;
}

// A copy of games/demo-srpg in `scratch`; returns its path.
inline QString copyDemoGame(const Scratch& scratch) {
  const QString dst = scratch.file("demo-srpg");
  copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/demo-srpg"), dst);
  return dst;
}

}  // namespace ote_test
