// Schema vs loader: the schemas must tell the truth about what the engine accepts.
//
// For every key of every schema (nested ones too) a minimal valid game is built in a temp folder (a copy of a test fixture
// plus the few edits that key depends on, see kContexts), the key is set to a schema-valid sample value, and the engine's own
// project check (validateProject, the one the Problems dock shows) must report nothing. For keys with an enum, a range or an
// id reference one invalid value is set as well and the check must complain about that file. Then the reverse: every key in
// the fixtures under tests/fixtures that sits in a described section must be known to the schema.
//
// A key that cannot be sampled automatically is listed in kSkipped with the reason, never skipped silently, and a key that no
// context claims is a failure (a new schema key must say where it is tested).
#include <QDir>
#include <QRegularExpression>
#include <QtTest>
#include <map>

#include "id_source.h"
#include "project.h"
#include "schema.h"
#include "table_shape.h"
#include "test_support.h"
#include "validate.h"

using namespace ote_native;
using ote::Json;

namespace {

// --- the table of contexts ------------------------------------------------------------------------------------------

struct Op {
  QString file;  // project-relative
  QString path;  // dotted, with [n]: rules.actionPoints, jobs[2].learn
  QString json;  // the new value; "-" removes the key
};

struct Context {
  const char* name;
  const char* fixture;
  std::vector<Op> mods;                    // edits that make the fixture suit the keys below
  std::vector<const char*> claims;         // "schema:key.path" prefixes this context tests (first matching context wins)
  std::map<QString, QString> overrides;    // "schema:key.path" -> sample JSON when the generated one would not do
};

// Keys are written "<schema>:<dotted path>", list items as "[]": "game:rules.ranks.levels[].at".
const std::vector<Op> kJobsMods = {
    {"data/skills.json", "skills[+]", R"({"id":"extra_skill","name":"Extra","blocks":[{"when":"always","do":[{"stats":{"def":2}}]}]})"},
    {"data/abilities.json", "abilities[+]",
     R"({"id":"extra_ability","name":"Extra","target":"ally","range":[1,2],"effect":{"heal":10},"cost":"action"})"},
    {"data/jobs.json", "jobs[+]", R"({"id":"tester","name":"Tester","unitType":"squire"})"}};

const std::vector<Context>& contexts() {
  static const std::vector<Context> c = {
      {"damage-types", "damage-types", {},
       {"game:damageTypes", "game:resistRange", "weapons:damageType", "units:resist"},
       {{"game:damageTypes", R"([{"id":"kinetic"},{"id":"emf"},{"id":"fire"}])"}}},
      {"action-points", "action-points", {},
       {"game:rules.actionPoints", "game:resources", "weapons:apCost", "weapons:area.apExtra", "units:ap"},
       {{"weapons:area", R"({"shape":"radius","radius":1})"}, {"game:resources", R"([{"id":"ap"},{"id":"gold"}])"}}},
      {"height", "height-rules", {},
       {"game:rules.height", "weapons:heightRange", "weapons:area.vertical", "units:jump"},
       {{"weapons:area", R"({"shape":"radius","radius":1})"}}},
      {"facing", "facing", {}, {"game:rules.facing", "maps:units[].facing"}, {{"maps:units[].facing", "\"north\""}}},
      {"speed", "speed-order", {},
       {"game:rules.turnOrder", "weapons:chargeSpeed", "weapons:chargeTargets", "weapons:interruptedBy"},
       {{"game:rules.turnOrder", R"({"mode":"speed","stat":"spd"})"}, {"game:rules.turnOrder.mode", "\"speed\""}}},
      {"research", "research-branches", {}, {"research:", "game:rules.research"},
       {{"research:requires", R"(["tactics"])"},
        {"research:grants", R"({"skills":["bulwark"]})"},
        {"research:grants.skills", R"(["bulwark"])"},
        {"research:bonus", R"({"stats":{"str":1}})"}}},
      // Commanders before factions: a faction's "commanders" list needs the commanders file.
      {"commanders", "commanders", {}, {"commanders:", "factions:commanders", "maps:sides[].commander"},
       {{"commanders:passive", R"(["bulwark"])"},
        {"commanders:meter.max", "200"},
        {"commanders:powers", R"([{"id":"p","cost":1,"modifiers":["frenzy"],"effects":[{"heal":5}]}])"},
        {"commanders:powers[].modifiers", R"(["frenzy"])"},
        {"commanders:powers[].effects", R"([{"heal":5}])"}}},
      {"factions", "factions", {}, {"factions:", "maps:sides[].faction"},
       {{"factions:modifiers", R"(["bulwark"])"},
        {"factions:ai", R"({"weights":{"dmg":3}})"},
        {"factions:ai.weights", R"({"dmg":3})"}}},
      {"merit-ranks", "merit-ranks", {}, {"game:rules.ranks"},
       {{"game:rules.ranks.levels[].badge", "\"I\""},
        {"game:rules.ranks.levels", R"([{"at":1000,"name":"Top"}])"},
        {"game:rules.ranks", R"({"levels":[{"at":1000,"name":"Top"}]})"}}},
      {"rank-table", "unit-ranks", {}, {"game:rules.progression.levels"},
       {{"game:rules.progression.levels[].badge", "\"I\""}, {"game:rules.progression.levels", R"([{"at":1000,"name":"Top"}])"}}},
      {"progression", "area-fire", {}, {"game:rules.progression", "units:maxLevel"},
       {{"game:rules.progression.xpHit", "\"10\""}, {"game:rules.progression.xpKill", "\"20\""}}},
      // Jobs: characters and maps of the fixture name the learned skills of the existing jobs, so those jobs stay as they are.
      // A new job "tester" (the last row, where the table tests place their samples) and a new skill and ability for it to
      // teach are added instead (one skill or ability is taught by one job only).
      {"jobs-learn-skill", "jobs", kJobsMods, {"jobs:learn[].skill"},
       {{"jobs:learn", R"([{"skill":"extra_skill","cost":1,"slot":"support"}])"}, {"jobs:learn[].skill", "\"extra_skill\""}}},
      {"jobs-learn-ability", "jobs", kJobsMods, {"jobs:learn[].ability"},
       {{"jobs:learn", R"([{"ability":"extra_ability","cost":1,"slot":"action"}])"}, {"jobs:learn[].ability", "\"extra_ability\""}}},
      {"jobs", "jobs", kJobsMods, {"jobs:", "game:rules.jobs"},
       {{"jobs:learn", R"([{"skill":"extra_skill","cost":1,"slot":"support"}])"},
        {"game:rules.jobs", R"({"jp":{"perAction":1},"jpLevels":[20,60,120]})"}}},
      {"characters", "damage-types", {}, {"weapons:wieldedBy"}, {}},
      {"area-line", "ai-scripted", {}, {"weapons:area.length"}, {{"weapons:area", R"({"shape":"line","length":2})"}}},
      {"core", "ai-scripted", {},
       {"terrain:", "items:", "units:", "weapons:", "maps:", "game:rules.counterAttacks", "game:rules.defeat"},
       {{"weapons:area", R"({"shape":"radius","radius":2})"},
        {"weapons:area.shape", "\"radius\""},
        {"maps:sides", R"([{"id":"blue","controller":"ai"},{"id":"red","controller":"ai"}])"},
        {"maps:sides[].controller", "\"ai\""},
        {"maps:units", R"([{"type":"soldier","side":"blue","x":0,"y":0}])"},
        {"maps:units[].side", "\"blue\""},
        {"maps:units[].x", "0"},
        {"maps:units[].y", "0"}}},
  };
  return c;
}

// Keys that are not sampled, with the reason. Listed on purpose: nothing is skipped silently.
const std::map<QString, QString>& skipped() {
  static const std::map<QString, QString> s = {};
  return s;
}

// Extra values (another enum word, the end of a range) the loader legitimately refuses in the fixture's context, and
// invalid values it does not refuse at load time. "key = value" -> reason.
const std::map<QString, QString>& extraSkips() {
  static const std::map<QString, QString> s = {
      {"commanders:meter.perDamageTaken = 0", "the meter must fill by at least one of its three rates (cross-key rule)"},
      {"commanders:powers[].cost = 100000", "a power costs at most the meter's own max (cross-key rule)"},
      {"game:rules.turnOrder.mode = \"sides\"", "\"sides\" takes no other keys of the block (cross-key rule)"},
      {"game:rules.progression.levels[].at = 1000000", "ranks must rise; the second rank cannot start at the same value (cross-key rule)"},
      {"game:rules.ranks.levels[].at = 1000000", "ranks must rise; the second rank cannot start at the same value (cross-key rule)"},
      {"game:rules.jobs.slots.support = 0", "the fixture's characters equip support abilities, so zero slots is refused (cross-file rule)"},
      {"items:kind = \"weapon\"", "heal / raise / bonus belong to one kind (cross-key rule)"},
      {"items:kind = \"accessory\"", "heal / raise / bonus belong to one kind (cross-key rule)"},
      {"items:kind = \"staff\"", "heal / raise / bonus belong to one kind (cross-key rule)"},
      {"jobs:requires[].level = 20", "a required level cannot exceed the required job's level table (cross-key rule)"},
      {"maps:sides[].controller = \"human\"", "a unit with a computer behaviour cannot stand on a person's side (cross-key rule)"},
  };
  return s;
}

// The last path component of an identity key: changing it would break every reference to the entry.
bool isIdentityKey(const QString& keyPath) { return keyPath.endsWith(".id") || keyPath.endsWith(":id"); }

// --- helpers ---------------------------------------------------------------------------------------------------------

struct KeyInfo {
  QString id;      // schema:path
  QString schema;  // schema name
  QStringList steps;  // "key" or "[]"
  const SchemaNode* node;
  const FileSchema* fs;
};

void collect(const FileSchema* fs, const SchemaNode& obj, const QString& prefix, QStringList steps, std::vector<KeyInfo>& out) {
  for (const auto& f : obj.fields) {
    const QString key = QString::fromStdString(f.first);
    const QString path = prefix.isEmpty() ? key : prefix + "." + key;
    QStringList st = steps;
    st << key;
    out.push_back({QString::fromStdString(fs->name) + ":" + path, QString::fromStdString(fs->name), st, f.second.get(), fs});
    if (f.second->kind == SchemaNode::Kind::Object) collect(fs, *f.second, path, st, out);
    if (f.second->kind == SchemaNode::Kind::List && f.second->length < 0 && f.second->of && f.second->of->kind == SchemaNode::Kind::Object) {
      QStringList st2 = st;
      st2 << "[]";
      collect(fs, *f.second->of, path + "[]", st2, out);
    }
  }
}

bool claims(const QString& claim, const QString& keyId) {
  if (!keyId.startsWith(claim)) return false;
  if (claim.endsWith(':') || keyId.size() == claim.size()) return true;
  const QChar next = keyId[claim.size()];
  return next == '.' || next == '[';
}

const Context* contextFor(const QString& keyId) {
  for (const Context& c : contexts())
    for (const char* cl : c.claims)
      if (claims(cl, keyId)) return &c;
  return nullptr;
}

Json parse(const QString& text) {
  Json j;
  std::string err;
  if (!Json::parse(text.toStdString(), j, err)) qFatal("bad test JSON: %s: %s", qPrintable(text), err.c_str());
  return j;
}

Json* memberOf(Json& obj, const std::string& key) {
  for (auto& m : obj.mutableMembers())
    if (m.first == key) return &m.second;
  return nullptr;
}

// "jobs[2].learn" -> steps; sets (or with "-" removes) the value there.
bool applyOp(Json& root, const QString& path, const QString& jsonText) {
  QStringList parts = path.split('.');
  Json* node = &root;
  for (int i = 0; i < parts.size(); ++i) {
    QString p = parts[i];
    int idx = -1;
    const int br = p.indexOf('[');
    if (br >= 0 && p.endsWith("[+]") && i + 1 == parts.size()) {  // append to the list
      Json* list = memberOf(*node, p.left(br).toStdString());
      if (!list || !list->isArray()) return false;
      list->push(parse(jsonText));
      return true;
    }
    if (br >= 0) { idx = p.mid(br + 1, p.size() - br - 2).toInt(); p = p.left(br); }
    const bool last = i + 1 == parts.size() && idx < 0;
    if (last) {
      if (jsonText == "-") return node->erase(p.toStdString());
      node->set(p.toStdString(), parse(jsonText));
      return true;
    }
    node = memberOf(*node, p.toStdString());
    if (!node) return false;
    if (idx >= 0) {
      if ((size_t)idx >= node->items().size()) return false;
      node = &node->mutableItems()[(size_t)idx];
      if (i + 1 == parts.size()) { *node = parse(jsonText); return true; }
    }
  }
  return false;
}

QByteArray text(const Json& j) { return QByteArray::fromStdString(j.dump(2) + "\n"); }

Json readJson(const QString& path) { return parse(QString::fromUtf8(ote_test::readFile(path))); }

// The first data file a schema describes in this game folder (maps: the first map).
QString fileOf(const QString& dir, const FileSchema* fs) {
  for (const std::string& f : fs->files) {
    if (f.find('*') == std::string::npos) return QString::fromStdString(f);
    const QString folder = QString::fromStdString(f.substr(0, f.find('*')));
    const QStringList names = QDir(dir + "/" + folder).entryList({"*.json"}, QDir::Files, QDir::Name);
    if (!names.isEmpty()) return folder + names.first();
  }
  return {};
}

QString problemsText(const std::vector<ote_editor::Problem>& ps) {
  QStringList l;
  for (const auto& p : ps) l << QString::fromStdString(p.file + ": " + p.message);
  return l.join(" | ");
}

}  // namespace

class ConformanceTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void everyContextStartsValid();
  void everySchemaKeyIsAcceptedByTheLoader();
  void anInvalidValueIsRefusedNamingTheFile();
  void everyFixtureKeyOfADescribedSectionIsKnownToTheSchema();
  void embeddedSchemasEqualTheSourceFiles();

 private:
  struct Built {
    ote_test::Scratch tmp;
    QString dir;
  };
  // Copy of the context's fixture with its edits applied.
  std::unique_ptr<Built> build(const Context& c);
  // Sets `value` (or the sample when null) at the key's place and returns the check's problems.
  // `value` null = the key's sample. `extras` (only with a null value) receives the other values worth trying for this key:
  // every word of an enum, both ends of a range.
  std::vector<ote_editor::Problem> place(const Context& c, const KeyInfo& k, const Json* value, QString* file, bool* placed,
                                         std::vector<Json>* extras = nullptr);
  Json sampleFor(const Context& c, const KeyInfo& k, const QString& keyId, const SchemaNode& node, const IdLookup& ids) const;

  SchemaSet schemas_;
  std::vector<KeyInfo> keys_;
};

void ConformanceTest::initTestCase() {
  schemas_.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  QVERIFY2(schemas_.errors().isEmpty(), qPrintable(schemas_.errors().join("\n")));
  QVERIFY(schemas_.all().size() >= 8);
  for (const auto& fs : schemas_.all()) {
    const SchemaNode* top = fs->rows ? fs->rows.get() : fs->root.get();
    collect(fs.get(), *top, QString(), {}, keys_);
  }
}

std::unique_ptr<ConformanceTest::Built> ConformanceTest::build(const Context& c) {
  auto b = std::make_unique<Built>();
  b->dir = ote_test::copyFixture(b->tmp, c.fixture);
  for (const Op& op : c.mods) {
    Json root = readJson(b->dir + "/" + op.file);
    if (!applyOp(root, op.path, op.json)) qFatal("context %s: cannot apply %s at %s", c.name, qPrintable(op.file), qPrintable(op.path));
    ote_test::writeFile(b->dir + "/" + op.file, text(root));
  }
  return b;
}

Json ConformanceTest::sampleFor(const Context& c, const KeyInfo&, const QString& keyId, const SchemaNode& node, const IdLookup& ids) const {
  auto it = c.overrides.find(keyId);
  if (it != c.overrides.end()) return parse(it->second);
  return sampleValue(node, ids);
}

std::vector<ote_editor::Problem> ConformanceTest::place(const Context& c, const KeyInfo& k, const Json* value, QString* fileOut, bool* placed,
                                                       std::vector<Json>* extras) {
  *placed = false;
  auto b = build(c);
  const QString rel = fileOf(b->dir, k.fs);
  *fileOut = rel;
  // The ids a sample may point at come from the game as it stands (live project, like the editor's combo boxes).
  Project project(b->dir);
  IdSource ids(&project);
  const IdLookup lookup = [&](const std::vector<std::string>& s) { return ids.ids(s); };

  Json root = readJson(b->dir + "/" + rel);
  Json* node = &root;
  const SchemaNode* sn = k.fs->rows ? k.fs->rows.get() : k.fs->root.get();
  if (k.fs->rows) {  // a table file: the last row (so references to the first entries never point at the row itself)
    auto path = findTableArray(root);
    if (!path) qFatal("%s is not a table in context %s", qPrintable(rel), c.name);
    node = resolvePath(root, *path) ? const_cast<Json*>(resolvePath(root, *path)) : nullptr;
    if (!node || node->items().empty()) qFatal("empty table");
    node = &node->mutableItems().back();
  }
  QString path;
  const QString schemaName = k.schema;
  for (int i = 0; i < k.steps.size(); ++i) {
    const QString step = k.steps[i];
    const bool last = i + 1 == k.steps.size();
    if (step == "[]") {
      sn = sn->of.get();
      if (!node->isArray() || node->items().empty()) qFatal("no list item at %s", qPrintable(path));
      node = &node->mutableItems()[0];
      path += "[]";
      continue;
    }
    path = path.isEmpty() ? step : (path.endsWith("[]") ? path + "." + step : path + "." + step);
    const SchemaNode* child = sn->field(step.toStdString());
    const QString keyId = schemaName + ":" + path;
    if (!node->isObject()) return {};  // cannot happen: ancestors are made objects below
    if (last) {
      const Json first = sampleFor(c, k, keyId, *child, lookup);
      if (extras && !value) {
        const bool overridden = c.overrides.count(keyId) > 0 && child->kind != SchemaNode::Kind::Enum;
        if (child->kind == SchemaNode::Kind::Enum)
          for (const std::string& w : child->values)
            if (Json::makeString(w).dump(0) != first.dump(0)) extras->push_back(Json::makeString(w));
        if ((child->kind == SchemaNode::Kind::Int || child->kind == SchemaNode::Kind::Number) && !overridden) {
          if (child->min && Json::makeNumber(*child->min).dump(0) != first.dump(0)) extras->push_back(Json::makeNumber(*child->min));
          if (child->max) extras->push_back(Json::makeNumber(*child->max));
        }
      }
      if (k.id == "weapons:area.shape" && value) {
        // Each shape needs its own companion keys (radius / length): the whole area object is replaced.
        const std::string shape = value->asString();
        Json area = Json::makeObject();
        area.set("shape", *value);
        if (shape == "radius" || shape == "cross") area.set("radius", Json::makeNumber(2));
        if (shape == "line") area.set("length", Json::makeNumber(2));
        *node = area;
        *placed = true;
        break;
      }
      node->set(step.toStdString(), value ? *value : first);
      *placed = memberOf(*node, step.toStdString()) != nullptr;  // a sample that never landed would test nothing
      break;
    }
    Json* existing = memberOf(*node, step.toStdString());
    // An ancestor that exists in the wrong form (jump.default written as a plain number) is replaced by the sample form.
    if (existing && !typeOk(*child, *existing)) {
      node->erase(step.toStdString());
      existing = nullptr;
    }
    if (existing && isAlt(*child, *existing)) {
      node->erase(step.toStdString());
      existing = nullptr;
    }
    if (!existing) {
      node->set(step.toStdString(), sampleFor(c, k, keyId, *child, lookup));
      existing = memberOf(*node, step.toStdString());
    }
    node = existing;
    sn = child;
  }
  ote_test::writeFile(b->dir + "/" + rel, text(root));
  return ote_editor::validateProject(b->dir.toStdString());
}

void ConformanceTest::everyContextStartsValid() {
  QStringList bad;
  for (const Context& c : contexts()) {
    auto b = build(c);
    const auto problems = ote_editor::validateProject(b->dir.toStdString());
    if (!problems.empty()) bad << QString("%1 (%2): %3").arg(c.name, c.fixture, problemsText(problems));
  }
  QVERIFY2(bad.isEmpty(), qPrintable(bad.join("\n")));
}

void ConformanceTest::everySchemaKeyIsAcceptedByTheLoader() {
  QStringList failures, unclaimed, notes;
  int tested = 0, skippedCount = 0, extraChecked = 0;
  for (const KeyInfo& k : keys_) {
    if (isIdentityKey(k.id)) {
      ++skippedCount;
      notes << k.id + "  (identity key: renaming it would break the references to the entry)";
      continue;
    }
    if (k.node->kind == SchemaNode::Kind::Object && !k.node->block && !k.node->alt.size()) {
      ++skippedCount;  // a plain nested object: replacing it would drop its siblings; its members are tested one by one
      notes << k.id + "  (container: its members are tested one by one, as are its blocks)";
      continue;
    }
    auto sk = skipped().find(k.id);
    if (sk != skipped().end()) {
      ++skippedCount;
      notes << k.id + "  (" + sk->second + ")";
      continue;
    }
    const Context* c = contextFor(k.id);
    if (!c) {
      unclaimed << k.id;
      continue;
    }
    QString file;
    bool placed = false;
    std::vector<Json> extras;
    const auto problems = place(*c, k, nullptr, &file, &placed, &extras);
    ++tested;
    if (!placed) failures << QString("%1 [%2]: the sample could not be placed in the fixture").arg(k.id, c->name);
    else if (!problems.empty()) failures << QString("%1 [%2]: %3").arg(k.id, c->name, problemsText(problems));
    for (const Json& v : extras) {  // every enum word and both ends of a range must be accepted too
      const QString note = k.id + " = " + QString::fromStdString(v.dump(0));
      if (extraSkips().count(note)) {
        notes << note + "  (" + extraSkips().at(note) + ")";
        continue;
      }
      const auto more = place(*c, k, &v, &file, &placed);
      ++extraChecked;
      if (!placed) failures << note + ": could not be placed";
      else if (!more.empty()) failures << QString("%1 [%2]: %3").arg(note, c->name, problemsText(more));
    }
  }
  qInfo("conformance: %d keys set to a sample and accepted (+%d more enum words / range ends), %d skipped on purpose, %d schema files",
        tested, extraChecked, skippedCount, (int)schemas_.all().size());
  for (const QString& n : notes) qInfo("  skipped: %s", qPrintable(n));
  QVERIFY2(unclaimed.isEmpty(), qPrintable("keys no context tests:\n" + unclaimed.join("\n")));
  QVERIFY2(failures.isEmpty(), qPrintable("the loader refuses the sample of:\n" + failures.join("\n")));
}

void ConformanceTest::anInvalidValueIsRefusedNamingTheFile() {
  QStringList failures, notes;
  int checked = 0;
  for (const KeyInfo& k : keys_) {
    if (isIdentityKey(k.id) || skipped().count(k.id)) continue;
    const Context* c = contextFor(k.id);
    if (!c) continue;
    const SchemaNode& n = *k.node;
    // Invalid values for a node that has a rule the loader enforces (enum, range, id reference): below and above the range.
    auto badScalar = [](const SchemaNode& s, int which) -> std::optional<Json> {  // which: 0 = below the range, 1 = above
      if (s.kind == SchemaNode::Kind::Enum) return which == 0 ? std::optional<Json>(Json::makeString("zz_not_allowed")) : std::nullopt;
      if (s.kind == SchemaNode::Kind::Ref) return which == 0 ? std::optional<Json>(Json::makeString("zz_no_such_id")) : std::nullopt;
      const bool num = s.kind == SchemaNode::Kind::Int || s.kind == SchemaNode::Kind::Number;
      if (num && which == 0 && s.min) return Json::makeNumber(*s.min - 1);
      if (num && which == 1 && s.max) return Json::makeNumber(*s.max + 1);
      return std::nullopt;
    };
    for (int which = 0; which < 2; ++which) {
      std::optional<Json> bad;
      if (n.isScalar()) bad = badScalar(n, which);
      else if (n.kind == SchemaNode::Kind::List && n.of && n.of->isScalar()) {
        if (auto b2 = badScalar(*n.of, which)) {
          Json arr = Json::makeArray();
          arr.push(*b2);
          for (int i = 1; i < n.length; ++i) arr.push(Json::makeNumber(1));  // a pair: the first item is the bad one
          bad = arr;
        }
      } else if (n.kind == SchemaNode::Kind::Map && n.of && which == 0) {  // a key that is no id of the game
        Json o = Json::makeObject();
        o.set("zz_no_such_id", Json::makeNumber(1));
        bad = o;
      }
      if (!bad) continue;
      const QString note = k.id + " = " + QString::fromStdString(bad->dump(0));
      if (extraSkips().count(note)) {
        notes << note + "  (" + extraSkips().at(note) + ")";
        continue;
      }
      QString file;
      bool placed = false;
      const auto problems = place(*c, k, &*bad, &file, &placed);
      ++checked;
      if (!placed) { failures << QString("%1 [%2]: the invalid value could not be placed").arg(k.id, c->name); continue; }
      bool named = false;
      for (const auto& p : problems) named = named || QString::fromStdString(p.file) == file;
      if (!named) failures << QString("%1 [%2] -> %3").arg(note, c->name, problems.empty() ? "no problem at all" : problemsText(problems));
    }
  }
  qInfo("conformance: %d invalid values refused with a problem naming the file", checked);
  for (const QString& n : notes) qInfo("  not refused by the loader: %s", qPrintable(n));
  QVERIFY(checked >= 20);
  QVERIFY2(failures.isEmpty(), qPrintable("the loader accepts or misplaces an invalid value of:\n" + failures.join("\n")));
}

namespace {

// Keys in the fixtures that sit in a section the schema describes only in part. Excused ONLY where the schema is partial by
// design (the top level of a core file's rows, the keys directly under "rules", the placement keys of a map unit); a key
// missing from inside a described block (rules.actionPoints, a faction, a job ...) is always a failure.
bool excusedKey(const QString& schema, const QString& path, QString* why) {
  const QStringList seg = path.split('.');
  auto yes = [&](const char* w) { *why = w; return true; };
  if (schema == "game") {
    if (seg.size() == 1) return yes("game.json top level (name, stats, grid, view, formulas ...): the schema starts with the mechanics");
    if (seg.size() == 2 && seg[0] == "rules") return yes("older rule blocks (economy, production, items, supply, ai ...): not described yet");
    return false;
  }
  if (schema == "maps") {
    if (seg.size() == 1) return yes("map structure (tiles, legend, events, structures, objectives ...): not described yet");
    if (seg.size() == 2 && (seg[0] == "units[]" || seg[0] == "sides[]"))
      return yes("older side / unit placement keys (behaviour, items, xp, merit, ...): not described yet");
    return false;
  }
  if (schema == "units" || schema == "weapons" || schema == "items" || schema == "terrain")
    if (seg.size() == 1) return yes("older keys of a core file (abilities, promotions, ammo, skills, inflict, vision ...): not described yet");
  return false;
}

// Walks a value against its schema node; unknown keys of objects are reported unless an Uncovered prefix excuses them.
void reverse(const SchemaNode& node, const Json& value, const QString& schema, const QString& path, const QString& where,
             QStringList& unknown, QStringList& excusedList) {
  if (!value.isObject()) return;
  for (const auto& m : value.members()) {
    const QString key = QString::fromStdString(m.first);
    const QString p = path.isEmpty() ? key : path + "." + key;
    const SchemaNode* f = node.field(m.first);
    if (!f) {
      const QString id = schema + ":" + p;
      QString why;
      (excusedKey(schema, p, &why) ? excusedList : unknown) << where + "  " + id;
      continue;
    }
    if (f->kind == SchemaNode::Kind::Object) reverse(*f, m.second, schema, p, where, unknown, excusedList);
    if (f->kind == SchemaNode::Kind::List && f->length < 0 && f->of && f->of->kind == SchemaNode::Kind::Object)
      for (const Json& item : m.second.items()) reverse(*f->of, item, schema, p + "[]", where, unknown, excusedList);
  }
}

}  // namespace

void ConformanceTest::everyFixtureKeyOfADescribedSectionIsKnownToTheSchema() {
  QStringList unknown, excused;
  const QDir fixtures(QStringLiteral(OTE_SOURCE_DIR "/tests/fixtures"));
  int files = 0;
  for (const QString& name : fixtures.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
    const QString dir = fixtures.filePath(name);
    QStringList rels;
    QDirIterator it(dir, {"*.json"}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) rels << QDir(dir).relativeFilePath(it.next());
    for (const QString& rel : rels) {
      const FileSchema* fs = schemas_.forFile(rel);
      if (!fs) continue;
      ote::Json j;
      std::string err;
      if (!Json::parse(QString::fromUtf8(ote_test::readFile(dir + "/" + rel)).toStdString(), j, err)) continue;  // bad_* fixtures
      ++files;
      const QString where = name + "/" + rel;
      const QString sname = QString::fromStdString(fs->name);
      if (fs->rows) {
        auto p = findTableArray(j);
        if (!p) continue;
        for (const Json& row : resolvePath(j, *p)->items()) reverse(*fs->rows, row, sname, QString(), where, unknown, excused);
      } else {
        reverse(*fs->root, j, sname, QString(), where, unknown, excused);
      }
    }
  }
  unknown.removeDuplicates();
  excused.removeDuplicates();
  qInfo("reverse check: %d fixture files looked at; %d keys excused as not described yet", files, (int)excused.size());
  QVERIFY2(unknown.isEmpty(), qPrintable("keys in fixtures that the schema does not know (add them to editor/schema, or to kUncovered with a reason):\n" + unknown.join("\n")));
}

void ConformanceTest::embeddedSchemasEqualTheSourceFiles() {
  SchemaSet embedded;
  embedded.loadFolder(QStringLiteral(":/schema"));
  QVERIFY2(embedded.errors().isEmpty(), qPrintable(embedded.errors().join("\n")));
  QDir src(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  const QStringList names = src.entryList({"*.json"}, QDir::Files);
  QCOMPARE((int)embedded.all().size(), (int)names.size());
  for (const QString& n : names) {
    QFile e(":/schema/" + n);
    QVERIFY2(e.open(QIODevice::ReadOnly), qPrintable(n + " is not built into the program"));
    QCOMPARE(e.readAll(), ote_test::readFile(src.filePath(n)));
  }
}

QTEST_MAIN(ConformanceTest)
#include "test_conformance.moc"
