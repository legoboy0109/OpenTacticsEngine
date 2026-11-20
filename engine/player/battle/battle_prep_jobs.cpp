// The preparation screen's Jobs entry (docs/JOBS.md "Player UI"; only where rules.jobs.changeIn is "camp+prep"): hosts the same JobsScreen the camp uses. Every change
// rebuilds the battle from the roster (prepChanged), so the map, the info panel and the battle that begins all show the new job.
#include "battle/battle_prep_ui.h"
#include "core/base/log.h"
#include "core/jobs/job_camp.h"
#include "jobs/jobs_screen.h"

namespace ote {

void BattleScene::prepJobsOpen() {
  PrepUi& p = *prep_;
  p.screen = PrepUi::Screen::Jobs;
  menu_.close();
  itemPanel_.close();
  p.jobs.reset(new JobsScreen(session_.db, *p.pt, &p.map));
  OTE_INFO("prep: jobs screen opens");
}

void BattleScene::prepJobsInput(const InputEvent& e) {
  PrepUi& p = *prep_;
  if (!p.jobs) {
    prepMainMenu();
    return;
  }
  p.jobs->setArea(0, 0, layout_.vw + layout_.panelW, layout_.vh);  // the whole width, over the info panel too: the three tabs and the name need it at UI scale 150
  const JobsScreen::Result res = p.jobs->onInput(e, host_.renderer());
  if (p.jobs->takeChanged()) prepChanged();
  if (res == JobsScreen::Closed) {
    p.jobs.reset();
    prepMainMenu();
  }
}

void BattleScene::prepJobsDraw(Renderer& r) {
  PrepUi& p = *prep_;
  if (!p.jobs) return;
  p.jobs->setArea(0, 0, layout_.vw + layout_.panelW, layout_.vh);  // the whole width, over the info panel too: the three tabs and the name need it at UI scale 150
  p.jobs->draw(r);
}

}  // namespace ote
