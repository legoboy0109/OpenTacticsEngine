// The map painter page of a map file: tool bar, terrain palette, the canvas and a status line. Everything it edits goes
// through MapPainter (one drag = one undo step); zoom, tool and overlays are remembered through AppSettings.
#pragma once
#include <QWidget>
#include <functional>

#include "app_settings.h"
#include "json_path.h"
#include "map_art.h"
#include "map_painter.h"
#include "map_view.h"

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QListWidget;
class QSpinBox;

namespace ote_native {

class Project;

class MapPage : public QWidget {
  Q_OBJECT
 public:
  MapPage(Project* project, JsonDocument* doc, QWidget* parent = nullptr);

  // Loads zoom / tool / overlays and keeps saving them. Optional (tests without settings leave defaults).
  void setAppSettings(AppSettings* settings);

  MapPainter* painter() const { return painter_; }
  MapView* view() const { return view_; }
  QListWidget* palette() const { return palette_; }
  void selectTerrain(const QString& id);
  QString selectedTerrain() const;
  void setTool(MapTool tool);
  void setBrushSize(int size);  // 1, 3, 5, 7 or 9; remembered

  // Resize: the dialog and the "units would be cut off" question are hooks so tests never open a window.
  using ResizeChooser = std::function<bool(ote_editor::ResizeSpec* spec)>;
  using CutConfirm = std::function<bool(const QString& text)>;
  void setResizeChooser(ResizeChooser f) { resizeChooser_ = std::move(f); }
  void setCutConfirm(CutConfirm f) { cutConfirm_ = std::move(f); }
  bool resizeMap();  // asks, then resizes; false when cancelled / refused
  bool applyResize(const ote_editor::ResizeSpec& spec);  // the second half: confirm the cut, then resize

 signals:
  // A unit / structure / slot marker was clicked with the Inspect tool: the path of its entry in the map file.
  void inspectRequested(const JsonPath& path);

 private:
  void rebuildPalette();
  void updateStatus(int x, int y);
  void updateZoomLabel();
  void saveOverlay(const QString& name, bool on);

  Project* project_;
  MapPainter* painter_;
  MapArt* art_;
  MapView* view_;
  QListWidget* palette_;
  QButtonGroup* tools_;
  QSpinBox* heightSpin_;
  QComboBox* brushBox_;
  QLabel *status_, *zoomLabel_, *note_, *message_;
  QCheckBox *units_, *structures_, *heights_, *layer_;
  AppSettings* settings_ = nullptr;
  QString paletteSignature_;
  ResizeChooser resizeChooser_;
  CutConfirm cutConfirm_;
};

}  // namespace ote_native
