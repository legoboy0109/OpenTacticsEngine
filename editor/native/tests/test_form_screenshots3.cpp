// Screenshots of milestone 3 (QWidget::grab, offscreen) at 1280x720 and 1920x1080 saved as PNGs to $OTE_SHOT_DIR: a skills form, an
// item with kind-specific fields, characters, the campaign (still the generic tree), a map's sides / units, an event (still the
// generic tree), the General page of game.json, and the three table / form view states. Clipping and crowding cannot be asserted,
// only seen; the test checks each picture is real and the scene shows what its name says.
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QSettings>
#include <QTabWidget>
#include <QToolButton>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_view.h"
#include "json_table_model.h"
#include "main_window.h"
#include "project_session.h"
#include "table_editor.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;

namespace {

struct Shell {
  ote_test::Scratch tmp;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<MainWindow> w;
  Shell(int width, int height) {
    settings = std::make_unique<QSettings>(tmp.file("s.ini"), QSettings::IniFormat);
    w = std::make_unique<MainWindow>(settings.get());
    w->setPrompter(std::make_unique<ote_test::ScriptedPrompter>());
    w->setSchemaDir(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    w->resize(width, height);
    w->show();
  }
  bool openDemo() {
    if (!w->openProject(ote_test::copyDemoGame(tmp))) return false;
    ProblemsRunner* r = w->session()->problems();
    QSignalSpy done(r, &ProblemsRunner::finished);
    return !r->isRunning() || done.wait(60000);
  }
  TableEditor* table(const QString& rel) {
    w->openFile(rel);
    return qobject_cast<TableEditor*>(w->editorFor(rel));
  }
  // The row whose `key` list / value has the most content: the demo row that shows the most of a form.
  static int richestRow(TableEditor* t, const char* key, const char* mustHave = nullptr, const char* mustEqual = nullptr) {
    int best = 0;
    size_t bestSize = 0;
    for (int r = 0; r < t->model()->rowCount(); ++r) {
      const ote::Json* row = t->model()->rowValue(r);
      if (!row) continue;
      if (mustHave && !(*row).has(mustHave)) continue;
      if (mustEqual && (*row)["kind"].asString() != mustEqual) continue;
      const size_t size = (*row)[key].dump(0).size();
      if (size > bestSize) { bestSize = size; best = r; }
    }
    return best;
  }
};

}  // namespace

class Shots3Test : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void scene_data();
  void scene();

 private:
  void save(QWidget* w, const QString& name, int ew, int eh);
  QString shotDir_;
};

void Shots3Test::initTestCase() {
  shotDir_ = qEnvironmentVariable("OTE_SHOT_DIR");
  if (shotDir_.isEmpty()) shotDir_ = qEnvironmentVariable("OTE_TEST_TMP", QDir::tempPath()) + "/native-shots3";
  QVERIFY(QDir().mkpath(shotDir_));
}

void Shots3Test::save(QWidget* w, const QString& name, int ew, int eh) {
  QTest::qWait(80);
  const QImage img = w->grab().toImage();
  QCOMPARE(img.width(), ew);
  QCOMPARE(img.height(), eh);
  QSet<QRgb> colours;
  for (int y = 0; y < img.height(); y += 7)
    for (int x = 0; x < img.width(); x += 7) colours.insert(img.pixel(x, y));
  QVERIFY2(colours.size() > 8, "the picture is blank");
  QVERIFY(img.save(shotDir_ + "/" + name));
  qInfo("screenshot: %s/%s", qPrintable(shotDir_), qPrintable(name));
}

void Shots3Test::scene_data() {
  QTest::addColumn<QString>("scene");
  QTest::addColumn<int>("w");
  QTest::addColumn<int>("h");
  for (const char* s : {"skills", "items", "characters", "campaign", "mapsides", "mapevents", "general", "view-table", "view-both", "view-form"})
    for (auto sz : {QSize(1280, 720), QSize(1920, 1080)})
      QTest::newRow(qPrintable(QString("%1-%2x%3").arg(s).arg(sz.width()).arg(sz.height()))) << QString(s) << sz.width() << sz.height();
}

void Shots3Test::scene() {
  QFETCH(QString, scene);
  QFETCH(int, w);
  QFETCH(int, h);
  Shell s(w, h);
  QVERIFY(s.openDemo());
  auto settle = [&](FormView* f) {
    QTest::qWait(80);  // the deferred form retarget runs, then layouts settle
    f->flush();
    QCoreApplication::processEvents();
  };
  if (scene == "skills") {
    TableEditor* t = s.table("data/skills.json");
    QVERIFY(t && t->form());
    t->selectRow(Shell::richestRow(t, "blocks"));
    settle(t->form());
    t->form()->expandAll();
    QVERIFY(t->form()->findChild<QWidget*>("variantField"));
  } else if (scene == "items") {
    TableEditor* t = s.table("data/items.json");
    QVERIFY(t && t->form());
    t->selectRow(Shell::richestRow(t, "effects", "effects"));
    settle(t->form());
    t->form()->expandAll();
  } else if (scene == "characters") {
    TableEditor* t = s.table("data/characters.json");
    QVERIFY(t && t->form());
    t->selectRow(Shell::richestRow(t, "quotes", "quotes"));
    settle(t->form());
    t->form()->expandAll();
  } else if (scene == "campaign") {
    s.w->openFile("data/campaign.json");  // not described yet: the generic tree of milestone 1
    QVERIFY(s.w->editorFor("data/campaign.json"));
  } else if (scene == "mapsides") {
    const QString rel = s.w->session()->project()->jsonFiles().filter(QRegularExpression("^maps/")).value(0);
    s.w->openFile(rel);
    auto* t = qobject_cast<TreeEditor*>(s.w->editorFor(rel));
    QVERIFY(t && t->form());
    t->showFormPage();
    settle(t->form());
    t->form()->expandAll();
    QVERIFY(t->form()->focusPath({PathStep::member("sides")}));
  } else if (scene == "mapevents") {
    const QString rel = s.w->session()->project()->jsonFiles().filter(QRegularExpression("^maps/")).value(0);
    s.w->openFile(rel);
    auto* t = qobject_cast<TreeEditor*>(s.w->editorFor(rel));
    QVERIFY(t);
    t->showTreePage();  // events are not described yet: the tree is where they are edited
    t->selectPath({PathStep::member("events")});
  } else if (scene == "general") {
    s.w->openFile("game.json");
    auto* t = qobject_cast<TreeEditor*>(s.w->editorFor("game.json"));
    QVERIFY(t && t->form());
    t->showFormPage();
    settle(t->form());
  } else {  // view-table / view-both / view-form
    TableEditor* t = s.table("data/units.json");
    QVERIFY(t && t->form());
    t->selectRow(1);
    t->setViewMode(scene == "view-table" ? 0 : scene == "view-both" ? 1 : 2);
    settle(t->form());
  }
  save(s.w.get(), QString("m3-%1-%2x%3.png").arg(scene).arg(w).arg(h), w, h);
}

QTEST_MAIN(Shots3Test)
#include "test_form_screenshots3.moc"
