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

  // Which view a table file kind (the schema's name: "units", "skills") opens in: 0 table, 1 table + form, 2 form.
  int tableViewMode(const QString& kind) const { return s_->value("view/table/" + kind, 1).toInt(); }
  void setTableViewMode(const QString& kind, int mode) { s_->setValue("view/table/" + kind, mode); }

  // The map painter: zoom (1.0 = 100%), the chosen tool (map_tool.h key) and which overlays are shown.
  double mapZoom() const { return s_->value("map/zoom", 1.0).toDouble(); }
  void setMapZoom(double z) { s_->setValue("map/zoom", z); }
  QString mapTool() const { return s_->value("map/tool", "paint").toString(); }
  void setMapTool(const QString& key) { s_->setValue("map/tool", key); }
  int mapBrush() const { return s_->value("map/brush", 1).toInt(); }
  void setMapBrush(int size) { s_->setValue("map/brush", size); }
  bool mapOverlay(const QString& name, bool def = true) const { return s_->value("map/overlay/" + name, def).toBool(); }
  void setMapOverlay(const QString& name, bool on) { s_->setValue("map/overlay/" + name, on); }

  void sync() { s_->sync(); }

 private:
  QSettings* s_;
};

}  // namespace ote_native
