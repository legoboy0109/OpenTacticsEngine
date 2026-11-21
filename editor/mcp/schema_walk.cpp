#include "schema_walk.h"

#include <algorithm>

namespace ote_mcp {

using ote_editor::SchemaNode;
using K = SchemaNode::Kind;

namespace {
const Json kNull;

// The variant node a variant value falls under, resolved; nullptr when none matches or the variant is a constant.
const SchemaNode* variantNodeFor(const SchemaNode& n, const Json& v) {
  const int idx = ote_editor::variantIndex(n, v);
  if (idx < 0) return nullptr;
  const SchemaNode::Variant& var = n.variants[(size_t)idx];
  return var.node ? &var.node->resolved() : nullptr;
}

std::string joinWords(const std::vector<std::string>& l, size_t max = 12) {
  std::string out;
  for (size_t i = 0; i < l.size() && i < max; ++i) out += (i ? ", " : "") + l[i];
  if (l.size() > max) out += ", ... (" + std::to_string(l.size()) + " in all)";
  return out;
}

void addIssue(std::vector<Issue>& issues, const std::string& path, const std::string& message, std::vector<std::string> allowed = {}) {
  if (issues.size() >= 8) return;
  issues.push_back({path, message, std::move(allowed)});
}

bool freeWordSource(const std::string& s) { return startsWith(s, "tags:") || startsWith(s, "field:"); }
}  // namespace

NodeAt schemaAt(const ote_editor::FileSchema* fs, const Json& root, const ConcretePath& path) {
  NodeAt res;
  if (!fs) return res;
  const SchemaNode* cur = nullptr;
  const Json* val = &root;
  size_t i = 0;
  if (fs->rows) {
    // A table file: [the table's key] then the row index, then the keys of the row.
    ConcretePath prefix;
    const Json* table = &root;
    if (!root.isArray() && root.isObject() && root.members().size() == 1) {
      Step k;
      k.key = root.members()[0].first;
      prefix.push_back(k);
      table = &root.members()[0].second;
    }
    for (size_t k = 0; k < prefix.size(); ++k) {
      if (k >= path.size()) { res.isTable = k == path.size() && path.size() == prefix.size(); return res; }
      if (path[k].isIndex || path[k].key != prefix[k].key) return res;
    }
    i = prefix.size();
    if (i == path.size()) { res.isTable = true; return res; }
    if (!path[i].isIndex) return res;
    if (path[i].index >= table->items().size()) return res;
    cur = fs->rows.get();
    val = &table->items()[path[i].index];
    ++i;
  } else {
    cur = fs->root.get();
  }
  if (!cur) return res;
  for (; i < path.size(); ++i) {
    cur = &cur->resolved();
    if (cur->kind == K::Variant) {
      cur = variantNodeFor(*cur, *val);
      if (!cur) return res;  // no shape known for this value: raw
    }
    res.parent = cur;
    res.parentValue = val;
    res.key = path[i].isIndex ? std::string() : path[i].key;
    const SchemaNode* next = nullptr;
    if (cur->kind == K::Object) {
      if (path[i].isIndex) return NodeAt{nullptr, cur, val, "", false, false};
      next = cur->field(path[i].key);
      if (!next) { res.unknownKey = true; return res; }
    } else if (cur->kind == K::Map || cur->kind == K::List) {
      next = cur->kind == K::List ? cur->itemNode(path[i].isIndex ? path[i].index : 0) : cur->of.get();
      if (!next) return res;
    } else {
      return NodeAt{nullptr, cur, val, res.key, false, false};  // below a scalar or raw json: the schema says nothing
    }
    const Json* nv = val->isObject() && !path[i].isIndex ? (val->has(path[i].key.c_str()) ? &(*val)[path[i].key] : &kNull)
                     : val->isArray() && path[i].isIndex && path[i].index < val->items().size() ? &val->items()[path[i].index] : &kNull;
    val = nv;
    cur = next;
  }
  if (cur) res.node = &cur->resolved();
  return res;
}

void checkValue(const SchemaNode* nodeIn, const Json& v, const CheckEnv& env, const std::string& path, bool allowUnknown,
                std::vector<Issue>& issues) {
  if (!nodeIn || issues.size() >= 8) return;
  const SchemaNode& n = nodeIn->resolved();
  if (ote_editor::isAlt(n, v)) return;
  if (n.kind == K::Json) return;
  if (n.kind == K::Variant) {
    const int idx = ote_editor::variantIndex(n, v);
    if (idx < 0) {
      std::vector<std::string> names;
      for (const auto& var : n.variants) names.push_back(var.name);
      addIssue(issues, path, "not one of the known shapes: this place takes " + joinWords(names), names);
      return;
    }
    const SchemaNode::Variant& var = n.variants[(size_t)idx];
    if (var.node) checkValue(var.node.get(), v, env, path, allowUnknown, issues);
    return;
  }
  std::string why;
  if (!ote_editor::schemaAccepts(n, v, &why)) {
    std::vector<std::string> allowed;
    if (n.kind == K::Enum) allowed = n.values;
    std::string msg = why;
    if (n.kind == K::Enum) msg += ": allowed words are " + joinWords(n.values, 40);
    if ((n.kind == K::Int || n.kind == K::Number) && (n.min || n.max))
      msg += " (allowed " + (n.min ? Json::makeNumber(*n.min).dump(0) : std::string("any")) + " to " + (n.max ? Json::makeNumber(*n.max).dump(0) : std::string("any")) + ")";
    addIssue(issues, path, msg, allowed);
    return;
  }
  switch (n.kind) {
    case K::Ref: {
      const std::string s = v.asString();
      if (std::find(n.values.begin(), n.values.end(), s) != n.values.end()) return;
      bool free = false;
      for (const std::string& src : n.refs) free = free || freeWordSource(src);
      if (free) return;  // the words of a tag list are the user's own
      const std::vector<std::string> ids = env.ids(n.refs);
      if (std::find(ids.begin(), ids.end(), s) != ids.end()) return;
      std::vector<std::string> allowed = ids;
      for (const std::string& w : n.values) allowed.push_back(w);
      addIssue(issues, path, "\"" + s + "\" does not exist" + (allowed.empty() ? " (nothing of that kind exists yet: add one first)" : "; valid values are " + joinWords(allowed, 25)), allowed);
      return;
    }
    case K::Object: {
      for (const auto& m : v.members()) {
        const SchemaNode* f = n.field(m.first);
        if (!f) {
          if (!allowUnknown) {
            std::vector<std::string> keys;
            for (const auto& fl : n.fields) keys.push_back(fl.first);
            addIssue(issues, path.empty() ? m.first : path + "." + m.first, "unknown key \"" + m.first + "\" (this section knows: " + joinWords(keys, 40) + "; pass allowUnknown to write it anyway)", keys);
          }
          continue;
        }
        checkValue(f, m.second, env, path.empty() ? m.first : path + "." + m.first, allowUnknown, issues);
      }
      for (const auto& fl : n.fields)
        if (fl.second->required && !v.has(fl.first.c_str()) && ote_editor::fieldApplies(n, *fl.second, &v))
          addIssue(issues, path, "missing required key \"" + fl.first + "\"" + (fl.second->help.empty() ? "" : " (" + fl.second->help + ")"));
      return;
    }
    case K::Map: {
      if (n.min && (double)v.size() < *n.min) addIssue(issues, path, "needs at least " + Json::makeNumber(*n.min).dump(0) + " entries");
      std::vector<std::string> ids;
      if (!n.free && !n.refs.empty()) ids = env.ids(n.refs);
      for (const auto& m : v.members()) {
        if (!n.free && !n.refs.empty() && std::find(ids.begin(), ids.end(), m.first) == ids.end())
          addIssue(issues, path.empty() ? m.first : path + "." + m.first, "key \"" + m.first + "\" is not a known id; valid keys are " + joinWords(ids, 25), ids);
        checkValue(n.of.get(), m.second, env, path.empty() ? m.first : path + "." + m.first, allowUnknown, issues);
      }
      return;
    }
    case K::List: {
      if (n.length >= 0 && (int)v.size() != n.length) addIssue(issues, path, "needs exactly " + std::to_string(n.length) + " items (has " + std::to_string(v.size()) + ")");
      if (n.min && (double)v.size() < *n.min) addIssue(issues, path, "needs at least " + Json::makeNumber(*n.min).dump(0) + " items");
      if (n.max && (double)v.size() > *n.max) addIssue(issues, path, "takes at most " + Json::makeNumber(*n.max).dump(0) + " items");
      size_t i = 0;
      for (const Json& item : v.items()) {
        if (issues.size() >= 8) return;
        checkValue(n.itemNode(i), item, env, path + "[" + std::to_string(i) + "]", allowUnknown, issues);
        ++i;
      }
      return;
    }
    default: return;
  }
}

Json variantList(const SchemaNode& node) {
  Json a = jarr();
  for (const auto& v : node.variants) {
    Json o = jobj();
    o.set("kind", jstr(v.name));
    if (!v.help.empty()) o.set("help", jstr(v.help));
    if (!v.matchKey.empty()) o.set("key", jstr(v.matchKey));
    else if (!v.matchKind.empty()) {
      o.set("jsonType", jstr(v.matchKind));
      if (v.matchLength >= 0) o.set("items", jnum(v.matchLength));
      if (!v.matchFirst.empty()) o.set("firstItem", jstr(v.matchFirst));
    }
    else if (v.matchIsValue) o.set("value", v.matchValue);
    a.push(o);
  }
  return a;
}

bool newValueFor(const SchemaNode& nodeIn, const std::string& kind, const CheckEnv& env, Json& out, std::string& err) {
  const SchemaNode& n = nodeIn.resolved();
  if (n.kind == K::Variant) {
    if (n.variants.empty()) { err = "this place has no known shapes"; return false; }
    const SchemaNode::Variant* pick = &n.variants.front();
    if (!kind.empty()) {
      pick = nullptr;
      for (const auto& v : n.variants) if (v.name == kind) pick = &v;
      if (!pick) {
        std::string names;
        for (const auto& v : n.variants) names += (names.empty() ? "" : ", ") + v.name;
        err = "no kind \"" + kind + "\" here; the kinds are: " + names + " (describe the list to see what each does)";
        return false;
      }
    }
    out = ote_editor::schemaVariantDefault(*pick, env.lookup());
    return true;
  }
  if (!kind.empty()) { err = "this list does not have kinds; leave \"kind\" out"; return false; }
  out = ote_editor::schemaDefault(n, env.lookup());
  return true;
}

std::string itemSummary(const SchemaNode* itemNode, const Json& item) {
  if (!itemNode) return "";
  const SchemaNode& n = itemNode->resolved();
  std::string tmpl;
  if (n.kind == K::Variant) {
    const int idx = ote_editor::variantIndex(n, item);
    if (idx < 0) return "";
    const auto& v = n.variants[(size_t)idx];
    tmpl = v.summary;
    if (tmpl.empty() && v.node) tmpl = v.node->resolved().summary;
    const std::string text = tmpl.empty() ? "" : ote_editor::schemaSummary(tmpl, item);
    return v.name + (text.empty() ? "" : ": " + text);
  }
  if (!n.summary.empty()) return ote_editor::schemaSummary(n.summary, item);
  return "";
}

namespace {
void describeInto(const SchemaNode& nodeIn, const CheckEnv& env, int depth, const Json* value, Json& o) {
  const SchemaNode& n = nodeIn.resolved();
  o.set("type", jstr(n.kind == K::Json ? "raw json" : SchemaNode::kindName(n.kind)));
  if (!n.help.empty()) o.set("help", jstr(n.help));
  if (n.required) o.set("required", jbool(true));
  if (!n.deflt.isNull()) o.set("default", n.deflt);
  if (n.min) o.set("min", jnum(*n.min));
  if (n.max) o.set("max", jnum(*n.max));
  if (n.length >= 0) o.set("length", jnum(n.length));
  if (!n.alt.empty()) o.set("alsoAccepts", jstr(n.alt));
  if (n.kind == K::Enum || (n.kind == K::Ref && !n.values.empty())) o.set(n.kind == K::Enum ? "values" : "extraValues", jstrs(n.values));
  if (n.kind == K::Ref || (n.kind == K::Map && !n.free)) {
    Json r = jobj();
    r.set("from", jstrs(n.refs));
    std::vector<std::string> ids = env.ids(n.refs);
    r.set("count", jnum((double)ids.size()));
    if (ids.size() > 60) ids.resize(60);
    r.set("validIds", jstrs(ids));
    o.set(n.kind == K::Ref ? "reference" : "keysAreIds", r);
  }
  if (n.kind == K::Map && n.free) o.set("keys", jstr("any name you choose"));
  if (n.when) {
    Json w = jobj();
    w.set(n.when->key, jstrs(n.when->values));
    o.set("onlyWhen", w);
  }
  if (n.block) {
    o.set("optionalBlock", jbool(true));
    if (value) o.set("enabled", jbool(!value->isNull()));
    if (n.needs) {
      Json nd = jobj();
      nd.set("needs", jstrs(n.needs->refs));
      nd.set("hint", jstr(n.needs->hint));
      o.set("enableNeeds", nd);
    }
  }
  if (!n.excludes.empty()) o.set("removesWhenSet", jstrs(n.excludes));
  if (n.kind == K::Variant) o.set("kinds", variantList(n));
  if (depth <= 0) return;
  if (n.kind == K::Object) {
    Json fields = jobj();
    for (const auto& f : n.fields) {
      Json fo = jobj();
      describeInto(*f.second, env, depth - 1, value && value->isObject() && value->has(f.first.c_str()) ? &(*value)[f.first] : nullptr, fo);
      if (value && value->isObject() && value->has(f.first.c_str()) && f.second->resolved().kind != K::Object) {
        const Json& cur = (*value)[f.first];  // containers as a count, so one describe never carries a whole event list
        fo.set("current", cur.isArray() ? jstr("[" + std::to_string(cur.size()) + " items]") : cur.isObject() ? jstr("{" + std::to_string(cur.size()) + " keys}") : cur);
      }
      fields.set(f.first, fo);
    }
    o.set("keys", fields);
  } else if (n.kind == K::List && !n.tuple.empty()) {
    Json items = jarr();  // a fixed list whose positions differ: one description per position
    for (const auto& t : n.tuple) {
      Json item = jobj();
      describeInto(*t, env, depth - 1, nullptr, item);
      items.push(item);
    }
    o.set("positions", items);
  } else if ((n.kind == K::List || n.kind == K::Map) && n.of) {
    Json item = jobj();
    describeInto(*n.of, env, depth - 1, nullptr, item);
    o.set(n.kind == K::List ? "items" : "values", item);
  }
}
}  // namespace

Json describeNode(const SchemaNode& node, const CheckEnv& env, int depth, const Json* value) {
  Json o = jobj();
  describeInto(node, env, depth, value, o);
  return o;
}

}  // namespace ote_mcp
