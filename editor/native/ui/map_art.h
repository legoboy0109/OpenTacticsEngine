// The game's own tile pictures for the painter, resolved the simple way: a terrain's top-down look in data/graphics.json
// ("cell", the first of "variants" / "frames", or the first piece of a "join" block) cut out of its sheet, and the tiles of a
// tile layer's tileset. Anything that cannot be resolved (no graphics.json, a missing png) gives a null pixmap and the view
// falls back to the terrain's colour and legend letter. Reads the project's live graphics.json, so edits show at once.
#pragma once
#include <QHash>
#include <QObject>
#include <QPixmap>
#include <string>

namespace ote_native {

class JsonDocument;
class Project;

class MapArt : public QObject {
  Q_OBJECT
 public:
  explicit MapArt(Project* project, QObject* parent = nullptr);

  // Null when the terrain has no picture that can be found.
  QPixmap terrainTile(const std::string& terrainId);
  // Tile `tile` of the tile layer's tileset; null when unknown.
  QPixmap layerTile(const std::string& tileset, int tile);

 signals:
  void artChanged();  // graphics.json changed: the view repaints

 private:
  struct Sheet { QPixmap image; int cellW = 0, cellH = 0; };
  const Sheet* sheet(const std::string& id);
  QPixmap cut(const std::string& sheetId, int col, int row);
  void reset();

  Project* project_;
  JsonDocument* doc_ = nullptr;
  QHash<QString, Sheet> sheets_;
  QHash<QString, QPixmap> cache_;
  bool dirty_ = false;
};

}  // namespace ote_native
