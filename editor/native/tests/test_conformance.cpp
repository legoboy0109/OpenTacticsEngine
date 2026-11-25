// Schema vs loader: the schemas must tell the truth about what the engine accepts.
//
// For every key of every schema (nested ones, list items, map entries and every shape of a variant too) a minimal valid game is
// built in a temp folder (a copy of a test fixture plus the few edits that key depends on, see conformance_*.cpp), the key is
// set to a schema-valid sample value, and the engine's own project check (validateProject, the one the Problems dock shows)
// must report nothing. For keys with an enum, a range or an id reference one invalid value is set as well and the check must
// complain about that file. Every optional block is also "enabled alone" (what the form's Enable switch writes) and must be
// accepted. Then the reverse: every key in the fixtures and demo games that sits in a described section must be known to the
// schema.
//
// A key that cannot be sampled automatically is listed with the reason in the tables of conformance_*.cpp, never skipped
// silently, and a key that no context claims is a failure (a new schema key must say where it is tested).
#include <QDir>
#include <QRegularExpression>
#include <QtTest>
#include <map>

#include "conformance_contexts.h"
#include "id_source.h"
#include "project.h"
#include "schema.h"
#include "story_ids.h"
#include "table_shape.h"
#include "test_support.h"
#include "validate.h"

using namespace ote_native;
using namespace conformance;
using ote::Json;

namespace {

// Exact key, or a prefix written with a trailing "*" ("conditions:blocks[].do[]{arc}*").
const QString* skipReason(const QString& id) {
  auto it = skipped().find(id);
  if (it != skipped().end()) return &it->second;
  for (const auto& s : skipped())
    if (s.first.endsWith('*') && id.startsWith(s.first.left(s.first.size() - 1))) return &s.second;
  return nullptr;
}

// An extra skip is "key = value" exactly, or a pattern with a leading "*" (any key ending so: "*{tag}.tag = \"zz_no_such_id\"").
const QString* extraSkipReason(const QString& note) {
  auto it = extraSkips().find(note);
  if (it != extraSkips().end()) return &it->second;
  for (const auto& s : extraSkips())
    if (s.first.startsWith('*') && note.endsWith(s.first.mid(1))) return &s.second;
  return nullptr;
}

// OTE_CONFORMANCE_ONLY="graphics:battle" tries only the keys that start so (a way to iterate on one section; unset: every key).
bool filteredOut(const QString& keyId) {
  static const QString only = qEnvironmentVariable("OTE_CONFORMANCE_ONLY");
  return !only.isEmpty() && !keyId.startsWith(only);
}

// The last path component of an identity key: changing it would break every reference to the entry.
bool isIdentityKey(const QString& keyPath) { return keyPath.endsWith(".id") || keyPath.endsWith(":id"); }

// --- helpers ---------------------------------------------------------------------------------------------------------

struct KeyInfo {
  QString id;         // schema:path
  QString schema;     // schema name
  QStringList steps;  // "key", "[]" (list item), "*" (map entry) or "{variant name}"
  const SchemaNode* node;
  const FileSchema* fs;
};

// A stand-in node for a constant variant ("sides"): nothing to describe but the value.
const SchemaNode& constantNode() {
  static const SchemaNode n;
  return n;
}

void collectObject(const FileSchema* fs, const SchemaNode& obj, const QString& prefix, const QStringList& steps, std::vector<KeyInfo>& out);

// Walks into the children of a node whose own key is already listed: object members, list items, map entries, variant shapes.
void descend(const FileSchema* fs, const SchemaNode& n, const QString& path, const QStringList& steps, std::vector<KeyInfo>& out) {
  using K = SchemaNode::Kind;
  if (n.lazy) return;  // a recursive definition nested deeper than two levels: the same keys as one level up
  const QString schema = QString::fromStdString(fs->name);
  if (n.kind == K::Object) collectObject(fs, n, path, steps, out);
  if (n.kind == K::Variant) {
    for (const auto& v : n.variants) {
      const QString name = QString::fromStdString(v.name);
      const QString vp = path + "{" + name + "}";
      QStringList vs = steps;
      vs << "{" + name + "}";
      const SchemaNode* node = v.node ? v.node.get() : &constantNode();
      out.push_back({schema + ":" + vp, schema, vs, node, fs});
      if (v.node) descend(fs, *v.node, vp, vs, out);
    }
  }
  if (n.kind == K::List && n.length < 0 && n.of) {
    QStringList st = steps;
    st << "[]";
    descend(fs, *n.of, path + "[]", st, out);
  }
  if (n.kind == K::Map && n.of && !n.of->isScalar() && n.of->kind != K::Json) {
    QStringList st = steps;
    st << "*";
    descend(fs, *n.of, path + ".*", st, out);
  }
}

void collectObject(const FileSchema* fs, const SchemaNode& obj, const QString& prefix, const QStringList& steps, std::vector<KeyInfo>& out) {
  for (const auto& f : obj.fields) {
    if (f.second->lazy) continue;
    const QString key = QString::fromStdString(f.first);
    const QString path = prefix.isEmpty() ? key : prefix + "." + key;
    QStringList st = steps;
    st << key;
    out.push_back({QString::fromStdString(fs->name) + ":" + path, QString::fromStdString(fs->name), st, f.second.get(), fs});
    descend(fs, *f.second, path, st, out);
  }
}

bool claims(const QString& claim, const QString& keyId) {
  if (!keyId.startsWith(claim)) return false;
  const qsizetype lastOpen = claim.lastIndexOf('{');
  if (lastOpen >= 0 && claim.indexOf('}', lastOpen) < 0) return true;  // a claim naming the start of a variant's name
  if (claim.endsWith(':') || keyId.size() == claim.size()) return true;
  const QChar next = keyId[claim.size()];
  return next == '.' || next == '[' || next == '{';
}

// The context with the most specific (longest) matching claim; the first of equals.
const Context* contextFor(const QString& keyId) {
  const Context* best = nullptr;
  size_t bestLen = 0;
  for (const Context& c : contexts())
    for (const char* cl : c.claims)
      if (claims(cl, keyId) && (!best || strlen(cl) > bestLen)) {
        best = &c;
        bestLen = strlen(cl);
      }
  return best;
}

QString g_working;  // the key being tried, for the message of a bad test table entry

Json parse(const QString& text) {
  Json j;
  std::string err;
  if (!Json::parse(text.toStdString(), j, err)) qFatal("bad test JSON while trying %s: %s: %s", qPrintable(g_working), qPrintable(text), err.c_str());
  return j;
}

Json* memberOf(Json& obj, const std::string& key) {
  for (auto& m : obj.mutableMembers())
    if (m.first == key) return &m.second;
  return nullptr;
}

// "jobs[2].learn" -> steps; sets (or with "-" removes) the value there. "list[+]" appends, "list[-1]" is the last item.
bool applyOp(Json& root, const QString& path, const QString& jsonText) {
  const QStringList parts = path.split('.');
  Json* node = &root;
  for (int i = 0; i < parts.size(); ++i) {
    QString p = parts[i];
    const bool lastPart = i + 1 == parts.size();
    const int br = p.indexOf('[');
    if (br < 0) {  // a plain key
      if (lastPart) {
        if (jsonText == "-") return node->erase(p.toStdString());
        node->set(p.toStdString(), parse(jsonText));
        return true;
      }
      node = memberOf(*node, p.toStdString());
      if (!node) return false;
      continue;
    }
    const QString index = p.mid(br + 1, p.size() - br - 2);
    Json* list = memberOf(*node, p.left(br).toStdString());
    if (!list || !list->isArray()) return false;
    if (index == "+") {
      if (!lastPart) return false;
      list->push(parse(jsonText));
      return true;
    }
    int idx = index.toInt();
    if (idx < 0) idx += (int)list->items().size();
    if (idx < 0 || (size_t)idx >= list->items().size()) return false;
    node = &list->mutableItems()[(size_t)idx];
    if (lastPart) { *node = parse(jsonText); return true; }
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

// Other values worth trying for a scalar: every word of an enum, both ends of a range.
void extrasFor(const SchemaNode& child, const Json& first, bool overridden, std::vector<Json>* extras) {
  if (child.kind == SchemaNode::Kind::Enum)
    for (const std::string& w : child.values)
      if (Json::makeString(w).dump(0) != first.dump(0)) extras->push_back(Json::makeString(w));
  if ((child.kind == SchemaNode::Kind::Int || child.kind == SchemaNode::Kind::Number) && !overridden) {
    if (child.min && Json::makeNumber(*child.min).dump(0) != first.dump(0)) extras->push_back(Json::makeNumber(*child.min));
    if (child.max) extras->push_back(Json::makeNumber(*child.max));
  }
}

void adjustToKindFwd(const SchemaNode& parent, const std::string& key, Json& obj);

// A field with a `when` needs its sibling "kind" to have one of the words; sets it (the first word) when it has not.
void satisfyWhen(const SchemaNode& parent, const SchemaNode& field, Json& obj) {
  if (!field.when || !obj.isObject()) return;
  if (fieldApplies(parent, field, &obj)) return;
  obj.set(field.when->key, Json::makeString(field.when->values.front()));
  adjustToKindFwd(parent, field.when->key, obj);
}

// After the discriminator `key` of `parent` was set to a new word: keys that do not apply any more go, required keys that
// apply now are added (this is what a person switching the kind of an item would end up with).
void adjustToKind(const SchemaNode& parent, const std::string& key, Json& obj, const IdLookup& ids) {
  for (const auto& f : parent.fields) {
    if (!f.second->when || f.second->when->key != key) continue;
    if (fieldApplies(parent, *f.second, &obj)) {
      if (f.second->required && !memberOf(obj, f.first)) obj.set(f.first, sampleValue(*f.second, ids));
    } else {
      obj.erase(f.first);
    }
  }
}

void adjustToKindFwd(const SchemaNode& parent, const std::string& key, Json& obj) { adjustToKind(parent, key, obj, IdLookup()); }

}  // namespace

class ConformanceTest : public QObject {
  Q_OBJECT
 private slots:
  void initTestCase();
  void everyContextStartsValid();
  void everySchemaKeyIsAcceptedByTheLoader();
  void anInvalidValueIsRefusedNamingTheFile();
  void everyOptionalBlockEnabledAloneIsAccepted();
  void everyFixtureKeyOfADescribedSectionIsKnownToTheSchema();
  void embeddedSchemasEqualTheSourceFiles();

 private:
  struct Built {
    ote_test::Scratch tmp;
    QString dir;
  };
  std::unique_ptr<Built> build(const Context& c);
  // Sets `value` (or the sample when null) at the key's place and returns the check's problems.
  // `extras` (only with a null value) receives the other values worth trying for this key.
  std::vector<ote_editor::Problem> place(const Context& c, const KeyInfo& k, const Json* value, QString* file, bool* placed,
                                         std::vector<Json>* extras = nullptr);
  Json sampleFor(const Context& c, const QString& keyId, const SchemaNode& node, const IdLookup& ids) const;

  SchemaSet schemas_;
  std::vector<KeyInfo> keys_;
};

void ConformanceTest::initTestCase() {
  schemas_.loadFolder(QStringLiteral(OTE_SOURCE_DIR "/editor/schema"));
  QVERIFY2(schemas_.errors().isEmpty(), qPrintable(schemas_.errors().join("\n")));
  QVERIFY(schemas_.all().size() >= 8);
  for (const auto& fs : schemas_.all()) {
    const SchemaNode* top = fs->rows ? fs->rows.get() : fs->root.get();
    if (!top) continue;  // a file of shared definitions only
    collectObject(fs.get(), *top, QString(), {}, keys_);
  }
}

std::unique_ptr<ConformanceTest::Built> ConformanceTest::build(const Context& c) {
  auto b = std::make_unique<Built>();
  if (QString(c.fixture).startsWith("demo-")) {  // a demo game of games/ (for what no fixture has: fuel, fog bonuses)
    b->dir = b->tmp.file(c.fixture);
    ote_test::copyTree(QStringLiteral(OTE_SOURCE_DIR "/games/") + c.fixture, b->dir);
  } else {
    b->dir = ote_test::copyFixture(b->tmp, c.fixture);
  }
  for (const Op& op : c.mods) {
    Json root = readJson(b->dir + "/" + op.file);
    if (!applyOp(root, op.path, op.json)) qFatal("context %s: cannot apply %s at %s", c.name, qPrintable(op.file), qPrintable(op.path));
    ote_test::writeFile(b->dir + "/" + op.file, text(root));
  }
  return b;
}

Json ConformanceTest::sampleFor(const Context& c, const QString& keyId, const SchemaNode& node, const IdLookup& ids) const {
  auto it = c.overrides.find(keyId);
  if (it != c.overrides.end()) return parse(it->second);
  return sampleValue(node, ids);
}

std::vector<ote_editor::Problem> ConformanceTest::place(const Context& c, const KeyInfo& k, const Json* value, QString* fileOut, bool* placed,
                                                       std::vector<Json>* extras) {
  *placed = false;
  g_working = k.id + " [" + c.name + "]";
  auto b = build(c);
  const QString rel = fileOf(b->dir, k.fs);
  *fileOut = rel;
  // The ids a sample may point at come from the game as it stands (live project, like the editor's combo boxes).
  Project project(b->dir);
  IdSource ids(&project);
  Json root = readJson(b->dir + "/" + rel);
  // "@sides", "@storyUnits" ... read the file being built, so a sample names a side or unit the map really has.
  const IdLookup lookup = [&](const std::vector<std::string>& s) { return resolveSources(root, &ids, s); };

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
  const SchemaNode* parentObj = nullptr;  // the object node whose member the current step names
  for (int i = 0; i < k.steps.size(); ++i) {
    const QString step = k.steps[i];
    const bool last = i + 1 == k.steps.size();
    if (!sn) return {};
    if (step == "[]") {
      sn = sn->of.get();
      if (!node->isArray() || node->items().empty()) qFatal("no list item at %s (%s)", qPrintable(path), qPrintable(k.id));
      node = &node->mutableItems()[0];
      path += "[]";
      continue;
    }
    if (step == "*") {
      sn = sn->of.get();
      if (!node->isObject()) qFatal("no map at %s (%s)", qPrintable(path), qPrintable(k.id));
      if (node->members().empty()) node->set("a", sampleValue(*sn, lookup));
      node = &node->mutableMembers()[0].second;
      path += ".*";
      continue;
    }
    if (step.startsWith('{')) {  // one shape of a variant: the value there becomes that shape's sample
      const std::string name = step.mid(1, step.size() - 2).toStdString();
      const SchemaNode::Variant* v = nullptr;
      int idx = -1;
      for (size_t vi = 0; vi < sn->variants.size(); ++vi)
        if (sn->variants[vi].name == name) { v = &sn->variants[vi]; idx = (int)vi; }
      if (!v) qFatal("no variant %s in %s", name.c_str(), qPrintable(k.id));
      path += step;
      const QString keyId = schemaName + ":" + path;
      auto ov = c.overrides.find(keyId);
      if (ov != c.overrides.end()) *node = parse(ov->second);
      else if (variantIndex(*sn, *node) != idx || !last) {
        *node = sampleValueOfVariant(*v, lookup);
        // Overrides of keys below this shape ("...do[]{stats}.stats") apply to the fresh value too.
        for (const auto& o : c.overrides)
          if (o.first.startsWith(keyId + ".")) {
            const QString rest = o.first.mid(keyId.size() + 1);
            if (!rest.contains('[') && !rest.contains('*') && !rest.contains('{')) applyOp(*node, rest, o.second);
          }
      }
      if (last) {
        const SchemaNode& leaf = v->node ? *v->node : constantNode();
        if (extras && !value && v->node) extrasFor(leaf, *node, ov != c.overrides.end() && leaf.kind != SchemaNode::Kind::Enum, extras);
        if (value) *node = *value;
        *placed = true;
        break;
      }
      sn = v->node.get();
      continue;
    }
    // a member of an object
    path = path.isEmpty() ? step : (path.endsWith("[]") || path.endsWith("}") ? path + "." + step : path + "." + step);
    const SchemaNode* child = sn->field(step.toStdString());
    if (!child) qFatal("no field %s in %s", qPrintable(step), qPrintable(k.id));
    const QString keyId = schemaName + ":" + path;
    if (!node->isObject()) return {};  // cannot happen: ancestors are made objects below
    satisfyWhen(*sn, *child, *node);
    if (last) {
      const Json first = sampleFor(c, keyId, *child, lookup);
      if (extras && !value) {
        const bool overridden = c.overrides.count(keyId) > 0 && child->kind != SchemaNode::Kind::Enum;
        extrasFor(*child, first, overridden, extras);
      }
      if (k.id == "weapons:area.shape" && value) {
        // Each shape needs its own companion keys (radius / length): the whole area object is replaced.
        const std::string shape = value->asString();
        Json area = Json::makeObject();
        area.set("shape", *value);
        if (shape == "radius" || shape == "cross" || shape == "ring") area.set("radius", Json::makeNumber(2));
        if (shape == "line") area.set("length", Json::makeNumber(2));
        *node = area;
        *placed = true;
        break;
      }
      node->set(step.toStdString(), value ? *value : first);
      for (const std::string& ex : child->excludes) node->erase(ex);
      adjustToKind(*sn, step.toStdString(), *node, lookup);
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
      node->set(step.toStdString(), sampleFor(c, keyId, *child, lookup));
      for (const std::string& ex : child->excludes) node->erase(ex);  // (a join in place of a cell)
      existing = memberOf(*node, step.toStdString());
    }
    node = existing;
    parentObj = sn;
    sn = child;
  }
  (void)parentObj;
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

namespace {
// A plain nested object or a variant with several shapes is a container: its members / shapes are tested one by one.
bool isContainer(const SchemaNode& n) {
  return (n.kind == SchemaNode::Kind::Object && !n.block && n.alt.empty()) || n.kind == SchemaNode::Kind::Variant;
}
}  // namespace

void ConformanceTest::everySchemaKeyIsAcceptedByTheLoader() {
  QStringList failures, unclaimed, notes;
  QMap<QString, int> skippedByReason;  // thousands of nested keys share one reason: print it once with a count
  int tested = 0, skippedCount = 0, extraChecked = 0;
  for (const KeyInfo& k : keys_) {
    if (filteredOut(k.id)) continue;
    if (isIdentityKey(k.id)) {
      ++skippedCount;
      ++skippedByReason[QStringLiteral("identity keys: renaming one would break the references to the entry")];
      continue;
    }
    if (isContainer(*k.node)) {
      ++skippedCount;  // replacing a plain object would drop its siblings; its members are tested one by one
      ++skippedByReason[QStringLiteral("containers: their members are tested one by one, as are their blocks")];
      continue;
    }
    if (const QString* sk = skipReason(k.id)) {
      ++skippedCount;
      ++skippedByReason[*sk];
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
      if (const QString* why = extraSkipReason(note)) {
        notes << note + "  (" + *why + ")";
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
  for (auto it = skippedByReason.cbegin(); it != skippedByReason.cend(); ++it) qInfo("  skipped %d keys: %s", it.value(), qPrintable(it.key()));
  QVERIFY2(unclaimed.isEmpty(), qPrintable("keys no context tests:\n" + unclaimed.join("\n")));
  QVERIFY2(failures.isEmpty(), qPrintable("the loader refuses the sample of:\n" + failures.join("\n")));
}

void ConformanceTest::anInvalidValueIsRefusedNamingTheFile() {
  QStringList failures, notes;
  int checked = 0;
  for (const KeyInfo& k : keys_) {
    if (filteredOut(k.id) || isIdentityKey(k.id) || skipReason(k.id)) continue;
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
      } else if (n.kind == SchemaNode::Kind::Map && n.of && !n.free && which == 0) {  // a key that is no id of the game
        Json o = Json::makeObject();
        o.set("zz_no_such_id", Json::makeNumber(1));
        bad = o;
      }
      if (!bad) continue;
      const QString note = k.id + " = " + QString::fromStdString(bad->dump(0));
      if (const QString* why = extraSkipReason(note)) {
        notes << note + "  (" + *why + ")";
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

// What the form's Enable switch writes (the block's `enable` value, or required keys filled with defaults and the first valid
// reference) must be accepted by the engine as it stands. A block that needs something that does not exist yet says so in the
// form (`needs`), and one that cannot be valid alone is listed with its reason in blockExcuses().
void ConformanceTest::everyOptionalBlockEnabledAloneIsAccepted() {
  QStringList failures, notes;
  int checked = 0;
  for (const KeyInfo& k : keys_) {
    if (k.node->kind != SchemaNode::Kind::Object || !k.node->block) continue;
    if (filteredOut(k.id) || skipReason(k.id)) continue;  // a block inside a skipped section (a nested copy of a definition tested elsewhere)
    const Context* c = contextFor(k.id);
    if (!c) continue;  // already reported by the key test
    auto ex = blockExcuses().find(k.id);
    if (ex != blockExcuses().end()) {
      notes << k.id + "  (" + ex->second + ")";
      continue;
    }
    // The form offers Enable only when the ids it needs exist; compute that in the context's own game.
    auto b = build(*c);
    Project project(b->dir);
    IdSource ids(&project);
    const Json whole = readJson(b->dir + "/" + fileOf(b->dir, k.fs));
    const IdLookup lookup = [&](const std::vector<std::string>& s) { return resolveSources(whole, &ids, s); };
    if (k.node->needs && lookup(k.node->needs->refs).isEmpty()) {
      notes << k.id + "  (the form offers Enable only after you add what it needs: \"" + QString::fromStdString(k.node->needs->hint) + "\")";
      continue;
    }
    const Json value = defaultValue(*k.node, lookup);
    QString file;
    bool placed = false;
    const auto problems = place(*c, k, &value, &file, &placed);
    ++checked;
    if (!placed) failures << k.id + ": the block could not be placed";
    else if (!problems.empty()) failures << QString("%1 [%2] = %3: %4").arg(k.id, c->name, QString::fromStdString(value.dump(0)), problemsText(problems));
  }
  qInfo("conformance: %d optional blocks enabled alone and accepted", checked);
  for (const QString& n : notes) qInfo("  block not enabled alone: %s", qPrintable(n));
  QVERIFY2(failures.isEmpty(), qPrintable("Enable writes something the engine refuses for:\n" + failures.join("\n")));
}

namespace {

// Walks a value against its schema node; keys the schema does not know are reported unless excusedKey() allows them.
void reverseValue(const SchemaNode& node, const Json& value, const QString& schema, const QString& path, const QString& where,
                  QStringList& unknown, QStringList& excusedList);

void reverseObject(const SchemaNode& node, const Json& value, const QString& schema, const QString& path, const QString& where,
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
    reverseValue(*f, m.second, schema, p, where, unknown, excusedList);
  }
}

void reverseValue(const SchemaNode& node0, const Json& value, const QString& schema, const QString& path, const QString& where,
                  QStringList& unknown, QStringList& excusedList) {
  using K = SchemaNode::Kind;
  const SchemaNode& node = node0.resolved();  // commands nested deeper than the loaded tree expand here
  if (isAlt(node, value)) return;
  if (node.kind == K::Object) reverseObject(node, value, schema, path, where, unknown, excusedList);
  else if (node.kind == K::List && node.length < 0 && node.of && value.isArray())
    for (const Json& item : value.items()) reverseValue(*node.of, item, schema, path + "[]", where, unknown, excusedList);
  else if (node.kind == K::Map && node.of && value.isObject())
    for (const auto& m : value.members()) reverseValue(*node.of, m.second, schema, path + ".*", where, unknown, excusedList);
  else if (node.kind == K::Variant) {
    const int i = variantIndex(node, value);
    if (i < 0) {
      unknown << where + "  " + schema + ":" + path + "  (a shape no variant describes: " + QString::fromStdString(value.dump(0)).left(70) + ")";
      return;
    }
    const auto& v = node.variants[(size_t)i];
    if (v.node) reverseValue(*v.node, value, schema, path + "{" + QString::fromStdString(v.name) + "}", where, unknown, excusedList);
  }
}

// Fixture games and the demo games: every JSON file that has a schema.
QStringList gameFolders() {
  QStringList out;
  for (const char* root : {OTE_SOURCE_DIR "/tests/fixtures", OTE_SOURCE_DIR "/games"}) {
    const QDir d(QString::fromUtf8(root));
    for (const QString& name : d.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
      if (QString::fromUtf8(root).endsWith("/games") && !name.startsWith("demo-")) continue;  // practice-* and the owner's games are not ours to read
      if (name.startsWith("bad_") || name.startsWith("dup_") || name.startsWith("unknown_") || name == "no_roles") continue;  // broken on purpose
      out << d.filePath(name);
    }
  }
  return out;
}

}  // namespace

void ConformanceTest::everyFixtureKeyOfADescribedSectionIsKnownToTheSchema() {
  QStringList unknown, excused;
  int files = 0;
  for (const QString& dir : gameFolders()) {
    const QString name = QFileInfo(dir).fileName();
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
        for (const Json& row : resolvePath(j, *p)->items()) reverseObject(*fs->rows, row, sname, QString(), where, unknown, excused);
      } else {
        reverseObject(*fs->root, j, sname, QString(), where, unknown, excused);
      }
    }
  }
  unknown.removeDuplicates();
  excused.removeDuplicates();
  // Distinct keys (not per file) is the number that shrinks as schemas grow.
  QSet<QString> distinct;
  for (const QString& e : excused) distinct.insert(e.mid(e.indexOf("  ") + 2));
  qInfo("reverse check: %d fixture and demo files looked at; %d keys excused as not described yet (%d distinct)", files, (int)excused.size(),
        (int)distinct.size());
  QStringList sorted = distinct.values();
  sorted.sort();
  for (const QString& d : sorted) qInfo("  excused: %s", qPrintable(d));
  QVERIFY2(unknown.isEmpty(), qPrintable("keys or shapes in fixtures that the schema does not know (add them to editor/schema, or excuse them in conformance_contexts.cpp with a reason):\n" + unknown.join("\n")));
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
