// The dialogue box: a panel across the bottom of the screen with the speaker's name, the text
// appearing letter by letter (long lines continue on further pages) and, when the speaker has
// one, a portrait to the left of the panel. It is a plain 2D overlay, so it looks the same over
// every kind of view. It only talks to its owner through Result: the owner (the battle scene)
// decides what "line finished" and "skip everything" mean.
//
// A line can be a question (dialogue choices, docs/EVENTS.md "Dialogue choices"): setAnswers() adds two to four answers.
// The question is read like any line; when its last page is complete the answers appear in a panel just above the box.
// Up / Down (they wrap) move the highlight and Confirm picks it; a pointer over an answer highlights it, a tap on the
// highlighted answer picks it (so a first tap on a touch screen only highlights: a slip of the finger does not decide
// the story). There is no way out of a question: Cancel, Menu and the Skip tab and button only show the whole question.
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
// The hint line while a question's answers are up.
constexpr const char* kChoiceHint = "Up/Down + Z: answer   tap: pick, tap again: confirm";

// Where a portrait is in a texture (tex 0 = the speaker has none).
struct PortraitArt {
  TextureId tex = 0;
  int sx = 0, sy = 0, sw = 0, sh = 0;
  bool any() const { return tex != 0 && sw > 0 && sh > 0; }
};

class DialogueBox {
 public:
  enum Result { None, Finished, SkipAll, Chosen };

  // Lays the line out for a screen `screenW` x `screenH` whose free area ends at `bottom` (the top
  // of the hint strip) and starts typing it.
  void open(const Renderer& r, int screenW, int screenH, int bottom, const DialogueLine& line, const PortraitArt& art);
  // Makes the open line a question with these answers (two to four; call right after open()).
  void setAnswers(const Renderer& r, const std::vector<std::string>& answers);
  // True once the answers are on screen (the question has been read); chosen() is then valid after Result::Chosen.
  bool choosing() const { return choosing_; }
  int chosen() const { return chosen_; }
  bool isQuestion() const { return !answers_.empty(); }
  void close() { open_ = false; held_ = false; choosing_ = false; answers_.clear(); }
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
  // The answers' panel and rows (valid once setAnswers ran), for tests and screenshots.
  struct Rows { int x, y, w, h; };
  Rows panel() const { return {panelX_, panelY_, panelW_, panelH_}; }
  int answerCount() const { return (int)answers_.size(); }
  Rows answerRow(int i) const { return {panelX_, rowY_[(size_t)i], panelW_, rowH_[(size_t)i]}; }
  int answerLines(int i) const { return (int)answerLines_[(size_t)i].size(); }
  int highlighted() const { return sel_; }
  // One log line saying where the answers are (test scripts tap them): "choice: 3 answers: 0@x,y 1@x,y 2@x,y" (row centres).
  std::string describeAnswers() const;

 private:
  Result advance();
  void startChoosing();
  int rowAt(int x, int y) const;
  Result chooseInput(const InputEvent& e);
  void drawAnswers(Renderer& r) const;
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
  // Question: the answers, wrapped into rows, and where the panel stands (above the box).
  std::vector<std::string> answers_;
  std::vector<std::vector<std::string>> answerLines_;
  std::vector<int> rowY_, rowH_;
  int panelX_ = 0, panelY_ = 0, panelW_ = 0, panelH_ = 0, markW_ = 0;
  bool choosing_ = false;
  int sel_ = 0, chosen_ = -1;
};

}  // namespace ote
