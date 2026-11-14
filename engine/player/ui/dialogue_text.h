// The text side of the dialogue box, kept free of drawing so it can be tested: who speaks, which
// portrait id a line uses, word-wrapping into pages and the letter-by-letter reveal.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "platform/platform.h"

namespace ote {

// How fast text appears (letters per second); one constant for the whole player.
constexpr float kDialogueLettersPerSecond = 60.0f;

struct DialogueLine {
  std::string name;      // the speaker: the unit's name or the free name; "" = no name row
  std::string text;
  std::string portrait;  // portrait id to look up in the graphics, "" = none
};

// Reads a `Dialogue` stream entry (a = speaker unit, b = text, c = free name, d = portrait id).
// The portrait is the line's own id if it has one, else the speaking unit's story id, else none.
DialogueLine dialogueLine(const Battle& b, const Event& e);

// Word-wraps `text` to `maxWidth` (the active font) and splits the lines into pages of at most
// `linesPerPage`. Always returns at least one page (possibly empty).
std::vector<std::vector<std::string>> paginateDialogue(const Renderer& r, const std::string& text, int maxWidth,
                                                       int linesPerPage);

// Number of characters (UTF-8 letters, not bytes) in the lines.
int countLetters(const std::vector<std::string>& lines);
// The lines cut after the first `letters` characters (never inside a multi-byte character).
std::vector<std::string> revealLetters(const std::vector<std::string>& lines, int letters);

}  // namespace ote
