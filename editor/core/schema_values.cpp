// Checking a value against a schema node, and building default / sample values and one-line summaries from one.
// Moved from the native editor's model (it was QString-based) so ote-mcp shares it; behaviour is unchanged.
#include <algorithm>
#include <cmath>

#include "schema_engine.h"

namespace ote_editor {

namespace {
std::string numberText(double d) { return ote::Json::makeNumber(d).dump(0); }
}  // namespace

bool schemaAccepts(const SchemaNode& n, const ote::Json& v, std::string* why) {
  auto no = [&](const std::string& m) { if (why) *why = m; return false; };
  using K = SchemaNode::Kind;
  switch (n.kind) {
    case K::Json: return true;
    case K::String: case K::Ref: return v.isString() ? true : no("expected text");
    case K::Bool: return v.isBool() ? true : no("expected true or false");
    case K::Enum: {
      if (!v.isString()) return no("expected one of the listed words");
      for (const std::string& s : n.values) if (s == v.asString()) return true;
      return no("not one of the listed words");
    }
    case K::Int: case K::Number: {
      if (!v.isNumber()) return no(n.kind == K::Int ? "expected a whole number" : "expected a number");
      const double d = v.asNumber();
      if (n.kind == K::Int && d != std::floor(d)) return no("expected a whole number");
      if (n.min && d < *n.min) return no("below the minimum " + numberText(*n.min));
      if (n.max && d > *n.max) return no("above the maximum " + numberText(*n.max));
      return true;
    }
    case K::List: return v.isArray() ? true : no("expected a list");
    case K::Object: case K::Map: return v.isObject() ? true : no("expected an object { }");
    case K::Variant: return variantIndex(n, v) >= 0 ? true : no("not one of the known shapes");
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
      if (!v.isObject() || !v.has(m.matchKey.c_str())) continue;
      bool ok = true;
      for (const std::string& k : m.matchWith) ok = ok && v.has(k.c_str());
      for (const std::string& k : m.matchUnless) ok = ok && !v.has(k.c_str());
      if (ok) return (int)i;
    } else if (jsonKindIs(m.matchKind, v)) {
      if (m.matchLength >= 0 && (int)v.size() != m.matchLength) continue;
      if (!m.matchFirst.empty() && (v.size() == 0 || !jsonKindIs(m.matchFirst, v.items()[0]))) continue;
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
  return (n.alt == "string" && v.isString()) || (n.alt == "number" && v.isNumber()) || (n.alt == "bool" && v.isBool()) || (n.alt == "null" && v.isNull());
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

std::vector<std::string> idsOf(const IdLookup& ids, const SchemaNode& n) { return ids ? ids(n.refs) : std::vector<std::string>(); }

ote::Json variantValue(const SchemaNode::Variant& v, const IdLookup& ids, bool sample) {
  if (v.matchIsValue) return v.matchValue;
  if (!v.sample.isNull()) return v.sample;
  return build(*v.node, ids, sample);
}

ote::Json build(const SchemaNode& n, const IdLookup& ids, bool sample) {
  using K = SchemaNode::Kind;
  if (sample && !n.sample.isNull()) return n.sample;
  if (!sample && !n.enable.isNull()) return n.enable;  // a block's Enable, a new list item, a required list that must not start empty
  if (!sample && !n.deflt.isNull() && n.isScalar()) return n.deflt;
  switch (n.kind) {
    case K::String: return ote::Json::makeString(sample ? "sample" : "");
    case K::Bool: return ote::Json::makeBool(sample ? !n.deflt.asBool() : n.deflt.asBool());
    case K::Int: case K::Number: return ote::Json::makeNumber(clampTo(n, sample ? std::max(1.0, n.min.value_or(1.0)) : 0));
    case K::Enum: return ote::Json::makeString(n.values.empty() ? "" : n.values.front());
    case K::Ref: {
      const std::vector<std::string> l = idsOf(ids, n);
      return ote::Json::makeString(l.empty() ? "" : l.front());
    }
    case K::List: {
      ote::Json a = ote::Json::makeArray();
      if (!n.tuple.empty()) {
        for (const NodePtr& t : n.tuple) a.push(build(*t, ids, sample));
        return a;
      }
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
        const std::vector<std::string> l = idsOf(ids, n);
        for (int i = 0; i < std::max(1, want) && i < (int)l.size(); ++i) o.set(l[(size_t)i], build(*n.of, ids, true));
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

// --- summaries --------------------------------------------------------------------------------------------------------
namespace {
// Shortens to `max` characters (UTF-8 code points, not bytes) with an ellipsis; newlines read as spaces.
std::string shortText(std::string s, size_t max) {
  for (char& c : s) if (c == '\n') c = ' ';
  size_t count = 0, cut = std::string::npos;
  for (size_t i = 0; i < s.size(); ++i) {
    if ((static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) continue;  // continuation byte
    if (count == max - 1) cut = i;
    ++count;
  }
  if (count <= max) return s;
  return s.substr(0, cut) + "\xE2\x80\xA6";
}

std::vector<std::string> split(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  for (;;) {
    const size_t p = s.find(sep, start);
    if (p == std::string::npos) { out.push_back(s.substr(start)); return out; }
    out.push_back(s.substr(start, p - start));
    start = p + 1;
  }
}

std::string join(const std::vector<std::string>& l, const char* sep) {
  std::string out;
  for (size_t i = 0; i < l.size(); ++i) out += (i ? sep : "") + l[i];
  return out;
}

std::string compactValue(const ote::Json& v) {
  if (v.isString()) return v.asString();
  if (v.isBool()) return v.asBool() ? "yes" : "no";
  if (v.isNumber()) return v.dump(0);
  if (v.isNull()) return "none";
  if (v.isArray()) {
    std::vector<std::string> l;
    for (const ote::Json& i : v.items()) l.push_back(compactValue(i));
    return join(l, ", ");
  }
  if (v.isObject()) {
    if (v.size() == 0) return "any unit";
    const auto& first = v.members().front();
    if (v.size() == 1) return first.first == "unit" ? compactValue(first.second) : first.first + ' ' + compactValue(first.second);
    std::vector<std::string> l;
    for (const auto& m : v.members()) l.push_back(m.first + ' ' + compactValue(m.second));
    return join(l, ", ");
  }
  return {};
}

// One placeholder "a" or "a|b" (the first that has a text): the value at that key path of `value`.
std::string placeholder(const std::string& names, const ote::Json& value) {
  for (const std::string& alt : split(names, '|')) {
    const ote::Json* v = &value;
    for (const std::string& part : split(alt, '.')) {
      if (!v->isObject() || !v->has(part.c_str())) { v = nullptr; break; }  // an absent key has no text (a null does: "none")
      v = &(*v)[part];
    }
    if (!v) continue;
    const std::string t = shortText(compactValue(*v), 60);
    if (!t.empty()) return t;
  }
  return {};
}

// Literal text and {placeholders}. `missing` is set when a placeholder had no text (a [group] with one is left out).
std::string expandTemplate(const std::string& tmpl, const ote::Json& value, bool* missing) {
  std::string out;
  for (size_t i = 0; i < tmpl.size();) {
    const char ch = tmpl[i];
    if (ch == '{') {
      const size_t close = tmpl.find('}', i);
      if (close == std::string::npos) { out += tmpl.substr(i); break; }
      const std::string t = placeholder(tmpl.substr(i + 1, close - i - 1), value);
      if (t.empty() && missing) *missing = true;
      out += t;
      i = close + 1;
    } else if (ch == '[') {
      const size_t close = tmpl.find(']', i);
      if (close == std::string::npos) { out += tmpl.substr(i); break; }
      bool inner = false;
      const std::string t = expandTemplate(tmpl.substr(i + 1, close - i - 1), value, &inner);
      if (!inner) out += t;
      i = close + 1;
    } else {
      out += ch;
      ++i;
    }
  }
  return out;
}

// Trims and collapses every run of white space to one space (what QString::simplified does).
std::string simplified(const std::string& s) {
  std::string out;
  bool pendingSpace = false;
  for (char c : s) {
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') { pendingSpace = !out.empty(); continue; }
    if (pendingSpace) out += ' ';
    pendingSpace = false;
    out += c;
  }
  return out;
}
}  // namespace

std::string schemaSummary(const std::string& tmpl, const ote::Json& value) { return simplified(expandTemplate(tmpl, value, nullptr)); }

ote::Json schemaDefault(const SchemaNode& n, const IdLookup& ids) { return build(n, ids, false); }
ote::Json schemaVariantDefault(const SchemaNode::Variant& v, const IdLookup& ids) { return variantValue(v, ids, false); }
ote::Json schemaVariantSample(const SchemaNode::Variant& v, const IdLookup& ids) { return variantValue(v, ids, true); }
ote::Json schemaSample(const SchemaNode& n, const IdLookup& ids) { return build(n, ids, true); }

}  // namespace ote_editor
