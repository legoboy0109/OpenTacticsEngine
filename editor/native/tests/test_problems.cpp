// The problems runner (engine check on a worker thread) and the Problems dock.
#include <QListWidget>
#include <QtTest>

#include "problems_dock.h"
#include "problems_runner.h"
#include "test_support.h"

using namespace ote_native;

class ProblemsTest : public QObject {
  Q_OBJECT
 private slots:
  void demoGameHasNoProblemsAndABrokenFileIsNamed();
  void requestsDuringARunAreCoalescedIntoOneFollowUp();
  void activatingAProblemAsksToOpenItsFile();
};

void ProblemsTest::demoGameHasNoProblemsAndABrokenFileIsNamed() {
  ote_test::Scratch tmp;
  const QString game = ote_test::copyDemoGame(tmp);
  ProblemsRunner runner;
  QSignalSpy done(&runner, &ProblemsRunner::finished);
  runner.run(game);
  QVERIFY(runner.isRunning());
  QVERIFY(done.wait(60000));
  QVERIFY2(runner.problems().empty(), runner.problems().empty() ? "" : runner.problems()[0].message.c_str());
  QCOMPARE(runner.checkedFolder(), game);

  ote_test::writeFile(game + "/data/units.json", "{ \"units\": [ { broken ");
  runner.run(game);
  QVERIFY(done.wait(60000));
  QVERIFY(!runner.problems().empty());
  QCOMPARE(QString::fromStdString(runner.problems()[0].file), QString("data/units.json"));
}

void ProblemsTest::requestsDuringARunAreCoalescedIntoOneFollowUp() {
  ote_test::Scratch tmp;
  const QString game = ote_test::copyDemoGame(tmp);
  ProblemsRunner runner;
  QSignalSpy started(&runner, &ProblemsRunner::started);
  QSignalSpy done(&runner, &ProblemsRunner::finished);
  runner.run(game);
  runner.run(game);
  runner.run(game);
  QVERIFY(done.wait(60000));
  QTest::qWait(300);
  QCOMPARE(done.count(), 1);     // one result for the three requests...
  QCOMPARE(started.count(), 2);  // ...from the running check and a single follow-up
}

void ProblemsTest::activatingAProblemAsksToOpenItsFile() {
  ProblemsDock dock;
  QSignalSpy open(&dock, &ProblemsDock::openFileRequested);
  dock.setProblems({{"data/units.json", "line 3: expected a value"}, {"", "something general"}});
  QCOMPARE(dock.problemCount(), 2);
  QVERIFY(dock.windowTitle().contains("2"));
  emit dock.list()->itemActivated(dock.list()->item(0));
  QCOMPARE(open.count(), 1);
  QCOMPARE(open.at(0).at(0).toString(), QString("data/units.json"));
  emit dock.list()->itemActivated(dock.list()->item(1));  // no file named: nothing to open
  QCOMPARE(open.count(), 1);
  dock.setProblems({});
  QCOMPARE(dock.problemCount(), 0);
}

QTEST_MAIN(ProblemsTest)
#include "test_problems.moc"
