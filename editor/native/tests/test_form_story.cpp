// The story forms (milestone 3b) with the real schemas on a copy of a fixture: which command a `{ "changeSide": ..., "side": ... }`
// entry is (detection by the command key, with / unless), one readable line per command, command lists inside commands (any
// depth, lazily), long scripts (paging, duplicate / delete / reorder / add through the undo stack) and the time a 300-command
// event takes.
#include <QComboBox>
#include <QElapsedTimer>
#include <QFrame>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QToolButton>
#include <QUndoStack>
#include <QtTest>
#include <memory>

#include "form_context.h"
#include "form_fields.h"
#include "form_parts.h"
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

struct StoryRig {
  ote_test::Scratch tmp;
  SchemaSet schemas;
  std::unique_ptr<Project> project;
  std::unique_ptr<IdSource> ids;
  JsonDocument* doc = nullptr;
  std::unique_ptr<FormContext> ctx;
  std::unique_ptr<FormView> form;

  StoryRig() {
    const QString dir = ote_test::copyFixture(tmp, "srpg-rename");
    schemas.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
    if (!schemas.errors().isEmpty()) qFatal("schemas: %s", qPrintable(schemas.errors().join("\n")));
    project = std::make_unique<Project>(dir);
    ids = std::make_unique<IdSource>(project.get());
    doc = project->document("maps/field.json");
    ctx = std::make_unique<FormContext>(project.get(), doc, ids.get());
    form = std::make_unique<FormView>(ctx.get(), schemas.forFile("maps/field.json")->root.get());
    form->resize(1100, 800);
    form->setTarget({});
    flush();
  }
  void flush() { form->flush(); QCoreApplication::processEvents(); form->flush(); }
  const SchemaNode& commandNode() const { return *schemas.forFile("maps/field.json")->root->field("events")->of->field("do")->of; }
  static JsonPath path(const QString& text) {  // "events[0].do[3].say"
    JsonPath out;
    for (const QString& part : text.split('.')) {
      const int br = part.indexOf('[');
      if (br < 0) { out.push_back(PathStep::member(part.toStdString())); continue; }
      if (br > 0) out.push_back(PathStep::member(part.left(br).toStdString()));
      out.push_back(PathStep::at(part.mid(br + 1, part.size() - br - 2).toUInt()));
    }
    return out;
  }
  const Json* at(const QString& p) { return resolvePath(doc->root(), path(p)); }
  void setEvents(const QString& json) { ctx->set(path("events"), parse(json)); flush(); }
  QWidget* listAt(const QString& p) {
    for (QWidget* w : form->body()->findChildren<QWidget*>("listField"))
      if (w->property("path").toString() == p) return w;
    return nullptr;
  }
  QUndoStack* stack() { return project->undoStack(); }
};

QString variantNameOf(StoryRig& r, const QString& commandJson) {
  const Json v = parse(commandJson);
  const int i = variantIndex(r.commandNode(), v);
  return i < 0 ? QStringLiteral("(none)") : QString::fromStdString(r.commandNode().variants[(size_t)i].name);
}

}  // namespace

class FormStoryTest : public QObject {
  Q_OBJECT
 private slots:
  void aCommandIsRecognisedByItsCommandKey();
  void everyCommandTheLoaderKnowsHasAShape();
  void summariesReadLikeThePlayersStory();
  void nestedCommandListsOpenAtAnyDepth();
  void aLongScriptIsPagedAndEditableThroughUndo();
  void addingACommandAsksWhichKind();
  void aThreeHundredCommandEventOpensFast();
};

void FormStoryTest::aCommandIsRecognisedByItsCommandKey() {
  StoryRig r;
  QCOMPARE(variantNameOf(r, R"({"say":"x","speaker":"hero","auto":300})"), QString("say"));
  QCOMPARE(variantNameOf(r, R"({"changeSide":{"unit":"ally"},"side":"red","ready":true})"), QString("changeSide"));
  // The two keys that two kinds of command share: `wait` (a command of its own, and an extra of focus / moveUnit) and `fade`.
  QCOMPARE(variantNameOf(r, R"({"wait":500})"), QString("wait"));
  QCOMPARE(variantNameOf(r, R"({"focus":"hero","wait":true})"), QString("focus"));
  QCOMPARE(variantNameOf(r, R"({"moveUnit":"hero","x":1,"y":1,"walk":true,"wait":true})"), QString("moveUnit"));
  QCOMPARE(variantNameOf(r, R"({"fade":"out","ms":300})"), QString("fade"));
  QCOMPARE(variantNameOf(r, R"({"playMusic":"battle","fade":1})"), QString("playMusic"));
  // setVariable is five shapes: a number, or one thing read from a unit.
  QCOMPARE(variantNameOf(r, R"({"setVariable":"lvl","to":3})"), QString("setVariable"));
  QCOMPARE(variantNameOf(r, R"({"setVariable":"lvl","level":"hero"})"), QString("setVariable (unit's level)"));
  QCOMPARE(variantNameOf(r, R"({"addVariable":"lvl","stat":"str","of":"hero"})"), QString("addVariable (unit's stat)"));
  QCOMPARE(variantNameOf(r, R"({"mystery":1})"), QString("(none)"));  // shown as JSON with a warning, never converted
}

void FormStoryTest::everyCommandTheLoaderKnowsHasAShape() {
  StoryRig r;
  // Copied from engine/core/data/loader_story_commands.cpp (kCommandKeys and the tables after it): the commands of a map event.
  const char* const kLoader[] = {"say", "addUnit", "removeUnit", "moveUnit", "changeSide", "giveItem", "giveResource", "setSwitch", "setVariable",
                                 "addVariable", "objective", "win", "lose", "if", "endTurn", "focus", "grantResearch", "playMusic", "stopMusic",
                                 "playSound", "takeItem", "promote", "teachSkill", "forgetSkill", "addSupport", "choice", "openDoor", "closeDoor",
                                 "lockDoor", "unlockDoor", "openChest", "addCondition", "removeCondition", "setBehaviour", "wakeGroup",
                                 "setTerrain", "applyChange", "revertChange", "setFog", "setVision", "background", "fade", "wait", "shake", "leave",
                                 "clearStage", "weather", "narrate", "enter", "move", "setPhaseMusic", "storeItems", "showUnit", "fight", "showMap", "returnMap"};
  QStringList missing;
  for (const char* k : kLoader) {
    bool found = false;
    for (const auto& v : r.commandNode().variants) found = found || v.matchKey == k;
    if (!found) missing << k;
  }
  QVERIFY2(missing.isEmpty(), qPrintable("commands without a shape: " + missing.join(", ")));
  // ... and the conditions and triggers.
  const SchemaNode* ev = r.schemas.forFile("maps/field.json")->root->field("events")->of.get();
  for (const char* k : {"switch", "variable", "alive", "dead", "round", "carries", "researched", "sees", "hasSkill", "support", "hasCondition",
                        "actor", "difficulty", "rosterAlive", "rosterDead", "inRoster"}) {
    bool found = false;
    for (const auto& v : ev->field("if")->of->variants) found = found || v.matchKey == k;
    QVERIFY2(found, k);
  }
  for (const char* k : {"battle", "turn", "reach", "defeated", "fight", "talk", "promoted", "support", "place"}) {
    bool found = false;
    for (const auto& v : ev->field("when")->variants) found = found || v.matchKey == k;
    QVERIFY2(found, k);
  }
}

void FormStoryTest::summariesReadLikeThePlayersStory() {
  StoryRig r;
  const SchemaNode& n = r.commandNode();
  QCOMPARE(summaryOf(n, parse(R"({"say":"The ford is the only way.","speaker":"hero"})")), QString("hero: \"The ford is the only way.\""));
  QCOMPARE(summaryOf(n, parse(R"({"say":"A voice.","speakerName":"Villager"})")), QString("Villager: \"A voice.\""));
  QCOMPARE(summaryOf(n, parse(R"({"say":"No speaker."})")), QString("\"No speaker.\""));  // the optional part is left out
  QCOMPARE(summaryOf(n, parse(R"({"changeSide":{"unit":"ally"},"side":"red"})")), QString("ally to red"));
  QCOMPARE(summaryOf(n, parse(R"({"changeSide":{"side":"blue"},"side":"red"})")), QString("side blue to red"));
  QCOMPARE(summaryOf(n, parse(R"({"removeUnit":{}})")), QString("any unit"));
  QCOMPARE(summaryOf(n, parse(R"({"moveUnit":"hero","x":3,"y":4})")), QString("hero to 3,4"));
  QString longText(200, 'x');
  QVERIFY(summaryOf(n, parse("{\"say\":\"" + longText + "\"}")).size() < 80);  // shortened with an ellipsis
}

void FormStoryTest::nestedCommandListsOpenAtAnyDepth() {
  StoryRig r;
  // if -> then -> if -> then -> if -> then -> say: three levels, one more than the schema tree holds.
  r.setEvents(R"([{"id":"deep","when":{"battle":"start"},"do":[
    {"if":[{"alive":"hero"}],"then":[
      {"if":[{"alive":"ally"}],"then":[
        {"if":[{"switch":"met"}],"then":[ {"say":"deep","speaker":"hero"} ],"else":[ {"setSwitch":"met","to":true} ]} ]} ]} ]}])");
  r.form->expandAll();
  r.flush();
  QVERIFY(r.listAt("events[0].do[0].then"));
  QVERIFY(r.listAt("events[0].do[0].then[0].then"));
  QVERIFY(r.listAt("events[0].do[0].then[0].then[0].then"));  // the third level is a lazy placeholder: it is built all the same
  auto* say = qobject_cast<ScalarField*>(r.form->fieldAt(StoryRig::path("events[0].do[0].then[0].then[0].then[0].say")));
  QVERIFY(say);
  QVERIFY(!r.form->body()->findChild<QLabel*>("notApplicable"));
  // Editing there is one undo step like anywhere else.
  const int before = r.stack()->index();
  QVERIFY(r.ctx->set(StoryRig::path("events[0].do[0].then[0].then[0].then[0].say"), Json::makeString("changed")));
  QCOMPARE(r.stack()->index(), before + 1);
  r.stack()->undo();
  QCOMPARE(QString::fromStdString((*r.at("events[0].do[0].then[0].then[0].then[0].say")).asString()), QString("deep"));
  // Nothing under a command is "Other keys" or a raw JSON field: every key of this script is described.
  r.flush();
  // The only "Other keys" box is the map's own, for the tile grid that stays raw JSON until a map painter exists.
  const auto boxes = r.form->body()->findChildren<QFrame*>("otherKeys");
  QCOMPARE((int)boxes.size(), 1);
  const QString text = boxes.front()->findChild<QPlainTextEdit*>("otherKeysText")->toPlainText();
  QVERIFY2(text.contains("\"tiles\"") && !text.contains("events") && !text.contains("then"), qPrintable(text));
}

void FormStoryTest::aLongScriptIsPagedAndEditableThroughUndo() {
  StoryRig r;
  QString cmds;
  for (int i = 0; i < 120; ++i) cmds += QString(R"(%1{"say":"line %2","speaker":"hero"})").arg(i ? "," : "").arg(i + 1);
  r.setEvents(QString(R"([{"id":"long","when":{"battle":"start"},"do":[%1]}])").arg(cmds));
  // Reaching a command on the third page shows that page.
  QVERIFY(r.form->focusPath(StoryRig::path("events[0].do[110].say")));
  r.flush();
  QWidget* list = r.listAt("events[0].do");
  QVERIFY(list);
  auto* label = list->findChild<QLabel*>("pageLabel");
  QVERIFY(label);
  QCOMPARE(label->text(), QString("Items 101 to 120 of 120"));
  QVERIFY(r.form->fieldAt(StoryRig::path("events[0].do[110].say")));
  QVERIFY(!r.form->fieldAt(StoryRig::path("events[0].do[3].say")));  // another page: not built
  // One collapsed line per command: the summary is on it.
  auto summaries = list->findChildren<QLabel*>("itemSummary");
  QCOMPARE((int)summaries.size(), 20);
  QVERIFY(summaries.front()->text().contains("line 101"));

  // Duplicate, move, delete: each one undo step, and the document says what it should.
  auto button = [&](const char* name, int itemOnPage) {
    return list->findChildren<QFrame*>("listItem").at(itemOnPage)->findChild<QToolButton*>(name);
  };
  const int base = r.stack()->index();
  button("itemDuplicate", 0)->click();
  r.flush();
  QCOMPARE((int)r.at("events[0].do")->size(), 121);
  QCOMPARE(QString::fromStdString(r.at("events[0].do[100]")->dump(0)), QString::fromStdString(r.at("events[0].do[101]")->dump(0)));
  QCOMPARE(r.stack()->index(), base + 1);
  list = r.listAt("events[0].do");
  button("itemDown", 0)->click();
  r.flush();
  QCOMPARE(r.stack()->index(), base + 2);
  list = r.listAt("events[0].do");
  button("itemRemove", 0)->click();
  r.flush();
  QCOMPARE((int)r.at("events[0].do")->size(), 120);
  QCOMPARE(r.stack()->index(), base + 3);
  r.stack()->undo();
  r.stack()->undo();
  r.stack()->undo();
  r.flush();
  QCOMPARE((int)r.at("events[0].do")->size(), 120);
  QVERIFY(r.at("events[0].do[100].say")->asString() == "line 101");
}

void FormStoryTest::addingACommandAsksWhichKind() {
  StoryRig r;
  r.setEvents(R"([{"id":"e","when":{"battle":"start"},"do":[{"say":"first"}]}])");
  std::vector<std::pair<QString, QString>> offered;
  r.ctx->setVariantPicker([&](const QString&, const std::vector<std::pair<QString, QString>>& entries) {
    offered = entries;
    for (size_t i = 0; i < entries.size(); ++i)
      if (entries[i].first == "giveItem") return (int)i;
    return -1;
  });
  r.form->focusPath(StoryRig::path("events[0].do[0].say"));
  r.flush();
  auto* add = r.listAt("events[0].do")->findChild<QPushButton*>("addItem");
  QVERIFY(add);
  const int before = r.stack()->index();
  add->click();
  r.flush();
  QVERIFY(offered.size() >= 60);
  for (const auto& e : offered) QVERIFY2(!e.second.isEmpty(), qPrintable(e.first + " has no help line"));  // every kind explains itself
  QCOMPARE(r.stack()->index(), before + 1);
  QCOMPARE((int)r.at("events[0].do")->size(), 2);
  QVERIFY(r.at("events[0].do[1].giveItem"));
  QVERIFY(r.at("events[0].do[1].to"));  // the required keys of the kind are there
  // Cancelling adds nothing.
  r.ctx->setVariantPicker([](const QString&, const std::vector<std::pair<QString, QString>>&) { return -1; });
  r.listAt("events[0].do")->findChild<QPushButton*>("addItem")->click();
  r.flush();
  QCOMPARE((int)r.at("events[0].do")->size(), 2);
}

void FormStoryTest::aThreeHundredCommandEventOpensFast() {
  StoryRig r;
  QString cmds;
  for (int i = 0; i < 300; ++i) {
    switch (i % 4) {
      case 0: cmds += QString(R"(%1{"say":"line %2","speaker":"hero"})").arg(i ? "," : "").arg(i); break;
      case 1: cmds += QString(R"(%1{"changeSide":{"unit":"ally"},"side":"red"})").arg(i ? "," : ""); break;
      case 2: cmds += QString(R"(%1{"if":[{"alive":"hero"}],"then":[{"say":"inside %2"}]})").arg(i ? "," : "").arg(i); break;
      default: cmds += QString(R"(%1{"moveUnit":"hero","x":2,"y":1})").arg(i ? "," : ""); break;
    }
  }
  QElapsedTimer t;
  t.start();
  r.setEvents(QString(R"([{"id":"big","when":{"battle":"start"},"do":[%1]}])").arg(cmds));
  const qint64 setMs = t.elapsed();
  t.restart();
  QVERIFY(r.form->focusPath(StoryRig::path("events[0].do[0].say")));
  r.flush();
  const qint64 openMs = t.elapsed();
  t.restart();
  QVERIFY(r.form->focusPath(StoryRig::path("events[0].do[248].say")));  // another page, all the way down
  r.flush();
  const qint64 jumpMs = t.elapsed();
  qInfo("300-command event: shown in %lld ms, opened in %lld ms, page 6 in %lld ms", (long long)setMs, (long long)openMs, (long long)jumpMs);
  QVERIFY2(setMs + openMs + jumpMs < 3000, "a 300-command event must not make the form slow");
  QVERIFY(r.form->fieldAt(StoryRig::path("events[0].do[248].say")));
}

QTEST_MAIN(FormStoryTest)
#include "test_form_story.moc"
