#include "tool.h"

#include <cmath>

namespace ote_mcp {

const Tool* ToolRegistry::find(const std::string& name) const {
  for (const Tool& t : tools_)
    if (t.name == name) return &t;
  return nullptr;
}

namespace {
const char* typeNameOf(const Json& v) {
  if (v.isNull()) return "null";
  if (v.isBool()) return "boolean";
  if (v.isNumber()) return "number";
  if (v.isString()) return "string";
  if (v.isArray()) return "array";
  return "object";
}

bool typeMatches(const std::string& t, const Json& v) {
  if (t == "string") return v.isString();
  if (t == "boolean") return v.isBool();
  if (t == "number") return v.isNumber();
  if (t == "integer") return v.isNumber() && v.asNumber() == std::floor(v.asNumber());
  if (t == "array") return v.isArray();
  if (t == "object") return v.isObject();
  if (t == "null") return v.isNull();
  return true;
}
}  // namespace

std::string checkAgainstSchema(const Json& schema, const Json& value, const std::string& where) {
  if (!schema.isObject()) return "";
  if (schema["type"].isString() && !typeMatches(schema["type"].asString(), value))
    return where + " must be " + (schema["type"].asString() == "integer" ? "a whole number" : "of type " + schema["type"].asString()) +
           " (got " + typeNameOf(value) + ")";
  if (schema["enum"].isArray()) {
    bool found = false;
    for (const Json& e : schema["enum"].items()) found = found || e.dump(0) == value.dump(0);
    if (!found) {
      std::string all;
      for (const Json& e : schema["enum"].items()) all += (all.empty() ? "" : ", ") + e.asString();
      return where + " must be one of: " + all;
    }
  }
  if (value.isNumber()) {
    if (schema["minimum"].isNumber() && value.asNumber() < schema["minimum"].asNumber())
      return where + " is below the minimum " + schema["minimum"].dump(0);
    if (schema["maximum"].isNumber() && value.asNumber() > schema["maximum"].asNumber())
      return where + " is above the maximum " + schema["maximum"].dump(0);
  }
  if (value.isObject()) {
    for (const Json& r : schema["required"].items())
      if (r.isString() && !value.has(r.asString().c_str())) return where + " needs the key \"" + r.asString() + "\"";
    const Json& props = schema["properties"];
    for (const auto& m : value.members()) {
      if (props.has(m.first.c_str())) {
        const std::string e = checkAgainstSchema(props[m.first], m.second, where + "." + m.first);
        if (!e.empty()) return e;
      } else if (schema["additionalProperties"].isBool() && !schema["additionalProperties"].asBool()) {
        std::string all;
        for (const auto& p : props.members()) all += (all.empty() ? "" : ", ") + p.first;
        return where + " has an unknown key \"" + m.first + "\" (this tool takes: " + all + ")";
      }
    }
  }
  if (value.isArray() && schema["items"].isObject()) {
    size_t i = 0;
    for (const Json& item : value.items()) {
      const std::string e = checkAgainstSchema(schema["items"], item, where + "[" + std::to_string(i++) + "]");
      if (!e.empty()) return e;
    }
  }
  return "";
}

Json sType(const char* type, const std::string& description) {
  Json o = jobj();
  o.set("type", jstr(type));
  if (!description.empty()) o.set("description", jstr(description));
  return o;
}

Json sEnum(const std::vector<std::string>& words, const std::string& description) {
  Json o = sType("string", description);
  o.set("enum", jstrs(words));
  return o;
}

Json sInt(const std::string& description, double minimum, double maximum) {
  Json o = sType("integer", description);
  o.set("minimum", jnum(minimum));
  o.set("maximum", jnum(maximum));
  return o;
}

Json sObject(const std::vector<std::pair<std::string, Json>>& props, const std::vector<std::string>& required, bool additional) {
  Json o = jobj();
  o.set("type", jstr("object"));
  Json p = jobj();
  for (const auto& e : props) p.set(e.first, e.second);
  o.set("properties", p);
  if (!required.empty()) o.set("required", jstrs(required));
  o.set("additionalProperties", jbool(additional));
  return o;
}

Json sArray(const Json& items, const std::string& description) {
  Json o = sType("array", description);
  o.set("items", items);
  return o;
}

}  // namespace ote_mcp
