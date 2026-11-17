// Dialogue choices in a battle (docs/EVENTS.md "Dialogue choices").
//
// The rules core never waits inside an action, so a question that a person must answer is done by replay:
//   1. apply / start (when setAskChoices is on and the map has a choice) first keep a copy of the Battle (`pre`).
//   2. The action runs. At the first question without an answer the run takes a snapshot (`atQuestion`), notes how many
//      entries exist so far, and carries on with every command after it turned off (`halted`), so the rest of the
//      action only finishes its own C++ calls. Its result is thrown away.
//   3. The Battle becomes the snapshot again (the state exactly at the question), the caller gets the entries up to and
//      including the Choice entry, and `wait` remembers `pre`, the action, the answers given so far and how many entries
//      the caller has seen.
//   4. answerChoice goes back to `pre` and runs the same action with one more answer. Everything is deterministic, so
//      the run reaches the same question with the same entries; those the caller has are skipped, the rest are handed
//      over (up to the next unanswered question, if any).
// Not saved: a question on screen cannot be saved (the player is busy then), and a save made earlier is untouched.
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

struct ChoiceWait {
  std::shared_ptr<const Battle> pre;  // the Battle before the action that reached the question
  bool isStart = false;               // the action was Battle::start
  Action action;
  std::vector<int> answers;           // positions already answered (among the offered answers), in order
  size_t delivered = 0;               // entries of the replayed run that the caller has (up to the question)
  std::vector<int> offered;           // the waiting question's offered answers (indices into the command's answers)
  const Command* asked = nullptr;     // the waiting question (kept alive by Battle::story)
};

void Battle::runAsking(bool isStart, const Action& action, std::shared_ptr<const Battle> pre,
                       const std::vector<int>& answers, size_t skip, std::vector<Event>& out) {
  // `action` and `answers` may live inside the wait that the next lines destroy: copy them first.
  const Action act = action;
  const std::vector<int> given = answers;
  if (!pre) pre = std::make_shared<const Battle>(*this);
  else *this = *pre;
  choice_.running = true;
  choice_.answers = given;
  choice_.used = 0;
  choice_.halted = false;
  choice_.atQuestion.reset();
  choice_.asked = nullptr;
  choice_.offered.clear();
  std::vector<Event> run;
  if (isStart) startNow(run);
  else applyAction(act, run);
  choice_.running = false;
  if (!choice_.halted) {  // no (further) question: done
    choice_.answers.clear();
    choice_.used = 0;
    for (size_t i = skip; i < run.size(); ++i) out.push_back(std::move(run[i]));
    return;
  }
  std::shared_ptr<Battle> at = choice_.atQuestion;
  auto wait = std::make_shared<ChoiceWait>();
  wait->pre = pre;
  wait->isStart = isStart;
  wait->action = act;
  wait->answers = given;
  wait->delivered = choice_.askedAt;
  wait->offered = choice_.offered;
  wait->asked = choice_.asked;
  *this = *at;  // the state at the question
  choice_ = StoryChoice();
  choice_.ask = true;
  choice_.wait = wait;
  sightEvents_ = nullptr;  // the snapshot was taken inside the run's EventScope
  stampFrom_ = 0;
  storyRun_ = StoryRun();
  for (size_t i = skip; i < wait->delivered && i < run.size(); ++i) out.push_back(std::move(run[i]));
  OTE_INFO("story: a question waits for an answer: %s", storyText(wait->asked->text).c_str());
}

bool Battle::answerChoice(int pick, std::vector<Event>& events) {
  std::shared_ptr<ChoiceWait> w = choice_.wait;
  if (!w || pick < 0 || pick >= (int)w->offered.size()) return false;
  std::vector<int> answers = w->answers;
  answers.push_back(pick);
  runAsking(w->isStart, w->action, w->pre, answers, w->delivered, events);
  return true;
}

void Battle::storyChoice(const Command& c, std::vector<Event>& events) {
  if (choice_.halted) return;
  std::vector<int> offered;
  for (size_t i = 0; i < c.answers.size(); ++i)
    if (conditionsHold(c.answers[i].cond)) offered.push_back((int)i);
  if (offered.empty()) {  // every answer is hidden: nothing to ask, nothing to record
    OTE_INFO("story: choice \"%s\": no answer is offered; skipped", storyText(c.text).c_str());
    return;
  }
  size_t picked = (size_t)offered[0];
  if (offered.size() >= 2) {
    Event e;
    e.type = choice_.running ? EventType::Choice : EventType::Dialogue;  // nobody to ask: the question is only said
    e.a = c.unit.empty() ? -1 : unitByStoryId(c.unit);
    e.b = c.text;
    e.c = c.speakerName;
    e.d = c.portrait;
    if (choice_.running)
      for (int i : offered) e.data.push_back(c.answers[(size_t)i].text);
    events.push_back(e);
    if (choice_.running) {
      if (choice_.used < choice_.answers.size()) {
        int pos = choice_.answers[choice_.used++];
        picked = (size_t)offered[(size_t)std::max(0, std::min(pos, (int)offered.size() - 1))];
      } else {  // no answer yet: this is where the run stops
        stampEvents();  // the entries so far are stamped with what the teams see now, not at the run's end
        choice_.atQuestion = std::make_shared<Battle>(*this);
        choice_.halted = true;
        choice_.askedAt = events.size();
        choice_.offered = offered;
        choice_.asked = &c;
        return;
      }
    }
  }
  // Fewer than two answers offered: nothing to ask, the one answer is taken.
  if (c.index >= 0) storyState.variables[(size_t)c.index] = (int)picked + 1;
  runCommands(c.answers[picked].commands, events);
}

}  // namespace ote
