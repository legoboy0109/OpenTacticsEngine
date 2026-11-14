// The dialogue box: a panel across the bottom of the screen with the speaker's name, the text
// appearing letter by letter (long lines continue on further pages) and, when the speaker has
// one, a portrait to the left of the panel. It is a plain 2D overlay, so it looks the same over
// every kind of view. It only talks to its owner through Result: the owner (the battle scene)
// decides what "line finished" and "skip everything" mean.
//
// Input: Confirm / tap = finish the page, then the next page, then the line. Holding Cancel or
// Menu for kHoldToSkip seconds, the Skip button, or the Skip script button = skip everything.
#pragma once
#include <string>
#include <vector>

#include "platform/platform.h"
#include "ui/dialogue_text.h"

namespace ote {

constexpr float kDialogueHoldToSkip = 0.5f;  // seconds Cancel / Menu must be held
// The hint line while a line is up (battle and campaign scenes).
constexpr const char* kDialogueHint = "Z / tap: next   hold X or Menu, or tap Skip: skip all";

// Where a portrait is in a texture (tex 0 = the speaker has none).
struct PortraitArt {
  TextureId tex = 0;
  int sx = 0, sy = 0, sw = 0, sh = 0;
  bool any() const { return tex != 0 && sw > 0 && sh > 0; }
};

class DialogueBox {
 public:
  enum Result { None, Finished, SkipAll };

  // Lays the line out for a screen `screenW` x `screenH` whose free area ends at `bottom` (the top
  // of the hint strip) and starts typing it.
  void open(const Renderer& r, int screenW, int screenH, int bottom, const DialogueLine& line, const PortraitArt& art);
  void close() { open_ = false; held_ = false; }
  bool isOpen() const { return open_; }

  Result update(double dt);
  Result onInput(const InputEvent& e);
  void draw(Renderer& r) const;

  // Layout, public for tests and screenshots.
  int pageCount() const { return (int)pages_.size(); }
  int page() const { return page_; }
  int linesPerPage() const { return linesPerPage_; }
  int top() const { return boxY_; }
  int portraitSize() const { return art_.any() ? ps_ : 0; }

 private:
  Result advance();
  bool pageDone() const { return letters_ >= (float)countLetters(pages_[(size_t)page_]); }

  bool open_ = false;
  std::string name_;
  std::vector<std::vector<std::string>> pages_;
  int page_ = 0;
  float letters_ = 0;  // how many letters of the page are showing
  float time_ = 0;     // for the blinking "more" mark
  bool held_ = false;  // Cancel / Menu is down
  float heldFor_ = 0;
  PortraitArt art_;
  int lh_ = 8, step_ = 10, pad_ = 4, linesPerPage_ = 3, headerH_ = 0;
  int boxX_ = 0, boxY_ = 0, boxW_ = 0, boxH_ = 0;
  int ps_ = 0, portraitX_ = 0, portraitY_ = 0;
  int skipX_ = 0, skipY_ = 0, skipW_ = 0, skipH_ = 0;
};

}  // namespace ote
