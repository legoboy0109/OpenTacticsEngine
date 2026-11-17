// The campaign flow in the player App (docs/EVENTS.md "Campaign", docs/SAVES.md "Campaign saves"):
// New game, the scenes between battles, starting / resuming a chapter's battle, the retry choice and
// the end screens. The decisions (what follows what) are in campaign/campaign_logic.*; this file
// only turns them into scenes. Scene switches arrive through later(), so nothing here runs while the
// scene that asked for it is still on the stack.
#include "app.h"
#include "battle/battle_scene.h"
#include "campaign/camp_scene.h"
#include "campaign/campaign_logic.h"
#include "campaign/end_scenes.h"
#include "campaign/story_scene.h"
#include "core/base/log.h"
#include "core/campaign/difficulty.h"
#include "core/campaign/prep.h"
#include "core/data/loader.h"

namespace ote {

void PlayerApp::startCampaign(const std::string& gameId) {
  later([this, gameId] {
    if (!openGame(gameId)) return;
    firstLoad_ = false;
    doStartCampaign();
  });
}

void PlayerApp::startFreeBattle(const std::string& gameId, const std::string& mapId) {
  later([this, gameId, mapId] {
    if (!openGame(gameId)) return;
    firstLoad_ = false;
    doStartBattle(mapId);
  });
}

void PlayerApp::campaignNext() {
  later([this] { doCampaignNext(); });
}

void PlayerApp::campaignSceneDone() {
  later([this] {
    saveAfterScene();
    doCampaignNext();
  });
}

void PlayerApp::campaignBattleOver(ChapterResult result) {
  later([this, result] {
    if (!session_ || !session_->campaign) return;
    if (nextAfterBattle(result) != Next::RetryChoice) { doCampaignNext(); return; }
    session_->campaign->prepDone = false;  // trying again: a chapter with a preparation shows its screen once more
    const Playthrough& pt = session_->campaign->pt;
    const ChapterDef* ch = currentChapter(session_->db, pt);
    scene_.reset();
    scene_.reset(new RetryScene(*this, chapterHeading(pt.chapter, activePartyName(session_->db, pt)), ch ? ch->name : ""));
  });
}

void PlayerApp::doStartCampaign() {
  if (!session_->db.campaign) {
    showError("Cannot start a new game", "This game has no campaign.", ToGame);
    runPending();
    return;
  }
  scene_.reset();
  session_->campaign.reset(new CampaignRun());
  session_->campaign->pt.begin(session_->db);
  OTE_INFO("campaign: new game of '%s'", gameId_.c_str());
  std::string why;
  if (difficultyNeeded()) {  // docs/DIFFICULTY.md: New Game asks which difficulty first
    askDifficulty("New game", [this](const std::string& id) {
      std::string note;
      if (session_ && session_->campaign && setPlaythroughDifficulty(session_->db, session_->campaign->pt, id, true, note)) {
        OTE_INFO("campaign: difficulty \"%s\"", id.c_str());
        doCampaignNext();
      }
    });
    return;
  }
  if (!args_.difficulty.empty()) setPlaythroughDifficulty(session_->db, session_->campaign->pt, args_.difficulty, true, why);
  doCampaignNext();
}

// The Continue save after a scene: the state the scene left behind (a won last chapter is the end of
// the story, which deletes it). Failure is only logged: the next battle turn reports a broken save.
void PlayerApp::saveAfterScene() {
  if (!session_ || !session_->campaign) return;
  SaveStore store = saveStore(gameId_);
  const Database& db = session_->db;
  if (!store.enabled() || db.saving == SavingMode::Off || args_.autoplay) return;
  const Playthrough& pt = session_->campaign->pt;
  std::string err;
  bool ok = pt.active() ? writeCampaignSave(store, 0, db, gameId_, pt, nullptr, err) : store.remove(0, err);
  if (!ok) OTE_WARN("campaign: saving after a scene failed: %s", err.c_str());
}

// Runs a chapter's scene on the playthrough. With a person at the screen a question stops the scene (dialogue choices,
// docs/EVENTS.md): the playthrough is then as the scene left it up to the question, and the returned function runs the
// scene again from `before` with the answers so far and stores the result in the playthrough. Null when the scene
// has no question to wait for (the playthrough is then final) or in --autoplay (the first answer is taken).
SceneResolver PlayerApp::runSceneFor(SceneKind kind, const Playthrough& before, std::vector<SceneLine>& lines) {
  Database& db = session_->db;
  Playthrough& pt = session_->campaign->pt;
  if (!session_->askChoices) {
    runScene(db, pt, kind, lines);
    return nullptr;
  }
  runScene(db, pt, kind, lines, {});
  if (lines.empty() || lines.back().kind != SceneLine::Kind::Choice || !lines.back().waiting) return nullptr;
  return [this, kind, before](const std::vector<int>& answers, std::vector<SceneLine>& out) {
    if (!session_ || !session_->campaign) return false;
    Playthrough again = before;
    out.clear();
    if (!runScene(session_->db, again, kind, out, answers)) return false;
    session_->campaign->pt = again;
    return true;
  };
}

void PlayerApp::prepareDone() {
  later([this] {
    if (!session_) return;
    if (session_->campaign) {
      session_->campaign->prepDone = true;
      doCampaignNext();
      return;
    }
    scene_.reset();  // the preparation scene's battle must die before the real one replaces it
    std::string err;
    if (!session_->startPreparedMap(args_.seed, err)) {
      showError("Cannot start the battle", err, ToMapSelect);
      runPending();
      return;
    }
    scene_.reset(new BattleScene(*this));
  });
}

void PlayerApp::doCampaignNext() {
  if (!session_ || !session_->campaign) return;
  scene_.reset();  // a battle's scene must die before its Battle is replaced
  CampaignRun& run = *session_->campaign;
  const Database& db = session_->db;
  Playthrough& pt = run.pt;
  const bool battleInHand = run.battleInHand;
  run.battleInHand = false;
  const Next next = nextForPhase(pt.phase, battleInHand);
  std::vector<SceneLine> lines;
  switch (next) {
    case Next::BeforeScene: {
      // The camp (support conversations) comes first, from the second chapter on, when anything is pending.
      if (std::unique_ptr<Scene> camp = campStep(*this)) { scene_ = std::move(camp); break; }
      run.campDone = false;
      run.prepDone = false;
      const Playthrough before = pt;
      SceneResolver again = runSceneFor(SceneKind::Before, before, lines);  // applies at once; the screen only shows it
      const ChapterDef* ch = currentChapter(db, pt);
      scene_.reset(new StoryScene(*this, chapterHeading(pt.chapter, activePartyName(db, pt)), ch ? ch->name : "", lines, again));
      break;
    }
    case Next::AfterScene: {
      const Playthrough before = pt;
      SceneResolver again = runSceneFor(SceneKind::After, before, lines);
      if (lines.empty()) {  // nothing to show: straight on (the new state is saved all the same)
        saveAfterScene();
        doCampaignNext();
      } else {
        scene_.reset(new StoryScene(*this, "", "", lines, again));
      }
      break;
    }
    case Next::StartBattle: {
      session_->battle = Battle();
      if (!pt.party.empty()) OTE_INFO("campaign: chapter %d is played by party '%s' (%s)", pt.chapter + 1, pt.party.c_str(), activePartyName(db, pt).c_str());
      session_->battle.setAskChoices(session_->askChoices);  // before start: a question may come with the first events
      session_->startEvents.clear();
      std::string err;
      if (!args_.autoplay && !run.prepDone) {  // a chapter whose map has a preparation: the screen first (docs/EVENTS.md "Preparation")
        const ChapterDef* ch = currentChapter(db, pt);
        MapDef map;
        if (ch && loadMap(db, db.maps[(size_t)ch->map].id, map, err) && prepOn(map)) {
          prepOpen(db, pt, map, true);
          if (!prepareChapterBattle(db, pt, session_->formulas, args_.seed, session_->battle, err)) {
            showError("Cannot start the chapter", err, ToGame);
            runPending();
            return;
          }
          OTE_INFO("prep: the preparation screen of chapter %d opens", pt.chapter + 1);
          scene_.reset(new BattleScene(*this, true));
          break;
        }
        err.clear();
      }
      if (!startChapterBattle(db, pt, session_->formulas, run.nextSeed(args_.seed), session_->battle,
                              session_->startEvents, err)) {
        showError("Cannot start the chapter", err, ToGame);
        runPending();
        return;
      }
      scene_.reset(new BattleScene(*this));
      break;
    }
    case Next::ResumeBattle:
      session_->startEvents.clear();  // a loaded battle replays nothing
      scene_.reset(new BattleScene(*this));
      break;
    case Next::EndFinished:
      scene_.reset(new EndScene(*this, "The End", db.campaign->name));
      break;
    case Next::EndOver:
      scene_.reset(new EndScene(*this, "The story ends here.", ""));
      break;
    default: break;
  }
}

// Loading a campaign save: the playthrough (and its battle) come from the file, then the flow goes on
// by phase exactly like after a scene. Nothing of the session changes unless the whole file is valid.
bool PlayerApp::resumeCampaignSave(const SaveFile& file, std::string& err) {
  std::unique_ptr<CampaignRun> run(new CampaignRun());
  Battle fresh;
  fresh.setAskChoices(session_->askChoices);
  bool hasBattle = false;
  std::string detail;
  if (!restoreCampaignSave(session_->db, session_->formulas, gameId_, file.root, run->pt, fresh, hasBattle, detail)) {
    OTE_WARN("save rejected: %s", detail.c_str());  // the exact field, for whoever fixes it
    err = "The save is damaged (" + detail + ")";
    return false;
  }
  if (hasBattle) session_->battle = fresh;
  session_->startEvents.clear();
  run->battleInHand = hasBattle;
  session_->campaign = std::move(run);
  return true;
}

}  // namespace ote
