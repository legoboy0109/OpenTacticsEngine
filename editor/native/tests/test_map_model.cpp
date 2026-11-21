// The map painter's model (model/map_painter.*): strokes, fills, heights, legend entries and resizing on a square and a hexagon
// game, each checked against the exact bytes the file must contain afterwards (built independently with plain Json edits),
// one undo step per action, and "the engine still loads the map" through the real project check.
#include <QtTest>
#include <filesystem>
#include <functional>

#include "json_commands.h"
#include "map_markers.h"
#include "map_painter.h"
#include "project.h"
#include "test_support.h"
#include "validate.h"

using namespace ote_native;
using ote::Json;

namespace {

Json parse(const QByteArray& bytes) {
  Json j;
  std::string err;
  Json::parse(bytes.toStdString(), j, err);
  return j;
}

// What the editor writes for `j`: two-space indent and a trailing newline.
QByteArray bytesOf(const Json& j) { return QByteArray::fromStdString(j.dump(2) + "\n"); }

// The canonical file with `edit` applied to its Json tree: the expectation, built without the painter.
QByteArray expected(const QByteArray& canonical, const std::function<void(Json&)>& edit) {
  Json j = parse(canonical);
  edit(j);
  return bytesOf(j);
}

void setRow(Json& map, const char* key, size_t y, const std::string& text) {
  for (auto& m : map.mutableMembers())
    if (m.first == key) m.second.mutableItems()[y] = Json::makeString(text);
}
Json& member(Json& j, const char* key) {
  for (auto& m : j.mutableMembers())
    if (m.first == key) return m.second;
  static Json none;
  return none;
}

// A scratch copy of one demo game with its map open in a painter.
struct Fixture {
  ote_test::Scratch tmp;
  QString dir;
  std::unique_ptr<Project> project;
  JsonDocument* doc = nullptr;
  std::unique_ptr<MapPainter> painter;

  Fixture(const char* game, const char* map) {
    dir = tmp.file(game);
    ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/") + game, dir);
    open(QStringLiteral("maps/%1.json").arg(map));
  }
  void open(const QString& rel) {
    painter.reset();
    project = std::make_unique<Project>(dir);
    doc = project->document(rel);
    painter = std::make_unique<MapPainter>(project.get(), doc);
  }
  QByteArray saved() {
    project->save(doc->relPath());
    return ote_test::readFile(dir + "/" + doc->relPath());
  }
  // The map file as the editor writes it (comments dropped), before any edit.
  QByteArray canonical() { return doc->serialize(); }
  int steps() const { return project->undoStack()->count(); }
  bool stroke(const QString& text, const std::function<void(MapPainter&)>& f) {
    if (!painter->beginStroke(text)) return false;
    f(*painter);
    return painter->endStroke();
  }
  // The project check of the saved files, for this one map.
  QStringList problems() {
    QStringList out;
    for (const auto& p : ote_editor::validateProject(std::filesystem::path(dir.toStdString())))
      out << QString::fromStdString(p.file + ": " + p.message);
    return out;
  }
};

// Hexagon neighbours as docs/HEX.md lists them (pointy: odd rows shoved right), written out independently of the engine.
std::vector<int> pointyNeighbours(int x, int y, int w, int h) {
  static const int even[6][2] = {{0, -1}, {1, 0}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1}};
  static const int odd[6][2] = {{1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 0}, {0, -1}};
  std::vector<int> out;
  for (int i = 0; i < 6; ++i) {
    const int nx = x + ((y & 1) ? odd[i][0] : even[i][0]), ny = y + ((y & 1) ? odd[i][1] : even[i][1]);
    if (nx >= 0 && ny >= 0 && nx < w && ny < h) out.push_back(ny * w + nx);
  }
  return out;
}

}  // namespace

class MapModelTest : public QObject {
  Q_OBJECT
 private slots:
  void aStrokeIsOneUndoStepAndExactBytes();
  void paintingOverWithTheSameTerrainChangesNothing();
  void newTerrainGetsALegendEntryInTheMapsStyle();
  void anUnusedLegendEntryIsNotKept();
  void rectangleFillSquare();
  void floodFillSquare();
  void floodFillHexagonsFollowsTheNeighbourTable();
  void heightsRaiseLowerSetOncePerCell();
  void firstHeightEditCreatesTheListAfterTiles();
  void resizeSquareGrowAndShrink();
  void resizeHexKeepsRowPairsAndTheMapLoads();
  void resizeKeepsTileLayerConsistent();
  void resizeUndoRestoresEveryByte();
  void editsBehindTheStrokeCancelIt();
  void aMapWithABrokenGridIsRefused();
  void markersAndFootprints();
};

void MapModelTest::aStrokeIsOneUndoStepAndExactBytes() {
  Fixture f("demo-srpg", "bridge");
  const QByteArray before = f.canonical();
  QVERIFY(f.stroke("Paint terrain", [](MapPainter& p) {
    p.paintCell(0, 0, "forest");
    p.paintCell(1, 0, "mountain");
    p.paintCell(2, 0, "mountain");
    p.paintCell(2, 0, "water");  // the same cell twice in one stroke: only the last counts
  }));
  QCOMPARE(f.steps(), 1);
  QVERIFY(f.doc->isDirty());
  const QByteArray want = expected(before, [](Json& m) { setRow(m, "tiles", 0, "fm~f..~..mm.."); });
  // row 0 was "..ff..~..mm..": cells 0..2 become f, m, ~ (the last paint of cell 2 wins)
  QCOMPARE(f.saved(), want);
  QVERIFY(!f.doc->isDirty());
  f.project->undoStack()->undo();
  QCOMPARE(f.doc->serialize(), before);
  f.project->undoStack()->redo();
  QCOMPARE(f.doc->serialize(), want);
}

void MapModelTest::paintingOverWithTheSameTerrainChangesNothing() {
  Fixture f("demo-srpg", "bridge");
  QVERIFY(!f.stroke("Paint terrain", [](MapPainter& p) { p.paintCell(0, 0, "plain"); }));  // (0,0) already is plain
  QCOMPARE(f.steps(), 0);
  QVERIFY(!f.doc->isDirty());
}

void MapModelTest::newTerrainGetsALegendEntryInTheMapsStyle() {
  Fixture f("demo-srpg", "bridge");
  // Two new terrains: "sand" has a free glyph, "frost" wants 'f', which the forest has.
  QFile tf(f.dir + "/data/terrain.json");
  QVERIFY(tf.open(QIODevice::ReadOnly));
  Json terrain = parse(tf.readAll());
  tf.close();
  Json sand = Json::makeObject(), frost = Json::makeObject();
  sand.set("id", Json::makeString("sand"));
  sand.set("name", Json::makeString("Sand"));
  sand.set("glyph", Json::makeString("s"));
  frost.set("id", Json::makeString("frost"));
  frost.set("name", Json::makeString("Frost"));
  frost.set("glyph", Json::makeString("f"));
  member(terrain, "terrain").push(sand);
  member(terrain, "terrain").push(frost);
  ote_test::writeFile(f.dir + "/data/terrain.json", bytesOf(terrain));
  f.open("maps/bridge.json");
  const QByteArray before = f.canonical();
  QVERIFY(f.stroke("Paint terrain", [](MapPainter& p) {
    p.paintCell(0, 0, "sand");
    p.paintCell(1, 0, "frost");
    p.paintCell(2, 0, "sand");
  }));
  QCOMPARE(f.steps(), 1);
  const QByteArray want = expected(before, [](Json& m) {
    member(m, "legend").set("s", Json::makeString("sand"));
    member(m, "legend").set("r", Json::makeString("frost"));  // 'f' taken, 'r' is the next letter of "frost", lower case like the legend
    setRow(m, "tiles", 0, "srsf..~..mm..");
  });
  QCOMPARE(f.saved(), want);
  QCOMPARE(f.problems(), QStringList());  // the engine loads it
  f.project->undoStack()->undo();
  QCOMPARE(f.doc->serialize(), before);  // the legend entries go with the stroke

  // A map whose legend is in capitals gets a capital for a terrain with no glyph of its own.
  Json mud = Json::makeObject();
  mud.set("id", Json::makeString("mud"));
  mud.set("name", Json::makeString("Mud"));
  member(terrain, "terrain").push(mud);
  ote_test::writeFile(f.dir + "/data/terrain.json", bytesOf(terrain));
  Json up = parse(before);
  Json rows = Json::makeArray();
  for (int y = 0; y < 9; ++y) rows.push(Json::makeString(std::string(13, '.')));
  member(up, "tiles") = rows;
  member(up, "legend") = parse(R"({".": "plain", "F": "forest", "W": "water"})");
  ote_test::writeFile(f.dir + "/maps/capitals.json", bytesOf(up));
  f.open("maps/capitals.json");
  QVERIFY(f.stroke("Paint terrain", [](MapPainter& p) { p.paintCell(3, 3, "mud"); }));
  QCOMPARE(QString::fromStdString(parse(f.saved())["legend"]["M"].asString()), QStringLiteral("mud"));
}

void MapModelTest::anUnusedLegendEntryIsNotKept() {
  Fixture f("demo-srpg", "bridge");
  const QByteArray before = f.canonical();
  QFile tf(f.dir + "/data/terrain.json");
  QVERIFY(tf.open(QIODevice::ReadOnly));
  Json terrain = parse(tf.readAll());
  tf.close();
  Json lava = Json::makeObject();
  lava.set("id", Json::makeString("lava"));
  lava.set("name", Json::makeString("Lava"));
  member(terrain, "terrain").push(lava);
  ote_test::writeFile(f.dir + "/data/terrain.json", bytesOf(terrain));
  f.open("maps/bridge.json");
  // Lava is painted and painted over again inside one stroke: nothing about it may reach the file.
  QVERIFY(!f.stroke("Paint terrain", [](MapPainter& p) {
    p.paintCell(0, 0, "lava");
    p.paintCell(0, 0, "plain");
  }));
  QCOMPARE(f.steps(), 0);
  QCOMPARE(f.doc->serialize(), before);
}

void MapModelTest::rectangleFillSquare() {
  Fixture f("demo-srpg", "bridge");
  const QByteArray before = f.canonical();
  QVERIFY(f.stroke("Fill rectangle", [](MapPainter& p) { p.paintRect(4, 1, 2, 2, "water"); }));  // corners in any order
  QCOMPARE(f.steps(), 1);
  // rows 1 and 2 were "...f..~...m.." and "......~.f....": columns 2..4 become water
  QCOMPARE(f.saved(), expected(before, [](Json& m) {
             setRow(m, "tiles", 1, "..~~~.~...m..");
             setRow(m, "tiles", 2, "..~~~.~.f....");
           }));
}

void MapModelTest::floodFillSquare() {
  Fixture f("demo-srpg", "bridge");
  const QByteArray before = f.canonical();
  // The river column x = 6 is water in rows 0..3 and 5..8; the bridge at row 4 cuts it. Fill the upper part with plain.
  QVERIFY(f.stroke("Fill area", [](MapPainter& p) { p.floodFill(6, 0, "plain"); }));
  const QByteArray want = expected(before, [](Json& m) {
    const char* rows[] = {"..ff..~..mm..", "...f..~...m..", "......~.f....", "......~......"};
    for (size_t y = 0; y < 4; ++y) {
      std::string r = rows[y];
      r[6] = '.';
      setRow(m, "tiles", y, r);
    }
  });
  QCOMPARE(f.saved(), want);
  // A cell outside the map is ignored.
  QVERIFY(!f.stroke("Fill area", [](MapPainter& p) { p.floodFill(40, 40, "plain"); }));
}

void MapModelTest::floodFillHexagonsFollowsTheNeighbourTable() {
  Fixture f("demo-hex", "foggy_hills");
  const QByteArray before = f.canonical();
  const Json map = parse(before);
  std::vector<std::string> rows;
  for (const Json& r : map["tiles"].items()) rows.push_back(r.asString());
  const int w = 13, h = 9;
  // Oracle: breadth-first search over the neighbour table of docs/HEX.md from the forest at (2, 3)... any start works.
  const int sx = 4, sy = 4;
  const char want = rows[(size_t)sy][(size_t)sx];
  std::vector<int> seen(w * h, 0), queue{sy * w + sx};
  seen[(size_t)(sy * w + sx)] = 1;
  for (size_t i = 0; i < queue.size(); ++i)
    for (int n : pointyNeighbours(queue[i] % w, queue[i] / w, w, h))
      if (!seen[(size_t)n] && rows[(size_t)(n / w)][(size_t)(n % w)] == want) {
        seen[(size_t)n] = 1;
        queue.push_back(n);
      }
  QVERIFY(queue.size() > 3);  // a real region, so the test means something
  QVERIFY(f.stroke("Fill area", [&](MapPainter& p) { p.floodFill(sx, sy, "forest"); }));
  const QByteArray out = f.saved();
  Json fixed = parse(before);
  for (int y = 0; y < h; ++y) {
    std::string r = rows[(size_t)y];
    for (int x = 0; x < w; ++x)
      if (seen[(size_t)(y * w + x)]) r[(size_t)x] = 'f';
    setRow(fixed, "tiles", (size_t)y, r);
  }
  QCOMPARE(out, bytesOf(fixed));
}

void MapModelTest::heightsRaiseLowerSetOncePerCell() {
  Fixture f("demo-srpg", "bridge");
  const QByteArray before = f.canonical();
  // (0,0) is 1 and (6,0) is 0. Visiting a cell twice in one stroke changes it once; lowering zero stays zero.
  QVERIFY(f.stroke("Raise height", [](MapPainter& p) {
    p.bumpHeight(0, 0, +1);
    p.bumpHeight(0, 0, +1);
    p.bumpHeight(6, 0, -1);
  }));
  QCOMPARE(f.steps(), 1);
  QCOMPARE(f.saved(), expected(before, [](Json& m) {
             member(m, "heights").mutableItems()[0] = parse("[2,1,1,1,1,1,0,1,1,3,3,2,1]");
           }));
  QVERIFY(f.stroke("Set height", [](MapPainter& p) {
    p.setHeight(1, 1, 7);
    p.setHeight(2, 1, 500);  // clamped to the largest height
  }));
  QCOMPARE(f.steps(), 2);
  QCOMPARE(QString::fromStdString(parse(f.saved())["heights"][1].dump(0)), QStringLiteral("[1,7,99,1,1,1,0,1,1,2,3,2,1]"));
  f.project->undoStack()->undo();
  f.project->undoStack()->undo();
  QCOMPARE(f.doc->serialize(), before);
}

void MapModelTest::firstHeightEditCreatesTheListAfterTiles() {
  Fixture f("demo-srpg", "bridge");
  Json tiny = Json::makeObject();
  tiny.set("name", Json::makeString("Tiny"));
  tiny.set("grid", parse(R"({"type": "square", "width": 3, "height": 2})"));
  tiny.set("legend", parse(R"({".": "plain", "f": "forest"})"));
  tiny.set("tiles", parse(R"(["...", ".f."])"));
  tiny.set("sides", parse(f.doc->root()["sides"].dump(0).c_str()));
  ote_test::writeFile(f.dir + "/maps/tiny.json", bytesOf(tiny));
  f.open("maps/tiny.json");
  const QByteArray before = f.canonical();
  QVERIFY(!f.painter->grid().hasHeights);
  QVERIFY(!f.stroke("Set height", [](MapPainter& p) { p.setHeight(0, 0, 0); }));  // setting 0 where there is no list changes nothing
  QVERIFY(f.stroke("Raise height", [](MapPainter& p) { p.bumpHeight(1, 1, +1); }));
  QCOMPARE(f.steps(), 1);
  Json want = tiny;
  // key order: name, grid, legend, tiles, heights, sides
  Json ordered = Json::makeObject();
  for (const auto& m : want.members()) {
    ordered.set(m.first, m.second);
    if (m.first == "tiles") ordered.set("heights", parse("[[0,0,0],[0,1,0]]"));
  }
  QCOMPARE(f.saved(), bytesOf(ordered));
  f.project->undoStack()->undo();
  QCOMPARE(f.doc->serialize(), before);
}

void MapModelTest::resizeSquareGrowAndShrink() {
  Fixture f("demo-srpg", "bridge");
  const QByteArray before = f.canonical();
  ote_editor::ResizeSpec grow;
  grow.width = 15;
  grow.height = 10;
  grow.fill = '.';
  QVERIFY(f.painter->lostByResize(grow).empty());
  QVERIFY(f.painter->resize(grow));
  QCOMPARE(f.steps(), 1);
  const Json after = parse(f.saved());
  QCOMPARE(QString::fromStdString(after["grid"].dump(0)), QStringLiteral("{\"type\":\"square\",\"width\":15,\"height\":10}"));
  QCOMPARE((int)after["tiles"].size(), 10);
  QCOMPARE(QString::fromStdString(after["tiles"][0].asString()), QStringLiteral("..ff..~..mm....")) ;
  QCOMPARE(QString::fromStdString(after["tiles"][9].asString()), QStringLiteral("..............."));
  QCOMPARE(QString::fromStdString(after["heights"][0].dump(0)), QStringLiteral("[1,1,1,1,1,1,0,1,1,3,3,2,1,0,0]"));
  QCOMPARE((int)after["heights"].size(), 10);
  QCOMPARE(f.problems(), QStringList());
  // Shrinking cuts squares that hold units and structures: they are reported first, then removed in the same step.
  ote_editor::ResizeSpec shrink;
  shrink.width = 6;
  shrink.height = 5;
  shrink.fill = '.';
  const auto lost = f.painter->lostByResize(shrink);
  QVERIFY(lost.size() >= 3);
  QVERIFY(f.painter->resize(shrink));
  QCOMPARE(f.steps(), 2);
  const Json small = parse(f.saved());
  QCOMPARE((int)small["tiles"].size(), 5);
  QCOMPARE((int)small["tiles"][4].asString().size(), 6);
  QCOMPARE((int)small["units"].size(), (int)after["units"].size() - (int)std::count_if(lost.begin(), lost.end(), [](const auto& t) { return t.kind == "unit"; }));
  for (const Json& u : small["units"].items()) {
    QVERIFY(u["x"].asInt() < 6);
    QVERIFY(u["y"].asInt() < 5);
  }
  // Nothing happens for the size the map already has.
  ote_editor::ResizeSpec same;
  same.width = 6;
  same.height = 5;
  same.fill = '.';
  QVERIFY(!f.painter->resize(same));
  QCOMPARE(f.steps(), 2);
}

void MapModelTest::resizeHexKeepsRowPairsAndTheMapLoads() {
  Fixture f("demo-hex", "foggy_hills");
  const QByteArray before = f.canonical();
  ote_editor::ResizeSpec s;
  s.width = 15;
  s.height = 14;  // grows by 5 rows: centred, that would be 2.5 -> a move of 2 rows (even), never 3
  s.anchorX = 1;
  s.anchorY = 1;
  s.fill = '.';
  QVERIFY(f.painter->lostByResize(s).empty());
  QVERIFY(f.painter->resize(s));
  const Json now = parse(f.saved());
  const Json old = parse(before);
  QCOMPARE((int)now["tiles"].size(), 14);
  // dx = (15 - 13) / 2 = 1, dy = (14 - 9) / 2 = 2: old row 0 is new row 2, shifted one column.
  QCOMPARE(QString::fromStdString(now["tiles"][2].asString()), QStringLiteral(".") + QString::fromStdString(old["tiles"][0].asString()) + QStringLiteral("."));
  for (size_t i = 0; i < old["structures"].size(); ++i) {
    QCOMPARE(now["structures"][i]["x"].asInt(), old["structures"][i]["x"].asInt() + 1);
    QCOMPARE(now["structures"][i]["y"].asInt(), old["structures"][i]["y"].asInt() + 2);
  }
  QCOMPARE(f.problems(), QStringList());  // fortress rings, units and fog all still load
}

void MapModelTest::resizeKeepsTileLayerConsistent() {
  Fixture f("demo-srpg", "bridge");
  // Give the game a tileset and the map a tile layer (through the same commands the editor uses), then resize.
  JsonDocument* gfx = f.project->document("data/graphics.json");
  QVERIFY(gfx);
  f.project->push(gfx, new InsertCommand(gfx, {}, gfx->root().members().size(), "tilesets", parse(R"({"world": {"sheet": "tiles"}})")));
  Json cells = Json::makeArray();
  for (int y = 0; y < 9; ++y) {
    std::string row;
    for (int x = 0; x < 13; ++x) row += (x + y) % 3 == 0 ? "...." : (x % 2 ? "0001" : "0002");
    cells.push(Json::makeString(row));
  }
  Json layer = Json::makeObject();
  layer.set("tileset", Json::makeString("world"));
  layer.set("cells", cells);
  size_t at = 0;
  for (size_t i = 0; i < f.doc->root().members().size(); ++i)
    if (f.doc->root().members()[i].first == "heights") at = i + 1;
  f.project->push(f.doc, new InsertCommand(f.doc, {}, at, "tileLayer", layer));
  f.project->save("data/graphics.json");
  f.project->undoStack()->clear();
  const QByteArray before = f.canonical();
  QCOMPARE(f.problems(), QStringList());

  // Painting terrain never touches the layer.
  QVERIFY(f.stroke("Paint terrain", [](MapPainter& p) { p.paintCell(0, 0, "mountain"); }));
  QCOMPARE(QString::fromStdString(parse(f.saved())["tileLayer"].dump(0)), QString::fromStdString(layer.dump(0)));

  ote_editor::ResizeSpec s;
  s.width = 14;
  s.height = 10;
  s.anchorX = 2;  // the old map stays at the right and bottom: one column and one row of "no tile" come first
  s.anchorY = 2;
  s.fill = '.';
  QVERIFY(f.painter->resize(s));
  const Json out = parse(f.saved());
  QCOMPARE((int)out["tileLayer"]["cells"].size(), 10);
  QCOMPARE((int)out["tileLayer"]["cells"][0].asString().size(), 14 * 4);
  QCOMPARE(QString::fromStdString(out["tileLayer"]["cells"][0].asString()), QString(14 * 4, QLatin1Char('.')));
  QCOMPARE(QString::fromStdString(out["tileLayer"]["cells"][1].asString()), QStringLiteral("....") + QString::fromStdString(cells[0].asString()));
  QCOMPARE(QString::fromStdString(out["tileLayer"]["tileset"].asString()), QStringLiteral("world"));
  QCOMPARE(f.problems(), QStringList());  // the engine accepts the resized layer
  // Shrinking drops the same squares from the layer.
  s.width = 5;
  s.height = 4;
  s.anchorX = s.anchorY = 0;
  QVERIFY(f.painter->resize(s));
  const Json small = parse(f.saved());
  QCOMPARE((int)small["tileLayer"]["cells"].size(), 4);
  QCOMPARE(QString::fromStdString(small["tileLayer"]["cells"][1].asString()),
           QString::fromStdString(out["tileLayer"]["cells"][1].asString()).left(5 * 4));
}

void MapModelTest::resizeUndoRestoresEveryByte() {
  for (const char* game : {"demo-srpg", "demo-hex"}) {
    Fixture f(game, game == std::string("demo-srpg") ? "north_road" : "fortress_ford");
    const QByteArray before = f.canonical();
    ote_editor::ResizeSpec s;
    s.width = 8;
    s.height = 6;
    s.anchorX = 1;
    s.anchorY = 1;
    s.fill = '.';
    QVERIFY(f.painter->resize(s));
    QVERIFY(f.doc->serialize() != before);
    f.project->undoStack()->undo();
    QCOMPARE(f.doc->serialize(), before);
    QVERIFY(!f.doc->isDirty());
    f.project->undoStack()->redo();
    QVERIFY(f.doc->isDirty());
  }
}

void MapModelTest::editsBehindTheStrokeCancelIt() {
  Fixture f("demo-srpg", "bridge");
  QVERIFY(f.painter->beginStroke("Paint terrain"));
  f.painter->paintCell(0, 0, "forest");
  QVERIFY(f.painter->grid().terrainAt(0, 0) == "forest");  // the view sees the stroke at once
  QVERIFY(!f.doc->isDirty());                              // ... but the file has not been touched
  f.project->push(f.doc, new SetValueCommand(f.doc, {PathStep::member("name")}, Json::makeString("Other")));
  QVERIFY(!f.painter->inStroke());                         // the stroke was dropped, not written over the new edit
  QVERIFY(f.painter->grid().terrainAt(0, 0) == "plain");
  QCOMPARE(f.steps(), 1);
}

void MapModelTest::aMapWithABrokenGridIsRefused() {
  Fixture f("demo-srpg", "bridge");
  f.project->push(f.doc, new SetValueCommand(f.doc, {PathStep::member("tiles"), PathStep::at(0)}, Json::makeString("short")));
  QVERIFY(!f.painter->valid());
  QVERIFY(f.painter->error().contains("tiles row 0"));
  QVERIFY(!f.painter->beginStroke("Paint terrain"));
  f.project->undoStack()->undo();
  QVERIFY(f.painter->valid());
}

void MapModelTest::markersAndFootprints() {
  Fixture f("demo-srpg", "bridge");
  const auto markers = readMapMarkers(f.doc->root(), f.painter->footprints());
  int units = 0, structures = 0;
  for (const MapMarker& m : markers) {
    units += m.kind == MapMarker::Kind::Unit;
    structures += m.kind == MapMarker::Kind::Structure;
  }
  QCOMPARE(units, 9);
  QCOMPARE(structures, 5);
  QVERIFY(markers.front().tip.contains("Square"));
  Fixture h("demo-hex", "fortress_ford");
  bool ring = false;
  for (const MapMarker& m : readMapMarkers(h.doc->root(), h.painter->footprints())) ring |= m.rings == 1;
  QVERIFY(ring);  // the fortress covers its ring of six
}

QTEST_MAIN(MapModelTest)
#include "test_map_model.moc"
