#include "core/data/name_tokens.h"

#include "core/data/events.h"  // isStoryId

namespace ote {

namespace {
const char kOpen[] = "{name:";
constexpr size_t kOpenLen = sizeof kOpen - 1;
}  // namespace

bool nameTokensOk(const std::string& text, std::string& why) {
  for (size_t at = text.find(kOpen); at != std::string::npos; at = text.find(kOpen, at + 1)) {
    const size_t end = text.find('}', at + kOpenLen);
    const std::string id = end == std::string::npos ? std::string() : text.substr(at + kOpenLen, end - at - kOpenLen);
    if (end == std::string::npos || !isStoryId(id)) {
      why = "\"{name:\" must be followed by a character id and \"}\", such as {name:hero}";
      return false;
    }
  }
  return true;
}

std::string expandNameTokens(const std::string& text, const std::function<std::string(const std::string&)>& nameOf) {
  if (text.find(kOpen) == std::string::npos) return text;
  std::string out;
  size_t from = 0;
  for (size_t at = text.find(kOpen); at != std::string::npos; at = text.find(kOpen, from)) {
    const size_t end = text.find('}', at + kOpenLen);
    if (end == std::string::npos) break;  // (the loader refuses this; the rest is shown as written)
    out += text.substr(from, at - from);
    out += nameOf(text.substr(at + kOpenLen, end - at - kOpenLen));
    from = end + 1;
  }
  return out + text.substr(from);
}

bool validEnteredName(const std::string& name) {
  int letters = 0;
  for (char ch : name) {
    const unsigned char u = (unsigned char)ch;
    if (u < 0x20 || u == 0x7F || ch == '{' || ch == '}') return false;
    if ((u & 0xC0) != 0x80) ++letters;  // a UTF-8 continuation byte is part of the letter before it
  }
  return letters >= 1 && letters <= kMaxNameLetters;
}

}  // namespace ote
