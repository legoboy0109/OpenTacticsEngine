// The battle scene's part in a campaign (docs/EVENTS.md "Campaign"): when a chapter's battle is
// decided, the result goes into the playthrough (finishChapterBattle), the Continue save follows the
// new state, and after the usual VICTORY / DEFEAT display the App moves the story on.
#include "battle/battle_scene.h"
#include "campaign/campaign_run.h"
#include "core/base/log.h"

namespace ote {

void BattleScene::chapterDecided() {
  Playthrough& pt = session_.campaign->pt;
  chapterResult_ = finishChapterBattle(session_.db, pt, battle_);
  OTE_INFO("campaign: chapter %d battle over (%s)", pt.chapter + 1,
           chapterResult_ == ChapterResult::Won ? "won" : chapterResult_ == ChapterResult::Retry ? "lost, retry"
                                                          : chapterResult_ == ChapterResult::Ended ? "lost, story ends"
                                                                                                    : "undecided");
  if (!continueOn()) return;
  std::string err;
  bool ok = true;
  if (chapterResult_ == ChapterResult::Won || chapterResult_ == ChapterResult::Retry) {
    // Won: the after scene is due. Retry: the chapter's battle is to start again (no battle in the file,
    // so a closed window does not bring back the lost position).
    ok = writeCampaignSave(store_, 0, session_.db, host_.currentGameId(), pt, nullptr, err);
  } else if (chapterResult_ == ChapterResult::Ended) {
    ok = store_.remove(0, err);  // the story is over: nothing to continue
    continueCleared_ = true;
  }
  if (!ok) saveFailed(err);
}

void BattleScene::leaveResult() {
  if (!chapterBattle()) {
    host_.showMapSelect();  // (a campaign game's page, for its free battles)
    return;
  }
  if (leaving_) return;  // autoplay can ask more than once before the App has switched scenes
  leaving_ = true;
  host_.campaignBattleOver(chapterResult_);
}

}  // namespace ote
