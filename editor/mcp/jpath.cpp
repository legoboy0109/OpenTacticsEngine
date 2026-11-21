#include "jpath.h"

#include <cstdlib>


namespace ote_mcp {

bool parsePath(const std::string& text, std::vector<Seg>& out, std::string& err) {
  out.clear();
  size_t i = 0;
  const size_t n = text.size();
  if (n > 2000) { err = "path is too long"; return false; }
  while (i < n) {
    const char c = text[i];
    if (c == '.') { ++i; continue; }
    Seg s;
    if (c == '[') {
      const size_t close = text.find(']', i);
      std::string inner;
      if (i + 1 < n && text[i + 1] == '"') {  // ["quoted key"]
        size_t q = i + 2;
        std::string key;
        while (q < n && text[q] != '"') { if (text[q] == '\\' && q + 1 < n) ++q; key += text[q++]; }
        if (q + 1 >= n || text[q + 1] != ']') { err = "unterminated [\"...\"] in path"; return false; }
        s.kind = Seg::Key;
        s.key = key;
        out.push_back(s);
        i = q + 2;
        continue;
      }
      if (close == std::string::npos) { err = "missing ']' in path"; return false; }
      inner = text.substr(i + 1, close - i - 1);
      i = close + 1;
      if (inner.empty()) { err = "empty [] in path"; return false; }
      char* end = nullptr;
      const long v = std::strtol(inner.c_str(), &end, 10);
      if (end && *end == 0) { s.kind = Seg::Index; s.index = v; }
      else {
        s.kind = Seg::Match;
        const size_t eq = inner.find('=');
        if (eq == std::string::npos) { s.key = "id"; s.matchVal = inner; }
        else { s.key = inner.substr(0, eq); s.matchVal = inner.substr(eq + 1); if (s.key.empty()) { err = "empty key before '=' in path"; return false; } }
      }
      out.push_back(s);
      continue;
    }
    size_t j = i;
    while (j < n && text[j] != '.' && text[j] != '[') ++j;
    s.kind = Seg::Key;
    s.key = text.substr(i, j - i);
    out.push_back(s);
    i = j;
  }
  return true;
}

namespace {

std::string keysOf(const Json& obj) {
  std::string all;
  size_t shown = 0;
  for (const auto& m : obj.members()) {
    if (shown++ >= 25) { all += ", ..."; break; }
    all += (all.empty() ? "" : ", ") + m.first;
  }
  return all.empty() ? "(none)" : all;
}

std::string idsOfList(const Json& list) {
  std::string all;
  size_t shown = 0;
  for (const Json& it : list.items()) {
    if (shown++ >= 25) { all += ", ..."; break; }
    const std::string id = it.isString() ? it.asString() : it["id"].isString() ? it["id"].asString() : std::string();
    if (!id.empty()) all += (all.empty() ? "" : ", ") + id;
  }
  return all.empty() ? "(no entries with an id)" : all;
}

}  // namespace

bool resolvePathIn(Json& root, const std::vector<Seg>& segsIn, ConcretePath& out, std::string& err, bool create) {
  out.clear();
  std::vector<Seg> segs = segsIn;
  // Table shorthand: "[archer].stats.hp" on a file that holds one table array under one key.
  if (!segs.empty() && segs[0].kind != Seg::Key && root.isObject() && root.members().size() == 1 && root.members()[0].second.isArray()) {
    Seg k;
    k.key = root.members()[0].first;
    segs.insert(segs.begin(), k);
  }
  Json* cur = &root;
  for (size_t si = 0; si < segs.size(); ++si) {
    const Seg& s = segs[si];
    Step st;
    if (s.kind == Seg::Key) {
      if (!cur->isObject()) { err = "cannot step into key \"" + s.key + "\": " + (out.empty() ? "the file" : pathLabel(root, out)) + " is not an object"; return false; }
      Json* next = nullptr;
      for (auto& m : cur->mutableMembers()) if (m.first == s.key) { next = &m.second; break; }
      if (!next) {
        if (create && si + 1 < segs.size() && segs[si + 1].kind == Seg::Key) {
          cur->set(s.key, Json::makeObject());
          for (auto& m : cur->mutableMembers()) if (m.first == s.key) { next = &m.second; break; }
        } else if (si + 1 == segs.size() && create) {
          st.key = s.key;
          out.push_back(st);  // the last key may be new: the caller inserts it
          return true;
        } else {
          err = "no key \"" + s.key + "\" at " + (out.empty() ? std::string("the top of the file") : pathLabel(root, out)) + " (keys there: " + keysOf(*cur) + ")";
          return false;
        }
      }
      st.key = s.key;
      cur = next;
    } else {
      if (!cur->isArray()) { err = "cannot index into " + (out.empty() ? std::string("the file") : pathLabel(root, out)) + ": it is not a list"; return false; }
      const size_t count = cur->items().size();
      size_t idx = 0;
      if (s.kind == Seg::Index) {
        const long v = s.index < 0 ? (long)count + s.index : s.index;
        if (v < 0 || (size_t)v >= count) {
          err = "index " + std::to_string(s.index) + " is out of range at " + (out.empty() ? std::string("the list") : pathLabel(root, out)) + " (it has " + std::to_string(count) + " entries)";
          return false;
        }
        idx = (size_t)v;
      } else {
        bool found = false;
        for (size_t i = 0; i < count && !found; ++i) {
          const Json& item = cur->items()[i];
          if (item.isObject() ? item[s.key].isString() && item[s.key].asString() == s.matchVal : (s.key == "id" && item.isString() && item.asString() == s.matchVal)) { idx = i; found = true; }
        }
        if (!found) {
          err = "no entry with " + s.key + " \"" + s.matchVal + "\" in " + (out.empty() ? std::string("the list") : pathLabel(root, out)) + " (ids there: " + idsOfList(*cur) + ")";
          return false;
        }
      }
      st.isIndex = true;
      st.index = idx;
      cur = &cur->mutableItems()[idx];
    }
    out.push_back(st);
  }
  return true;
}

Json* valueAt(Json& root, const ConcretePath& p) {
  Json* cur = &root;
  for (const Step& s : p) {
    if (s.isIndex) {
      if (!cur->isArray() || s.index >= cur->items().size()) return nullptr;
      cur = &cur->mutableItems()[s.index];
    } else {
      if (!cur->isObject()) return nullptr;
      Json* next = nullptr;
      for (auto& m : cur->mutableMembers()) if (m.first == s.key) { next = &m.second; break; }
      if (!next) return nullptr;
      cur = next;
    }
  }
  return cur;
}

const Json* valueAt(const Json& root, const ConcretePath& p) { return valueAt(const_cast<Json&>(root), p); }

std::string pathLabel(const Json& root, const ConcretePath& p) {
  std::string out;
  const Json* cur = &root;
  for (const Step& s : p) {
    if (s.isIndex) {
      std::string label = std::to_string(s.index);
      if (cur && cur->isArray() && s.index < cur->items().size()) {
        const Json& item = cur->items()[s.index];
        if (item.isObject() && item["id"].isString() && !item["id"].asString().empty()) {
          label = item["id"].asString();
          char* end = nullptr;
          std::strtol(label.c_str(), &end, 10);
          if (end && *end == 0) label = "id=" + label;  // an all-digit id would read as an index
        }
        cur = &item;
      } else cur = nullptr;
      out += "[" + label + "]";
    } else {
      const bool plain = s.key.find_first_of(".[]\"") == std::string::npos && !s.key.empty();
      out += plain ? (out.empty() ? "" : ".") + s.key : "[\"" + s.key + "\"]";
      cur = cur && cur->isObject() && cur->has(s.key.c_str()) ? &(*cur)[s.key] : nullptr;
    }
  }
  return out;
}

}  // namespace ote_mcp
