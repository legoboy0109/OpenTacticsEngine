// Screenshots of the schema forms (QWidget::grab) at 1280x720 and 1920x1080, light and dark palette, saved as PNGs to
// $OTE_SHOT_DIR so a person can look at them: clipped text, cramped fields and unreadable colours cannot be asserted, only
// seen. The test checks that each picture is real and that the scene shows what its name says.
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QPalette>
#include <QPushButton>
#include <QSettings>
#include <QStyleFactory>
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

void applyPalette(bool dark) {
  static const QPalette original = QApplication::palette();
  if (!dark) {
    QApplication::setPalette(original);
    return;
  }
  QPalette p;
  p.setColor(QPalette::Window, QColor(43, 43, 43));
  p.setColor(QPalette::WindowText, QColor(230, 230, 230));
  p.setColor(QPalette::Base, QColor(30, 30, 30));
  p.setColor(QPalette::AlternateBase, QColor(40, 40, 40));
  p.setColor(QPalette::Text, QColor(230, 230, 230));
  p.setColor(QPalette::Button, QColor(58, 58, 58));
  p.setColor(QPalette::ButtonText, QColor(230, 230, 230));
  p.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
  p.setColor(QPalette::Highlight, QColor(42, 130, 218));
  p.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
  p.setColor(QPalette::ToolTipBase, QColor(58, 58, 58));
  p.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
  p.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
  p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
  p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
  QApplication::setStyle(QStyleFactory::create("Fusion"));
  QApplication::setPalette(p);
}

struct Shell {
  ote_test::Scratch tmp;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<MainWindow> w;
  QString folder;
  Shell(int width, int height) {
    settings = std::make_unique<QSettings>(tmp.file("s.ini"), QSettings::IniFormat);
    w = std::make_unique<MainWindow>(settings.get());
    w->setPrompter(std::make_unique<ote_test::ScriptedPrompter>());
    w->setSchemaDir(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    w->resize(width, height);
    w->show();
  }
  bool openFixture(const QString& name) { return open(ote_test::copyFixture(tmp, name)); }
  bool open(const QString& dir) {
    folder = dir;
    if (!w->openProject(dir)) return false;
    ProblemsRunner* r = w->session()->problems();
    QSignalSpy done(r, &ProblemsRunner::finished);
    return !r->isRunning() || done.wait(60000);
  }
  TableEditor* table(const QString& rel, int row) {
    w->openFile(rel);
    auto* t = qobject_cast<TableEditor*>(w->editorFor(rel));
    if (t) t->selectRow(row);
    return t;
  }
};

}  // namespace

class FormScreenshotTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void cleanup() { applyPalette(false); }
  void scene_data();
  void scene();

 private:
  void save(QWidget* w, const QString& name, int ew, int eh);
  QString shotDir_;
};

void FormScreenshotTest::initTestCase() {
  shotDir_ = qEnvironmentVariable("OTE_SHOT_DIR");
  if (shotDir_.isEmpty()) shotDir_ = qEnvironmentVariable("OTE_TEST_TMP", QDir::tempPath()) + "/native-form-shots";
  QVERIFY(QDir().mkpath(shotDir_));
}

void FormScreenshotTest::save(QWidget* w, const QString& name, int ew, int eh) {
  QTest::qWait(60);
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

void FormScreenshotTest::scene_data() {
  QTest::addColumn<QString>("scene");
  QTest::addColumn<int>("w");
  QTest::addColumn<int>("h");
  QTest::addColumn<bool>("dark");
  for (const char* s : {"units", "weapons", "rules", "factions", "jobs", "commanders", "mismatch", "otherkeys"})
    for (bool dark : {false, true})
      for (auto sz : {QSize(1280, 720), QSize(1920, 1080)})
        QTest::newRow(qPrintable(QString("%1-%2x%3-%4").arg(s).arg(sz.width()).arg(sz.height()).arg(dark ? "dark" : "light")))
            << QString(s) << sz.width() << sz.height() << dark;
}

void FormScreenshotTest::scene() {
  QFETCH(QString, scene);
  QFETCH(int, w);
  QFETCH(int, h);
  QFETCH(bool, dark);
  applyPalette(dark);
  Shell s(w, h);
  const QString name = QString("form-%1-%2x%3-%4.png").arg(scene).arg(w).arg(h).arg(dark ? "dark" : "light");

  if (scene == "units") {
    QVERIFY(s.open(ote_test::copyDemoGame(s.tmp)));
    TableEditor* t = s.table("data/units.json", 1);
    QVERIFY(t && t->form() && t->form()->hasTarget());
  } else if (scene == "weapons") {
    QVERIFY(s.openFixture("damage-types"));
    TableEditor* t = s.table("data/weapons.json", 1);
    QVERIFY(t);
    // Give the weapon an area attack and a damage type through the form's own edit path, as a user would.
    const JsonPath row = {PathStep::member("weapons"), PathStep::at(1)};
    auto child = [&](const char* k) { JsonPath p = row; p.push_back(PathStep::member(k)); return p; };
    ote::Json area = ote::Json::makeObject();
    area.set("shape", ote::Json::makeString("radius"));
    area.set("radius", ote::Json::makeNumber(2));
    area.set("friendlyFire", ote::Json::makeBool(true));
    t->formContext()->set(child("area"), area);
    t->formContext()->set(child("damageType"), ote::Json::makeString("fire"));
    t->formContext()->set(child("fire"), ote::Json::makeString("line"));
    t->form()->flush();
  } else if (scene == "rules") {
    QVERIFY(s.openFixture("action-points"));
    s.w->openFile("game.json");
    auto* t = qobject_cast<TreeEditor*>(s.w->editorFor("game.json"));
    QVERIFY(t && t->form());
    t->showFormPage();
    QVERIFY(t->form()->focusPath({PathStep::member("rules"), PathStep::member("actionPoints")}));
  } else if (scene == "factions") {
    QVERIFY(s.openFixture("factions"));
    QVERIFY(s.table("data/factions.json", 0));
  } else if (scene == "jobs") {
    QVERIFY(s.openFixture("jobs"));
    QVERIFY(s.table("data/jobs.json", 1));
  } else if (scene == "commanders") {
    QVERIFY(s.openFixture("commanders"));
    QVERIFY(s.table("data/commanders.json", 0));
  } else if (scene == "mismatch") {
    QVERIFY(s.open(ote_test::copyDemoGame(s.tmp)));
    TableEditor* t = s.table("data/weapons.json", 0);
    QVERIFY(t);
    const JsonPath row = {PathStep::member("weapons"), PathStep::at(0)};
    auto child = [&](const char* k) { JsonPath p = row; p.push_back(PathStep::member(k)); return p; };
    t->formContext()->set(child("range"), ote::Json::makeString("far"));
    t->formContext()->set(child("counter"), ote::Json::makeString("yes"));
    t->formContext()->set(child("apCost"), ote::Json::makeNumber(500));  // a number outside the range the schema states
    t->form()->flush();
    QVERIFY(t->form()->findChild<QWidget*>("fieldNote"));
  } else if (scene == "otherkeys") {
    QVERIFY(s.open(ote_test::copyDemoGame(s.tmp)));
    TableEditor* t = s.table("data/units.json", 0);
    QVERIFY(t);
    t->form()->flush();
    auto* toggle = t->form()->findChild<QToolButton*>("otherKeysToggle");
    QVERIFY2(toggle, "the demo's units have keys the schema does not describe");
    toggle->setChecked(true);
    t->form()->ensureWidgetVisible(toggle);
  }
  save(s.w.get(), name, w, h);
}

QTEST_MAIN(FormScreenshotTest)
#include "test_form_screenshots.moc"
