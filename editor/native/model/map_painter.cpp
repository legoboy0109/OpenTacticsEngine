#include "map_painter.h"

#include <QUndoStack>
#include <algorithm>

#include "json_commands.h"
#include "project.h"

namespace ote_native {

using ote::Json;
using ote_editor::MapGrid;

namespace {
// A colour that is always the same for the same id, for terrains without a "color" of their own.
QColor stableColor(const std::string& id) {
  uint32_t h = 2166136261u;
  for (char c : id) h = (h ^ (unsigned char)c) * 16777619u;
  return QColor::fromHsv((int)(h % 360), 120, 200);
}

JsonPath member(const char* a) { return {PathStep::member(a)}; }
JsonPath member(const char* a, size_t i) { return {PathStep::member(a), PathStep::at(i)}; }
}  // namespace

MapPainter::MapPainter(Project* project, JsonDocument* doc, QObject* parent) : QObject(parent), project_(project), doc_(doc) {
  const Json* game = nullptr;
  if (JsonDocument* g = project->document(QStringLiteral("game.json"))) game = &g->root();
  flatTop_ = game && (*game)["grid"]["layout"].asString() == "flat";
  // Any change of the file (also undo / redo / reload) makes the parsed copy stale; the view repaints and asks again.
  auto touched = [this] {
    stale_ = true;
    if (inStroke_ && !pushing_) cancelStroke();  // the file changed under a drag (e.g. Ctrl+Z): drop the half-made stroke
    emit gridChanged();
  };
  connect(doc, &JsonDocument::changed, this, touched);
  connect(doc, &JsonDocument::reloaded, this, touched);
}

void MapPainter::refresh() {
  if (!stale_) return;
  stale_ = false;
  std::string err;
  MapGrid g;
  ok_ = ote_editor::readMapGrid(doc_->root(), flatTop_, g, err);
  error_ = QString::fromStdString(err);
  if (ok_) base_ = std::move(g);
}

bool MapPainter::valid() {
  refresh();
  return ok_;
}

QString MapPainter::error() {
  refresh();
  return error_;
}

const MapGrid& MapPainter::grid() {
  if (inStroke_) return work_;
  refresh();
  return base_;
}

const Json* MapPainter::tableRows(const QString& rel, const char* key) {
  JsonDocument* d = project_->document(rel);
  if (!d) return nullptr;
  const Json& root = d->root();
  if (root.isArray()) return &root;
  return root[key].isArray() ? &root[key] : nullptr;
}

QList<TerrainInfo> MapPainter::terrains() {
  QList<TerrainInfo> out;
  std::set<std::string> seen;
  if (const Json* rows = tableRows(QStringLiteral("data/terrain.json"), "terrain"))
    for (const Json& r : rows->items()) {
      if (!r["id"].isString() || seen.count(r["id"].asString())) continue;
      TerrainInfo t;
      t.id = r["id"].asString();
      t.name = QString::fromStdString(r["name"].asString(t.id));
      t.color = QColor(QString::fromStdString(r["color"].asString()));
      if (!t.color.isValid()) t.color = stableColor(t.id);
      const std::string glyph = r["glyph"].asString();
      t.glyph = glyph.size() == 1 ? glyph[0] : 0;
      seen.insert(t.id);
      out << t;
    }
  if (valid())
    for (const auto& e : base_.legend)
      if (!seen.count(e.second)) {
        seen.insert(e.second);
        out << TerrainInfo{e.second, QString::fromStdString(e.second), stableColor(e.second), e.first};
      }
  return out;
}

TerrainInfo MapPainter::terrainInfo(const std::string& id) {
  for (const TerrainInfo& t : terrains())
    if (t.id == id) return t;
  return TerrainInfo{id, QString::fromStdString(id), stableColor(id), 0};
}

std::map<std::string, ote_editor::Footprint> MapPainter::footprints() {
  std::map<std::string, ote_editor::Footprint> out;
  if (const Json* rows = tableRows(QStringLiteral("data/structures.json"), "structures"))
    for (const Json& r : rows->items()) {
      if (!r["id"].isString()) continue;
      ote_editor::Footprint f;
      f.w = std::max(1, r["width"].asInt(1));
      f.h = std::max(1, r["height"].asInt(1));
      f.rings = r["rings"].asInt(0);
      out[r["id"].asString()] = f;
    }
  return out;
}

// ---------------------------------------------------------------- strokes

bool MapPainter::beginStroke(const QString& undoText) {
  if (inStroke_ || !valid()) return false;
  work_ = base_;
  bumped_.clear();
  undoText_ = undoText;
  inStroke_ = true;
  return true;
}

void MapPainter::cancelStroke() {
  inStroke_ = false;
  emit gridChanged();
}

bool MapPainter::setSymbol(int x, int y, const std::string& terrain, char glyph) {
  bool isNew = false;
  const char sym = ote_editor::legendSymbolFor(work_, terrain, glyph, isNew);
  if (!sym) return false;
  if (isNew) work_.legend.emplace_back(sym, terrain);
  if (work_.inside(x, y) && work_.rows[(size_t)y][(size_t)x] != sym) {
    work_.rows[(size_t)y][(size_t)x] = sym;
    return true;
  }
  return false;
}

void MapPainter::setBrushSize(int size) { brush_ = std::max(1, std::min(kMaxBrush, size)); }

bool MapPainter::paintCell(int x, int y, const std::string& terrain) {
  if (!inStroke_ || !work_.inside(x, y)) return false;
  const char glyph = terrainInfo(terrain).glyph;
  bool changed = false;
  for (int c : ote_editor::brushCells(work_, x, y, brush_)) changed |= setSymbol(c % work_.width, c / work_.width, terrain, glyph);
  if (changed) emit gridChanged();
  return changed;
}

bool MapPainter::paintRect(int x0, int y0, int x1, int y1, const std::string& terrain) {
  if (!inStroke_) return false;
  bool any = false;
  const char glyph = terrainInfo(terrain).glyph;
  for (int y = std::max(0, std::min(y0, y1)); y <= std::min(work_.height - 1, std::max(y0, y1)); ++y)
    for (int x = std::max(0, std::min(x0, x1)); x <= std::min(work_.width - 1, std::max(x0, x1)); ++x) any |= setSymbol(x, y, terrain, glyph);
  if (any) emit gridChanged();
  return any;
}

bool MapPainter::floodFill(int x, int y, const std::string& terrain) {
  if (!inStroke_ || !work_.inside(x, y)) return false;
  bool any = false;
  const char glyph = terrainInfo(terrain).glyph;
  for (int c : ote_editor::floodRegion(work_, x, y)) any |= setSymbol(c % work_.width, c / work_.width, terrain, glyph);
  if (any) emit gridChanged();
  return any;
}

void MapPainter::ensureHeights() {
  if (work_.hasHeights) return;
  work_.hasHeights = true;
  work_.heights.assign((size_t)work_.width * (size_t)work_.height, 0);
}

bool MapPainter::setHeight(int x, int y, int value) {
  if (!inStroke_ || !work_.inside(x, y)) return false;
  bool changed = false;
  for (int c : ote_editor::brushCells(work_, x, y, brush_)) changed |= setHeightCell(c % work_.width, c / work_.width, value);
  if (changed) emit gridChanged();
  return changed;
}

bool MapPainter::bumpHeight(int x, int y, int delta) {
  if (!inStroke_ || !work_.inside(x, y)) return false;
  bool changed = false;
  for (int c : ote_editor::brushCells(work_, x, y, brush_)) changed |= bumpHeightCell(c % work_.width, c / work_.width, delta);
  if (changed) emit gridChanged();
  return changed;
}

bool MapPainter::setHeightCell(int x, int y, int value) {
  if (!inStroke_ || !work_.inside(x, y)) return false;
  value = std::max(0, std::min(kMaxHeight, value));
  if (work_.heightAt(x, y) == value && (work_.hasHeights || value == 0)) return false;
  ensureHeights();
  work_.heights[(size_t)y * (size_t)work_.width + (size_t)x] = value;
  return true;
}

bool MapPainter::bumpHeightCell(int x, int y, int delta) {
  if (!inStroke_ || !work_.inside(x, y)) return false;
  if (!bumped_.insert(y * work_.width + x).second) return false;
  return setHeightCell(x, y, work_.heightAt(x, y) + delta);
}

bool MapPainter::endStroke() {
  if (!inStroke_) return false;
  inStroke_ = false;
  struct Edit { JsonPath path; Json value; bool insert; size_t index; std::string key; };
  std::vector<Edit> edits;
  const Json& root = doc_->root();
  for (int y = 0; y < work_.height; ++y)
    if (work_.rows[(size_t)y] != base_.rows[(size_t)y]) edits.push_back({member("tiles", (size_t)y), ote_editor::tilesRowJson(work_, y), false, 0, ""});
  // New legend entries, but only those still used after the whole stroke (a symbol painted over again is not kept).
  // Each goes to the end, after the ones queued before it (the position is read before any command has run).
  size_t legendAt = root["legend"].members().size();
  for (size_t i = base_.legend.size(); i < work_.legend.size(); ++i) {
    const char sym = work_.legend[i].first;
    const bool used = std::any_of(work_.rows.begin(), work_.rows.end(), [sym](const std::string& r) { return r.find(sym) != std::string::npos; });
    if (used)
      edits.push_back({member("legend"), Json::makeString(work_.legend[i].second), true, legendAt++, std::string(1, sym)});
  }
  if (work_.hasHeights && !base_.hasHeights) {
    size_t at = root.members().size();
    for (size_t i = 0; i < root.members().size(); ++i)
      if (root.members()[i].first == "tiles") at = i + 1;
    edits.push_back({JsonPath(), ote_editor::heightsJson(work_), true, at, "heights"});
  } else if (work_.hasHeights) {
    for (int y = 0; y < work_.height; ++y)
      if (ote_editor::heightsRowJson(work_, y).dump(0) != ote_editor::heightsRowJson(base_, y).dump(0))
        edits.push_back({member("heights", (size_t)y), ote_editor::heightsRowJson(work_, y), false, 0, ""});
  }
  if (edits.empty()) {
    emit gridChanged();
    return false;
  }
  pushing_ = true;
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(undoText_);
  for (const Edit& e : edits)
    project_->push(doc_, e.insert ? static_cast<QUndoCommand*>(new InsertCommand(doc_, e.path, e.index, e.key, e.value))
                                  : static_cast<QUndoCommand*>(new SetValueCommand(doc_, e.path, e.value)));
  stack->endMacro();
  pushing_ = false;
  stale_ = true;
  emit gridChanged();
  return true;
}

// ---------------------------------------------------------------- resize

std::vector<ote_editor::LostThing> MapPainter::lostByResize(const ote_editor::ResizeSpec& spec, QString* err) {
  ote_editor::ResizeResult r;
  std::string e;
  if (!valid() || !ote_editor::resizeMap(doc_->root(), base_, spec, footprints(), r, e)) {
    if (err) *err = valid() ? QString::fromStdString(e) : error();
    return {};
  }
  return r.lost;
}

bool MapPainter::resize(const ote_editor::ResizeSpec& spec, QString* err) {
  if (inStroke_ || !valid()) return false;
  ote_editor::ResizeResult r;
  std::string e;
  if (!ote_editor::resizeMap(doc_->root(), base_, spec, footprints(), r, e)) {
    if (err) *err = QString::fromStdString(e);
    return false;
  }
  if (r.set.empty()) return false;
  pushing_ = true;
  QUndoStack* stack = project_->undoStack();
  stack->beginMacro(tr("Resize map to %1 x %2").arg(spec.width).arg(spec.height));
  for (const auto& kv : r.set) {
    // Every changed key already exists (tiles, grid ...), except none: resizeMap only rewrites keys the file has.
    project_->push(doc_, new SetValueCommand(doc_, member(kv.first.c_str()), kv.second));
  }
  stack->endMacro();
  pushing_ = false;
  stale_ = true;
  emit gridChanged();
  return true;
}

}  // namespace ote_native
