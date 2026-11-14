// The dialogue box's text logic (engine/player/ui/dialogue_text.*): speaker, portrait choice,
// word-wrap into pages and the letter reveal. No window: a stand-in renderer measures text.
#include "fake_renderer.h"
#include "story_fixture.h"
#include "ui/dialogue_text.h"

using namespace ote;
using namespace story;

namespace {

std::vector<std::string> field() { return {".........", ".........", "........."}; }

const Event* dialogue(const StoryWorld& w, int nth) {
  int seen = 0;
  for (const Event& e : w.events)
    if (e.type == EventType::Dialogue && seen++ == nth) return &e;
  return nullptr;
}

}  // namespace

TEST(dialogue_line_names_the_speaker_and_picks_the_portrait) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK(w.play(R"([{"id":"e","when":{"battle":"start"},"do":[
      {"say":"A","speaker":"eirik"},
      {"say":"B","speaker":"eirik","portrait":"eirik_calm"},
      {"say":"C","speakerName":"Villager"},
      {"say":"D","speakerName":"Villager","portrait":"crowd"},
      {"say":"E"}]}])"));
  const Event* a = dialogue(w, 0);
  const Event* b = dialogue(w, 1);
  const Event* c = dialogue(w, 2);
  const Event* d = dialogue(w, 3);
  const Event* e = dialogue(w, 4);
  CHECK(a && b && c && d && e);
  if (!(a && b && c && d && e)) return;
  // The unit's name; with no portrait of its own the line uses the unit's story id.
  DialogueLine l = dialogueLine(w.battle, *a);
  CHECK_STR(l.name, w.battle.units[0].name);
  CHECK_STR(l.text, "A");
  CHECK_STR(l.portrait, "eirik");
  // The line's own portrait wins.
  CHECK_STR(dialogueLine(w.battle, *b).portrait, "eirik_calm");
  // A free name: no unit, so no portrait unless the line gives one.
  l = dialogueLine(w.battle, *c);
  CHECK_STR(l.name, "Villager");
  CHECK_STR(l.portrait, "");
  CHECK_STR(dialogueLine(w.battle, *d).portrait, "crowd");
  // Neither: no name row, no portrait.
  l = dialogueLine(w.battle, *e);
  CHECK_STR(l.name, "");
  CHECK_STR(l.portrait, "");
}

TEST(dialogue_unit_without_story_id_has_no_portrait) {
  StoryWorld w(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik"});  // the second has no id
  // Said by a unit through a free name on another line; a unit speaks only through its id, so check the
  // unnamed unit through a hand-made entry.
  CHECK(w.play(R"([{"id":"e","when":{"battle":"start"},"do":[{"say":"hi","speaker":"eirik"}]}])"));
  Event e;
  e.type = EventType::Dialogue;
  e.a = 1;  // the unit with no id
  e.b = dialogue(w, 0)->b;
  e.c = e.d = -1;
  CHECK_STR(dialogueLine(w.battle, e).portrait, "");
  CHECK_STR(dialogueLine(w.battle, e).name, w.battle.units[1].name);
}

TEST(dialogue_wraps_to_the_box_and_pages_a_long_line) {
  ote_test::FakeRenderer r;
  // 10 characters per line (60 px). "alpha beta gamma delta epsilon zeta eta theta".
  std::vector<std::vector<std::string>> pages = paginateDialogue(r, "alpha beta gamma delta epsilon zeta eta theta", 60, 2);
  // alpha beta / gamma / delta / epsilon / zeta eta / theta -> 6 lines = 3 pages of 2.
  CHECK_EQ(pages.size(), (size_t)3);
  if (pages.size() != 3) return;
  CHECK_STR(pages[0][0], "alpha beta");
  CHECK_STR(pages[0][1], "gamma");
  CHECK_STR(pages[1][0], "delta");
  CHECK_STR(pages[2][1], "theta");
  for (const auto& p : pages)
    for (const std::string& line : p) CHECK(r.textWidth(line.c_str(), 1) <= 60);

  // A short line is one page; the last page may be short; an empty line is one empty page.
  CHECK_EQ(paginateDialogue(r, "Hello there", 200, 3).size(), (size_t)1);
  CHECK_EQ(paginateDialogue(r, "", 200, 3).size(), (size_t)1);
  CHECK(paginateDialogue(r, "", 200, 3)[0].empty());
  // A word longer than the box is split rather than lost.
  pages = paginateDialogue(r, "abcdefghijklmnopqrstuvwxyz", 60, 3);
  CHECK_EQ(pages.size(), (size_t)1);
  CHECK_EQ(pages[0].size(), (size_t)3);
  // Explicit line breaks are kept; trailing empty lines are dropped.
  pages = paginateDialogue(r, "one\ntwo\n\n", 200, 5);
  CHECK_EQ(pages[0].size(), (size_t)2);
  // One line per page at the smallest.
  CHECK_EQ(paginateDialogue(r, "a b c", 6, 0).size(), (size_t)3);
}

TEST(dialogue_reveal_counts_letters_not_bytes) {
  std::vector<std::string> lines = {"ab", "c\xC3\xA9" "d"};  // "céd": 3 letters, 4 bytes
  CHECK_EQ(countLetters(lines), 5);
  CHECK_EQ(revealLetters(lines, 0).size(), (size_t)0);
  std::vector<std::string> r = revealLetters(lines, 1);
  CHECK(r.size() == 1 && r[0] == "a");
  r = revealLetters(lines, 2);
  CHECK(r.size() == 1 && r[0] == "ab");
  r = revealLetters(lines, 4);  // c, é: the two-byte letter is never cut in half
  CHECK(r.size() == 2 && r[1] == "c\xC3\xA9");
  r = revealLetters(lines, 3);
  CHECK(r.size() == 2 && r[1] == "c");
  r = revealLetters(lines, 99);
  CHECK(r == lines);
}
