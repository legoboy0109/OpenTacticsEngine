// Finding every place that refers to an id, by walking each file together with its schema and looking at the reference
// fields ("ref") and the maps keyed by ids. Used by rename_id (rewrite them) and remove_entry (refuse while they exist).
#include "tools.h"

namespace ote_mcp {

using ote_editor::SchemaNode;
using K = SchemaNode::Kind;

namespace {

struct Domain {
  std::string file;      // the file the entry lives in
  std::string listKeys;  // key path of its list inside that file ("" for a root array), e.g. "units" or "rules.ranks"
  bool tableDomain = false;  // the list is the file's table array (so "data/units.json" names it)
};

std::string dottedKeys(const ConcretePath& p, bool& ok) {
  std::string out;
  ok = true;
  for (const Step& s : p) {
    if (s.isIndex) { ok = false; return ""; }
    out += (out.empty() ? "" : ".") + s.key;
  }
  return out;
}

bool sourceMatches(const std::string& src, const Domain& d, const std::string& currentFile) {
  if (!src.empty() && src[0] == '@') return currentFile == d.file && src.substr(1) == d.listKeys;
  if (startsWith(src, "tags:") || startsWith(src, "field:") || startsWith(src, "keys:")) return false;
  const size_t hash = src.find('#');
  if (hash == std::string::npos) return src == d.file && d.tableDomain;
  return src.substr(0, hash) == d.file && src.substr(hash + 1) == d.listKeys;
}

bool anyMatches(const std::vector<std::string>& refs, const Domain& d, const std::string& currentFile) {
  for (const std::string& s : refs)
    if (sourceMatches(s, d, currentFile)) return true;
  return false;
}

struct Walker {
  const Domain& dom;
  const std::string& id;
  const std::string& file;
  std::vector<RefHit>& hits;
  int budget = 200000;  // nodes visited: a hostile file cannot keep the walk busy for long

  void walk(const SchemaNode* nodeIn, const Json& v, ConcretePath& path) {
    if (!nodeIn || --budget < 0) return;
    const SchemaNode& n = nodeIn->resolved();
    switch (n.kind) {
      case K::Ref:
        if (v.isString() && v.asString() == id && anyMatches(n.refs, dom, file)) hits.push_back({file, path, false});
        return;
      case K::Variant: {
        const int idx = ote_editor::variantIndex(n, v);
        if (idx >= 0 && n.variants[(size_t)idx].node) walk(n.variants[(size_t)idx].node.get(), v, path);
        return;
      }
      case K::Object:
        if (!v.isObject()) return;
        for (const auto& m : v.members()) {
          const SchemaNode* f = n.field(m.first);
          if (!f) continue;
          Step s;
          s.key = m.first;
          path.push_back(s);
          walk(f, m.second, path);
          path.pop_back();
        }
        return;
      case K::List:
        if (!v.isArray()) return;
        for (size_t i = 0; i < v.items().size(); ++i) {
          Step s;
          s.isIndex = true;
          s.index = i;
          path.push_back(s);
          walk(n.of.get(), v.items()[i], path);
          path.pop_back();
        }
        return;
      case K::Map:
        if (!v.isObject()) return;
        for (const auto& m : v.members()) {
          Step s;
          s.key = m.first;
          path.push_back(s);
          if (!n.free && m.first == id && anyMatches(n.refs, dom, file)) hits.push_back({file, path, true});
          walk(n.of.get(), m.second, path);
          path.pop_back();
        }
        return;
      default: return;
    }
  }
};

}  // namespace

std::vector<RefHit> findReferences(Txn& txn, const std::string& listFile, const ConcretePath& listPath, const std::string& id) {
  std::vector<RefHit> hits;
  bool ok = true;
  Domain dom;
  dom.file = listFile;
  dom.listKeys = dottedKeys(listPath, ok);
  if (!ok) return hits;
  const Json* listRoot = txn.peek(listFile);
  if (!listRoot) return hits;
  const Json* table = ote_editor::tableArray(*listRoot);
  const Json* list = valueAt(*listRoot, listPath);
  dom.tableDomain = table && list == table;
  // An empty list is not "the table array" by shape; a root array or the only key of the file still is.
  if (!dom.tableDomain && list && list->isArray() && (listPath.empty() || (listRoot->isObject() && listRoot->members().size() == 1 && listPath.size() == 1)))
    dom.tableDomain = true;
  for (const std::string& rel : txn.knownFiles()) {
    const ote_editor::FileSchema* fs = txn.schemaOf(rel);
    const Json* doc = txn.peek(rel);
    if (!fs || !doc) continue;
    Walker w{dom, id, rel, hits};
    ConcretePath path;
    if (fs->rows) {
      const Json* rows = ote_editor::tableArray(*doc);
      if (!rows) continue;
      ConcretePath prefix;
      if (!doc->isArray() && doc->isObject() && doc->members().size() == 1) { Step k; k.key = doc->members()[0].first; prefix.push_back(k); }
      path = prefix;
      for (size_t i = 0; i < rows->items().size(); ++i) {
        Step s;
        s.isIndex = true;
        s.index = i;
        path.push_back(s);
        w.walk(fs->rows.get(), rows->items()[i], path);
        path.pop_back();
      }
    } else if (fs->root) {
      w.walk(fs->root.get(), *doc, path);
    }
  }
  return hits;
}

}  // namespace ote_mcp
