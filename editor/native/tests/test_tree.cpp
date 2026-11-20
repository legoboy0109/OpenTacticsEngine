// The generic JSON tree: structure, typed scalar edits, add/remove/rename, type changes, undo, expansion that survives edits.
#include <QAbstractItemModelTester>
#include <QLabel>
#include <QTreeView>
#include <QtTest>
#include <memory>

#include "json_tree_model.h"
#include "project.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

namespace {
const char* kDoc = R"({"name": "Demo", "size": {"w": 8, "h": 6}, "layers": [{"id": "a", "on": true}, {"id": "b", "on": false}], "note": null, "zeta": 1})";

struct Fixture {
  ote_test::Scratch tmp;
  std::unique_ptr<Project> project;
  JsonDocument* doc = nullptr;
  std::unique_ptr<TreeEditor> editor;
  explicit Fixture(const char* json = kDoc) {
    Json j;
    std::string err;
    Json::parse(json, j, err);
    ote_test::writeFile(tmp.file("game.json"), QByteArray::fromStdString(j.dump(2) + "\n"));
    project = std::make_unique<Project>(tmp.path());
    doc = project->document("game.json");
    editor = std::make_unique<TreeEditor>(project.get(), doc);
  }
  JsonTreeModel* model() { return editor->model(); }
  QModelIndex at(std::initializer_list<PathStep> steps) { return model()->indexForPath(JsonPath(steps)); }
  QModelIndex valueAt(std::initializer_list<PathStep> steps) { return at(steps).siblingAtColumn(JsonTreeModel::ValueColumn); }
};
PathStep M(const char* k) { return PathStep::member(k); }
PathStep I(size_t i) { return PathStep::at(i); }
}  // namespace

class TreeTest : public QObject {
  Q_OBJECT
 private slots:
  void showsStructureWithKeysValuesAndTypes();
  void modelPassesQtsConsistencyChecks();
  void scalarEditsKeepTypeAndRefuseWrongText();
  void addRemoveRenameAndTypeChangeUndoToTheSameBytes();
  void expansionSurvivesAnEditElsewhere();
  void demoGameJsonRoundTrips();
  void selectPathOpensTheBranch();
};

void TreeTest::showsStructureWithKeysValuesAndTypes() {
  Fixture f;
  JsonTreeModel* m = f.model();
  QCOMPARE(m->rowCount(), 5);
  QCOMPARE(m->index(0, 0).data().toString(), QString("name"));
  QCOMPARE(m->index(0, 1).data().toString(), QString("Demo"));
  QCOMPARE(m->index(0, 2).data().toString(), QString("string"));
  QCOMPARE(m->index(1, 1).data().toString(), QString("{2 keys}"));
  QCOMPARE(m->index(2, 1).data().toString(), QString("[2 items]"));
  QCOMPARE(f.at({M("layers"), I(1)}).data().toString(), QString("[1]"));
  QCOMPARE(f.valueAt({M("layers"), I(1), M("on")}).data(Qt::CheckStateRole).toInt(), (int)Qt::Unchecked);
  QCOMPARE(f.valueAt({M("note")}).data().toString(), QString("null"));
  QVERIFY(!f.at({M("size"), M("nope")}).isValid());
}

void TreeTest::modelPassesQtsConsistencyChecks() {
  Fixture f;
  QAbstractItemModelTester tester(f.model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
  f.editor->view()->expandAll();
  f.model()->setData(f.valueAt({M("size"), M("w")}), "9", Qt::EditRole);
  f.model()->addMember(f.at({M("size")}), "d", Json::makeNumber(1));
  f.model()->addItem(f.at({M("layers")}), Json::makeObject());
  f.model()->renameKey(f.at({M("name")}), "title");
  f.model()->setValue(f.at({M("layers")}), Json::makeString("flat"));  // container replaced by a scalar
  f.model()->setValue(f.at({M("note")}), Json::makeArray());
  f.model()->removeNode(f.at({M("zeta")}));
  while (f.project->undoStack()->canUndo()) f.project->undoStack()->undo();
  while (f.project->undoStack()->canRedo()) f.project->undoStack()->redo();
  f.project->reload("game.json");
  QCOMPARE(f.model()->rowCount(), 5);
}

void TreeTest::scalarEditsKeepTypeAndRefuseWrongText() {
  Fixture f;
  QSignalSpy rejected(f.model(), &JsonTreeModel::editRejected);
  QVERIFY(f.model()->setData(f.valueAt({M("size"), M("w")}), "12", Qt::EditRole));
  QVERIFY(f.doc->root()["size"]["w"].isNumber());
  QCOMPARE(f.doc->root()["size"]["w"].asInt(), 12);
  QVERIFY(!f.model()->setData(f.valueAt({M("size"), M("w")}), "wide", Qt::EditRole));
  QCOMPARE(rejected.count(), 1);
  QVERIFY(f.editor->messageLabel()->text().contains("not a number"));
  QVERIFY(f.editor->messageLabel()->text().contains("size.w"));
  QVERIFY(f.model()->setData(f.valueAt({M("layers"), I(0), M("on")}), Qt::Unchecked, Qt::CheckStateRole));
  QVERIFY(f.doc->root()["layers"][0]["on"].isBool());
  QVERIFY(!f.doc->root()["layers"][0]["on"].asBool());
  QVERIFY(f.model()->setData(f.valueAt({M("name")}), "123", Qt::EditRole));
  QVERIFY(f.doc->root()["name"].isString());
  // Containers are not edited inline.
  QVERIFY(!(f.model()->flags(f.valueAt({M("size")})) & Qt::ItemIsEditable));
  // Array positions cannot be renamed.
  QVERIFY(!(f.model()->flags(f.at({M("layers"), I(0)})) & Qt::ItemIsEditable));
}

void TreeTest::addRemoveRenameAndTypeChangeUndoToTheSameBytes() {
  Fixture f;
  const QByteArray original = f.doc->serialize();
  QTreeView* v = f.editor->view();
  QStringList asked;
  f.editor->setKeyPrompt([&](const QString& t) { asked << t; return QString("fresh"); });

  v->setCurrentIndex(f.at({M("size")}));
  QVERIFY(f.editor->addChild(Json::Type::Number));  // inside the selected object, prompted for a key
  QCOMPARE(asked.size(), 1);
  QVERIFY(f.doc->root()["size"].has("fresh"));
  QVERIFY(f.doc->root()["size"]["fresh"].isNumber());
  QCOMPARE(f.doc->root()["size"].members().back().first, std::string("fresh"));  // appended at the end

  v->setCurrentIndex(f.at({M("layers")}));
  QVERIFY(f.editor->addChild(Json::Type::Object));  // arrays need no key
  QCOMPARE(asked.size(), 1);
  QCOMPARE(f.doc->root()["layers"].size(), (size_t)3);

  v->setCurrentIndex(f.at({M("name")}));            // beside a scalar: goes into its parent (the root object)
  QVERIFY(f.editor->addChild(Json::Type::String));
  QVERIFY(f.doc->root().has("fresh"));

  QVERIFY(!f.model()->addMember(f.at({}), "fresh", Json()));  // duplicate key refused
  QVERIFY(f.editor->messageLabel()->text().contains("already exists"));
  QVERIFY(!f.model()->addMember(f.at({M("layers")}), "k", Json()));  // not an object

  // Rename keeps the position and the value.
  QVERIFY(f.model()->renameKey(f.at({M("size")}), "dimensions"));
  QCOMPARE(f.doc->root().members()[1].first, std::string("dimensions"));
  QCOMPARE(f.doc->root()["dimensions"]["w"].asInt(), 8);
  QVERIFY(!f.model()->renameKey(f.at({M("zeta")}), "name"));  // clash

  v->setCurrentIndex(f.at({M("zeta")}));
  QVERIFY(f.editor->changeCurrentType(Json::Type::Array));
  QVERIFY(f.doc->root()["zeta"].isArray());
  v->setCurrentIndex(f.at({M("note")}));
  QVERIFY(f.editor->removeCurrent());
  QVERIFY(!f.doc->root().has("note"));

  QVERIFY(f.doc->serialize() != original);
  while (f.project->undoStack()->canUndo()) f.project->undoStack()->undo();
  QCOMPARE(f.doc->serialize(), original);
  QVERIFY(!f.doc->isDirty());
}

void TreeTest::expansionSurvivesAnEditElsewhere() {
  Fixture f;
  QTreeView* v = f.editor->view();
  v->expand(f.at({M("layers")}));
  v->expand(f.at({M("layers"), I(0)}));
  QVERIFY(v->isExpanded(f.at({M("layers"), I(0)})));
  f.model()->setData(f.valueAt({M("size"), M("w")}), "3", Qt::EditRole);
  f.model()->setData(f.valueAt({M("name")}), "Other", Qt::EditRole);
  f.model()->addMember(f.at({M("size")}), "x", Json::makeNumber(1));
  QVERIFY(v->isExpanded(f.at({M("layers")})));
  QVERIFY(v->isExpanded(f.at({M("layers"), I(0)})));
  // Inserting before an expanded array item shifts it: the expansion goes with it.
  f.model()->setValue(f.at({M("layers"), I(1)}), Json::makeString("z"));
  QVERIFY(v->isExpanded(f.at({M("layers"), I(0)})));
}

void TreeTest::demoGameJsonRoundTrips() {
  ote_test::Scratch tmp;
  const QString game = ote_test::copyDemoGame(tmp);
  Project project(game);
  QStringList seen;
  for (const char* rel : {"game.json", "data/campaign.json", "data/audio.json"}) {
    JsonDocument* doc = project.document(rel);
    QVERIFY2(doc, rel);
    TreeEditor ed(&project, doc);
    QAbstractItemModelTester tester(ed.model(), QAbstractItemModelTester::FailureReportingMode::QtTest);
    ed.view()->expandAll();
    QVERIFY(ed.model()->rowCount() > 0);
    const QByteArray before = doc->serialize();
    // Edit the first scalar we find, then undo.
    std::function<QModelIndex(const QModelIndex&)> firstScalar = [&](const QModelIndex& p) -> QModelIndex {
      for (int r = 0; r < ed.model()->rowCount(p); ++r) {
        const QModelIndex k = ed.model()->index(r, 0, p);
        const Json* j = ed.model()->jsonAt(k);
        if (j && j->isNumber()) return k;
        if (ed.model()->hasChildren(k)) {
          QModelIndex d = firstScalar(k);
          if (d.isValid()) return d;
        }
      }
      return {};
    };
    const QModelIndex num = firstScalar({});
    if (num.isValid()) {
      QVERIFY(ed.model()->setData(num.siblingAtColumn(1), "77", Qt::EditRole));
      QVERIFY(doc->isDirty());
      project.undoStack()->undo();
    }
    QCOMPARE(doc->serialize(), before);
    QVERIFY(!doc->isDirty());
    seen << rel;
  }
  QCOMPARE(seen.size(), 3);
}

void TreeTest::selectPathOpensTheBranch() {
  Fixture f;
  f.editor->selectPath({M("layers"), I(1), M("id")});
  QCOMPARE(f.editor->view()->currentIndex(), f.at({M("layers"), I(1), M("id")}));
  QVERIFY(f.editor->view()->isExpanded(f.at({M("layers"), I(1)})));
}

QTEST_MAIN(TreeTest)
#include "test_tree.moc"
