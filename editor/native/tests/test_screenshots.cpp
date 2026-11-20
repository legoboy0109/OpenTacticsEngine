// Renders the main views with QWidget::grab() at 1280x720 and 1920x1080 and saves PNGs (to $OTE_SHOT_DIR, else next to
// the other test scratch data) so a person can look at them: clipped text, odd column widths and unreadable colours
// cannot be asserted, only seen. The test itself checks that the pictures are real (right size, not blank).
#include <QApplication>
#include <QDir>
#include <QPalette>
#include <QStyleFactory>
#include <QImage>
#include <QLabel>
#include <QSettings>
#include <QTabWidget>
#include <QTableView>
#include <QTreeView>
#include <QtTest>
#include <memory>

#include "json_table_model.h"
#include "json_tree_model.h"
#include "json_text_dialog.h"
#include "main_window.h"
#include "problems_dock.h"
#include "project_session.h"
#include "table_editor.h"
#include "table_proxy.h"
#include "test_prompter.h"
#include "test_support.h"
#include "tree_editor.h"

using namespace ote_native;

class ScreenshotTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void mainWindowWithUnitsTable_data() { sizes(); }
  void mainWindowWithUnitsTable();
  void jsonTreeOnGameJson_data() { sizes(); }
  void jsonTreeOnGameJson();
  void problemsDockWithAnError_data() { sizes(); }
  void problemsDockWithAnError();
  void nestedJsonDialog();
  void rejectedEditMessage();
  void darkPalette();

 private:
  static void sizes() {
    QTest::addColumn<int>("w");
    QTest::addColumn<int>("h");
    QTest::newRow("1280x720") << 1280 << 720;
    QTest::newRow("1920x1080") << 1920 << 1080;
  }
  void save(QWidget* w, const QString& name, int expectW, int expectH);
  QString shotDir_;
};

void ScreenshotTest::initTestCase() {
  shotDir_ = qEnvironmentVariable("OTE_SHOT_DIR");
  if (shotDir_.isEmpty()) shotDir_ = qEnvironmentVariable("OTE_TEST_TMP", QDir::tempPath()) + "/native-shots";
  QVERIFY(QDir().mkpath(shotDir_));
}

void ScreenshotTest::save(QWidget* w, const QString& name, int expectW, int expectH) {
  QTest::qWait(50);  // let layouts and the first paint settle
  const QImage img = w->grab().toImage();
  QCOMPARE(img.width(), expectW);
  QCOMPARE(img.height(), expectH);
  QSet<QRgb> colours;
  for (int y = 0; y < img.height(); y += 7)
    for (int x = 0; x < img.width(); x += 7) colours.insert(img.pixel(x, y));
  QVERIFY2(colours.size() > 8, "the picture is blank");
  QVERIFY(img.save(shotDir_ + "/" + name));
  qInfo("screenshot: %s/%s", qPrintable(shotDir_), qPrintable(name));
}

namespace {
struct Shell {
  ote_test::Scratch tmp;
  std::unique_ptr<QSettings> settings;
  std::unique_ptr<MainWindow> w;
  Shell(int width, int height) {
    settings = std::make_unique<QSettings>(tmp.file("s.ini"), QSettings::IniFormat);
    w = std::make_unique<MainWindow>(settings.get());
    w->setPrompter(std::make_unique<ote_test::ScriptedPrompter>());
    w->resize(width, height);
    w->show();
  }
  bool open() {
    if (!w->openProject(ote_test::copyDemoGame(tmp))) return false;
    ProblemsRunner* r = w->session()->problems();
    QSignalSpy done(r, &ProblemsRunner::finished);
    return !r->isRunning() || done.wait(60000);
  }
};
}  // namespace

void ScreenshotTest::mainWindowWithUnitsTable() {
  QFETCH(int, w);
  QFETCH(int, h);
  Shell s(w, h);
  QVERIFY(s.open());
  s.w->openFile("data/units.json");
  auto* t = qobject_cast<TableEditor*>(s.w->editorFor("data/units.json"));
  QVERIFY(t);
  t->view()->selectRow(1);
  save(s.w.get(), QString("main-units-%1x%2.png").arg(w).arg(h), w, h);
}

void ScreenshotTest::jsonTreeOnGameJson() {
  QFETCH(int, w);
  QFETCH(int, h);
  Shell s(w, h);
  QVERIFY(s.open());  // game.json opens first, as a tree
  auto* t = qobject_cast<TreeEditor*>(s.w->editorFor("game.json"));
  QVERIFY(t);
  t->view()->expandToDepth(1);
  t->view()->setCurrentIndex(t->model()->index(2, 0));
  save(s.w.get(), QString("tree-gamejson-%1x%2.png").arg(w).arg(h), w, h);
}

void ScreenshotTest::problemsDockWithAnError() {
  QFETCH(int, w);
  QFETCH(int, h);
  Shell s(w, h);
  QVERIFY(s.open());
  s.w->openFile("data/units.json");
  auto* t = qobject_cast<TableEditor*>(s.w->editorFor("data/units.json"));
  JsonTableModel* m = t->model();
  int col = -1;
  for (int c = 0; c < m->columnCount(); ++c)
    if (m->columnKey(c) == "moveType") col = c;
  QVERIFY(col >= 0);
  m->setData(m->index(0, col), "swimming-in-lava", Qt::EditRole);
  // A refused edit leaves a visible message strip above the table: show that too.
  m->setData(m->index(1, 0), "x", Qt::EditRole);
  const int numberCol = [&] { for (int c = 0; c < m->columnCount(); ++c) if (m->cellValue(0, c) && m->cellValue(0, c)->isNumber()) return c; return -1; }();
  if (numberCol >= 0) m->setData(m->index(0, numberCol), "lots", Qt::EditRole);
  QVERIFY(s.w->saveCurrent());
  ProblemsRunner* r = s.w->session()->problems();
  QSignalSpy done(r, &ProblemsRunner::finished);
  if (r->isRunning()) QVERIFY(done.wait(60000));
  QVERIFY(s.w->problemsDock()->problemCount() >= 1);
  save(s.w.get(), QString("problems-%1x%2.png").arg(w).arg(h), w, h);
}

void ScreenshotTest::nestedJsonDialog() {
  ote_test::Scratch tmp;
  ote::Json j;
  std::string err;
  ote::Json::parse(R"({"hp": 20, "str": 6, "skl": 7, "spd": 7, "def": 4, "mov": 5, "wields": ["sword", "lance"]})", j, err);
  JsonTextDialog good("Edit stats of row 1", j);
  good.show();
  save(&good, "nested-dialog-valid.png", good.width(), good.height());
  JsonTextDialog bad("Edit stats of row 1", j);
  bad.setText("{\"hp\": 20, \"str\": 6,\n \"skl\": ");
  bad.show();
  save(&bad, "nested-dialog-invalid.png", bad.width(), bad.height());
}

namespace {
// Opens items.json, types text into a number cell (refused, so the red message strip shows) and returns the table.
TableEditor* provokeRejectedEdit(Shell& s) {
  s.w->openFile("data/items.json");
  auto* t = qobject_cast<TableEditor*>(s.w->editorFor("data/items.json"));
  if (!t) return nullptr;
  JsonTableModel* m = t->model();
  for (int c = 0; c < m->columnCount(); ++c)
    if (m->cellValue(0, c) && m->cellValue(0, c)->isNumber()) {
      m->setData(m->index(0, c), "lots", Qt::EditRole);
      t->view()->setCurrentIndex(t->proxy()->index(0, c));
      break;
    }
  return t;
}
}  // namespace

void ScreenshotTest::rejectedEditMessage() {
  Shell s(1280, 720);
  QVERIFY(s.open());
  TableEditor* t = provokeRejectedEdit(s);
  QVERIFY(t);
  QVERIFY(t->messageLabel()->text().contains("not a number"));
  save(s.w.get(), "rejected-edit-1280x720.png", 1280, 720);
}

void ScreenshotTest::darkPalette() {
  // The editor sets no colours of its own except one red for messages; this puts a dark system palette under it.
  const QPalette original = QApplication::palette();
  QPalette dark;
  dark.setColor(QPalette::Window, QColor(43, 43, 43));
  dark.setColor(QPalette::WindowText, QColor(230, 230, 230));
  dark.setColor(QPalette::Base, QColor(30, 30, 30));
  dark.setColor(QPalette::AlternateBase, QColor(40, 40, 40));
  dark.setColor(QPalette::Text, QColor(230, 230, 230));
  dark.setColor(QPalette::Button, QColor(58, 58, 58));
  dark.setColor(QPalette::ButtonText, QColor(230, 230, 230));
  dark.setColor(QPalette::PlaceholderText, QColor(140, 140, 140));
  dark.setColor(QPalette::Highlight, QColor(42, 130, 218));
  dark.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
  dark.setColor(QPalette::ToolTipBase, QColor(58, 58, 58));
  dark.setColor(QPalette::ToolTipText, QColor(230, 230, 230));
  dark.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 120, 120));
  dark.setColor(QPalette::Disabled, QPalette::WindowText, QColor(120, 120, 120));
  dark.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(120, 120, 120));
  QApplication::setStyle(QStyleFactory::create("Fusion"));
  QApplication::setPalette(dark);
  {
    Shell s(1280, 720);
    QVERIFY(s.open());
    TableEditor* t = provokeRejectedEdit(s);
    QVERIFY(t);
    save(s.w.get(), "dark-items-rejected-1280x720.png", 1280, 720);
    s.w->openFile("game.json");
    save(s.w.get(), "dark-tree-1280x720.png", 1280, 720);
    JsonTextDialog bad("Edit stats", ote::Json());
    bad.setText("{\"a\": ");
    bad.show();
    save(&bad, "dark-nested-dialog-invalid.png", bad.width(), bad.height());
  }
  QApplication::setPalette(original);
}

QTEST_MAIN(ScreenshotTest)
#include "test_screenshots.moc"
