// Internal helpers shared by the game/map loaders. Every reader returns
// false after writing a creator-readable message to err:
//   "<file>: <where>: <what is wrong>"
#pragma once
#include <cmath>
#include <string>

#include "core/base/json.h"
#include "core/data/database.h"

namespace ote {

class LoadCtx {
 public:
  LoadCtx(std::string& err, const std::string& file) : err_(err), file_(file) {}
  const std::string& file() const { return file_; }

  bool fail(const std::string& where, const std::string& msg) const {
    err_ = file_ + ": " + (where.empty() ? std::string() : where + ": ") + msg;
    return false;
  }

  // Optional keys: a missing key leaves `out` untouched and succeeds; a key
  // of the wrong type is an error.
  bool str(const Json& o, const char* key, const std::string& where, std::string& out,
           bool required = false) const {
    if (!o.has(key)) return missing(key, where, required);
    if (!o[key].isString()) return wrongType(key, where, "a string");
    out = o[key].asString();
    return true;
  }
  bool num(const Json& o, const char* key, const std::string& where, double& out,
           bool required = false) const {
    if (!o.has(key)) return missing(key, where, required);
    if (!o[key].isNumber()) return wrongType(key, where, "a number");
    out = o[key].asNumber();
    return true;
  }
  bool integer(const Json& o, const char* key, const std::string& where, int& out,
               bool required = false) const {
    if (!o.has(key)) return missing(key, where, required);
    if (!isWhole(o[key])) return wrongType(key, where, "a whole number");
    out = o[key].asInt();
    return true;
  }
  bool boolean(const Json& o, const char* key, const std::string& where, bool& out,
               bool required = false) const {
    if (!o.has(key)) return missing(key, where, required);
    if (!o[key].isBool()) return wrongType(key, where, "true or false");
    out = o[key].asBool();
    return true;
  }
  // `out` points at the array (an empty Null when the key is absent).
  bool array(const Json& o, const char* key, const std::string& where, const Json*& out,
             bool required = false) const {
    out = &o[key];
    if (!o.has(key)) return missing(key, where, required);
    if (!o[key].isArray()) return wrongType(key, where, "a list");
    return true;
  }
  bool object(const Json& o, const char* key, const std::string& where, const Json*& out,
              bool required = false) const {
    out = &o[key];
    if (!o.has(key)) return missing(key, where, required);
    if (!o[key].isObject()) return wrongType(key, where, "an object");
    return true;
  }

  static bool isWhole(const Json& j) {
    return j.isNumber() && j.asNumber() == std::floor(j.asNumber()) &&
           std::fabs(j.asNumber()) < 2e9;
  }

  // {"statId": number} -> per-stat ints (out must already be sized to
  // db.stats.size()). Unknown stat ids are errors.
  bool statMap(const Database& db, const Json& o, const char* key, const std::string& where,
               std::vector<int>& out) const {
    const Json* m;
    if (!object(o, key, where, m)) return false;
    for (const Json::Member& e : m->members()) {
      int s = db.findStat(e.first);
      if (s < 0) return fail(where, std::string("\"") + key + "\" names unknown stat \"" + e.first + "\"");
      if (!isWhole(e.second))
        return fail(where, std::string("\"") + key + "\".\"" + e.first + "\" must be a whole number");
      out[static_cast<size_t>(s)] = e.second.asInt();
    }
    return true;
  }

  // {"resourceId": whole number} -> Amounts. Unknown resources are errors.
  bool amounts(const Database& db, const Json& o, const char* key, const std::string& where,
               std::vector<Amount>& out) const {
    const Json* m;
    if (!object(o, key, where, m)) return false;
    for (const Json::Member& e : m->members()) {
      int r = db.findResource(e.first);
      if (r < 0) return fail(where, std::string("\"") + key + "\" names unknown resource \"" + e.first + "\"");
      if (!isWhole(e.second))
        return fail(where, std::string("\"") + key + "\".\"" + e.first + "\" must be a whole number");
      Amount a;
      a.resource = r;
      a.amount = e.second.asInt();
      out.push_back(a);
    }
    return true;
  }

  // {"name": number} -> Props, order preserved.
  bool props(const Json& o, const char* key, const std::string& where, Props& out) const {
    const Json* m;
    if (!object(o, key, where, m)) return false;
    for (const Json::Member& e : m->members()) {
      if (!e.second.isNumber())
        return fail(where, std::string("\"") + key + "\".\"" + e.first + "\" must be a number");
      NamedNumber n;
      n.key = e.first;
      n.value = e.second.asNumber();
      out.push_back(n);
    }
    return true;
  }

  bool stringList(const Json& o, const char* key, const std::string& where,
                  std::vector<std::string>& out) const {
    const Json* a;
    if (!array(o, key, where, a)) return false;
    for (size_t i = 0; i < a->size(); ++i) {
      if (!(*a)[i].isString())
        return fail(where, std::string("\"") + key + "\" entry " + std::to_string(i + 1) + " must be a string");
      out.push_back((*a)[i].asString());
    }
    return true;
  }

 private:
  bool missing(const char* key, const std::string& where, bool required) const {
    if (!required) return true;
    return fail(where, std::string("missing required \"") + key + "\"");
  }
  bool wrongType(const char* key, const std::string& where, const char* expected) const {
    return fail(where, std::string("\"") + key + "\" must be " + expected);
  }

  std::string& err_;
  std::string file_;
};

// Reads an array of {id, name} entries into stats/resources/move types.
// Checks object shape, non-empty ids and duplicates.
template <class T>
bool readIdList(const LoadCtx& c, const Json& root, const char* key, bool required,
                std::vector<T>& out) {
  const Json* arr;
  if (!c.array(root, key, "", arr, required)) return false;
  if (required && arr->size() == 0) return c.fail("", std::string("\"") + key + "\" must not be empty");
  for (size_t i = 0; i < arr->size(); ++i) {
    const Json& e = (*arr)[i];
    std::string where = std::string(key) + "[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    T t;
    if (!c.str(e, "id", where, t.id, true)) return false;
    if (t.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = std::string(key) + " \"" + t.id + "\"";
    for (const T& o : out)
      if (o.id == t.id) return c.fail(where, "duplicate id");
    t.name = t.id;
    if (!c.str(e, "name", where, t.name)) return false;
    out.push_back(t);
  }
  return true;
}

// Loads a JSON file whose root must be an object; err is fully formed.
bool loadObjectFile(const std::string& path, Json& out, std::string& err);

}  // namespace ote
