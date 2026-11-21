#include "schema_engine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ote_editor {

const SchemaNode* SchemaNode::field(const std::string& key) const {
  for (const auto& f : fields)
    if (f.first == key) return f.second.get();
  return nullptr;
}

const SchemaNode* SchemaNode::itemNode(size_t index) const {
  if (!tuple.empty()) return index < tuple.size() ? tuple[index].get() : nullptr;
  return of.get();
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

const SchemaNode& SchemaNode::resolved() const {
  if (!lazy) return *this;
  if (!lazyCache) lazyCache = lazy();
  return *lazyCache;
}

namespace {

struct Parser {
  std::string where;
  std::vector<std::string>* errors;
  std::string base;                              // this file's name without ".json" (for "use": "name" in the same file)
  const std::map<std::string, ote::Json>* defs;  // every "defs" entry of every schema file, "file#name"
  std::vector<std::string> expanding;            // "use" chain being expanded (recursive definitions go lazy after two levels)
  std::shared_ptr<std::map<std::string, ote::Json>> ownedDefs;  // keeps the definitions alive for lazy placeholders

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
  NodePtr useNode(const ote::Json& j, const std::string& at) {
    std::string ref = j["use"].asString();
    if (ref.find('#') == std::string::npos) ref = base + "#" + ref;
    auto it = defs->find(ref);
    if (it == defs->end()) {
      errors->push_back(where + ": " + at + ": unknown def \"" + ref + "\"");
      return std::make_shared<SchemaNode>();
    }
    int depth = 0;
    for (const std::string& e : expanding) depth += e == ref;
    if (depth >= 2) {
      // Deeper than two levels: a placeholder that expands itself when a form first opens it (see SchemaNode::lazy).
      auto leaf = std::make_shared<SchemaNode>();
      leaf->help = j["help"].asString();
      const std::shared_ptr<std::map<std::string, ote::Json>> keep = ownedDefs;
      const std::string fileBase = base;
      leaf->lazy = [keep, fileBase, j, ref, at]() -> NodePtr {
        std::vector<std::string> sink;  // the schema was checked when it was loaded; a second set of messages would only repeat them
        Parser p{std::string("(lazy)"), &sink, fileBase, keep.get(), {ref}, keep};
        return p.useNode(j, at);
      };
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

  NodePtr node(const ote::Json& j, const std::string& at) {
    if (j.isObject() && j.has("use")) return useNode(j, at);
    auto n = std::make_shared<SchemaNode>();
    if (!j.isObject()) { errors->push_back(where + ": " + at + " must be an object"); return n; }
    std::string type = j["type"].asString();
    if (type.empty() && j.has("fields")) type = "object";
    if (type.empty()) type = "json";
    if (!kindFrom(type, n->kind)) errors->push_back(where + ": " + at + ": unknown type \"" + type + "\"");
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
    n->collapsed = j["collapsed"].asBool();
    n->summary = j["summary"].asString();
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
        const std::string vat = at + ".variants[" + std::to_string(i++) + "]";
        v.name = vj["name"].asString();
        v.help = vj["help"].asString();
        v.summary = vj["summary"].asString();
        v.matchWith = strings(vj["with"]);
        v.matchUnless = strings(vj["unless"]);
        if (v.name.empty()) errors->push_back(where + ": " + vat + ": a variant needs a \"name\"");
        v.matchKey = vj["key"].asString();
        v.matchKind = vj["kind"].asString();
        if (vj["length"].isNumber()) v.matchLength = vj["length"].asInt();
        v.matchFirst = vj["first"].asString();
        if ((v.matchLength >= 0 || !v.matchFirst.empty()) && v.matchKind != "array")
          errors->push_back(where + ": " + vat + ": \"length\" and \"first\" go with \"kind\": \"array\"");
        if (vj.has("value")) { v.matchValue = vj["value"]; v.matchIsValue = true; }
        if (int(!v.matchKey.empty()) + int(!v.matchKind.empty()) + int(v.matchIsValue) != 1)
          errors->push_back(where + ": " + vat + ": needs exactly one of \"key\", \"kind\", \"value\"");
        if (vj.has("node")) v.node = node(vj["node"], vat + ".node");
        else if (!v.matchIsValue) errors->push_back(where + ": " + vat + ": needs a \"node\"");
        if (vj.has("sample")) v.sample = vj["sample"];
        n->variants.push_back(std::move(v));
      }
    }
    if (n->kind == SchemaNode::Kind::Variant && n->variants.empty()) errors->push_back(where + ": " + at + ": variant without \"variants\"");
    if (j.has("of")) n->of = node(j["of"], at + ".of");
    if (j["tuple"].isArray()) {
      int i = 0;
      for (const ote::Json& tj : j["tuple"].items()) n->tuple.push_back(node(tj, at + ".tuple[" + std::to_string(i++) + "]"));
      n->length = (int)n->tuple.size();
      if (n->kind != SchemaNode::Kind::List) errors->push_back(where + ": " + at + ": \"tuple\" belongs to a list");
    }
    if (j["fields"].isObject())
      for (const auto& m : j["fields"].members()) n->fields.emplace_back(m.first, node(m.second, at + "." + m.first));
    if (n->kind == SchemaNode::Kind::Enum && n->values.empty()) errors->push_back(where + ": " + at + ": enum without \"values\"");
    if ((n->kind == SchemaNode::Kind::Ref || (n->kind == SchemaNode::Kind::Map && !n->free)) && n->refs.empty())
      errors->push_back(where + ": " + at + ": " + SchemaNode::kindName(n->kind) + " without \"ref\"");
    if ((n->kind == SchemaNode::Kind::List || n->kind == SchemaNode::Kind::Map) && !n->of && n->tuple.empty())
      errors->push_back(where + ": " + at + ": " + SchemaNode::kindName(n->kind) + " without \"of\"");
    return n;
  }
};

}  // namespace

namespace {
// "units" from "units.json" (what the schema files call themselves in "use": "file#name").
std::string baseName(const std::string& name) {
  const size_t slash = name.find_last_of("/\\");
  const std::string leaf = slash == std::string::npos ? name : name.substr(slash + 1);
  const size_t dot = leaf.find_last_of('.');
  return dot == std::string::npos || dot == 0 ? leaf : leaf.substr(0, dot);
}

// Shell-style match of a whole project-relative path: `*` = any run of characters except '/', `?` = one such character.
bool wildMatch(const char* pat, const char* s) {
  if (!*pat) return !*s;
  if (*pat == '*') {
    for (const char* t = s;; ++t) {
      if (wildMatch(pat + 1, t)) return true;
      if (!*t || *t == '/') return false;
    }
  }
  if (!*s) return false;
  if (*pat == '?') return *s != '/' && wildMatch(pat + 1, s + 1);
  return *pat == *s && wildMatch(pat + 1, s + 1);
}
}  // namespace

void SchemaSet::registerDefs(const std::string& name, const ote::Json& j) {
  const std::string base = baseName(name);
  if (j["defs"].isObject())
    for (const auto& m : j["defs"].members()) (*defs_)[base + "#" + m.first] = m.second;
}

void SchemaSet::build(const std::string& name, const ote::Json& j) {
  auto schema = std::make_shared<FileSchema>();
  schema->name = baseName(name);
  schema->source = name;
  schema->title = j["title"].asString();
  Parser p{name, &errors_, schema->name, defs_.get(), {}, defs_};
  for (const ote::Json& f : j["files"].items()) if (f.isString()) schema->files.push_back(f.asString());
  if (j["defs"].isObject()) {  // a file of shared definitions: check them as well, so a mistake is reported once, here
    for (const auto& m : j["defs"].members()) p.node(m.second, "defs." + m.first);
  }
  const bool onlyDefs = j["defs"].isObject() && !j.has("rows") && !j.has("root");
  if (schema->files.empty() && !onlyDefs) errors_.push_back(name + ": \"files\" is missing");
  if (j.has("rows")) schema->rows = p.node(j["rows"], "rows");
  if (j.has("root")) schema->root = p.node(j["root"], "root");
  if (!schema->rows && !schema->root && !onlyDefs) errors_.push_back(name + ": needs \"rows\" (a table file) or \"root\"");
  schemas_.push_back(schema);
}

void SchemaSet::addTexts(const std::vector<Text>& texts) {
  std::vector<std::pair<std::string, ote::Json>> docs;
  for (const Text& t : texts) {
    ote::Json j;
    std::string e;
    if (!ote::Json::parse(t.second, j, e)) {
      errors_.push_back(t.first + ": not valid JSON (" + e + ")");
      continue;
    }
    registerDefs(t.first, j);
    docs.emplace_back(t.first, std::move(j));
  }
  for (const auto& doc : docs) build(doc.first, doc.second);
}

void SchemaSet::loadFolder(const std::string& dir) {
  namespace sfs = std::filesystem;
  std::vector<std::string> names;
  std::error_code ec;
  for (sfs::directory_iterator it(sfs::u8path(dir), ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    if (it->is_regular_file(e2) && it->path().extension() == ".json") names.push_back(it->path().filename().string());
  }
  std::sort(names.begin(), names.end());
  std::vector<Text> texts;
  for (const std::string& n : names) {
    std::ifstream f(sfs::u8path(dir) / sfs::u8path(n), std::ios::binary);
    if (!f) {
      errors_.push_back(n + ": cannot be read");
      continue;
    }
    std::ostringstream ss;
    ss << f.rdbuf();
    texts.emplace_back(n, ss.str());
  }
  addTexts(texts);
}

const FileSchema* SchemaSet::forFile(const std::string& rel) const {
  for (const auto& s : schemas_)
    for (const std::string& f : s->files)
      if (f == rel) return s.get();
  // Patterns such as "maps/*.json" (every map of a game has the same shape).
  for (const auto& s : schemas_)
    for (const std::string& f : s->files)
      if (f.find('*') != std::string::npos && wildMatch(f.c_str(), rel.c_str())) return s.get();
  return nullptr;
}

}  // namespace ote_editor
