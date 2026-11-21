// Internal helpers shared by the game/map loaders. Every reader returns
// false after writing a creator-readable message to err:
//   "<file>: <where>: <what is wrong>"
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

#include "core/base/json.h"
#include "core/data/database.h"
#include "core/data/events.h"

namespace ote {

// ---- unknown keys: a typo must not pass silently, and the message helps to find the key that was meant ----

// Edit distance between two keys: inserting, deleting or replacing a letter, or swapping two neighbours, each count 1 ("rnage" is one
// slip from "range").
inline int keyDistance(const std::string& a, const std::string& b) {
  std::vector<std::vector<int>> d(a.size() + 1, std::vector<int>(b.size() + 1, 0));
  for (size_t i = 0; i <= a.size(); ++i) d[i][0] = (int)i;
  for (size_t j = 0; j <= b.size(); ++j) d[0][j] = (int)j;
  for (size_t i = 1; i <= a.size(); ++i)
    for (size_t j = 1; j <= b.size(); ++j) {
      d[i][j] = std::min(d[i - 1][j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1), std::min(d[i - 1][j], d[i][j - 1]) + 1);
      if (i > 1 && j > 1 && a[i - 1] == b[j - 2] && a[i - 2] == b[j - 1]) d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
    }
  return d[a.size()][b.size()];
}

// The known key that `key` is most probably a slip of (at most 2 edits, and not more than half the letters of the longer
// key), or "".
inline std::string closestKey(const std::string& key, const std::vector<std::string>& known) {
  std::string best;
  int bestDistance = 3;
  for (const std::string& k : known) {
    const int d = keyDistance(key, k);
    const int longer = (int)std::max(key.size(), k.size());
    if (d < bestDistance && d * 2 <= longer) {
      bestDistance = d;
      best = k;
    }
  }
  return best;
}

// " - did you mean \"x\"?" when a known key is close to `key`, else "". For the messages that word their own refusal.
inline std::string didYouMean(const std::string& key, const std::vector<std::string>& known) {
  const std::string k = closestKey(key, known);
  return k.empty() ? std::string() : " - did you mean \"" + k + "\"?";
}
inline std::string didYouMean(const std::string& key, std::initializer_list<const char*> known) {
  return didYouMean(key, std::vector<std::string>(known.begin(), known.end()));
}

// The whole refusal text: unknown key "colour" (expected "id", "name", ...) - did you mean "color"?
// The list of expected keys is left out when it is long (more than 14): the hint is what helps then.
inline std::string unknownKeyText(const std::string& key, const std::vector<std::string>& known) {
  std::string text = "unknown key \"" + key + "\"";
  if (known.size() <= 14) {
    std::string list;
    for (const std::string& k : known) list += (list.empty() ? "\"" : ", \"") + k + "\"";
    text += " (expected " + list + ")";
  }
  return text + didYouMean(key, known);
}
inline std::string unknownKeyText(const std::string& key, std::initializer_list<const char*> known) {
  return unknownKeyText(key, std::vector<std::string>(known.begin(), known.end()));
}

// The most a stat number (a unit's stat or growth, a bonus) may be, plus or minus: far above any real game, far below an int overflow.
constexpr int kMaxStatNumber = 99999;

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
  // A whole number from `lo` to `hi`. Anything that is not a whole number, or is outside the range, gets the one
  // message "\"key\" must be a whole number from lo to hi". Key optional (`out` untouched when absent) unless `required`.
  bool integerIn(const Json& o, const char* key, const std::string& where, int lo, int hi, int& out,
                 bool required = false) const {
    if (!o.has(key)) return missing(key, where, required);
    if (!isWhole(o[key]) || o[key].asInt() < lo || o[key].asInt() > hi)
      return fail(where, std::string("\"") + key + "\" must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi));
    out = o[key].asInt();
    return true;
  }
  // The other wording in use: a non-whole value is `integer`'s "must be a whole number", and only a whole number
  // outside the range gets "\"key\" must be from lo to hi". Key optional.
  bool integerFrom(const Json& o, const char* key, const std::string& where, int lo, int hi, int& out) const {
    if (!integer(o, key, where, out)) return false;
    if (o.has(key) && (out < lo || out > hi))
      return fail(where, std::string("\"") + key + "\" must be from " + std::to_string(lo) + " to " + std::to_string(hi));
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

  // An object may only have the keys of `keys`. `quiet` keys are accepted too but never listed or suggested: bookkeeping that
  // something else reads and the loader ignores (a comment at the call says who reads each). `where` names the object.
  bool onlyKeys(const Json& o, const std::vector<std::string>& keys, const std::string& where,
                std::initializer_list<const char*> quiet = {}) const {
    for (const Json::Member& m : o.members()) {
      bool known = false;
      for (const std::string& k : keys) known = known || m.first == k;
      for (const char* k : quiet) known = known || m.first == k;
      if (!known) return fail(where, unknownKeyText(m.first, keys));
    }
    return true;
  }
  bool onlyKeys(const Json& o, std::initializer_list<const char*> keys, const std::string& where,
                std::initializer_list<const char*> quiet = {}) const {
    return onlyKeys(o, std::vector<std::string>(keys.begin(), keys.end()), where, quiet);
  }

  static bool isWhole(const Json& j) {
    return j.isNumber() && j.asNumber() == std::floor(j.asNumber()) &&
           std::fabs(j.asNumber()) < 2e9;
  }

  // {"statId": number} -> per-stat ints (out must already be sized to
  // db.stats.size()). Unknown stat ids are errors.
  // Every number is within +-`limit` (default kMaxStatNumber; a bonus that is smaller by nature passes a smaller one).
  bool statMap(const Database& db, const Json& o, const char* key, const std::string& where,
               std::vector<int>& out, int limit = kMaxStatNumber) const {
    const Json* m;
    if (!object(o, key, where, m)) return false;
    for (const Json::Member& e : m->members()) {
      int s = db.findStat(e.first);
      if (s < 0) return fail(where, std::string("\"") + key + "\" names unknown stat \"" + e.first + "\"");
      if (!isWhole(e.second))
        return fail(where, std::string("\"") + key + "\".\"" + e.first + "\" must be a whole number");
      if (std::abs(e.second.asInt()) > limit)
        return fail(where, std::string("\"") + key + "\".\"" + e.first + "\" must be a whole number from -" + std::to_string(limit) + " to " + std::to_string(limit));
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
// Checks object shape, non-empty ids, duplicates and unknown keys; `extra` are the keys read elsewhere (a resource's "refill",
// a move type's "sounds").
template <class T>
bool readIdList(const LoadCtx& c, const Json& root, const char* key, bool required,
                std::vector<T>& out, std::initializer_list<const char*> extra = {}) {
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
    std::vector<std::string> keys = {"id", "name"};
    keys.insert(keys.end(), extra.begin(), extra.end());
    if (!c.onlyKeys(e, keys, where)) return false;
    t.name = t.id;
    if (!c.str(e, "name", where, t.name)) return false;
    out.push_back(t);
  }
  return true;
}

// The reading keys of setVariable / addVariable ("level", "hp", "maxHp", "stat" + "of"), shared by map events and
// campaign scenes (docs/EVENTS.md "Reading a unit's numbers"). `number` is the written number's key ("to" / "by").
// Sets `given` when a reading is present (then `out.unit` is the id, shape-checked here as "not empty" only; the
// caller checks it against its own world); without one nothing changes. `allowHp` = false in scenes.
bool readUnitReading(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, const char* number,
                     bool allowHp, UnitReading& out, bool& given);

// True when the file can be opened for reading (optional data files).
bool fileExists(const std::string& path);

// Loads a JSON file whose root must be an object; err is fully formed.
bool loadObjectFile(const std::string& path, Json& out, std::string& err);

}  // namespace ote
