// Moving list items in a form (the grip at the left of every item): the drag is simulated with real mouse events on a shown form
// (offscreen platform). Each drop is ONE undo step; the JSON is checked exactly; items that were open stay open, the keyboard focus
// goes with the item, a command can be dragged into another event's list or into an `if` branch, never into itself; the up / down
// buttons and Alt+Up / Alt+Down still work.
#include <QApplication>
#include <QLineEdit>
#include <QScrollBar>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_drag.h"
#include "form_fields.h"
#include "form_view.h"
#include "id_source.h"
#include "project.h"
#include "test_support.h"

using namespace ote_native;
using ote::Json;

namespace {

Json parse(const QString& text) {
  Json j;
  std::string err;
  if (!Json::parse(text.toStdString(), j, err)) qFatal("bad JSON in the test: %s", err.c_str());
  return j;
}

JsonPath path(const QString& text) {  // "events[0].do[3]"
  JsonPath out;
  for (const QString& part : text.split('.')) {
    const int br = part.indexOf('[');
    if (br < 0) { out.push_back(PathStep::member(part.toStdString())); continue; }
    if (br > 0) out.push_back(PathStep::member(part.left(br).toStdString()));
    out.push_back(PathStep::at(part.mid(br + 1, part.size() - br - 2).toUInt()));
  }
  return out;
}

struct Rig {
  ote_test::Scratch tmp;
  SchemaSet schemas;
  std::unique_ptr<Project> project;
  std::unique_ptr<IdSource> ids;
  JsonDocument* doc = nullptr;
  std::unique_ptr<FormContext> ctx;
  std::unique_ptr<FormView> form;

  explicit Rig(const char* events) {
    const QString dir = ote_test::copyFixture(tmp, "srpg-rename");
    schemas.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    project = std::make_unique<Project>(dir);
    ids = std::make_unique<IdSource>(project.get());
    doc = project->document("maps/field.json");
    ctx = std::make_unique<FormContext>(project.get(), doc, ids.get());
    form = std::make_unique<FormView>(ctx.get(), schemas.forFile("maps/field.json")->root.get());
    form->resize(1000, 1400);
    ctx->set(path("events"), parse(events));
    form->setTarget({});
    form->show();
    QVERIFY(QTest::qWaitForWindowExposed(form.get()));
    form->activateWindow();
    QVERIFY(QTest::qWaitForWindowActive(form.get()));
    flush();
    // The events of the map are collapsed one line each; open the first two (their commands stay collapsed).
    for (size_t i = 0; i < 2 && i < ctx->at(path("events"))->size(); ++i) form->expandAlong(path(QString("events[%1]").arg(i)));
    flush();
  }
  ~Rig() { form->hide(); }
  void flush() { form->flush(); QCoreApplication::processEvents(); form->flush(); }
  const Json* at(const QString& p) { return resolvePath(doc->root(), path(p)); }
  QString text(const QString& p) { const Json* j = at(p); return j ? QString::fromStdString(j->dump(0)) : QStringLiteral("(absent)"); }
  QUndoStack* stack() { return project->undoStack(); }
  QWidget* grip(const QString& list, int index) { return form->dragHandleAt(path(list), (size_t)index); }
  void open(const QString& p) { form->expandAlong(path(p)); flush(); }

  // The row (frame) an item is in.
  QWidget* row(const QString& list, int index) {
    for (QWidget* w = grip(list, index); w; w = w->parentWidget())
      if (w->objectName() == "listItem") return w;
    return nullptr;
  }
  // A global point in the lower half of item `index`'s row (a drop there lands after it) / in the upper half (before it).
  QPoint below(const QString& list, int index) {
    QWidget* w = row(list, index);
    return w ? w->mapToGlobal(QPoint(60, w->height() - 4)) : QPoint();
  }
  QPoint above(const QString& list, int index) {
    QWidget* w = row(list, index);
    return w ? w->mapToGlobal(QPoint(60, 4)) : QPoint();
  }
  // Presses the grip, moves in two steps (the first past the drag distance), and optionally releases at the end.
  void drag(QWidget* grip, QPoint targetGlobal, bool release = true) {
    QVERIFY(grip);
    const QPoint start = grip->rect().center();
    QTest::mousePress(grip, Qt::LeftButton, {}, start);
    QTest::mouseMove(grip, start + QPoint(0, 12));
    QTest::mouseMove(grip, grip->mapFromGlobal(targetGlobal));
    if (release) QTest::mouseRelease(grip, Qt::LeftButton, {}, grip->mapFromGlobal(targetGlobal));
    flush();
  }
};

const char* const kFour = R"([{"id":"e","when":{"battle":"start"},"do":[
  {"say":"A","speaker":"hero"},{"say":"B","speaker":"hero"},{"say":"C","speaker":"hero"},{"say":"D","speaker":"hero"}]}])";

QString says(Rig& r, const QString& list) {
  QString out;
  const Json* l = r.at(list);
  for (size_t i = 0; l && i < l->size(); ++i) out += QString::fromStdString((*l)[i]["say"].asString());
  return out;
}

}  // namespace

class FormDragTest : public QObject {
  Q_OBJECT
 private slots:
  void everyItemHasAGrip();
  void dragWithinAListIsOneUndoStep();
  void aDropThatChangesNothingWritesNothing();
  void openItemsStayOpenAndFocusFollowsTheItem();
  void otherOpenItemsShiftWithTheirIndex();
  void theShownPageFollowsAnItemMovedAcrossAPageEnd();
  void aCommandCanGoToAnotherEventAndIntoABranch();
  void anItemNeverGoesIntoItselfOrAListOfAnotherKind();
  void escapeCancelsAndTheForwardKeysStillWork();
  void aPlainValueListReorders();
  void aLongListScrollsWhileDragging();
};

void FormDragTest::everyItemHasAGrip() {
  Rig r(kFour);
  for (int i = 0; i < 4; ++i) {
    QVERIFY2(r.grip("events[0].do", i), qPrintable(QString::number(i)));
    QVERIFY(r.grip("events[0].do", i)->isVisible());
  }
  QVERIFY(!r.grip("events[0].do", 4));
  QVERIFY(r.grip("events", 0));  // the events of the map themselves are a list
}

void FormDragTest::dragWithinAListIsOneUndoStep() {
  Rig r(kFour);
  const QString before = r.text("events");
  const int steps = r.stack()->count();
  // A over C's lower half: [B, C, A, D]; the picture during the drag shows where it would land.
  QWidget* g = r.grip("events[0].do", 0);
  r.drag(g, r.below("events[0].do", 2), false);
  QVERIFY(r.form->isDragging());
  JsonPath list;
  size_t index = 99;
  QVERIFY(r.form->dropTarget(&list, &index));
  QCOMPARE(pathToString(list), QString("events[0].do"));
  QCOMPARE((int)index, 3);
  QVERIFY(r.form->findChild<QWidget*>("dropLine")->isVisible());
  QVERIFY(r.form->findChild<QWidget*>("dragGhost")->isVisible());
  QCOMPARE(says(r, "events[0].do"), QString("ABCD"));  // nothing written before the drop
  QTest::mouseRelease(g, Qt::LeftButton, {}, g->mapFromGlobal(r.below("events[0].do", 2)));
  r.flush();
  QVERIFY(!r.form->isDragging());
  QVERIFY(!r.form->findChild<QWidget*>("dropLine"));
  QCOMPARE(says(r, "events[0].do"), QString("BCAD"));
  QCOMPARE(r.stack()->count(), steps + 1);
  QCOMPARE(r.text("events[0].do[2]"), QString("{\"say\":\"A\",\"speaker\":\"hero\"}"));
  r.stack()->undo();
  r.flush();
  QCOMPARE(r.text("events"), before);
  r.stack()->redo();
  QCOMPARE(says(r, "events[0].do"), QString("BCAD"));
  // And upwards: D above B.
  r.drag(r.grip("events[0].do", 3), r.above("events[0].do", 0));
  QCOMPARE(says(r, "events[0].do"), QString("DBCA"));
  QCOMPARE(r.stack()->count(), steps + 2);
}

void FormDragTest::aDropThatChangesNothingWritesNothing() {
  Rig r(kFour);
  const int steps = r.stack()->count();
  // Dropped on its own place (just above itself, just below itself), outside every list, and not moved at all.
  r.drag(r.grip("events[0].do", 1), r.above("events[0].do", 1));
  r.drag(r.grip("events[0].do", 1), r.below("events[0].do", 1));
  r.drag(r.grip("events[0].do", 1), QPoint(-50, -50));
  QTest::mousePress(r.grip("events[0].do", 1), Qt::LeftButton);
  QTest::mouseRelease(r.grip("events[0].do", 1), Qt::LeftButton);
  QCOMPARE(r.stack()->count(), steps);
  QCOMPARE(says(r, "events[0].do"), QString("ABCD"));
  QVERIFY(!r.doc->isDirty() || r.stack()->count() > 0);
}

void FormDragTest::openItemsStayOpenAndFocusFollowsTheItem() {
  Rig r(kFour);
  r.open("events[0].do[0]");  // A is open, the others are collapsed (a command list starts collapsed)
  QVERIFY(r.form->hasAnchor(path("events[0].do[0]")));
  QVERIFY(!r.form->hasAnchor(path("events[0].do[1]")));
  // Keyboard focus in A's text field.
  auto* say = qobject_cast<ScalarField*>(r.form->fieldAt(path("events[0].do[0].say")));
  QVERIFY(say);
  say->edit()->setFocus();
  QVERIFY(QTest::qWaitFor([&] { return say->edit()->hasFocus(); }, 2000));
  // Move A to the end with the up / down buttons' sibling: a drop below D.
  r.drag(r.grip("events[0].do", 0), r.below("events[0].do", 3));
  QCOMPARE(says(r, "events[0].do"), QString("BCDA"));
  QVERIFY2(r.form->hasAnchor(path("events[0].do[3]")), "A stayed open at its new place");
  QVERIFY(!r.form->hasAnchor(path("events[0].do[0]")));
  QVERIFY(!r.form->hasAnchor(path("events[0].do[1]")));
  // The grip that moved it has the focus (a drag takes the focus onto the grip, which is what a keyboard user continues from).
  QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == r.grip("events[0].do", 3); }, 2000));
  // Alt+Up from the grip moves it one place up and the focus stays on its grip.
  QTest::keyClick(QApplication::focusWidget(), Qt::Key_Up, Qt::AltModifier);
  r.flush();
  QCOMPARE(says(r, "events[0].do"), QString("BCAD"));
  QVERIFY(r.form->hasAnchor(path("events[0].do[2]")));
  QVERIFY(QTest::qWaitFor([&] { return QApplication::focusWidget() == r.grip("events[0].do", 2); }, 2000));
  // Focus in a text field goes with the item through a button move: A's field keeps the keyboard.
  auto* field = qobject_cast<ScalarField*>(r.form->fieldAt(path("events[0].do[2].say")));
  QVERIFY(field);
  field->edit()->setFocus();
  QVERIFY(QTest::qWaitFor([&] { return field->edit()->hasFocus(); }, 2000));
  r.form->moveListItem(path("events[0].do"), 2, path("events[0].do"), 0, "itemUp");
  r.flush();
  QCOMPARE(says(r, "events[0].do"), QString("ABCD"));
  auto* moved = qobject_cast<ScalarField*>(r.form->fieldAt(path("events[0].do[0].say")));
  QVERIFY(moved);
  QVERIFY(QTest::qWaitFor([&] { return moved->edit()->hasFocus(); }, 2000));
  QCOMPARE(moved->edit()->text(), QString("A"));
}

void FormDragTest::otherOpenItemsShiftWithTheirIndex() {
  Rig r(kFour);
  r.open("events[0].do[1]");  // B is open
  r.open("events[0].do[3]");  // and D
  r.drag(r.grip("events[0].do", 0), r.below("events[0].do", 3));  // A to the end: B C D A
  QCOMPARE(says(r, "events[0].do"), QString("BCDA"));
  QVERIFY(r.form->hasAnchor(path("events[0].do[0]")));   // B, still open, one place up
  QVERIFY(!r.form->hasAnchor(path("events[0].do[1]")));  // C
  QVERIFY(r.form->hasAnchor(path("events[0].do[2]")));   // D, still open
  QVERIFY(!r.form->hasAnchor(path("events[0].do[3]")));  // A, collapsed as it was
  // And back the other way: D above B.
  r.drag(r.grip("events[0].do", 2), r.above("events[0].do", 0));
  QCOMPARE(says(r, "events[0].do"), QString("DBCA"));
  QVERIFY(r.form->hasAnchor(path("events[0].do[0]")));
  QVERIFY(r.form->hasAnchor(path("events[0].do[1]")));
  QVERIFY(!r.form->hasAnchor(path("events[0].do[2]")));
  QVERIFY(!r.form->hasAnchor(path("events[0].do[3]")));
}

void FormDragTest::theShownPageFollowsAnItemMovedAcrossAPageEnd() {
  QString cmds;
  for (int i = 0; i < 120; ++i) cmds += (i ? "," : "") + QStringLiteral("{\"say\":\"L%1\",\"speaker\":\"hero\"}").arg(i);
  Rig r(qPrintable(QString("[{\"id\":\"long\",\"when\":{\"battle\":\"start\"},\"do\":[%1]}]").arg(cmds)));
  QVERIFY(r.grip("events[0].do", 49));
  QVERIFY(!r.grip("events[0].do", 50));  // a page holds 50 items
  QWidget* last = r.grip("events[0].do", 49);
  QFrame* row = nullptr;
  for (QFrame* f : r.form->findChildren<QFrame*>("listItem"))
    if (f->findChild<DragHandle*>("dragHandle") == last) row = f;
  QVERIFY(row);
  row->findChild<QToolButton*>("itemDown")->click();  // item 49 becomes item 50: the next page
  r.flush();
  QCOMPARE(r.text("events[0].do[50]"), QString("{\"say\":\"L49\",\"speaker\":\"hero\"}"));
  QVERIFY2(r.grip("events[0].do", 50), "the page with the moved item is shown");
  QVERIFY(!r.grip("events[0].do", 49));
}

void FormDragTest::aCommandCanGoToAnotherEventAndIntoABranch() {
  Rig r(R"([{"id":"one","when":{"battle":"start"},"do":[
              {"say":"A","speaker":"hero"},
              {"if":[{"alive":"hero"}],"then":[{"say":"T1","speaker":"hero"}],"else":[]},
              {"say":"C","speaker":"hero"}]},
            {"id":"two","when":{"battle":"start"},"do":[{"say":"X","speaker":"hero"},{"say":"Y","speaker":"hero"}]}])");
  r.open("events[0].do[1]");  // the if command is open: its two branches are lists of their own
  const QString before = r.text("events");
  const int steps = r.stack()->count();
  QVERIFY(r.grip("events[0].do[1].then", 0));
  // X of the second event into the first event, between A and the if.
  r.drag(r.grip("events[1].do", 0), r.below("events[0].do", 0));
  QCOMPARE(says(r, "events[0].do"), QString("AXC"));  // (the if has no `say`)
  QCOMPARE(r.text("events[0].do[1]"), QString("{\"say\":\"X\",\"speaker\":\"hero\"}"));
  QCOMPARE(says(r, "events[1].do"), QString("Y"));
  QCOMPARE(r.stack()->count(), steps + 1);  // one macro: insert and erase together
  r.stack()->undo();
  r.flush();
  QCOMPARE(r.text("events"), before);
  QCOMPARE(r.stack()->count(), steps + 1);
  r.stack()->redo();
  r.flush();
  QCOMPARE(says(r, "events[1].do"), QString("Y"));
  r.stack()->undo();
  r.flush();
  // Y of the second event into the first event's `then` branch (after T1), and C of the first event into its own else branch,
  // which is empty: a drop anywhere on an empty list puts the item there.
  r.open("events[0].do[1]");
  r.drag(r.grip("events[1].do", 1), r.below("events[0].do[1].then", 0));
  QCOMPARE(r.text("events[0].do[1].then"), QString("[{\"say\":\"T1\",\"speaker\":\"hero\"},{\"say\":\"Y\",\"speaker\":\"hero\"}]"));
  QCOMPARE(says(r, "events[1].do"), QString("X"));
  QCOMPARE(r.stack()->count(), steps + 1);  // (the undone first move was dropped from the stack by this one)
  QWidget* elseList = r.form->anchorWidget(path("events[0].do[1].else"));
  QVERIFY(elseList);
  r.drag(r.grip("events[0].do", 2), elseList->mapToGlobal(QPoint(40, 6)));
  QCOMPARE(r.text("events[0].do[1].else"), QString("[{\"say\":\"C\",\"speaker\":\"hero\"}]"));
  QCOMPARE(r.text("events[0].do[2]"), QString("(absent)"));
  QCOMPARE(r.stack()->count(), steps + 2);
  // The other way, out of a branch into the list that holds the if (the original index of the if shifts by the insert).
  r.drag(r.grip("events[0].do[1].else", 0), r.above("events[0].do", 0));
  QCOMPARE(says(r, "events[0].do"), QString("CA"));
  QCOMPARE(r.text("events[0].do[2].else"), QString("[]"));
  QCOMPARE(r.stack()->count(), steps + 3);
  // Everything back with undo, one step each.
  for (int i = 0; i < 3; ++i) r.stack()->undo();
  r.flush();
  QCOMPARE(r.text("events"), before);
}

void FormDragTest::anItemNeverGoesIntoItselfOrAListOfAnotherKind() {
  Rig r(R"([{"id":"one","when":{"battle":"start"},"do":[
              {"if":[{"alive":"hero"}],"then":[{"say":"T1","speaker":"hero"}],"else":[]},{"say":"B","speaker":"hero"}]}])");
  r.open("events[0].do[0]");
  const QString before = r.text("events");
  // The if command dragged into its own `then` branch: refused, nothing moves.
  QWidget* g = r.grip("events[0].do", 0);
  r.drag(g, r.below("events[0].do[0].then", 0), false);
  QVERIFY(r.form->isDragging());
  JsonPath list;
  size_t index = 0;
  QVERIFY(!r.form->dropTarget(&list, &index));
  QVERIFY(!r.form->findChild<QWidget*>("dropLine")->isVisible());
  QTest::mouseRelease(g, Qt::LeftButton, {}, g->mapFromGlobal(r.below("events[0].do[0].then", 0)));
  r.flush();
  QCOMPARE(r.text("events"), before);
  QVERIFY(!r.ctx->transferItem(path("events[0].do"), 0, path("events[0].do[0].then"), 0));
  // A command is not a condition: the `if` list of the if command takes no command.
  r.drag(r.grip("events[0].do", 1), r.below("events[0].do[0].then", 0), false);
  QVERIFY(r.form->dropTarget(&list, &index));
  QCOMPARE(pathToString(list), QString("events[0].do[0].then"));  // the commands' own branch is fine
  QTest::keyClick(r.grip("events[0].do", 1), Qt::Key_Escape);
  QVERIFY(!r.form->isDragging());
  QCOMPARE(r.text("events"), before);
  // The events list (objects) and a command list are different kinds.
  r.drag(r.grip("events[0].do", 1), r.below("events", 0), false);
  QVERIFY(!r.form->dropTarget(&list, &index) || pathToString(list) != "events");
  QTest::keyClick(r.grip("events[0].do", 1), Qt::Key_Escape);
  QCOMPARE(r.text("events"), before);
}

void FormDragTest::escapeCancelsAndTheForwardKeysStillWork() {
  Rig r(kFour);
  const int steps = r.stack()->count();
  QWidget* g = r.grip("events[0].do", 0);
  r.drag(g, r.below("events[0].do", 3), false);
  QVERIFY(r.form->isDragging());
  QTest::keyClick(g, Qt::Key_Escape);
  QVERIFY(!r.form->isDragging());
  QTest::mouseRelease(g, Qt::LeftButton);
  r.flush();
  QCOMPARE(says(r, "events[0].do"), QString("ABCD"));
  QCOMPARE(r.stack()->count(), steps);
  // The buttons: down on A, up on D; each one undo step.
  auto click = [&](const QString& list, int i, const char* name) {
    QFrame* row = nullptr;
    for (QFrame* f : r.form->findChildren<QFrame*>("listItem"))
      if (f->findChild<DragHandle*>("dragHandle") == r.grip(list, i)) row = f;
    QVERIFY(row);
    auto* b = row->findChild<QToolButton*>(name);
    QVERIFY(b && b->isEnabled());
    b->click();
    r.flush();
  };
  click("events[0].do", 0, "itemDown");
  QCOMPARE(says(r, "events[0].do"), QString("BACD"));
  click("events[0].do", 3, "itemUp");
  QCOMPARE(says(r, "events[0].do"), QString("BADC"));
  QCOMPARE(r.stack()->count(), steps + 2);
  // After a button move the same button, now on the item's new row, has the keyboard focus (repeat to walk an item along).
  QVERIFY(QTest::qWaitFor([&] { return qobject_cast<QToolButton*>(QApplication::focusWidget()) && QApplication::focusWidget()->objectName() == "itemUp"; }, 2000));
  // Alt+Down on a grip.
  QTest::keyClick(r.grip("events[0].do", 0), Qt::Key_Down, Qt::AltModifier);
  r.flush();
  QCOMPARE(says(r, "events[0].do"), QString("ABDC"));
  QCOMPARE(r.stack()->count(), steps + 3);
  // The first item cannot go up, the last cannot go down.
  QTest::keyClick(r.grip("events[0].do", 0), Qt::Key_Up, Qt::AltModifier);
  QTest::keyClick(r.grip("events[0].do", 3), Qt::Key_Down, Qt::AltModifier);
  QCOMPARE(r.stack()->count(), steps + 3);
}

void FormDragTest::aPlainValueListReorders() {
  Rig r(kFour);
  // sides of the map: a list of objects of its own; a list of plain texts is `rules.defeat`-like; here the unit `skills` list.
  r.ctx->set(path("units"), parse(R"([{"id":"u1","type":"fighter","x":0,"y":0,"side":"blue","skills":["s1","s2","s3"]}])"));
  r.flush();
  r.open("units[0]");
  r.form->expandAll();
  r.flush();
  QVERIFY(r.grip("units[0].skills", 0));
  const QString list = "units[0].skills";
  r.drag(r.grip(list, 0), r.below(list, 2));
  QCOMPARE(r.text(list), QString("[\"s2\",\"s3\",\"s1\"]"));
  // Values do not move into a list of commands (the lists are not of the same kind).
  r.drag(r.grip(list, 0), r.below("events[0].do", 0), false);
  JsonPath l;
  size_t i = 0;
  QVERIFY(!r.form->dropTarget(&l, &i));
  QTest::keyClick(r.grip(list, 0), Qt::Key_Escape);
  QCOMPARE(r.text(list), QString("[\"s2\",\"s3\",\"s1\"]"));
}

void FormDragTest::aLongListScrollsWhileDragging() {
  QString cmds;
  for (int i = 0; i < 40; ++i) cmds += (i ? "," : "") + QStringLiteral("{\"say\":\"L%1\",\"speaker\":\"hero\"}").arg(i);
  Rig r(qPrintable(QString("[{\"id\":\"long\",\"when\":{\"battle\":\"start\"},\"do\":[%1]}]").arg(cmds)));
  r.form->resize(1000, 400);
  r.flush();
  QScrollBar* bar = r.form->verticalScrollBar();
  QWidget* g = r.grip("events[0].do", 0);
  QVERIFY(g);
  // The list is far down the map's form: scroll until its first item is at the top of the view.
  bar->setValue(bar->value() + g->mapToGlobal(QPoint(0, 0)).y() - r.form->viewport()->mapToGlobal(QPoint(0, 0)).y() - 40);
  r.flush();
  QVERIFY(r.form->viewport()->rect().contains(r.form->viewport()->mapFromGlobal(g->mapToGlobal(QPoint(2, 2)))));
  const int start = bar->value();
  // Held near the bottom edge the form scrolls by itself.
  const QPoint bottom = r.form->viewport()->mapToGlobal(QPoint(300, r.form->viewport()->height() - 6));
  r.drag(g, bottom, false);
  QVERIFY(QTest::qWaitFor([&] { return bar->value() > start + 60; }, 3000));
  JsonPath dl;
  size_t di = 0;
  QVERIFY2(r.form->dropTarget(&dl, &di), "a drop target below the start");
  QVERIFY(di > 3);  // (rows are ~40 px high, the view is 400 px: far below the start)
  // Dropped somewhere well below the start: the item is no longer first.
  QTest::mouseRelease(g, Qt::LeftButton, {}, g->mapFromGlobal(bottom));
  r.flush();
  QVERIFY(says(r, "events[0].do").startsWith("L1"));
  QCOMPARE(r.stack()->count() > 0, true);
  QVERIFY(!r.form->isDragging());
}

QTEST_MAIN(FormDragTest)
#include "test_form_drag.moc"
