// The schema layer without any widget: parsing, mistakes in a schema file, kinds and ranges, default and sample values,
// the id sources of a project, and where the program reads its schemas from.
#include <QDir>
#include <QSignalSpy>
#include <QtTest>

#include "id_source.h"
#include "json_commands.h"
#include "project.h"
#include "schema.h"
#include "test_support.h"

using namespace ote_native;
using ote::Json;

namespace {
Json parse(const char* text) {
  Json j;
  std::string err;
  if (!Json::parse(text, j, err)) qFatal("bad test JSON: %s", err.c_str());
  return j;
}
}  // namespace

class SchemaTest : public QObject {
  Q_OBJECT
 private slots:
  void parsesEveryNodeKind();
  void mistakesInASchemaFileAreReportedNotFatal();
  void filePatternsMatchMaps();
  void typeOkAndNodeAccepts();
  void defaultsAndSamples();
  void idSourcesFollowTheProject();
  void theProgramsSchemasLoadWithoutErrors();
  void schemaFolderOverridesTheBuiltInCopy();
};

void SchemaTest::parsesEveryNodeKind() {
  SchemaSet set;
  set.addFromText("t.json", R"({
    "title": "T", "files": ["data/t.json"],
    "rows": { "type": "object", "fields": {
      "a": { "type": "string", "required": true, "help": "h" },
      "b": { "type": "int", "min": 1, "max": 5, "default": 2 },
      "c": { "type": "enum", "values": ["x", "y"] },
      "d": { "type": "ref", "ref": ["data/a.json", "game.json#b"], "values": ["choose"] },
      "e": { "type": "list", "length": 2, "of": { "type": "number" } },
      "f": { "type": "map", "ref": "game.json#stats", "expand": true, "of": { "type": "int" } },
      "g": { "block": true, "alt": "number", "fields": { "h": { "type": "bool" } } },
      "i": { "type": "json" }
    } } })");
  QVERIFY2(set.errors().isEmpty(), qPrintable(set.errors().join("\n")));
  const FileSchema* fs = set.forFile("data/t.json");
  QVERIFY(fs && fs->rows);
  QCOMPARE(fs->title, std::string("T"));
  const SchemaNode& rows = *fs->rows;
  QCOMPARE(rows.fields.size(), size_t(8));
  QVERIFY(rows.field("a")->required);
  QCOMPARE(rows.field("a")->help, std::string("h"));
  QCOMPARE(*rows.field("b")->min, 1.0);
  QCOMPARE(rows.field("b")->deflt.asInt(), 2);
  QCOMPARE(rows.field("c")->values.size(), size_t(2));
  QCOMPARE(rows.field("d")->refs.size(), size_t(2));
  QCOMPARE(rows.field("d")->values.front(), std::string("choose"));
  QCOMPARE(rows.field("e")->length, 2);
  QVERIFY(rows.field("f")->expand && rows.field("f")->kind == SchemaNode::Kind::Map);
  QVERIFY(rows.field("g")->kind == SchemaNode::Kind::Object && rows.field("g")->block);  // fields => object
  QCOMPARE(rows.field("g")->alt, std::string("number"));
  QVERIFY(rows.field("i")->kind == SchemaNode::Kind::Json);
  QVERIFY(!set.forFile("data/other.json"));
}

void SchemaTest::mistakesInASchemaFileAreReportedNotFatal() {
  SchemaSet set;
  set.addFromText("broken.json", "{ not json");
  set.addFromText("nofiles.json", R"({ "rows": { "type": "object" } })");
  set.addFromText("badtype.json", R"({ "files": ["x.json"], "rows": { "fields": { "a": { "type": "banana" }, "b": { "type": "enum" },
                                         "c": { "type": "ref" }, "d": { "type": "list" } } } })");
  const QString all = set.errors().join("\n");
  QVERIFY(all.contains("broken.json: not valid JSON"));
  QVERIFY(all.contains("nofiles.json: \"files\" is missing"));
  QVERIFY(all.contains("unknown type \"banana\""));
  QVERIFY(all.contains("enum without \"values\""));
  QVERIFY(all.contains("ref without \"ref\""));
  QVERIFY(all.contains("list without \"of\""));
}

void SchemaTest::filePatternsMatchMaps() {
  SchemaSet set;
  set.addFromText("maps.json", R"({ "files": ["maps/*.json"], "root": { "type": "object" } })");
  QVERIFY(set.forFile("maps/duel.json"));
  QVERIFY(!set.forFile("game.json"));
  QVERIFY(!set.forFile("data/maps.json"));
}

void SchemaTest::typeOkAndNodeAccepts() {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["t.json"], "rows": { "fields": {
      "n": { "type": "int", "min": 0, "max": 10 }, "s": { "type": "string" }, "e": { "type": "enum", "values": ["a"] },
      "p": { "type": "list", "length": 2, "of": { "type": "int" } }, "a": { "type": "object", "alt": "number", "fields": {} } } } })");
  const SchemaNode& r = *set.forFile("t.json")->rows;
  const SchemaNode& n = *r.field("n");
  QVERIFY(typeOk(n, Json::makeNumber(5)));
  QVERIFY(!typeOk(n, Json::makeNumber(5.5)));  // not whole
  QVERIFY(!typeOk(n, Json::makeString("5")));
  QVERIFY(typeOk(n, Json::makeNumber(99)));    // right kind, wrong range: still the right kind
  QString why;
  QVERIFY(!nodeAccepts(n, Json::makeNumber(99), &why));
  QVERIFY(why.contains("maximum"));
  QVERIFY(!nodeAccepts(n, Json::makeNumber(-1), &why));
  QVERIFY(why.contains("minimum"));
  QVERIFY(nodeAccepts(n, Json::makeNumber(0)) && nodeAccepts(n, Json::makeNumber(10)));
  QVERIFY(typeOk(*r.field("e"), Json::makeString("zzz")));  // a word outside the list is the right kind...
  QVERIFY(!nodeAccepts(*r.field("e"), Json::makeString("zzz"), &why));  // ...but not an accepted one
  QVERIFY(!typeOk(*r.field("p"), parse("[1,2,3]")));
  QVERIFY(typeOk(*r.field("p"), parse("[1,2]")));
  const SchemaNode& alt = *r.field("a");
  QVERIFY(typeOk(alt, Json::makeNumber(1)) && isAlt(alt, Json::makeNumber(1)));
  QVERIFY(!isAlt(alt, parse("{}")));
  QVERIFY(!typeOk(alt, Json::makeString("x")));
}

void SchemaTest::defaultsAndSamples() {
  SchemaSet set;
  set.addFromText("t.json", R"({ "files": ["t.json"], "rows": { "fields": {
      "id": { "type": "string", "required": true }, "n": { "type": "int", "min": 3 }, "b": { "type": "bool", "default": true },
      "r": { "type": "ref", "ref": "ids" }, "m": { "type": "map", "ref": "ids", "of": { "type": "int" } },
      "blk": { "block": true, "fields": { "first": { "type": "int", "min": 2 }, "second": { "type": "string" } } },
      "lst": { "type": "list", "of": { "type": "int" } }, "pair": { "type": "list", "length": 2, "of": { "type": "int" } },
      "own": { "type": "int", "sample": 42 } } } })");
  const SchemaNode& r = *set.forFile("t.json")->rows;
  const IdLookup ids = [](const std::vector<std::string>&) { return QStringList{"one", "two"}; };
  // A new list item / block: required keys only.
  const Json d = defaultValue(r, ids);
  QVERIFY(d.has("id") && !d.has("n"));
  QCOMPARE(defaultValue(*r.field("b"), ids).asBool(), true);  // a stated default wins
  QCOMPARE(defaultValue(*r.field("r"), ids).asString().c_str(), "one");
  // Samples: valid values within the stated ranges; a block states at least its first key; maps use the first id.
  QCOMPARE(sampleValue(*r.field("n"), ids).asInt(), 3);
  QCOMPARE(sampleValue(*r.field("b"), ids).asBool(), false);  // the other value than the default: the test sees a change
  const Json blk = sampleValue(*r.field("blk"), ids);
  QCOMPARE(blk["first"].asInt(), 2);
  QCOMPARE(sampleValue(*r.field("m"), ids).dump(0).c_str(), "{\"one\":1}");
  QCOMPARE(sampleValue(*r.field("lst"), ids).size(), size_t(1));
  QCOMPARE(sampleValue(*r.field("pair"), ids).size(), size_t(2));
  QCOMPARE(sampleValue(*r.field("own"), ids).asInt(), 42);
}

void SchemaTest::idSourcesFollowTheProject() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("g/game.json"), R"({ "stats": [ {"id": "hp"}, {"id": "str"} ], "damageTypes": [ {"id": "fire"} ], "words": ["a", "b"] })");
  ote_test::writeFile(tmp.file("g/data/units.json"), R"({ "units": [ {"id": "lancer"}, {"id": "archer"} ] })");
  ote_test::writeFile(tmp.file("g/data/empty.json"), R"({ "things": [] })");
  Project p(tmp.file("g"));
  IdSource ids(&p);
  QCOMPARE(ids.ids({"data/units.json"}), (QStringList{"lancer", "archer"}));
  QCOMPARE(ids.ids({"game.json#stats", "game.json#damageTypes"}), (QStringList{"hp", "str", "fire"}));
  QCOMPARE(ids.ids({"game.json#words"}), (QStringList{"a", "b"}));  // plain strings count
  QCOMPARE(ids.ids({"data/empty.json"}), QStringList());
  QCOMPARE(ids.ids({"data/missing.json"}), QStringList());
  QCOMPARE(ids.ids({"game.json#nothing.here"}), QStringList());
  // An unsaved edit shows at once and is announced.
  QSignalSpy spy(&ids, &IdSource::idsChanged);
  JsonDocument* units = p.document("data/units.json");
  Json item = Json::makeObject();
  item.set("id", Json::makeString("scout"));
  p.push(units, new InsertCommand(units, {PathStep::member("units")}, 2, "", item));
  QVERIFY(spy.count() >= 1);
  QCOMPARE(ids.ids({"data/units.json"}), (QStringList{"lancer", "archer", "scout"}));
  p.undoStack()->undo();
  QCOMPARE(ids.ids({"data/units.json"}), (QStringList{"lancer", "archer"}));
}

void SchemaTest::theProgramsSchemasLoadWithoutErrors() {
  SchemaSet source;
  source.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  QVERIFY2(source.errors().isEmpty(), qPrintable(source.errors().join("\n")));
  for (const char* f : {"game.json", "data/weapons.json", "data/units.json", "data/research.json", "data/factions.json", "data/jobs.json",
                        "data/commanders.json", "maps/field.json", "data/terrain.json", "data/items.json"})
    QVERIFY2(source.forFile(f), f);
  // Every node that points at ids says where they come from; every enum has words (checked by the loader); every help line is one line.
  for (const auto& fs : source.all()) {
    const SchemaNode* top = fs->rows ? fs->rows.get() : fs->root.get();
    std::function<void(const SchemaNode&, const QString&)> walk = [&](const SchemaNode& n, const QString& path) {
      QVERIFY2(n.help.find('\n') == std::string::npos, qPrintable(path));
      for (const auto& f : n.fields) {
        QVERIFY2(!f.second->help.empty() || f.first == "id" || f.first == "name", qPrintable(path + "." + QString::fromStdString(f.first) + " has no help line"));
        walk(*f.second, path + "." + QString::fromStdString(f.first));
      }
      if (n.of) walk(*n.of, path + "[]");
    };
    walk(*top, QString::fromStdString(fs->name));
  }
}

void SchemaTest::schemaFolderOverridesTheBuiltInCopy() {
  ote_test::Scratch tmp;
  ote_test::writeFile(tmp.file("mine/only.json"), R"({ "files": ["data/only.json"], "rows": { "fields": {} } })");
  const SchemaSet mine = SchemaSet::loadDefault(tmp.file("mine"), tmp.path());
  QCOMPARE((int)mine.all().size(), 1);
  QVERIFY(mine.forFile("data/only.json"));
  QVERIFY(!mine.forFile("data/units.json"));
  // A "schema" folder next to the program comes second.
  ote_test::writeFile(tmp.file("app/schema/beside.json"), R"({ "files": ["data/beside.json"], "root": { "type": "object" } })");
  const SchemaSet beside = SchemaSet::loadDefault(QString(), tmp.file("app"));
  QVERIFY(beside.forFile("data/beside.json") && !beside.forFile("data/units.json"));
  // Neither: the copy inside the program.
  const SchemaSet builtIn = SchemaSet::loadDefault(QString(), tmp.file("nowhere"));
  QVERIFY(builtIn.forFile("data/units.json") && builtIn.forFile("game.json"));
  QVERIFY(builtIn.errors().isEmpty());
}

QTEST_MAIN(SchemaTest)
#include "test_schema.moc"
