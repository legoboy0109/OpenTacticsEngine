#include "campaign/jobs_scene.h"

#include "campaign/campaign_run.h"
#include "campaign/end_scenes.h"
#include "core/base/log.h"

namespace ote {

JobsScene::JobsScene(SceneHost& host) : host_(host), bg_(storyBackground(host)) {
  CampaignRun* run = host_.session()->campaign.get();
  screen_.reset(new JobsScreen(host_.session()->db, run->pt, nullptr));
}

// The roster is part of the Continue save, so a job taken a moment before the app is closed is still taken. Failure is only logged.
void JobsScene::saveNow() {
  GameSession* s = host_.session();
  if (!s || !s->campaign || host_.args().autoplay) return;
  SaveStore store = host_.saveStore(host_.currentGameId());
  if (!store.enabled() || s->db.saving == SavingMode::Off) return;
  std::string err;
  if (!writeCampaignSave(store, 0, s->db, host_.currentGameId(), s->campaign->pt, nullptr, err))
    OTE_WARN("jobs: saving after a change failed: %s", err.c_str());
}

void JobsScene::onInput(const InputEvent& e) {
  if (left_) return;
  const JobsScreen::Result res = screen_->onInput(e, host_.renderer());
  if (screen_->takeChanged()) saveNow();
  if (res != JobsScreen::Closed) return;
  left_ = true;
  if (CampaignRun* run = host_.session()->campaign.get()) run->campJobs = false;
  host_.campaignSceneDone();  // replaces this scene (and saves): nothing after this line
}

void JobsScene::draw(Renderer& r) {
  r.clear(bg_);
  screen_->setArea(0, 0, r.width(), r.height());
  screen_->draw(r);
}

}  // namespace ote
