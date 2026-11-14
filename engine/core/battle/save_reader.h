// Shared by the battle save code (save.cpp, save_story.cpp): reading a damaged or hand-edited save
// field by field with a readable path in every message ("units[3].hp: ...").
#pragma once
#include <cmath>
#include <cstdio>
#include <string>

#include "core/base/json.h"

namespace ote {
namespace savejson {

// Reads fields with a context path for error messages ("units[3].hp").
class Reader {
 public:
  explicit Reader(std::string& err) : err_(err) {}

  bool fail(const std::string& where, const std::string& what) {
    err_ = where + ": " + what;
    return false;
  }
  bool integer(const Json& o, const char* key, const std::string& where, int lo, int hi, int& out) {
    const Json& v = o[key];
    if (!v.isNumber()) return fail(where + "." + key, "missing or not a number");
    double d = v.asNumber();
    if (!(d == std::floor(d)) || d < (double)lo || d > (double)hi)
      return fail(where + "." + key, "value " + trimmed(d) + " is outside " + std::to_string(lo) + ".." + std::to_string(hi));
    out = (int)d;
    return true;
  }
  bool boolean(const Json& o, const char* key, const std::string& where, bool& out) {
    if (!o[key].isBool()) return fail(where + "." + key, "missing or not true/false");
    out = o[key].asBool();
    return true;
  }
  bool object(const Json& o, const char* key, const std::string& where, const Json*& out) {
    out = &o[key];
    if (!out->isObject()) return fail(where + "." + key, "missing or not an object");
    return true;
  }
  // Array of exactly `count` entries (count < 0: any length up to `maxCount`).
  bool array(const Json& o, const char* key, const std::string& where, int count, int maxCount, const Json*& out) {
    out = &o[key];
    if (!out->isArray()) return fail(where + "." + key, "missing or not a list");
    if (count >= 0 && (int)out->size() != count)
      return fail(where + "." + key, "has " + std::to_string(out->size()) + " entries, expected " + std::to_string(count));
    if (count < 0 && (int)out->size() > maxCount) return fail(where + "." + key, "is unreasonably long");
    return true;
  }

 private:
  static std::string trimmed(double d) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.15g", d);
    return buf;
  }
  std::string& err_;
};

constexpr int kMaxUnits = 100000;
constexpr int kMaxLevel = 1000000;
constexpr int kMaxNumber = 1000000000;  // resources, stats, xp

}  // namespace savejson
}  // namespace ote
