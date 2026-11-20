// View model for the battle screen. Battle::apply changes the rules state
// instantly; what is DRAWN comes from here, and this only changes by
// playing the Event queue in order (so the screen shows the fight as it
// unfolds). After the queue drains the view is re-synced from the Battle as
// a safety net.
#pragma once
#include <deque>
#include <map>
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "core/view/hex_geo.h"
#include "platform/platform.h"
#include "ui/dialogue_text.h"
#include "ui/scene_stage.h"

namespace ote {

class GameAudio;

constexpr int kRankOwn = -2;  // UnitView::rank: show the rank the unit really has (-1 = none is a real value)

struct UnitView {
  float px = 0, py = 0;  // top-left of the unit's tile in map pixels (animated)
  float ox = 0, oy = 0;  // extra offset (attack lunge)
  int hp = 0;            // displayed hit points
  float alpha = 1;       // fades out on death
  float scale = 1;       // pops in when built
  bool visible = true;
  int side = -1;         // side whose colour is drawn; -1 = the unit's own (differs while a SideChanged entry waits to play)
  float flash = 0;       // 0..1: the short flash of a unit that changed sides
  int type = -1;         // unit type to draw it as; -1 = the unit's own (the old type while a UnitPromoted entry waits to play)
  // Walking (docs/GRAPHICS.md "Walking"): true while a UnitMoved entry moves it; (walkDx, walkDy) is the current step in flat map pixels.
  bool walking = false;
  float walkDx = 0, walkDy = 0;
  mutable int walkLogged = -1;  // the walk direction the drawing last logged for this move (-1 = none yet); only for the debug log
  int rank = kRankOwn;    // the rank its map badge shows (docs/SRPG.md "Unit ranks"), as the events have played so far; kRankOwn = the unit's own
  int facing = -1;         // the direction its facing marker shows (docs/SRPG.md "Facing"), as the events have played so far; -1 = the unit's own
  std::vector<int> conds;  // the conditions its map marker shows, as the events have played so far (docs/SRPG.md section 9)
};

struct FloatText {
  std::string text;
  float x = 0, y = 0;  // map pixels (centre x, top y), or screen pixels if `screen`
  float age = 0, life = 0.9f;
  Color color;
  bool screen = false;
};

// The line of dialogue the view is waiting on (a Dialogue entry in the stream). The battle scene
// shows it in the dialogue box and calls skip() / skipDialogues() when the player is done; the
// view plays nothing after it until then. `serial` changes with every line.
struct DialogueState {
  bool active = false;
  int serial = 0;
  DialogueLine line;
  std::vector<std::string> answers;  // not empty = the line is a question with these answers (a Choice entry)
  bool staged = false;               // the speaker stands on the scene stage (docs/EVENTS.md "Scene presentation"): the box draws no portrait
};

struct LevelUpPanel {
  bool active = false;
  bool autoContinue = false;
  float timer = 0;
  std::string title;
  std::vector<std::string> lines;
  int ticks = 0, ticksSent = 0;  // levelUpTick sounds still to play / played for this box (battle_view_ticks.cpp)
  float tickClock = 0;
};

// Side whose resources the panel shows: the active side if it is human,
// else the first human side (autoplay: the active side). Fog: see BattleView::shownSide.
int viewSide(const Battle& b, bool autoplay);

class BattleView {
 public:
  void init(const Battle& b, int tileSize, bool autoplay);
  // Queue events produced by Battle::apply / start / aiTakeStep.
  void push(const std::vector<Event>& events);
  bool busy() const { return cur_.active || !queue_.empty() || !walkers_.empty(); }
  void update(double dt, const Battle& b);
  // Finishes the event currently playing (Confirm/Cancel/tap while animating).
  void skip(const Battle& b);
  // Snap everything to the Battle's real state (undo, safety net).
  void resync(const Battle& b);
  // Screen position where "+1000 Funds" floats spawn (the panel's resource area).
  void setResourceAnchor(int x, int y) { resX_ = x; resY_ = y; }
  // The player chose to skip: every Dialogue entry up to the end of this run of events passes at
  // once (the flag clears when the queue has drained).
  void skipDialogues(const Battle& b) {
    skipping_ = true;
    skipScene(b);  // fades, waits and walks under way complete at once (battle_view_story.cpp)
    skip(b);
  }
  // The sounds of the events it plays (battle_view_sound.cpp); null = none.
  void setAudio(GameAudio* audio) { audio_ = audio; }
  // The first TurnStarted entry of this view also shows the objective text under its banner.
  void announceObjective() { announce_ = true; }

  // ---- fog of war (battle_view_fog.cpp; docs/ECONOMY.md "Fog of war > Player")
  // Who the view is drawn for: a team (and the side whose resources the panel shows), or -1 = everyone sees
  // everything (no fog, --show-all, autoplay). Resyncs the view.
  void setViewer(const Battle& b, int side, int team);
  bool fogged() const { return viewerTeam_ >= 0; }
  int viewerTeam() const { return viewerTeam_; }
  // The side whose resources the panel shows.
  int shownSide(const Battle& b, bool autoplay) const { return viewerSide_ >= 0 ? viewerSide_ : viewSide(b, autoplay); }
  // Is the square dimmed (the viewer does not see it, as far as the animation has got)?
  bool dimmed(Cell c) const { return viewerTeam_ >= 0 && (size_t)c < shown_.size() && !shown_[(size_t)c]; }
  // Plays every event at the head of the queue that the viewer did not see (they take no time and show nothing),
  // so a run of unseen AI steps costs no frames. Does nothing while something is playing.
  void flushUnseen(const Battle& b);

  // ---- the side-view battle scene (battle_view_fight.cpp; docs/BATTLE_SCENE.md) ----
  // While `sceneMode` is on, the run of events of one fight (its skills, strikes, drain, deaths) is not played on the map: the
  // view holds it in `fight` and waits (busy) until the scene has shown it and called fightFinished(), which applies the run to
  // the view's state at once and silently (hit points, the fallen unit gone), exactly where the map playback would have left it.
  struct FightRun {
    bool active = false;
    int serial = 0;             // changes with every fight
    std::vector<Event> events;  // the run, in order
    // The rules have already applied the experience that follows the fight; the scene shows the fighters as they were when it
    // began. Per fighter: the levels it gains after the fight and the maximum hit points those levels add.
    std::map<int, int> levelsGained, hpGained;
  };
  FightRun fight;
  bool sceneMode = false;  // the player wants fights shown as the scene (set by the battle screen every frame)
  void fightFinished(const Battle& b);
  // A battle that starts with fights in its first events (a staged `fight` of a battle-start event): the view is made after the rules have run
  // them, so it shows the end of them. This puts the hit points and the fallen back as they were before the first one (battle_view_fight.cpp).
  void rewindStartFights(const Battle& b, const std::vector<Event>& startEvents);

  std::vector<UnitView> units;  // indexed by unit id
  std::vector<FloatText> floats;
  LevelUpPanel levelUp;
  std::string notice;  // a short message for the hint line (the scene says it and clears this)
  std::string banner;  // centred banner text, empty = none
  std::string bannerSub;  // smaller line under the banner (an objective), empty = none
  DialogueState dialogue;
  // Scene presentation (docs/EVENTS.md "Scene presentation"; battle_view_story.cpp): the background, fade, shake, weather and
  // standing portraits, played in order with the other entries. The battle scene draws it.
  SceneStage stage;
  // The camera of the latest Focus entry: its scroll time in ms (-1 = not given: the usual quick ease, 0 = jump) and a serial
  // that changes with every Focus entry, so the battle scene starts a new scroll for each.
  int focusMs = -1, focusSerial = 0;
  // True while a scene command (fade, wait, shake, background, walk) is playing or waiting in the queue: the skip control then
  // has something to fast-forward.
  bool inScene() const;
  std::vector<float> structFlash;  // per structure: 0..1, the white flash of an upgrade (battle_view_economy.cpp)
  std::vector<float> cellFlash;    // per square: 0..1, the white flash of a changed square (battle_view_mapchange.cpp)
  Color bannerColor;
  // While an event involving a unit plays, the camera should follow it.
  bool hasFocus = false;
  float focusX = 0, focusY = 0;  // map pixels (centre)

 private:
  struct Current {
    bool active = false;
    Event ev;
    float t = 0, dur = 0;
    bool fired = false;  // one-shot part of the event (attack impact) done
    std::vector<Cell> path;
    int hpFrom = 0, hpTo = 0;
    float dirX = 0, dirY = 0;  // attack lunge direction
    bool hideAtEnd = false;    // fog: a walk that ends out of the viewer's sight (the unit vanishes at its last seen square)
  };
  void ensureUnits(const Battle& b);
  void startNext(const Battle& b);
  void begin(const Battle& b);     // sets up cur_ from its event
  // The longer cases of begin() (battle_view.cpp), each setting up cur_ for its entry.
  void beginTurnStarted(const Battle& b);
  void beginAttack(const Battle& b);
  void beginLevelUp(const Battle& b);
  void beginCapture(const Battle& b);  // CaptureProgress, StructureCaptured
  void beginResource(const Battle& b);
  void beginUnitAppears(const Battle& b);  // UnitBuilt, UnitAdded
  void beginHealed(const Battle& b);
  void beginItemEvent(const Battle& b);  // ItemBroke, ItemGained, ItemLost
  void progress(const Battle& b);  // applies cur_ at time t
  void finish(const Battle& b);    // final state of cur_
  void impact(const Battle& b);    // attack: spawn numbers
  bool takeFightRun(const Battle& b);  // battle_view_fight.cpp: cur_.ev starts a fight the scene shows: hold the run
  bool quiet_ = false;             // fightFinished is replaying a run: no sounds
  // Story entries (docs/EVENTS.md): battle_view_story.cpp. Each returns false for other entries.
  struct Walker {  // a walked moveUnit with "wait": false: it goes on while the next commands play
    int unit = -1;
    std::vector<Cell> path;
    float t = 0, dur = 0;
  };
  std::vector<Walker> walkers_;
  void skipScene(const Battle& b);                 // battle_view_story.cpp
  void updateScene(double dt, const Battle& b);    // stage clock and background walkers
  void walkStep(const Battle& b, UnitView& v, const std::vector<Cell>& path, float p, bool sound);
  bool beginSceneEffect(const Battle& b);
  bool beginStory(const Battle& b);
  bool progressStory(const Battle& b);
  bool finishStory(const Battle& b);
  // Upgrades, research, repair and supply: battle_view_economy.cpp. Same contract as the story hooks.
  bool beginCommander(const Battle& b);  // battle_view_commander.cpp   PowerUsed, PowerEnded (docs/COMMANDERS.md); the banner goes with finish()
  bool beginEconomy(const Battle& b);
  bool progressEconomy(const Battle& b);
  bool finishEconomy(const Battle& b);
  // Strategy-RPG events (docs/SRPG.md "Events"): battle_view_srpg.cpp dispatches the eleven new entries to one file
  // per area, each with the same contract as the story hooks: return true when the entry was one of its own (and so
  // fully handled there). While an area's functions return false the entry is a logged no-op (plus the minimal state
  // change for carried units, see battle_view_srpg.cpp).
  bool beginSrpg(const Battle& b);
  bool progressSrpg(const Battle& b);
  bool finishSrpg(const Battle& b);
  void pushUnitRank(const Event& e);        // from push(): keeps the old badge until UnitRankUp plays (battle_view_unit_rank.cpp)
  bool beginUnitRank(const Battle& b);      // battle_view_unit_rank.cpp   UnitRankUp (docs/SRPG.md "Unit ranks")
  bool progressUnitRank(const Battle& b);
  bool finishUnitRank(const Battle& b);
  bool beginJobs(const Battle& b);          // battle_view_jobs.cpp   JpGained (docs/JOBS.md): "+10 JP" floats over the unit
  bool beginSkills(const Battle& b);       // battle_view_skills.cpp     SkillLearned, SkillActivated, UnitHurt   (U1)
  bool progressSkills(const Battle& b);
  bool finishSkills(const Battle& b);
  void addLearnedToBox(const Battle& b, int unit);  // a level-up box also lists the skills learned right after it (U1)
  void pushPromotion(const Event& e);      // from push(): keeps the old type drawn until the entry plays
  bool beginPromotion(const Battle& b);    // battle_view_promotion.cpp  UnitPromoted                              (U2)
  bool progressPromotion(const Battle& b);
  bool finishPromotion(const Battle& b);
  bool beginAbilities(const Battle& b);    // battle_view_abilities.cpp  AbilityUsed                               (U3)
  bool progressAbilities(const Battle& b);
  bool finishAbilities(const Battle& b);
  bool beginRescue(const Battle& b);       // battle_view_rescue.cpp     UnitRescued, UnitDropped, UnitHanded, PairSwitched (U4)
  bool progressRescue(const Battle& b);
  bool finishRescue(const Battle& b);
  bool beginStaves(const Battle& b);       // battle_view_staves.cpp     StaffUsed, StaffMissed (docs/SRPG.md section 8)
  bool progressStaves(const Battle& b);
  bool finishStaves(const Battle& b);
  bool beginMapChange(const Battle& b);    // battle_view_mapchange.cpp  TerrainChanged (docs/EVENTS.md "Changing the map")
  bool progressMapChange(const Battle& b);
  bool finishMapChange(const Battle& b);
  bool beginPlaces(const Battle& b);       // battle_view_places.cpp    DoorOpened .. PlaceUnlocked, the chest's "Found" lines (docs/SRPG.md section 10)
  bool progressPlaces(const Battle& b);
  bool finishPlaces(const Battle& b);
  bool placeTop(const Battle& b, int structure, float& x, float& y) const;  // the middle of the structure's top edge, map pixels
  bool progressSpeed(const Battle& b);
  bool beginSpeed(const Battle& b);        // battle_view_speed.cpp   UnitTurnBegan, RoundStarted, ActionCharging, ChargeResolved, ChargeCancelled (docs/TURN_ORDER.md)
  bool beginConditions(const Battle& b);   // battle_view_conditions.cpp ConditionApplied, ConditionEnded, ConditionResisted, ConditionTick (section 9)
  bool progressConditions(const Battle& b);
  bool finishConditions(const Battle& b);
  bool beginRanks(const Battle& b);        // battle_view_ranks.cpp      RankUp (docs/SRPG.md section 7)
  bool progressRanks(const Battle& b);
  bool finishRanks(const Battle& b);
  bool beginSupports(const Battle& b);     // battle_view_supports.cpp   SupportPoints, SupportReached             (U5)
  bool progressSupports(const Battle& b);
  bool finishSupports(const Battle& b);
  // Fog of war: battle_view_fog.cpp. beginFog returns true for the entries it fully set up (a partly seen walk, an ambush).
  bool unseenByViewer(const Event& e) const;
  void applyUnseen(const Battle& b, const Event& e);  // the effect on the view's own copy of the state, at once, silently
  bool beginFog(const Battle& b);
  void updateShown(const Battle& b);
  void addFloat(const std::string& text, float x, float y, Color c, float life = 0.9f, bool screen = false);
  // battle_view_sound.cpp
  void soundStarted(const Battle& b);
  void soundImpact(const Battle& b);
  void soundStep(const Battle& b, size_t index);
  void clampHpTo(const Battle& b, int unit);
  std::string itemName(const Battle& b, int item) const;
  void cellPos(const Battle& b, Cell c, float& x, float& y) const;
  void structureTop(const Battle& b, const StructureState& st, float& x, float& y) const;
  bool showsResources(const Battle& b, int side) const;

  Current cur_;
  std::deque<Event> queue_;
  GameAudio* audio_ = nullptr;
  int healItem_ = -1;  // the staff / healing item whose use the UnitHealed entries now playing belong to (its "sounds.heal" applies); -1 = none
  int stepped_ = -1;  // UnitMoved: the path square the last step sound was made for
  float promoSoundClock_ = -1;  // seconds since the promotion began while the promotionSounds list plays (-1 = none)
  size_t promoSoundNext_ = 0;   // the next entry of that list
  int risenStats(const Event& e) const;  // battle_view_ticks.cpp
  void startPromotionSounds();
  void tickSounds(float dt);
  int tile_ = 24;
  bool autoplay_ = false;
  bool wasBusy_ = false;
  int resX_ = 0, resY_ = 0;
  int resFloats_ = 0;  // stagger counter for resource floats
  int viewerTeam_ = -1, viewerSide_ = -1;  // see setViewer()
  std::vector<uint8_t> shown_;             // per square: the viewer sees it (while events play: from where its units are drawn)
  std::vector<int> shownKey_;              // the squares of the viewer's units shown_ was last worked out for
  bool skipping_ = false;  // dialogue skipping is on for the rest of this run
  bool announce_ = false;  // see announceObjective()
  // A Focus entry keeps the camera on its spot until the run of events is over (else the camera
  // would drift back to the cursor while the line it introduces is on screen).
  bool stickyFocus_ = false;
  float stickyX_ = 0, stickyY_ = 0;
};

}  // namespace ote
