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

// The preparation screen of the current chapter's map (docs/EVENTS.md "Preparation"; core/campaign/prep.h). All of it
// is empty / false for a chapter whose map has no preparation, and is then left out of the save.
struct PrepDeploy {
  std::string id;  // roster / story id of the character
  int slot = 0;    // index into the map's PrepDef::slots
};
struct PrepState {
  bool open = false;                // the screen is under way (a save made now restores into it)
  bool folded = false;              // the prep side's map allowance (resources, storage) is already in CarriedSide
  std::vector<PrepDeploy> deployed; // who is deployed and where, in order; empty = nothing chosen (the default pick applies)
  bool any() const { return open || folded || !deployed.empty(); }
};

struct Playthrough {
  int chapter = 0;  // index into CampaignDef::chapters; once Finished / Over, the last one played
  Phase phase = Phase::Before;
  Roster roster;                    // every named unit that ever joined, see RosterUnit
  std::vector<CarriedSide> sides;
  std::vector<uint8_t> switches;    // per Database::switches
  std::vector<int> variables;       // per Database::variables
  SupportState supports;            // closeness of the support pairs (docs/SRPG.md); empty in a game without pairs
  PrepState prep;                   // the preparation of the current chapter, see PrepState
  std::vector<std::string> lastDeployed;  // ids deployed in the last chapter that had a preparation, in order (the default pick uses it)
  std::vector<uint8_t> quotesSaid;  // per battle quote (BattleQuote::flat): said already; only "campaign" quotes are kept (docs/EVENTS.md)

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
  // Choice (docs/EVENTS.md "Dialogue choices"): `text` is the question, `answers` the offered answers' words; `waiting`
  // = nothing answered it yet, so the scene stops here (runScene with answers).
  // Fx (docs/EVENTS.md "Scene presentation"): `fx` says which command, `text` is its picture id / speaker key (""= none /
  // everybody), `fadeMs` its milliseconds, the rest are the extra keys; a Say line carries position / narrate / autoMs.
  enum class Kind { Say, PlayMusic, StopMusic, PlaySound, Choice, Fx };
  Kind kind = Kind::Say;
  bool waiting = false;
  std::vector<std::string> answers;
  int fadeMs = 0;
  std::string speakerId;  // the roster id named by "speaker" ("" = none)
  std::string speaker;    // the name to show ("" = nobody)
  std::string portrait;   // portrait id ("" = none; the player may look up speakerId's instead)
  std::string text;
  SceneFx::Kind fx = SceneFx::Kind::None;  // Fx lines
  uint32_t color = 0;                       // Fx Fade: 0xRRGGBB
  int strength = 3;                         // Fx Shake
  bool out = false, fadeTransition = false; // Fx Fade out / Background "fade" transition
  int position = 0;                         // Say: kPosLeft / kPosRight / kPosCenter, 0 = none
  bool narrate = false;                     // Say
  int autoMs = 0;                           // Say: the line moves on by itself after this long (0 = never)
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
// Dialogue choices (docs/EVENTS.md "Dialogue choices"): the same, but a question waits for a person. `answers` are the
// picks made so far, in order, each a position among the answers offered at that question. The scene runs until it
// meets a question with no pick: that question is its last line (Choice with `waiting`), the playthrough is as the
// scene left it up to there and the phase has not moved. To go on, start again from a copy of the playthrough as it
// was before the scene, with the picks plus the new one: the lines come back from the start (the same ones, then more).
// Answered questions come back as Choice lines with `waiting` false. A scene with no question behaves as above.
bool runScene(const Database& db, Playthrough& pt, SceneKind which, std::vector<SceneLine>& lines,
              const std::vector<int>& answers);

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

// ---- supports between chapters (supports_campaign.cpp; docs/SRPG.md "Story and campaign") ----
// A new playthrough: every pair at its "start" points and level 0 (called by Playthrough::begin).
void beginSupports(const Database& db, Playthrough& pt);
// After a won battle: the playthrough takes the battle's closeness (called by finishChapterBattle).
void takeSupports(const Database& db, Playthrough& pt, const Battle& battle);
// Pairs with a ready conversation whose two characters are in the roster and not gone, in pair order.
void pendingSupports(const Database& db, const Playthrough& pt, std::vector<int>& pairs);
// Plays it: the level is gained and the lines come back like a scene's. False if it is not pending.
bool playSupport(const Database& db, Playthrough& pt, int pair, std::vector<SceneLine>& lines);
// A scene's addSupport command: `by` points (may be negative) to the pair; nothing if one of them fell for good.
void sceneAddSupport(const Database& db, Playthrough& pt, int pair, int by);

}  // namespace ote
