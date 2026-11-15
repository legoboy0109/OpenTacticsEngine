// A campaign being played (docs/EVENTS.md "Campaign"): where the player is, who is in the
// party, what the parties carry, and the story switches / variables. Plain data, copyable.
// Everything that changes it is a free function below, all deterministic and free of input:
// scenes apply at once and hand the player the lines to show; battles are played as usual and
// their result is taken over by finishChapterBattle.
//
// The flow for the player (also in HANDOFF.md):
//   begin -> [runScene(Before)] -> prepare/startChapterBattle -> play -> finishChapterBattle
//         -> [runScene(After)] -> next chapter ... -> Finished   (or Over after a lost battle)
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "core/battle/carried.h"
#include "core/data/campaign.h"

namespace ote {

class Formulas;

enum class Phase {
  Before,    // the chapter's "before" scene has not run yet
  Battle,    // the scene is done; the chapter's battle is to be played (or is under way)
  After,     // the battle is won and taken over; the "after" scene has not run yet
  Finished,  // the last chapter's after scene ran: the campaign is complete
  Over,      // a battle was lost and the campaign's onDefeat is "end"
};

// What a side carries from chapter to chapter, by side id (every chapter's map names its own sides).
struct CarriedSide {
  std::string id;
  std::vector<int> resources;       // per Database::resources
  std::vector<ItemGrant> storage;   // the shared storage's items
  std::vector<int> research;        // finished technologies (indices into Database::research, ascending);
                                    // research still in progress is not carried (docs/ECONOMY.md)
};

struct Playthrough {
  int chapter = 0;  // index into CampaignDef::chapters; once Finished / Over, the last one played
  Phase phase = Phase::Before;
  Roster roster;                    // every named unit that ever joined, see RosterUnit
  std::vector<CarriedSide> sides;
  std::vector<uint8_t> switches;    // per Database::switches
  std::vector<int> variables;       // per Database::variables

  // A new playthrough at chapter 1; false (nothing changed) if the game has no campaign.
  bool begin(const Database& db);
  // True while there is something left to play (not Finished, not Over).
  bool active() const { return phase != Phase::Finished && phase != Phase::Over; }
  const RosterUnit* findUnit(const std::string& id) const;
  CarriedSide& sideCarry(const std::string& id, size_t resourceCount);  // creates the entry when needed
};

// One thing a scene shows, resolved like a battle's Dialogue entry: the speaker's name is the
// roster unit's own, or the free "speakerName", or empty.
struct SceneLine {
  // A line to show, or (docs/SOUND.md) a sound request that comes between lines: the player acts on it when it
  // reaches it. `text` is then the music / sound id ("" for StopMusic) and `fadeMs` the fade.
  enum class Kind { Say, PlayMusic, StopMusic, PlaySound };
  Kind kind = Kind::Say;
  int fadeMs = 0;
  std::string speakerId;  // the roster id named by "speaker" ("" = none)
  std::string speaker;    // the name to show ("" = nobody)
  std::string portrait;   // portrait id ("" = none; the player may look up speakerId's instead)
  std::string text;
};

enum class SceneKind { Before, After };

// The chapter in play (null when the game has no campaign or the playthrough is Finished / Over).
const ChapterDef* currentChapter(const Database& db, const Playthrough& pt);

// May this map be started on its own? True for every map of a game without a campaign; with one,
// it follows the campaign's "freeBattles". A free battle never touches a playthrough.
bool freeBattleAllowed(const Database& db, const std::string& mapId);

// Runs the current chapter's scene: state changes apply at once and the lines to show are appended
// to `lines`. Before needs phase Before (then phase = Battle); After needs phase After (then the
// next chapter in phase Before, or Finished). False and nothing done in any other phase.
bool runScene(const Database& db, Playthrough& pt, SceneKind which, std::vector<SceneLine>& lines);

// Gets the chapter's battle ready: loads the map, initialises `battle`, dresses the named units of
// human sides from the roster and hands in carried storage, resources, switches and variables. It
// does NOT call battle.start(): do that for a new battle, or restoreBattleState for a saved one.
// Needs phase Battle. Does not change the playthrough.
bool prepareChapterBattle(const Database& db, const Playthrough& pt, Formulas& formulas, uint32_t seed, Battle& battle,
                          std::string& err);
// prepareChapterBattle, then battle.start(events).
bool startChapterBattle(const Database& db, const Playthrough& pt, Formulas& formulas, uint32_t seed, Battle& battle,
                        std::vector<Event>& events, std::string& err);

enum class ChapterResult {
  NotOver,     // the battle is not decided yet; nothing changed
  WrongPhase,  // the playthrough is not in phase Battle; nothing changed
  Won,         // taken over; phase = After
  Retry,       // lost, onDefeat "retry": the playthrough is exactly as before the battle (phase Battle)
  Ended,       // lost, onDefeat "end": phase = Over
};

// Takes the result of a decided chapter battle over into the playthrough (rules in docs/EVENTS.md).
ChapterResult finishChapterBattle(const Database& db, Playthrough& pt, const Battle& battle);

}  // namespace ote
