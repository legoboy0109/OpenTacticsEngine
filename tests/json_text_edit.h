// Text edits of a JSON file for tests that need "the same game without one feature". Renaming a key to something unused used
// to do that, but the loader refuses unknown keys now, so the key and its value have to go.
#pragma once
#include <string>

namespace ote_test {

// Removes the first `"key": <object or list>` from `text`, with the comma that follows it (or, for the last member, the one before
// it). Returns false when the key is not there. (Braces inside strings are skipped; the files of the tests are plain.)
inline bool dropJsonValue(std::string& text, const std::string& key) {
  size_t at = text.find("\"" + key + "\":");
  if (at == std::string::npos) return false;
  size_t i = text.find_first_of("{[", at);
  if (i == std::string::npos) return false;
  int depth = 0;
  bool inString = false;
  for (; i < text.size(); ++i) {
    const char ch = text[i];
    if (inString) {
      if (ch == '\\') ++i;
      else if (ch == '"') inString = false;
    } else if (ch == '"') {
      inString = true;
    } else if (ch == '{' || ch == '[') {
      ++depth;
    } else if (ch == '}' || ch == ']') {
      if (--depth == 0) break;
    }
  }
  size_t end = i + 1;
  const size_t after = text.find_first_not_of(" \t\r\n", end);
  if (after != std::string::npos && text[after] == ',') {
    end = after + 1;
  } else {
    const size_t before = at == 0 ? std::string::npos : text.find_last_not_of(" \t\r\n", at - 1);
    if (before != std::string::npos && text[before] == ',') {
      text.erase(before, 1);
      --at;
      --end;
    }
  }
  text.erase(at, end - at);
  return true;
}

}  // namespace ote_test
