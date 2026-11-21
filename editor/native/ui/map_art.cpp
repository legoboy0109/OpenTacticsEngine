#include "map_art.h"

#include "project.h"

namespace ote_native {

using ote::Json;

MapArt::MapArt(Project* project, QObject* parent) : QObject(parent), project_(project) {
  doc_ = project->document(QStringLiteral("data/graphics.json"));
  if (doc_) {
    connect(doc_, &JsonDocument::changed, this, [this] { reset(); });
    connect(doc_, &JsonDocument::reloaded, this, [this] { reset(); });
  }
}

void MapArt::reset() {
  sheets_.clear();
  cache_.clear();
  emit artChanged();
}

const MapArt::Sheet* MapArt::sheet(const std::string& id) {
  const QString key = QString::fromStdString(id);
  if (const auto it = sheets_.constFind(key); it != sheets_.constEnd()) return it->image.isNull() ? nullptr : &*it;
  Sheet s;
  if (doc_)
    for (const Json& j : doc_->root()["sheets"].items())
      if (j["id"].asString() == id) {
        s.image = QPixmap(project_->absolutePath(QString::fromStdString(j["file"].asString())));
        s.cellW = j["cellWidth"].asInt();
        s.cellH = j["cellHeight"].asInt();
      }
  if (s.cellW < 1 || s.cellH < 1) s.image = QPixmap();
  const auto it = sheets_.insert(key, s);
  return it->image.isNull() ? nullptr : &*it;
}

QPixmap MapArt::cut(const std::string& sheetId, int col, int row) {
  const Sheet* s = sheet(sheetId);
  if (!s || col < 0 || row < 0) return QPixmap();
  const QRect r(col * s->cellW, row * s->cellH, s->cellW, s->cellH);
  if (!s->image.rect().contains(r)) return QPixmap();
  return s->image.copy(r);
}

QPixmap MapArt::terrainTile(const std::string& id) {
  const QString key = QStringLiteral("t:") + QString::fromStdString(id);
  if (const auto it = cache_.constFind(key); it != cache_.constEnd()) return *it;
  QPixmap out;
  if (doc_) {
    const Json& look = doc_->root()["terrain"][id];
    const Json* at = nullptr;
    if (look["cell"].isArray()) at = &look["cell"];
    else if (look["variants"].isArray() && look["variants"].size() > 0) at = &look["variants"][0];
    else if (look["frames"].isArray() && look["frames"].size() > 0) at = &look["frames"][0];
    else if (look["join"].isArray() && look["join"].size() > 0) at = look["join"][0].isArray() ? &look["join"][0] : &look["join"];
    if (at && at->size() == 2) out = cut(look["sheet"].asString(), (*at)[0].asInt(-1), (*at)[1].asInt(-1));
  }
  cache_.insert(key, out);
  return out;
}

QPixmap MapArt::layerTile(const std::string& tileset, int tile) {
  const QString key = QStringLiteral("l:%1:%2").arg(QString::fromStdString(tileset)).arg(tile);
  if (const auto it = cache_.constFind(key); it != cache_.constEnd()) return *it;
  QPixmap out;
  if (doc_ && tile >= 0) {
    const std::string sheetId = doc_->root()["tilesets"][tileset]["sheet"].asString();
    if (const Sheet* s = sheet(sheetId)) {
      const int cols = s->image.width() / s->cellW;
      if (cols > 0) out = cut(sheetId, tile % cols, tile / cols);
    }
  }
  cache_.insert(key, out);
  return out;
}

}  // namespace ote_native
