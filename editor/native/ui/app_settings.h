// What the editor remembers between runs, on top of a QSettings the caller provides (the real program: the user's
// settings file; tests: an ini file in a temp folder, so no real configuration is ever touched).
#pragma once
#include <QByteArray>
#include <QSettings>
#include <QStringList>

namespace ote_native {

class AppSettings {
 public:
  explicit AppSettings(QSettings* settings) : s_(settings) {}

  QStringList recentProjects() const;
  void addRecentProject(const QString& folder);  // newest first, no duplicates, at most kMaxRecent
  void removeRecentProject(const QString& folder);
  static constexpr int kMaxRecent = 10;

  QString lastProject() const { return s_->value("lastProject").toString(); }
  void setLastProject(const QString& folder) { s_->setValue("lastProject", folder); }

  QByteArray windowGeometry() const { return s_->value("window/geometry").toByteArray(); }
  QByteArray windowState() const { return s_->value("window/state").toByteArray(); }
  void saveWindow(const QByteArray& geometry, const QByteArray& state);

  // Empty = use ote-player next to the editor program.
  QString playerPathOverride() const { return s_->value("play/playerPath").toString(); }
  void setPlayerPathOverride(const QString& path) { s_->setValue("play/playerPath", path); }

  void sync() { s_->sync(); }

 private:
  QSettings* s_;
};

}  // namespace ote_native
