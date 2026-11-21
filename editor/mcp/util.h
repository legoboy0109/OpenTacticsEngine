// Small helpers shared by the whole of ote-mcp: building JSON, results of tools, text utilities and logging to stderr.
#pragma once
#include <cstdio>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

#include "core/base/json.h"

namespace ote_mcp {

namespace fs = std::filesystem;
using ote::Json;

inline Json jstr(const std::string& s) { return Json::makeString(s); }
inline Json jnum(double d) { return Json::makeNumber(d); }
inline Json jbool(bool b) { return Json::makeBool(b); }
inline Json jarr() { return Json::makeArray(); }
inline Json jobj() { return Json::makeObject(); }
inline Json jstrs(const std::vector<std::string>& l) {
  Json a = Json::makeArray();
  for (const std::string& s : l) a.push(jstr(s));
  return a;
}

// What a tool hands back. `data` is the structured result (also serialised into the text content); `isError` marks a tool
// execution error (the model is meant to read it and retry), as opposed to a protocol error.
struct ToolResult {
  Json data;
  bool isError = false;
  std::vector<Json> extraContent;  // image blocks and the like, after the text block
};

inline ToolResult toolOk(Json data) { ToolResult r; r.data = std::move(data); return r; }

// An error the model can act on: `message` says what is wrong, `next` (optional) what to call or change.
inline ToolResult toolFail(const std::string& message, const std::string& next = std::string(), Json extra = Json()) {
  ToolResult r;
  r.isError = true;
  r.data = extra.isObject() ? extra : jobj();
  r.data.set("error", jstr(message));
  if (!next.empty()) r.data.set("next", jstr(next));
  return r;
}

// stderr logging only: stdout carries protocol messages and nothing else.
void logLine(const char* level, const std::string& message);
inline void logInfo(const std::string& m) { logLine("info", m); }
inline void logWarn(const std::string& m) { logLine("warn", m); }
inline void logError(const std::string& m) { logLine("error", m); }

// Replaces bytes that are not valid UTF-8 with U+FFFD so every message on the wire is valid UTF-8.
std::string sanitizeUtf8(const std::string& s);

std::string baseNameOf(const std::string& rel);  // "data/units.json" -> "units.json"
bool endsWith(const std::string& s, const std::string& suffix);
bool startsWith(const std::string& s, const std::string& prefix);
std::vector<std::string> splitOn(const std::string& s, char sep);

// Hex digest (FNV-1a 64, 16 chars) of some bytes: change detection, not security.
std::string hashHex(const std::string& bytes);

// Lower-case a-z, 0-9 and '-', starting with a letter or digit, at most 40 characters (the editor's project id rule).
bool validName(const std::string& s);

}  // namespace ote_mcp
