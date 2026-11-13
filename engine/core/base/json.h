// Small JSON document model + parser/writer. No exceptions, no dependencies.
//
// The parser is deliberately lenient because game files are hand-edited:
// it accepts // and /* */ comments and trailing commas. Lookups never fail:
// a missing key/index returns a shared Null value, so loaders can chain
// j["a"]["b"].asInt(5) safely.
#pragma once
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ote {

class Json {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };
  using Member = std::pair<std::string, Json>;

  Json() = default;
  static Json makeBool(bool v);
  static Json makeNumber(double v);
  static Json makeString(const std::string& v);
  static Json makeArray();
  static Json makeObject();

  Type type() const { return type_; }
  bool isNull() const { return type_ == Type::Null; }
  bool isBool() const { return type_ == Type::Bool; }
  bool isNumber() const { return type_ == Type::Number; }
  bool isString() const { return type_ == Type::String; }
  bool isArray() const { return type_ == Type::Array; }
  bool isObject() const { return type_ == Type::Object; }

  // Typed reads; return `def` when the value has a different type.
  bool asBool(bool def = false) const;
  double asNumber(double def = 0) const;
  int asInt(int def = 0) const;  // truncates toward zero
  std::string asString(const std::string& def = std::string()) const;

  // Array length / object member count; 0 for scalars.
  size_t size() const;
  // Array element; shared Null if out of range or not an array.
  const Json& operator[](size_t index) const;
  const Json& operator[](int index) const { return (*this)[(size_t)(index < 0 ? ~(size_t)0 : (size_t)index)]; }
  // Object member; shared Null if missing or not an object.
  const Json& operator[](const char* key) const;
  const Json& operator[](const std::string& key) const;
  bool has(const char* key) const;

  // Raw access for iteration. Empty for other types. Object members keep
  // file order.
  const std::vector<Json>& items() const { return items_; }
  const std::vector<Member>& members() const { return members_; }

  // Builders (used for save files). set() replaces an existing key.
  void push(const Json& v);                        // turns Null into Array
  void set(const std::string& key, const Json& v); // turns Null into Object
  // Removes an object member; returns whether it existed.
  bool erase(const std::string& key);

  // Parse text. On failure returns false and sets err to "line N: message".
  static bool parse(const std::string& text, Json& out, std::string& err);
  // Serialize. indent <= 0 gives compact single-line output.
  std::string dump(int indent = 2) const;

 private:
  Type type_ = Type::Null;
  bool bool_ = false;
  double number_ = 0;
  std::string string_;
  std::vector<Json> items_;
  std::vector<Member> members_;
  friend class JsonParser;
};

// File helpers (stdio based; works on desktop, Emscripten's virtual FS and
// console SDKs). Paths use forward slashes.
bool readTextFile(const std::string& path, std::string& out);
bool writeTextFile(const std::string& path, const std::string& text);
// Reads + parses; err is prefixed with the path.
bool loadJsonFile(const std::string& path, Json& out, std::string& err);

}  // namespace ote
