#include "ui/dialogue_text.h"

#include <algorithm>

#include "ui/text.h"

namespace ote {
namespace {
// UTF-8 continuation bytes are 10xxxxxx; every other byte starts a character.
bool startsChar(char c) { return ((unsigned char)c & 0xC0) != 0x80; }
}  // namespace

DialogueLine dialogueLine(const Battle& b, const Event& e) {
  DialogueLine line;
  const bool unit = e.a >= 0 && (size_t)e.a < b.units.size();
  line.text = e.b >= 0 ? b.storyText(e.b) : "";
  if (unit) line.name = b.units[(size_t)e.a].name;
  else if (e.c >= 0) line.name = b.storyText(e.c);
  if (e.d >= 0) line.portrait = b.storyText(e.d);
  else if (unit) line.portrait = b.units[(size_t)e.a].storyId;
  line.key = unit && !b.units[(size_t)e.a].storyId.empty() ? b.units[(size_t)e.a].storyId : e.c >= 0 ? b.storyText(e.c) : std::string();
  line.position = e.type == EventType::Dialogue ? e.flags & kDialoguePosMask : 0;
  line.narrate = e.type == EventType::Dialogue && (e.flags & kDialogueNarrate) != 0;
  if (e.type == EventType::Dialogue && !e.data.empty()) line.autoMs = e.data[0];  // (a Choice entry's data are its answers)
  if (line.narrate) line.name.clear();
  return line;
}

std::vector<std::vector<std::string>> paginateDialogue(const Renderer& r, const std::string& text, int maxWidth,
                                                       int linesPerPage) {
  std::vector<std::string> lines = wrapText(r, text, maxWidth);
  while (!lines.empty() && lines.back().empty()) lines.pop_back();
  if (linesPerPage < 1) linesPerPage = 1;
  std::vector<std::vector<std::string>> pages;
  for (size_t i = 0; i < lines.size(); i += (size_t)linesPerPage) {
    size_t end = std::min(lines.size(), i + (size_t)linesPerPage);
    pages.emplace_back(lines.begin() + (long)i, lines.begin() + (long)end);
  }
  if (pages.empty()) pages.emplace_back();
  return pages;
}

int countLetters(const std::vector<std::string>& lines) {
  int n = 0;
  for (const std::string& l : lines)
    for (char c : l) n += startsChar(c) ? 1 : 0;
  return n;
}

std::vector<std::string> revealLetters(const std::vector<std::string>& lines, int letters) {
  std::vector<std::string> out;
  for (const std::string& l : lines) {
    if (letters <= 0) break;
    int n = countLetters({l});
    if (letters >= n) {
      out.push_back(l);
      letters -= n;
      continue;
    }
    size_t i = 0;
    int seen = 0;
    for (; i < l.size(); ++i)
      if (startsChar(l[i]) && seen++ == letters) break;  // the (letters+1)th character starts here
    out.push_back(l.substr(0, i));
    break;
  }
  return out;
}

}  // namespace ote
