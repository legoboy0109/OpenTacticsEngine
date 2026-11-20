#include "app_settings.h"

namespace ote_native {

QStringList AppSettings::recentProjects() const { return s_->value("recentProjects").toStringList(); }

void AppSettings::addRecentProject(const QString& folder) {
  QStringList l = recentProjects();
  l.removeAll(folder);
  l.prepend(folder);
  while (l.size() > kMaxRecent) l.removeLast();
  s_->setValue("recentProjects", l);
}

void AppSettings::removeRecentProject(const QString& folder) {
  QStringList l = recentProjects();
  l.removeAll(folder);
  s_->setValue("recentProjects", l);
}

void AppSettings::saveWindow(const QByteArray& geometry, const QByteArray& state) {
  s_->setValue("window/geometry", geometry);
  s_->setValue("window/state", state);
}

}  // namespace ote_native
