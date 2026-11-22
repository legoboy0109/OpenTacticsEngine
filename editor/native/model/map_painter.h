// The map painter's model: edits the tile grid of ONE map document and nothing else. It reads the grid through the Qt-free
// helpers of editor/core/map_grid.h and writes through the same path-based undo commands the forms use, so undo, redo, the
// dirty flag, save and the problems check all keep working.
//
// A drag is a "stroke": beginStroke() copies the grid, every paint call changes only that copy (so the view can draw it live
// without touching the file 60 times a second), and endStroke() writes the rows that differ as ONE undo step.
#pragma once
#include <QColor>
#include <QList>
#include <QObject>
#include <QString>
#include <map>
#include <set>
#include <vector>

#include "json_document.h"
#include "map_grid.h"

namespace ote_native {

class Project;

// One terrain of data/terrain.json (or one the map's legend names that the file does not have).
struct TerrainInfo {
  std::string id;
  QString name;
  QColor color;
  char glyph = 0;
};

class MapPainter : public QObject {
  Q_OBJECT
 public:
  MapPainter(Project* project, JsonDocument* doc, QObject* parent = nullptr);

  Project* project() const { return project_; }
  JsonDocument* doc() const { return doc_; }
  // False (with error()) when the map's grid / tiles / legend / heights / tileLayer are not in the shape the loader wants.
  bool valid();
  QString error();
  // The grid as it is now, including the stroke in progress.
  const ote_editor::MapGrid& grid();
  bool flatTop() const { return flatTop_; }

  // Terrains the palette offers: data/terrain.json in file order, then legend terrains it does not list.
  QList<TerrainInfo> terrains();
  TerrainInfo terrainInfo(const std::string& id);
  // Structure sizes of data/structures.json (for resizing).
  std::map<std::string, ote_editor::Footprint> footprints();

  // ---- brush ----
  // Paint, Raise / Lower and Set height cover a patch around the clicked square: 1, 3, 5, 7 or 9 across (squares: a block; hexagons:
  // every cell within (size-1)/2 steps). Fill, Rectangle and the single-cell calls below are not affected.
  static constexpr int kMaxBrush = 9;
  int brushSize() const { return brush_; }
  void setBrushSize(int size);

  // ---- strokes ----
  bool beginStroke(const QString& undoText);
  bool inStroke() const { return inStroke_; }
  // Terrain: a legend entry is added to the grid copy when the map has none for it (written by endStroke).
  bool paintCell(int x, int y, const std::string& terrain);  // the brush patch centred on (x, y)
  bool paintRect(int x0, int y0, int x1, int y1, const std::string& terrain);
  bool floodFill(int x, int y, const std::string& terrain);
  // Heights: the first height edit of a map without `heights` creates the list (all 0). bumpHeight changes a cell once per stroke.
  bool setHeight(int x, int y, int value);  // also the brush patch
  bool bumpHeight(int x, int y, int delta);
  // Writes the stroke (one undo step); false when nothing changed. cancelStroke() throws the copy away.
  bool endStroke();
  void cancelStroke();

  // ---- resize (one undo step) ----
  // What would be removed; empty when everything fits. `err` is set when the size is refused.
  std::vector<ote_editor::LostThing> lostByResize(const ote_editor::ResizeSpec& spec, QString* err = nullptr);
  bool resize(const ote_editor::ResizeSpec& spec, QString* err = nullptr);

  static constexpr int kMaxHeight = 99;

 signals:
  void gridChanged();  // the document changed (any edit, undo, reload) or the stroke copy did

 private:
  void refresh();
  bool setSymbol(int x, int y, const std::string& terrain, char glyph);
  void ensureHeights();
  bool setHeightCell(int x, int y, int value);
  bool bumpHeightCell(int x, int y, int delta);
  const ote::Json* tableRows(const QString& rel, const char* key);

  Project* project_;
  JsonDocument* doc_;
  ote_editor::MapGrid base_, work_;
  bool stale_ = true, ok_ = false, flatTop_ = false, inStroke_ = false, pushing_ = false;
  QString error_, undoText_;
  int brush_ = 1;
  std::set<int> bumped_;  // cells bumpHeight already changed in this stroke
};

}  // namespace ote_native
