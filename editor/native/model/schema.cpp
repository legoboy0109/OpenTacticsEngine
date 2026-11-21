#include "schema.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cmath>
#include <map>

namespace ote_native {

const SchemaNode* SchemaNode::field(const std::string& key) const {
  for (const auto& f : fields)
    if (f.first == key) return f.second.get();
  return nullptr;
}

bool SchemaNode::isScalar() const {
  switch (kind) {
    case Kind::String: case Kind::Int: case Kind::Number: case Kind::Bool: case Kind::Enum: case Kind::Ref: return true;
    default: return false;
  }
}

const char* SchemaNode::kindName(Kind k) {
  switch (k) {
    case Kind::String: return "string";
    case Kind::Int: return "int";
    case Kind::Number: return "number";
    case Kind::Bool: return "bool";
    case Kind::Enum: return "enum";
    case Kind::Ref: return "ref";
    case Kind::List: return "list";
    case Kind::Object: return "object";
    case Kind::Map: return "map";
    case Kind::Json: return "json";
    case Kind::Variant: return "variant";
  }
  return "json";
}

namespace {

struct Parser {
  QString where;
  QStringList* errors;
  std::string base;                              // this file's name without ".json" (for "use": "name" in the same file)
  const std::map<std::string, ote::Json>* defs;  // every "defs" entry of every schema file, "file#name"
  std::vector<std::string> expanding;            // "use" chain being expanded (recursive definitions stop after two levels)

  static bool kindFrom(const std::string& t, SchemaNode::Kind& k) {
    static const std::pair<const char*, SchemaNode::Kind> table[] = {
        {"string", SchemaNode::Kind::String}, {"int", SchemaNode::Kind::Int},       {"number", SchemaNode::Kind::Number},
        {"bool", SchemaNode::Kind::Bool},     {"enum", SchemaNode::Kind::Enum},     {"ref", SchemaNode::Kind::Ref},
        {"list", SchemaNode::Kind::List},     {"object", SchemaNode::Kind::Object}, {"map", SchemaNode::Kind::Map},
        {"json", SchemaNode::Kind::Json},     {"variant", SchemaNode::Kind::Variant}};
    for (const auto& e : table)
      if (t == e.first) { k = e.second; return true; }
    return false;
  }

  static std::vector<std::string> strings(const ote::Json& j) {
    std::vector<std::string> out;
    if (j.isString()) out.push_back(j.asString());
    else for (const ote::Json& s : j.items()) if (s.isString()) out.push_back(s.asString());
    return out;
  }

  // "use": "name" (a def of this file) or "file#name" (a def of another schema file): the def's keys, then this node's own
  // keys on top (help, required ... differ per use). A def that contains itself (a condition made of conditions) is
  // expanded twice, then ends as raw JSON.
  NodePtr useNode(const ote::Json& j, const QString& at) {
    std::string ref = j["use"].asString();
    if (ref.find('#') == std::string::npos) ref = base + "#" + ref;
    auto it = defs->find(ref);
    if (it == defs->end()) {
      errors->append(where + ": " + at + ": unknown def \"" + QString::fromStdString(ref) + "\"");
      return std::make_shared<SchemaNode>();
    }
    int depth = 0;
    for (const std::string& e : expanding) depth += e == ref;
    if (depth >= 2) {
      auto leaf = std::make_shared<SchemaNode>();
      leaf->help = j["help"].asString().empty() ? "Nested deeper than the form goes: edit as JSON." : j["help"].asString();
      return leaf;
    }
    ote::Json merged = ote::Json::makeObject();
    for (const auto& m : it->second.members()) merged.set(m.first, m.second);
    for (const auto& m : j.members())
      if (m.first != "use" && m.first != "moreVariants" && m.first != "moreFields") merged.set(m.first, m.second);
    // "moreVariants" / "moreFields": entries added to the def's own list of variants / object of fields (skills and conditions
    // share most effects and each adds a few of its own).
    if (j["moreVariants"].isArray()) {
      ote::Json all = merged["variants"].isArray() ? merged["variants"] : ote::Json::makeArray();
      for (const ote::Json& v : j["moreVariants"].items()) all.push(v);
      merged.set("variants", all);
    }
    if (j["moreFields"].isObject()) {
      ote::Json all = merged["fields"].isObject() ? merged["fields"] : ote::Json::makeObject();
      for (const auto& m : j["moreFields"].members()) all.set(m.first, m.second);
      merged.set("fields", all);
    }
    expanding.push_back(ref);
    // The def's own `use` entries are relative to the file that holds the def, not to the file that pulled it in.
    const std::string savedBase = base;
    base = ref.substr(0, ref.find('#'));
    NodePtr n = node(merged, at);
    base = savedBase;
    expanding.pop_back();
    return n;
  }

  NodePtr node(const ote::Json& j, const QString& at) {
    if (j.isObject() && j.has("use")) return useNode(j, at);
    auto n = std::make_shared<SchemaNode>();
    if (!j.isObject()) { errors->append(where + ": " + at + " must be an object"); return n; }
    std::string type = j["type"].asString();
    if (type.empty() && j.has("fields")) type = "object";
    if (type.empty()) type = "json";
    if (!kindFrom(type, n->kind)) errors->append(where + ": " + at + ": unknown type \"" + QString::fromStdString(type) + "\"");
    n->help = j["help"].asString();
    n->alt = j["alt"].asString();
    n->required = j["required"].asBool();
    n->block = j["block"].asBool();
    n->expand = j["expand"].asBool();
    if (j["min"].isNumber()) n->min = j["min"].asNumber();
    if (j["max"].isNumber()) n->max = j["max"].asNumber();
    if (j.has("default")) n->deflt = j["default"];
    if (j.has("sample")) n->sample = j["sample"];
    n->values = strings(j["values"]);
    n->refs = strings(j["ref"]);
    if (j["length"].isNumber()) n->length = j["length"].asInt();
    n->free = j["free"].asBool();
    n->excludes = strings(j["excludes"]);
    if (j.has("enable")) n->enable = j["enable"];
    if (j["when"].isObject()) {
      SchemaNode::When w;
      for (const auto& m : j["when"].members()) { w.key = m.first; w.values = strings(m.second); }
      n->when = w;
    }
    if (j["needs"].isObject()) {
      SchemaNode::Needs nd;
      nd.refs = strings(j["needs"]["ref"]);
      nd.hint = j["needs"]["hint"].asString();
      n->needs = nd;
    }
    if (j["variants"].isArray()) {
      int i = 0;
      for (const ote::Json& vj : j["variants"].items()) {
        SchemaNode::Variant v;
        const QString vat = at + ".variants[" + QString::number(i++) + "]";
        v.name = vj["name"].asString();
        if (v.name.empty()) errors->append(where + ": " + vat + ": a variant needs a \"name\"");
        v.matchKey = vj["key"].asString();
        v.matchKind = vj["kind"].asString();
        if (vj.has("value")) { v.matchValue = vj["value"]; v.matchIsValue = true; }
        if (int(!v.matchKey.empty()) + int(!v.matchKind.empty()) + int(v.matchIsValue) != 1)
          errors->append(where + ": " + vat + ": needs exactly one of \"key\", \"kind\", \"value\"");
        if (vj.has("node")) v.node = node(vj["node"], vat + ".node");
        else if (!v.matchIsValue) errors->append(where + ": " + vat + ": needs a \"node\"");
        if (vj.has("sample")) v.sample = vj["sample"];
        n->variants.push_back(std::move(v));
      }
    }
    if (n->kind == SchemaNode::Kind::Variant && n->variants.empty()) errors->append(where + ": " + at + ": variant without \"variants\"");
    if (j.has("of")) n->of = node(j["of"], at + ".of");
    if (j["fields"].isObject())
      for (const auto& m : j["fields"].members()) n->fields.emplace_back(m.first, node(m.second, at + "." + QString::fromStdString(m.first)));
    if (n->kind == SchemaNode::Kind::Enum && n->values.empty()) errors->append(where + ": " + at + ": enum without \"values\"");
    if ((n->kind == SchemaNode::Kind::Ref || (n->kind == SchemaNode::Kind::Map && !n->free)) && n->refs.empty())
      errors->append(where + ": " + at + ": " + SchemaNode::kindName(n->kind) + " without \"ref\"");
    if ((n->kind == SchemaNode::Kind::List || n->kind == SchemaNode::Kind::Map) && !n->of)
      errors->append(where + ": " + at + ": " + SchemaNode::kindName(n->kind) + " without \"of\"");
    return n;
  }
};

}  // namespace

void SchemaSet::registerDefs(const QString& name, const ote::Json& j) {
  const std::string base = QFileInfo(name).completeBaseName().toStdString();
  if (j["defs"].isObject())
    for (const auto& m : j["defs"].members()) defs_[base + "#" + m.first] = m.second;
}

void SchemaSet::build(const QString& name, const ote::Json& j) {
  auto fs = std::make_shared<FileSchema>();
  fs->name = QFileInfo(name).completeBaseName().toStdString();
  fs->source = name;
  fs->title = j["title"].asString();
  Parser p{name, &errors_, fs->name, &defs_, {}};
  for (const ote::Json& f : j["files"].items()) if (f.isString()) fs->files.push_back(f.asString());
  if (j["defs"].isObject()) {  // a file of shared definitions: check them as well, so a mistake is reported once, here
    for (const auto& m : j["defs"].members()) p.node(m.second, "defs." + QString::fromStdString(m.first));
  }
  const bool onlyDefs = j["defs"].isObject() && !j.has("rows") && !j.has("root");
  if (fs->files.empty() && !onlyDefs) errors_.append(name + ": \"files\" is missing");
  if (j.has("rows")) fs->rows = p.node(j["rows"], "rows");
  if (j.has("root")) fs->root = p.node(j["root"], "root");
  if (!fs->rows && !fs->root && !onlyDefs) errors_.append(name + ": needs \"rows\" (a table file) or \"root\"");
  schemas_.push_back(fs);
}

void SchemaSet::addFromText(const QString& name, const QByteArray& text) {
  ote::Json j;
  std::string e;
  if (!ote::Json::parse(text.toStdString(), j, e)) {
    errors_.append(name + ": not valid JSON (" + QString::fromStdString(e) + ")");
    return;
  }
  registerDefs(name, j);
  build(name, j);
}

void SchemaSet::loadFolder(const QString& dir) {
  const QDir d(dir);
  const QStringList names = d.entryList({"*.json"}, QDir::Files, QDir::Name);
  // Two passes: every file's defs must be known before any file's nodes ask for them.
  std::vector<std::pair<QString, ote::Json>> docs;
  for (const QString& n : names) {
    QFile f(d.filePath(n));
    if (!f.open(QIODevice::ReadOnly)) {
      errors_.append(n + ": cannot be read");
      continue;
    }
    ote::Json j;
    std::string e;
    if (!ote::Json::parse(f.readAll().toStdString(), j, e)) {
      errors_.append(n + ": not valid JSON (" + QString::fromStdString(e) + ")");
      continue;
    }
    registerDefs(n, j);
    docs.emplace_back(n, std::move(j));
  }
  for (const auto& doc : docs) build(doc.first, doc.second);
}

const FileSchema* SchemaSet::forFile(const QString& rel) const {
  const std::string r = rel.toStdString();
  for (const auto& s : schemas_)
    for (const std::string& f : s->files)
      if (f == r) return s.get();
  // Patterns such as "maps/*.json" (every map of a game has the same shape).
  for (const auto& s : schemas_)
    for (const std::string& f : s->files)
      if (f.find('*') != std::string::npos &&
          QRegularExpression(QRegularExpression::wildcardToRegularExpression(QString::fromStdString(f))).match(rel).hasMatch())
        return s.get();
  return nullptr;
}

SchemaSet SchemaSet::loadDefault(const QString& overrideDir, const QString& appDir) {
  SchemaSet set;
  if (!overrideDir.isEmpty() && QDir(overrideDir).exists()) set.loadFolder(overrideDir);
  else if (QDir(appDir + "/schema").exists()) set.loadFolder(appDir + "/schema");
  else set.loadFolder(QStringLiteral(":/schema"));
  return set;
}

// --- values -----------------------------------------------------------------------------------------------------

bool nodeAccepts(const SchemaNode& n, const ote::Json& v, QString* why) {
  auto no = [&](const QString& m) { if (why) *why = m; return false; };
  using K = SchemaNode::Kind;
  switch (n.kind) {
    case K::Json: return true;
    case K::String: case K::Ref: return v.isString() ? true : no(QStringLiteral("expected text"));
    case K::Bool: return v.isBool() ? true : no(QStringLiteral("expected true or false"));
    case K::Enum: {
      if (!v.isString()) return no(QStringLiteral("expected one of the listed words"));
      for (const std::string& s : n.values) if (s == v.asString()) return true;
      return no(QStringLiteral("not one of the listed words"));
    }
    case K::Int: case K::Number: {
      if (!v.isNumber()) return no(n.kind == K::Int ? QStringLiteral("expected a whole number") : QStringLiteral("expected a number"));
      const double d = v.asNumber();
      if (n.kind == K::Int && d != std::floor(d)) return no(QStringLiteral("expected a whole number"));
      if (n.min && d < *n.min) return no(QStringLiteral("below the minimum %1").arg(*n.min));
      if (n.max && d > *n.max) return no(QStringLiteral("above the maximum %1").arg(*n.max));
      return true;
    }
    case K::List: return v.isArray() ? true : no(QStringLiteral("expected a list"));
    case K::Object: case K::Map: return v.isObject() ? true : no(QStringLiteral("expected an object { }"));
    case K::Variant: return variantIndex(n, v) >= 0 ? true : no(QStringLiteral("not one of the known shapes"));
  }
  return true;
}

namespace {
bool jsonKindIs(const std::string& kind, const ote::Json& v) {
  if (kind == "string") return v.isString();
  if (kind == "number") return v.isNumber();
  if (kind == "bool") return v.isBool();
  if (kind == "array") return v.isArray();
  if (kind == "object") return v.isObject();
  return false;
}
}  // namespace

int variantIndex(const SchemaNode& n, const ote::Json& v) {
  // A constant ("sides") beats a kind ("any other text"), whatever their order in the schema.
  for (size_t i = 0; i < n.variants.size(); ++i)
    if (n.variants[i].matchIsValue && n.variants[i].matchValue.dump(0) == v.dump(0)) return (int)i;
  for (size_t i = 0; i < n.variants.size(); ++i) {
    const SchemaNode::Variant& m = n.variants[i];
    if (m.matchIsValue) {
      continue;
    } else if (!m.matchKey.empty()) {
      if (v.isObject() && v.has(m.matchKey.c_str())) return (int)i;
    } else if (jsonKindIs(m.matchKind, v)) {
      return (int)i;
    }
  }
  return -1;
}

bool fieldApplies(const SchemaNode& parent, const SchemaNode& field, const ote::Json* obj) {
  if (!field.when) return true;
  const SchemaNode* sib = parent.field(field.when->key);
  std::string cur;
  if (obj && obj->isObject() && obj->has(field.when->key.c_str()) && (*obj)[field.when->key].isString()) cur = (*obj)[field.when->key].asString();
  else if (sib && sib->deflt.isString()) cur = sib->deflt.asString();
  for (const std::string& w : field.when->values)
    if (w == cur) return true;
  return false;
}

bool isAlt(const SchemaNode& n, const ote::Json& v) {
  if (n.alt.empty()) return false;
  return (n.alt == "string" && v.isString()) || (n.alt == "number" && v.isNumber()) || (n.alt == "bool" && v.isBool());
}

bool typeOk(const SchemaNode& n, const ote::Json& v) {
  using K = SchemaNode::Kind;
  if (isAlt(n, v)) return true;
  switch (n.kind) {
    case K::Json: return true;
    case K::String: case K::Ref: case K::Enum: return v.isString();
    case K::Bool: return v.isBool();
    case K::Int: return v.isNumber() && v.asNumber() == std::floor(v.asNumber());
    case K::Number: return v.isNumber();
    case K::List: return v.isArray() && (n.length < 0 || (int)v.size() == n.length);
    case K::Object: case K::Map: return v.isObject();
    case K::Variant: return variantIndex(n, v) >= 0;
  }
  return true;
}

namespace {
double clampTo(const SchemaNode& n, double d) {
  if (n.min && d < *n.min) d = *n.min;
  if (n.max && d > *n.max) d = *n.max;
  return d;
}

ote::Json build(const SchemaNode& n, const IdLookup& ids, bool sample);

ote::Json variantValue(const SchemaNode::Variant& v, const IdLookup& ids, bool sample) {
  if (v.matchIsValue) return v.matchValue;
  if (!v.sample.isNull()) return v.sample;
  return build(*v.node, ids, sample);
}

ote::Json build(const SchemaNode& n, const IdLookup& ids, bool sample) {
  using K = SchemaNode::Kind;
  if (sample && !n.sample.isNull()) return n.sample;
  if (!sample && !n.enable.isNull() && n.block) return n.enable;
  if (!sample && !n.deflt.isNull() && n.isScalar()) return n.deflt;
  switch (n.kind) {
    case K::String: return ote::Json::makeString(sample ? "sample" : "");
    case K::Bool: return ote::Json::makeBool(sample ? !n.deflt.asBool() : n.deflt.asBool());
    case K::Int: case K::Number: return ote::Json::makeNumber(clampTo(n, sample ? std::max(1.0, n.min.value_or(1.0)) : 0));
    case K::Enum: return ote::Json::makeString(n.values.empty() ? "" : n.values.front());
    case K::Ref: {
      const QStringList l = ids ? ids(n.refs) : QStringList();
      return ote::Json::makeString(l.isEmpty() ? "" : l.front().toStdString());
    }
    case K::List: {
      ote::Json a = ote::Json::makeArray();
      if (n.of && (n.length > 0 || sample))
        for (int i = 0; i < std::max(1, n.length); ++i) a.push(build(*n.of, ids, sample));
      return a;
    }
    case K::Object: {
      ote::Json o = ote::Json::makeObject();
      bool any = false;
      for (const auto& f : n.fields)
        if (f.second->required) { o.set(f.first, build(*f.second, ids, sample)); any = true; }
      // A required key that only applies for some kind (grid.layout for hex) is dropped while the kind says otherwise.
      for (const auto& f : n.fields)
        if (f.second->when && !fieldApplies(n, *f.second, &o)) o.erase(f.first);
      any = o.size() > 0;
      // A sample of a block without required keys states something (an empty block is often refused by the engine).
      if (sample && !any && n.block && !n.fields.empty()) o.set(n.fields.front().first, build(*n.fields.front().second, ids, true));
      return o;
    }
    case K::Map: {
      ote::Json o = ote::Json::makeObject();
      // `min` on a map = at least that many entries (a bonus needs one stat); the sample always has one.
      const int want = (int)n.min.value_or(0);
      if ((sample || want > 0) && n.of && n.free) {
        o.set("a", build(*n.of, ids, true));  // any key will do
      } else if ((sample || want > 0) && n.of) {
        const QStringList l = ids ? ids(n.refs) : QStringList();
        for (int i = 0; i < std::max(1, want) && i < l.size(); ++i) o.set(l[i].toStdString(), build(*n.of, ids, true));
      }
      return o;
    }
    case K::Json: return ote::Json();
    case K::Variant: {
      const SchemaNode::Variant& first = n.variants.front();
      return variantValue(first, ids, sample);
    }
  }
  return ote::Json();
}
}  // namespace

ote::Json defaultValue(const SchemaNode& n, const IdLookup& ids) { return build(n, ids, false); }
ote::Json defaultValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids) { return variantValue(v, ids, false); }
ote::Json sampleValueOfVariant(const SchemaNode::Variant& v, const IdLookup& ids) { return variantValue(v, ids, true); }
ote::Json sampleValue(const SchemaNode& n, const IdLookup& ids) { return build(n, ids, true); }

}  // namespace ote_native
