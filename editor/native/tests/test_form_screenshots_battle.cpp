// Screenshots of the battle-art forms and of dragging a list item (QWidget::grab, offscreen) at 1280x720, saved as PNGs to $OTE_SHOT_DIR:
// the battle section of graphics.json (sets, effects, stage) and a command list while a command is dragged (the item dimmed, the
// floating copy, the line where it lands) and right after the drop. Clipping and crowding cannot be asserted, only seen; the test checks
// that each picture is real and that the scene shows what its name says.
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QScrollBar>
#include <QSettings>
#include <QSignalSpy>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_view.h"
#include "main_window.h"
#include "project_session.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

namespace {

JsonPath path(const QString& text) {
  JsonPath out;
  for (const QString& part : text.split('.')) {
    const int br = part.indexOf('[');
    if (br < 0) { out.push_back(PathStep::member(part.toStdString())); continue; }
    if (br > 0) out.push_back(PathStep::member(part.left(br).toStdString()));
    out.push_back(PathStep::at(part.mid(br + 1, part.size() - br - 2).toUInt()));
  }
  return out;
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
  bool open(const QString& fixture) {
    folder = ote_test::copyFixture(tmp, fixture);
    if (!w->openProject(folder)) return false;
    ProblemsRunner* r = w->session()->problems();
    QSignalSpy done(r, &ProblemsRunner::finished);
    return !r->isRunning() || done.wait(60000);
  }
  TreeEditor* tree(const QString& rel) {
    w->openFile(rel);
    auto* t = qobject_cast<TreeEditor*>(w->editorFor(rel));
    if (t && t->form()) t->showFormPage();
    return t;
  }
};

}  // namespace

class ShotsBattleTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void battleForms_data();
  void battleForms();
  void draggingACommand();

 private:
  void save(QWidget* w, const QString& name);
  QString shotDir_;
};

void ShotsBattleTest::initTestCase() {
  shotDir_ = qEnvironmentVariable("OTE_SHOT_DIR");
  if (shotDir_.isEmpty()) shotDir_ = qEnvironmentVariable("OTE_TEST_TMP", QDir::tempPath()) + "/native-shots-battle";
  QVERIFY(QDir().mkpath(shotDir_));
}

void ShotsBattleTest::save(QWidget* w, const QString& name) {
  QTest::qWait(80);
  const QImage img = w->grab().toImage();
  QCOMPARE(img.width(), 1280);
  QCOMPARE(img.height(), 720);
  QSet<QRgb> colours;
  for (int y = 0; y < img.height(); y += 7)
    for (int x = 0; x < img.width(); x += 7) colours.insert(img.pixel(x, y));
  QVERIFY2(colours.size() > 8, "the picture is blank");
  QVERIFY(img.save(shotDir_ + "/" + name));
  qInfo("screenshot: %s/%s", qPrintable(shotDir_), qPrintable(name));
}

void ShotsBattleTest::battleForms_data() {
  QTest::addColumn<QString>("scene");
  QTest::addColumn<QString>("top");
  QTest::addColumn<QString>("open");
  QTest::newRow("sets") << "sets" << "battle.sets.hero_set" << "battle.sets.hero_set.animations.attack.sounds";
  QTest::newRow("effects") << "effects" << "battle.effects[0]" << "battle.effects[0].shake[0]";
  QTest::newRow("effect-lists") << "effect-lists" << "battle.effects[4]" << "battle.effects[4].byType.archer";
  QTest::newRow("stage") << "stage" << "battle.stage" << "battle.stage.ranges[1].pan";
}

void ShotsBattleTest::battleForms() {
  QFETCH(QString, scene);
  QFETCH(QString, top);
  QFETCH(QString, open);
  Shell s(1280, 720);
  QVERIFY(s.open("battle-art"));
  TreeEditor* t = s.tree("data/graphics.json");
  QVERIFY(t && t->form());
  FormView* f = t->form();
  auto settle = [&] {
    QTest::qWait(80);
    f->flush();
    QCoreApplication::processEvents();
  };
  f->expandAlong(path(open));
  settle();
  f->focusPath(path(top));
  settle();
  if (QWidget* a = f->anchorWidget(path(top))) f->verticalScrollBar()->setValue(a->mapTo(f->widget(), QPoint(0, 0)).y() - 8);
  settle();
  save(s.w.get(), QString("battle-%1-1280x720.png").arg(scene));
}

void ShotsBattleTest::draggingACommand() {
  Shell s(1280, 720);
  QVERIFY(s.open("srpg-rename"));
  TreeEditor* t = s.tree("maps/field.json");
  QVERIFY(t && t->form());
  FormView* f = t->form();
  auto settle = [&] {
    QTest::qWait(80);
    f->flush();
    QCoreApplication::processEvents();
  };
  const JsonPath list = path("events[7].do");
  QVERIFY(t->formContext()->at(list) && t->formContext()->at(list)->size() == 20);
  f->expandAlong(list);  // (the commands stay collapsed, one line each)
  settle();
  f->focusPath(list);
  settle();
  if (QWidget* a = f->anchorWidget(list)) f->verticalScrollBar()->setValue(a->mapTo(f->widget(), QPoint(0, 0)).y() - 8);
  settle();
  const QString before = QString::fromStdString(t->formContext()->at(list)->dump(0));
  QWidget* grip = f->dragHandleAt(list, 1);
  QWidget* target = f->dragHandleAt(list, 5);
  QVERIFY(grip && target);
  const QPoint start = grip->rect().center();
  QTest::mousePress(grip, Qt::LeftButton, {}, start);
  QTest::mouseMove(grip, start + QPoint(0, 12));
  const QPoint goal = target->mapToGlobal(QPoint(120, target->height() / 2 + 8));
  QTest::mouseMove(grip, grip->mapFromGlobal(goal));
  QVERIFY(f->isDragging());
  JsonPath dst;
  size_t at = 0;
  QVERIFY(f->dropTarget(&dst, &at));
  save(s.w.get(), "drag-1-while-dragging-1280x720.png");
  QTest::mouseRelease(grip, Qt::LeftButton, {}, grip->mapFromGlobal(goal));
  settle();
  QVERIFY(!f->isDragging());
  QVERIFY(QString::fromStdString(t->formContext()->at(list)->dump(0)) != before);
  // The command that was second is now fifth (the ones between moved up one place), and its grip has the focus.
  save(s.w.get(), "drag-2-after-the-drop-1280x720.png");
}

QTEST_MAIN(ShotsBattleTest)
#include "test_form_screenshots_battle.moc"
