#include "core/base/json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "core/base/fileio.h"

namespace ote {

static const Json kNull;

Json Json::makeBool(bool v) { Json j; j.type_ = Type::Bool; j.bool_ = v; return j; }
Json Json::makeNumber(double v) { Json j; j.type_ = Type::Number; j.number_ = v; return j; }
Json Json::makeString(const std::string& v) { Json j; j.type_ = Type::String; j.string_ = v; return j; }
Json Json::makeArray() { Json j; j.type_ = Type::Array; return j; }
Json Json::makeObject() { Json j; j.type_ = Type::Object; return j; }

bool Json::asBool(bool def) const { return type_ == Type::Bool ? bool_ : def; }
double Json::asNumber(double def) const { return type_ == Type::Number ? number_ : def; }
int Json::asInt(int def) const {
  if (type_ != Type::Number) return def;
  // Clamp: casting an out-of-range double to int is undefined behavior.
  if (number_ >= 2147483647.0) return 2147483647;
  if (number_ <= -2147483648.0) return -2147483647 - 1;
  return static_cast<int>(number_);
}
std::string Json::asString(const std::string& def) const {
  return type_ == Type::String ? string_ : def;
}

size_t Json::size() const {
  if (type_ == Type::Array) return items_.size();
  if (type_ == Type::Object) return members_.size();
  return 0;
}

const Json& Json::operator[](size_t index) const {
  if (type_ != Type::Array || index >= items_.size()) return kNull;
  return items_[index];
}

const Json& Json::operator[](const std::string& key) const {
  if (type_ == Type::Object)
    for (const Member& m : members_)
      if (m.first == key) return m.second;
  return kNull;
}
const Json& Json::operator[](const char* key) const { return (*this)[std::string(key)]; }

bool Json::has(const char* key) const {
  if (type_ != Type::Object) return false;
  for (const Member& m : members_)
    if (m.first == key) return true;
  return false;
}

void Json::push(const Json& v) {
  if (type_ == Type::Null) type_ = Type::Array;
  if (type_ != Type::Array) return;
  items_.push_back(v);
}

void Json::set(const std::string& key, const Json& v) {
  if (type_ == Type::Null) type_ = Type::Object;
  if (type_ != Type::Object) return;
  for (Member& m : members_)
    if (m.first == key) { m.second = v; return; }
  members_.push_back(Member(key, v));
}

bool Json::erase(const std::string& key) {
  for (size_t i = 0; i < members_.size(); ++i)
    if (members_[i].first == key) {
      members_.erase(members_.begin() + (std::ptrdiff_t)i);
      return true;
    }
  return false;
}

// ---------------------------------------------------------------- parser

class JsonParser {
 public:
  explicit JsonParser(const std::string& t) : s_(t) {}

  bool run(Json& out) {
    Json v;
    if (!skipWs() || !value(v, 0)) return false;
    if (!skipWs()) return false;
    if (p_ < s_.size()) return fail("unexpected text after the end of the document");
    out = v;
    return true;
  }
  const std::string& error() const { return err_; }

 private:
  static const int kMaxDepth = 200;
  const std::string& s_;
  size_t p_ = 0;
  int line_ = 1;
  std::string err_;

  bool fail(const std::string& msg) {
    if (err_.empty()) err_ = "line " + std::to_string(line_) + ": " + msg;
    return false;
  }
  bool eof() const { return p_ >= s_.size(); }

  // Skips whitespace and comments. False only on an unterminated comment.
  bool skipWs() {
    while (!eof()) {
      char c = s_[p_];
      if (c == '\n') { ++line_; ++p_; }
      else if (c == ' ' || c == '\t' || c == '\r') ++p_;
      else if (c == '/' && p_ + 1 < s_.size() && s_[p_ + 1] == '/') {
        while (!eof() && s_[p_] != '\n') ++p_;
      } else if (c == '/' && p_ + 1 < s_.size() && s_[p_ + 1] == '*') {
        int startLine = line_;
        p_ += 2;
        bool closed = false;
        while (!eof()) {
          if (s_[p_] == '*' && p_ + 1 < s_.size() && s_[p_ + 1] == '/') { p_ += 2; closed = true; break; }
          if (s_[p_] == '\n') ++line_;
          ++p_;
        }
        if (!closed) { line_ = startLine; return fail("unterminated /* comment"); }
      } else break;
    }
    return true;
  }

  bool literal(const char* word) {
    size_t n = std::char_traits<char>::length(word);
    if (s_.compare(p_, n, word) != 0) return false;
    p_ += n;
    return true;
  }

  bool value(Json& out, int depth) {
    if (eof()) return fail("unexpected end of file");
    if (depth > kMaxDepth) return fail("nesting is too deep");
    char c = s_[p_];
    if (c == '{') return object(out, depth);
    if (c == '[') return array(out, depth);
    if (c == '"') {
      std::string str;
      if (!string(str)) return false;
      out = Json::makeString(str);
      return true;
    }
    if (c == 't') { if (literal("true")) { out = Json::makeBool(true); return true; } }
    else if (c == 'f') { if (literal("false")) { out = Json::makeBool(false); return true; } }
    else if (c == 'n') { if (literal("null")) { out = Json(); return true; } }
    else if (c == '-' || (c >= '0' && c <= '9')) return number(out);
    return fail(std::string("unexpected character '") + c + "'");
  }

  bool number(Json& out) {
    const char* start = s_.c_str() + p_;
    char* end = nullptr;
    double v = std::strtod(start, &end);
    if (end == start) return fail("invalid number");
    // strtod also accepts hex/inf/nan; reject those spellings.
    for (const char* q = start; q < end; ++q) {
      char c = *q;
      if (!((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' || c == 'e' || c == 'E'))
        return fail("invalid number");
    }
    p_ += static_cast<size_t>(end - start);
    out = Json::makeNumber(v);
    return true;
  }

  static void appendUtf8(std::string& o, unsigned cp) {
    if (cp < 0x80) o += static_cast<char>(cp);
    else if (cp < 0x800) {
      o += static_cast<char>(0xC0 | (cp >> 6));
      o += static_cast<char>(0x80 | (cp & 0x3F));
    } else if (cp < 0x10000) {
      o += static_cast<char>(0xE0 | (cp >> 12));
      o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      o += static_cast<char>(0x80 | (cp & 0x3F));
    } else {
      o += static_cast<char>(0xF0 | (cp >> 18));
      o += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
      o += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
      o += static_cast<char>(0x80 | (cp & 0x3F));
    }
  }

  bool hex4(unsigned& out) {
    if (p_ + 4 > s_.size()) return fail("bad \\u escape");
    unsigned v = 0;
    for (int i = 0; i < 4; ++i) {
      char c = s_[p_ + i];
      v <<= 4;
      if (c >= '0' && c <= '9') v |= static_cast<unsigned>(c - '0');
      else if (c >= 'a' && c <= 'f') v |= static_cast<unsigned>(c - 'a' + 10);
      else if (c >= 'A' && c <= 'F') v |= static_cast<unsigned>(c - 'A' + 10);
      else return fail("bad \\u escape");
    }
    p_ += 4;
    out = v;
    return true;
  }

  bool string(std::string& out) {
    ++p_;  // opening quote
    while (true) {
      if (eof()) return fail("unterminated string");
      char c = s_[p_++];
      if (c == '"') return true;
      if (c == '\n') return fail("newline inside string");
      if (c != '\\') { out += c; continue; }
      if (eof()) return fail("unterminated string");
      char e = s_[p_++];
      switch (e) {
        case '"': out += '"'; break;
        case '\\': out += '\\'; break;
        case '/': out += '/'; break;
        case 'b': out += '\b'; break;
        case 'f': out += '\f'; break;
        case 'n': out += '\n'; break;
        case 'r': out += '\r'; break;
        case 't': out += '\t'; break;
        case 'u': {
          unsigned cp;
          if (!hex4(cp)) return false;
          if (cp >= 0xD800 && cp < 0xDC00) {
            unsigned lo;
            if (p_ + 1 < s_.size() && s_[p_] == '\\' && s_[p_ + 1] == 'u') {
              p_ += 2;
              if (!hex4(lo)) return false;
              if (lo < 0xDC00 || lo > 0xDFFF) return fail("invalid surrogate pair");
              cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
            } else return fail("lone surrogate in \\u escape");
          } else if (cp >= 0xDC00 && cp <= 0xDFFF) return fail("lone surrogate in \\u escape");
          appendUtf8(out, cp);
          break;
        }
        default: return fail(std::string("bad escape '\\") + e + "'");
      }
    }
  }

  bool array(Json& out, int depth) {
    out = Json::makeArray();
    ++p_;
    while (true) {
      if (!skipWs()) return false;
      if (eof()) return fail("unterminated array");
      if (s_[p_] == ']') { ++p_; return true; }
      Json v;
      if (!value(v, depth + 1)) return false;
      out.items_.push_back(v);
      if (!skipWs()) return false;
      if (eof()) return fail("unterminated array");
      if (s_[p_] == ',') { ++p_; continue; }
      if (s_[p_] == ']') { ++p_; return true; }
      return fail("expected ',' or ']' in array");
    }
  }

  bool object(Json& out, int depth) {
    out = Json::makeObject();
    ++p_;
    while (true) {
      if (!skipWs()) return false;
      if (eof()) return fail("unterminated object");
      if (s_[p_] == '}') { ++p_; return true; }
      if (s_[p_] != '"') return fail("expected a quoted key in object");
      std::string key;
      if (!string(key)) return false;
      if (!skipWs()) return false;
      if (eof() || s_[p_] != ':') return fail("expected ':' after key \"" + key + "\"");
      ++p_;
      if (!skipWs()) return false;
      Json v;
      if (!value(v, depth + 1)) return false;
      out.set(key, v);  // duplicate keys: last one wins
      if (!skipWs()) return false;
      if (eof()) return fail("unterminated object");
      if (s_[p_] == ',') { ++p_; continue; }
      if (s_[p_] == '}') { ++p_; return true; }
      return fail("expected ',' or '}' in object");
    }
  }
};

bool Json::parse(const std::string& text, Json& out, std::string& err) {
  JsonParser parser(text);
  Json result;
  if (!parser.run(result)) { err = parser.error(); return false; }
  out = result;
  err.clear();
  return true;
}

// ---------------------------------------------------------------- writer

static void escapeInto(std::string& o, const std::string& s) {
  o += '"';
  for (char ch : s) {
    unsigned char c = static_cast<unsigned char>(ch);
    switch (c) {
      case '"': o += "\\\""; break;
      case '\\': o += "\\\\"; break;
      case '\n': o += "\\n"; break;
      case '\r': o += "\\r"; break;
      case '\t': o += "\\t"; break;
      case '\b': o += "\\b"; break;
      case '\f': o += "\\f"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          std::snprintf(buf, sizeof buf, "\\u%04x", c);
          o += buf;
        } else o += ch;
    }
  }
  o += '"';
}

static void numberInto(std::string& o, double v) {
  char buf[40];
  if (!std::isfinite(v)) { o += "null"; return; }  // JSON has no NaN/inf
  if (v == std::floor(v) && std::fabs(v) < 1e15) std::snprintf(buf, sizeof buf, "%.0f", v);
  else {
    // The shortest text that reads back as the same number: 0.1 stays "0.1", not "0.10000000000000001".
    for (int digits = 15; digits <= 17; ++digits) {
      std::snprintf(buf, sizeof buf, "%.*g", digits, v);
      if (std::strtod(buf, nullptr) == v) break;
    }
  }
  if (buf[0] == '-' && buf[1] == '0' && buf[2] == '\0') { o += '0'; return; }
  o += buf;
}

static void dumpInto(const Json& j, std::string& o, int indent, int level) {
  auto newline = [&](int lv) {
    if (indent <= 0) return;
    o += '\n';
    o.append(static_cast<size_t>(lv * indent), ' ');
  };
  switch (j.type()) {
    case Json::Type::Null: o += "null"; break;
    case Json::Type::Bool: o += j.asBool() ? "true" : "false"; break;
    case Json::Type::Number: numberInto(o, j.asNumber()); break;
    case Json::Type::String: escapeInto(o, j.asString()); break;
    case Json::Type::Array: {
      if (j.items().empty()) { o += "[]"; break; }
      o += '[';
      for (size_t i = 0; i < j.items().size(); ++i) {
        if (i) o += ',';
        newline(level + 1);
        dumpInto(j.items()[i], o, indent, level + 1);
      }
      newline(level);
      o += ']';
      break;
    }
    case Json::Type::Object: {
      if (j.members().empty()) { o += "{}"; break; }
      o += '{';
      for (size_t i = 0; i < j.members().size(); ++i) {
        if (i) o += ',';
        newline(level + 1);
        escapeInto(o, j.members()[i].first);
        o += indent > 0 ? ": " : ":";
        dumpInto(j.members()[i].second, o, indent, level + 1);
      }
      newline(level);
      o += '}';
      break;
    }
  }
}

std::string Json::dump(int indent) const {
  std::string o;
  dumpInto(*this, o, indent, 0);
  return o;
}

// ----------------------------------------------------------------- files

bool readTextFile(const std::string& path, std::string& out) {
  std::FILE* f = openFile(path, "rb");
  if (!f) return false;
  std::string data;
  char buf[4096];
  size_t n;
  while ((n = readChunk(f, buf, sizeof buf)) > 0) data.append(buf, n);
  bool ok = !std::ferror(f);
  std::fclose(f);
  if (ok) out = data;
  return ok;
}

bool writeTextFile(const std::string& path, const std::string& text) {
  std::FILE* f = openFile(path, "wb");
  if (!f) return false;
  bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
  if (std::fclose(f) != 0) ok = false;
  return ok;
}

bool loadJsonFile(const std::string& path, Json& out, std::string& err) {
  std::string text;
  if (!readTextFile(path, text)) { err = path + ": cannot read file"; return false; }
  // Skip a UTF-8 byte order mark (Windows editors add one).
  if (text.compare(0, 3, "\xEF\xBB\xBF") == 0) text.erase(0, 3);
  std::string perr;
  if (!Json::parse(text, out, perr)) { err = path + ": " + perr; return false; }
  return true;
}

}  // namespace ote
