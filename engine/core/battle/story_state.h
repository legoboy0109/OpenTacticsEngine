// What changes during play about a battle's story (docs/EVENTS.md), plus the small types the
// story code in Battle shares. The events themselves are fixed data (core/data/events.h).
#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/base/types.h"
#include "core/data/events.h"

namespace ote {

class Battle;

// Saved with the battle (docs/SAVES.md). A caller that carries state between battles (a
// campaign) writes `switches` / `variables` after Battle::init and before start(), and reads
// them after the battle; a standalone battle starts with everything off / 0.
struct StoryState {
  std::vector<uint8_t> switches;  // per Database::switches: 0 off, 1 on
  std::vector<int> variables;     // per Database::variables
  std::vector<uint8_t> fired;     // per StoryData::events: has run (non-repeating events run once)
  int objectives = 0;             // index into StoryData::objectives; 0 = the map's own
  // "The music the story asked for" (docs/SOUND.md): the last playMusic / stopMusic command that ran. Presentation
  // only, kept so a resumed battle plays the right track. Saved only when musicAsked (docs/SAVES.md).
  bool musicAsked = false;
  std::string music;              // the track id; empty = the story stopped the music
};

// Something that happened which events can be waiting for.
struct StoryOccurrence {
  Trigger::Kind kind = Trigger::Kind::BattleStart;
  int unit = -1;   // Reach: who moved; Defeated: who fell; Fight: attacker
  int other = -1;  // Defeated: who struck the blow (-1 = nobody); Fight: defender
  Cell cell = kNoCell;  // Reach: where it stopped
  int side = -1, round = 0;  // Turn
  int pair = -1, level = 0;  // Support (docs/SRPG.md): the pair and the level it gained
};

// One entry of the Talk menu: talk to unit `target`, which runs story event `event`.
struct TalkOption {
  int target = -1;
  int event = -1;
  int label = -1;  // index into the story text table (Battle::storyText)
};

// Bookkeeping while events run; empty between actions. Events that fire while another runs
// wait in `queue` (docs/EVENTS.md).
struct StoryRun {
  struct Queued { int event; int talker; };
  std::vector<Queued> queue;
  bool running = false;
  int ran = 0;       // events run in this go (the chain limit counts these)
  int talker = -1;   // the unit of the Talk event that is running (for endTurn)
};

struct ChoiceWait;  // story_choice.cpp

// Dialogue choices (docs/EVENTS.md "Dialogue choices"). Events never wait inside the core, so a question that the player
// must answer is done by replay: the Battle keeps the state before the action that reached it (`wait->pre`), hands the
// caller the entries up to the question, and when the answer arrives runs the same action again with the answers
// given so far. Transient: nothing here is saved (a question on screen is never saved; the last save stays).
struct StoryChoice {
  bool ask = false;                    // questions wait for a player (setAskChoices); off = the first answer is taken
  std::shared_ptr<ChoiceWait> wait;    // set while a question is waiting for its answer
  // While an asking run is under way (story_choice.cpp):
  bool running = false;                // inside runAsking
  std::vector<int> answers;            // the positions (among the answers offered) already given, in order
  size_t used = 0;                     // how many of them the run has consumed
  bool halted = false;                 // the run reached a question with no answer yet: nothing after it counts
  size_t askedAt = 0;                  // entries (of this run) up to and including that question
  std::vector<int> offered;            // that question's offered answers (indices into the command's answers)
  const Command* asked = nullptr;      // that question
  std::shared_ptr<Battle> atQuestion;  // the state at that question
};

constexpr int kStoryChainLimit = 100;

}  // namespace ote
