#include "schema_ids.h"

#include <algorithm>

namespace ote_editor {

namespace {

using Ids = std::vector<std::string>;

void addId(Ids& out, const std::string& s) {
  if (!s.empty() && std::find(out.begin(), out.end(), s) == out.end()) out.push_back(s);
}

void addString(Ids& out, const ote::Json& v) {
  if (v.isString()) addId(out, v.asString());
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

bool startsWith(const std::string& s, const char* prefix) { return s.rfind(prefix, 0) == 0; }

// The value at a dotted key path (a missing step reads as null, like the Json lookups).
const ote::Json* atKeyPath(const ote::Json* v, const std::string& dotted) {
  for (const std::string& part : split(dotted, '.')) v = &(*v)[part];
  return v;
}

// Items of a list that are ids: plain strings, or objects with an "id".
void addListIds(Ids& out, const ote::Json& list) {
  for (const ote::Json& item : list.items()) addId(out, item.isString() ? item.asString() : item["id"].asString());
}

// addUnit commands anywhere in a command list: inside `if` branches (then / else) and a choice's answers.
void addedUnits(const ote::Json& commands, Ids& out, int depth) {
  if (depth > 12) return;  // a pathological file must not recurse forever
  for (const ote::Json& c : commands.items()) {
    if (!c.isObject()) continue;
    const ote::Json& u = c["addUnit"];
    if (u.isObject()) {
      addString(out, u["id"]);
      addString(out, u["character"]);
    }
    addedUnits(c["then"], out, depth + 1);
    addedUnits(c["else"], out, depth + 1);
    for (const ote::Json& a : c["answers"].items()) addedUnits(a["do"], out, depth + 1);
  }
}

bool isArrayOfObjects(const ote::Json& j) {
  if (!j.isArray() || j.size() == 0) return false;
  for (const ote::Json& item : j.items())
    if (!item.isObject()) return false;
  return true;
}

}  // namespace

std::string sourceFile(const std::string& source) {
  if (!source.empty() && source[0] == '@') return "";
  if (startsWith(source, "keys:@")) return "";
  std::string s = source;
  if (startsWith(s, "tags:")) s = s.substr(5);
  else if (startsWith(s, "keys:")) s = s.substr(5);
  else if (startsWith(s, "field:")) s = s.substr(s.find(':', 6) + 1);
  const size_t hash = s.find('#');
  return hash == std::string::npos ? s : s.substr(0, hash);
}

const ote::Json* tableArray(const ote::Json& root) {
  if (isArrayOfObjects(root)) return &root;
  if (root.isObject() && root.members().size() == 1 && isArrayOfObjects(root.members()[0].second)) return &root.members()[0].second;
  return nullptr;
}

std::vector<std::string> storyUnitIds(const ote::Json& map) {
  Ids out;
  for (const ote::Json& u : map["units"].items()) {
    addString(out, u["id"]);
    addString(out, u["character"]);
  }
  for (const ote::Json& e : map["events"].items()) addedUnits(e["do"], out, 0);
  return out;
}

std::vector<std::string> teamIds(const ote::Json& map) {
  Ids out, sides;
  for (const ote::Json& s : map["sides"].items()) {
    addString(out, s["team"].isString() && !s["team"].asString().empty() ? s["team"] : s["id"]);
    addString(sides, s["id"]);
  }
  for (const std::string& s : sides) addId(out, s);
  return out;
}

std::vector<std::string> resolveIdSources(const ote::Json& doc, const DocLookup& docs, const std::vector<std::string>& sources) {
  Ids out;
  // The sources of the file being edited come first, then those that name other files (the order forms have always shown).
  std::vector<std::string> ordered;
  for (const std::string& src : sources) if (!src.empty() && (src[0] == '@' || startsWith(src, "keys:@"))) ordered.push_back(src);
  for (const std::string& src : sources) if (src.empty() || !(src[0] == '@' || startsWith(src, "keys:@"))) ordered.push_back(src);
  for (const std::string& src : ordered) {
    // Sources of the file being edited.
    if (startsWith(src, "keys:@")) {
      const ote::Json* obj = atKeyPath(&doc, src.substr(6));
      if (obj->isObject())
        for (const auto& m : obj->members()) addId(out, m.first);
      continue;
    }
    if (src == "@storyUnits") { for (const std::string& i : storyUnitIds(doc)) addId(out, i); continue; }
    if (src == "@teams") { for (const std::string& i : teamIds(doc)) addId(out, i); continue; }
    if (!src.empty() && src[0] == '@') {
      // "@units": the list under that key path of the file being edited (a map's own units, sides, structures ...).
      addListIds(out, *atKeyPath(&doc, src.substr(1)));
      continue;
    }
    // Sources that name another file of the project.
    if (!docs) continue;
    const ote::Json* root = docs(sourceFile(src));
    if (!root) continue;
    const size_t hash = src.find('#');
    if (startsWith(src, "tags:") || startsWith(src, "field:")) {
      // "tags:data/units.json" / "field:group:data/weapons.json": the free words a key of the rows holds (text or list of
      // texts): what unit filters, conditions and weapon locks may name.
      const std::string key = startsWith(src, "tags:") ? "tags" : src.substr(6, src.find(':', 6) - 6);
      if (const ote::Json* rows = tableArray(*root))
        for (const ote::Json& row : rows->items()) {
          if (row[key].isString()) addId(out, row[key].asString());
          for (const ote::Json& t : row[key].items()) addString(out, t);
        }
      continue;
    }
    if (startsWith(src, "keys:")) {
      // "keys:game.json#rules.ai.weights": the names of the entries of an object (named sets, change ids).
      const ote::Json* obj = hash == std::string::npos ? root : atKeyPath(root, src.substr(hash + 1));
      if (obj->isObject())
        for (const auto& m : obj->members()) addId(out, m.first);
      continue;
    }
    const ote::Json* list = root;
    if (hash != std::string::npos) {
      list = atKeyPath(root, src.substr(hash + 1));
    } else if (const ote::Json* rows = tableArray(*root)) {
      list = rows;
    } else if (root->isObject() && root->members().size() == 1) {
      // An empty table array is not recognised by tableArray (it says nothing about columns); fall through to the key.
      list = &root->members()[0].second;
    }
    addListIds(out, *list);
  }
  return out;
}

}  // namespace ote_editor
