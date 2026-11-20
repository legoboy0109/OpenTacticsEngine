#include "schema.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <cmath>

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
  }
  return "json";
}

namespace {

struct Parser {
  QString where;
  QStringList* errors;

  static bool kindFrom(const std::string& t, SchemaNode::Kind& k) {
    static const std::pair<const char*, SchemaNode::Kind> table[] = {
        {"string", SchemaNode::Kind::String}, {"int", SchemaNode::Kind::Int},       {"number", SchemaNode::Kind::Number},
        {"bool", SchemaNode::Kind::Bool},     {"enum", SchemaNode::Kind::Enum},     {"ref", SchemaNode::Kind::Ref},
        {"list", SchemaNode::Kind::List},     {"object", SchemaNode::Kind::Object}, {"map", SchemaNode::Kind::Map},
        {"json", SchemaNode::Kind::Json}};
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

  NodePtr node(const ote::Json& j, const QString& at) {
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
    if (j.has("of")) n->of = node(j["of"], at + ".of");
    if (j["fields"].isObject())
      for (const auto& m : j["fields"].members()) n->fields.emplace_back(m.first, node(m.second, at + "." + QString::fromStdString(m.first)));
    if (n->kind == SchemaNode::Kind::Enum && n->values.empty()) errors->append(where + ": " + at + ": enum without \"values\"");
    if ((n->kind == SchemaNode::Kind::Ref || n->kind == SchemaNode::Kind::Map) && n->refs.empty())
      errors->append(where + ": " + at + ": " + SchemaNode::kindName(n->kind) + " without \"ref\"");
    if ((n->kind == SchemaNode::Kind::List || n->kind == SchemaNode::Kind::Map) && !n->of)
      errors->append(where + ": " + at + ": " + SchemaNode::kindName(n->kind) + " without \"of\"");
    return n;
  }
};

}  // namespace

void SchemaSet::addFromText(const QString& name, const QByteArray& text) {
  ote::Json j;
  std::string e;
  if (!ote::Json::parse(text.toStdString(), j, e)) {
    errors_.append(name + ": not valid JSON (" + QString::fromStdString(e) + ")");
    return;
  }
  auto fs = std::make_shared<FileSchema>();
  fs->name = QFileInfo(name).completeBaseName().toStdString();
  fs->source = name;
  fs->title = j["title"].asString();
  Parser p{name, &errors_};
  for (const ote::Json& f : j["files"].items()) if (f.isString()) fs->files.push_back(f.asString());
  if (fs->files.empty()) errors_.append(name + ": \"files\" is missing");
  if (j.has("rows")) fs->rows = p.node(j["rows"], "rows");
  if (j.has("root")) fs->root = p.node(j["root"], "root");
  if (!fs->rows && !fs->root) errors_.append(name + ": needs \"rows\" (a table file) or \"root\"");
  schemas_.push_back(fs);
}

void SchemaSet::loadFolder(const QString& dir) {
  const QDir d(dir);
  const QStringList names = d.entryList({"*.json"}, QDir::Files, QDir::Name);
  for (const QString& n : names) {
    QFile f(d.filePath(n));
    if (f.open(QIODevice::ReadOnly)) addFromText(n, f.readAll());
    else errors_.append(n + ": cannot be read");
  }
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
  }
  return true;
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
  }
  return true;
}

namespace {
double clampTo(const SchemaNode& n, double d) {
  if (n.min && d < *n.min) d = *n.min;
  if (n.max && d > *n.max) d = *n.max;
  return d;
}

ote::Json build(const SchemaNode& n, const IdLookup& ids, bool sample) {
  using K = SchemaNode::Kind;
  if (sample && !n.sample.isNull()) return n.sample;
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
      for (const auto& f : n.fields)
        if (f.second->required) o.set(f.first, build(*f.second, ids, sample));
      return o;
    }
    case K::Map: {
      ote::Json o = ote::Json::makeObject();
      if (sample && n.of) {
        const QStringList l = ids ? ids(n.refs) : QStringList();
        if (!l.isEmpty()) o.set(l.front().toStdString(), build(*n.of, ids, true));
      }
      return o;
    }
    case K::Json: return ote::Json();
  }
  return ote::Json();
}
}  // namespace

ote::Json defaultValue(const SchemaNode& n, const IdLookup& ids) { return build(n, ids, false); }
ote::Json sampleValue(const SchemaNode& n, const IdLookup& ids) { return build(n, ids, true); }

}  // namespace ote_native
