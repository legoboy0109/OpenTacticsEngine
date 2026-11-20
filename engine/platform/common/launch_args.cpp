#include "platform/common/launch_args.h"

namespace ote {

std::vector<std::string> parseLaunchArgs(const std::string& text, std::vector<std::pair<std::string, std::string>>& env) {
  std::vector<std::string> out;
  std::string word;
  bool inQuote = false, haveWord = false, comment = false, lineStart = true;
  auto flush = [&]() {
    if (!haveWord) return;
    size_t eq = word.find('=');
    bool isEnv = eq != std::string::npos && eq > 0 && word.compare(0, 2, "--") != 0;
    for (size_t k = 0; isEnv && k < eq; ++k) {
      char w = word[k];
      isEnv = (w >= 'A' && w <= 'Z') || (w >= 'a' && w <= 'z') || (w >= '0' && w <= '9') || w == '_';
    }
    if (isEnv) env.push_back({word.substr(0, eq), word.substr(eq + 1)});
    else out.push_back(word);
    word.clear();
    haveWord = false;
  };
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (comment) {
      if (c == '\n') {
        comment = false;
        lineStart = true;
      }
      continue;
    }
    if (inQuote) {
      if (c == '"') inQuote = false;
      else word.push_back(c);
      continue;
    }
    if (c == '\n' || c == '\r') {
      flush();
      lineStart = true;
    } else if (c == ' ' || c == '\t') {
      flush();
    } else if (c == '#' && lineStart && !haveWord) {
      comment = true;
    } else if (c == '"') {
      inQuote = true;
      haveWord = true;
      lineStart = false;
    } else {
      word.push_back(c);
      haveWord = true;
      lineStart = false;
    }
  }
  flush();
  return out;
}

}  // namespace ote
