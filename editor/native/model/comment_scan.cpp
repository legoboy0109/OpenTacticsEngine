#include "comment_scan.h"

namespace ote_native {

bool jsonTextHasComments(const std::string& text) {
  bool inString = false;
  for (size_t i = 0; i < text.size(); ++i) {
    char c = text[i];
    if (inString) {
      if (c == '\\') ++i;  // skip the escaped character, so \" does not end the string
      else if (c == '"') inString = false;
    } else if (c == '"') {
      inString = true;
    } else if (c == '/' && i + 1 < text.size() && (text[i + 1] == '/' || text[i + 1] == '*')) {
      return true;
    }
  }
  return false;
}

}  // namespace ote_native
