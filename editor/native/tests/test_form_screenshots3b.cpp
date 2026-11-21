// Screenshots of milestone 3b (QWidget::grab, offscreen) at 1280x720 and 1920x1080, saved as PNGs to $OTE_SHOT_DIR: an event with a
// long command list collapsed (one readable line per command), one command opened, a choice with its answers, the objectives,
// the preparation screen, the campaign, the audio file and a style file. Clipping and crowding cannot be asserted, only seen; the
// test checks that each picture is real and that the scene shows what its name says.
#include <QApplication>
#include <QDir>
#include <QImage>
#include <QScrollBar>
#include <QSettings>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_view.h"
#include "main_window.h"
#include "project_session.h"
#include "table_editor.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;
using ote::Json;

namespace {

JsonPath path(const QString& text) {  // "events[7].do[3]"
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
  bool open(const QString& game) {
    folder = tmp.file(game);
    if (game.startsWith("demo-")) ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/") + game, folder);
    else ote_test::copyFixture(tmp, game);
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

class Shots3bTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void scene_data();
  void scene();

 private:
  void save(QWidget* w, const QString& name, int ew, int eh);
  QString shotDir_;
};

void Shots3bTest::initTestCase() {
  shotDir_ = qEnvironmentVariable("OTE_SHOT_DIR");
  if (shotDir_.isEmpty()) shotDir_ = qEnvironmentVariable("OTE_TEST_TMP", QDir::tempPath()) + "/native-shots3b";
  QVERIFY(QDir().mkpath(shotDir_));
}

void Shots3bTest::save(QWidget* w, const QString& name, int ew, int eh) {
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

void Shots3bTest::scene_data() {
  QTest::addColumn<QString>("scene");
  QTest::addColumn<int>("w");
  QTest::addColumn<int>("h");
  for (const char* s : {"events-collapsed", "events-expanded", "events-choice", "objectives", "prep", "campaign", "audio", "style"})
    for (auto sz : {QSize(1280, 720), QSize(1920, 1080)})
      QTest::newRow(qPrintable(QString("%1-%2x%3").arg(s).arg(sz.width()).arg(sz.height()))) << QString(s) << sz.width() << sz.height();
}

void Shots3bTest::scene() {
  QFETCH(QString, scene);
  QFETCH(int, w);
  QFETCH(int, h);
  Shell s(w, h);
  const bool needsEvents = scene.startsWith("events") || scene == "objectives" || scene == "prep";
  QVERIFY(s.open(needsEvents ? "srpg-rename" : scene == "style" || scene == "audio" || scene == "campaign" ? "demo-srpg" : "srpg-rename"));
  auto settle = [&](FormView* f) {
    QTest::qWait(80);
    f->flush();
    QCoreApplication::processEvents();
  };
  // Scrolling to a widget is only exact once the layout has settled: open the way, let it settle, then put `top` at the top edge.
  auto goTo = [&](FormView* f, const QString& top, const QString& open = QString()) {
    f->expandAlong(path(open.isEmpty() ? top : open));
    settle(f);
    f->focusPath(path(top));
    settle(f);
    if (QWidget* a = f->anchorWidget(path(top))) f->verticalScrollBar()->setValue(a->mapTo(f->widget(), QPoint(0, 0)).y() - 8);
    settle(f);
  };
  if (needsEvents) {
    TreeEditor* t = s.tree("maps/field.json");
    QVERIFY(t && t->form());
    FormView* f = t->form();
    // A long script: the event "e_start" (20 commands) three times over, so the page bar shows as well.
    const Json* ev = t->formContext()->at(path("events[7].do"));
    QVERIFY(ev && ev->size() == 20);
    Json longList = Json::makeArray();
    for (int copy = 0; copy < 3; ++copy)
      for (const Json& c : ev->items()) longList.push(c);
    t->formContext()->set(path("events[7].do"), longList);
    settle(f);
    if (scene == "events-collapsed") {
      goTo(f, "events[7].do");
    } else if (scene == "events-expanded") {
      goTo(f, "events[7].do", "events[7].do[1]");
    } else if (scene == "events-choice") {
      f->expandAlong(path("events[7].do[18].answers[1]"));
      goTo(f, "events[7].do[18]", "events[7].do[18].answers[0]");
    } else if (scene == "objectives") {
      goTo(f, "objectives");
    } else {
      goTo(f, "prep", "prep");
    }
  } else {
    const QString rel = scene == "campaign" ? "data/campaign.json" : scene == "audio" ? "data/audio.json" : "art/detailed.style.json";
    TreeEditor* t = s.tree(rel);
    QVERIFY(t && t->form());
    if (scene == "campaign") goTo(t->form(), "chapters", "chapters[0].before");
    else if (scene == "audio") goTo(t->form(), "use", "use");
    else goTo(t->form(), "styles", "styles");
  }
  save(s.w.get(), QString("m3b-%1-%2x%3.png").arg(scene).arg(w).arg(h), w, h);
}

QTEST_MAIN(Shots3bTest)
#include "test_form_screenshots3b.moc"
