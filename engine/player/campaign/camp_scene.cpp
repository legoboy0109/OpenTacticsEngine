#include "campaign/camp_scene.h"

#include <algorithm>

#include "campaign/campaign_run.h"
#include "campaign/end_scenes.h"
#include "campaign/jobs_scene.h"
#include "core/jobs/job_camp.h"
#include "campaign/story_scene.h"
#include "core/base/log.h"
#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

namespace {
const char* const kContinue = "Continue the story";

void drawWrapped(Renderer& r, const std::string& text, int x, int y, int w, Color c) {
  for (const std::string& line : wrapText(r, text, w)) {
    r.drawText(x, y, line.c_str(), c);
    y += r.lineHeight();
  }
}
}  // namespace

CampScene::CampScene(SceneHost& host, std::vector<CampRow> rows)
    : host_(host), rows_(std::move(rows)), bg_(storyBackground(host)) {
  list_.count = (int)rows_.size() + 1;
}

// A label that does not fit the row's width goes on a second line (narrow screens, large text); all rows then have
// the same height so the list keeps working.
int CampScene::linesPerRow(const Renderer& r) const {
  size_t n = wrapText(r, kContinue, list_.w - 16).size();
  for (const CampRow& row : rows_) n = std::max(n, wrapText(r, row.label, list_.w - 16).size());
  return (int)std::max<size_t>(1, n);
}

void CampScene::layout(const Renderer& r) {
  const int lh = r.lineHeight();
  list_.w = std::min(r.width() - 16, 400);
  list_.x = (r.width() - list_.w) / 2;
  list_.rowH = linesPerRow(r) * lh + 10;
  subtitle_ = wrapText(r, "Conversations waiting", r.width() - 24);
  list_.y = lh + 8 + (int)subtitle_.size() * (lh + 2) + 8;  // below the title and the explanation
  list_.h = std::max(list_.rowH, r.height() - footerHeight(r) - list_.y - 4);
}

void CampScene::choose(int row) {
  if (left_) return;
  left_ = true;
  uiCue(Cue::Confirm);
  if (CampaignRun* run = host_.session()->campaign.get()) {
    if (row < (int)rows_.size() && rows_[(size_t)row].jobs) run->campJobs = true;
    else if (row < (int)rows_.size()) run->campPlay = rows_[(size_t)row].pair;
    else run->campDone = true;
  }
  host_.campaignNext();  // replaces this scene: nothing after this line
}

void CampScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) { list_.sel = row; choose(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return;
    if (e.button == Button::Confirm) choose(list_.sel);
    else if (e.button == Button::Cancel || e.button == Button::Skip) choose((int)rows_.size());  // leave the camp
  }
}

void CampScene::draw(Renderer& r) {
  layout(r);
  r.clear(bg_);
  const int lh = r.lineHeight(), w = r.width();
  drawTextCentered(r, w / 2, 4, fitText(r, "Camp", w - 24), chrome::kTitle);
  for (size_t i = 0; i < subtitle_.size(); ++i)
    drawTextCentered(r, w / 2, 4 + lh + 4 + (int)i * (lh + 2), subtitle_[i], chrome::kDim);
  for (int i = list_.first(); i < list_.count && i < list_.first() + list_.visible(); ++i) {
    int ry = list_.rowY(i);
    bool sel = i == list_.sel, last = i == (int)rows_.size();
    drawRowBox(r, list_.x, ry, list_.w, list_.rowH - 3, sel);
    Color c = sel ? chrome::kTextSel : chrome::kText;
    if (last) {
      drawWrapped(r, kContinue, list_.x + 8, ry + 3, list_.w - 16, c);
      continue;
    }
    const CampRow& row = rows_[(size_t)i];
    // The level the conversation unlocks sits on the right when both fit; on a narrow screen the names win.
    const int extraW = textW(r, row.extra);
    const bool both = textW(r, row.label) + extraW + 24 <= list_.w;
    drawWrapped(r, row.label, list_.x + 8, ry + 3, list_.w - 16, c);
    if (both) r.drawText(list_.x + list_.w - 8 - extraW, ry + 3, row.extra.c_str(), chrome::kDim);
  }
  list_.drawMore(r, chrome::kDim);
  drawFooter(r, "{udok}: choose");
}

std::unique_ptr<Scene> campStep(SceneHost& host) {
  GameSession* s = host.session();
  if (!s || !s->campaign) return nullptr;
  CampaignRun& run = *s->campaign;
  const Database& db = s->db;
  Playthrough& pt = run.pt;
  if (pt.chapter <= 0 || host.args().autoplay) return nullptr;  // the first chapter has no camp before it
  const int chosen = run.campPlay;
  run.campPlay = -1;
  if (chosen >= 0) {
    std::vector<SceneLine> lines;
    if (playSupport(db, pt, chosen, lines) && !lines.empty()) {
      OTE_INFO("camp: playing %s", supportPairName(db, chosen).c_str());
      return std::unique_ptr<Scene>(new StoryScene(host, "", "", lines));  // saves, then the camp comes back
    }
    OTE_WARN("camp: pair %d has no conversation waiting", chosen);
  }
  if (run.campDone) return nullptr;
  if (run.campJobs) return std::unique_ptr<Scene>(new JobsScene(host));  // the Jobs screen was chosen in the camp (docs/JOBS.md)
  std::vector<int> pending;
  pendingSupports(db, pt, pending);
  std::vector<CampRow> rows = campRows(db, pt.supports, pending);
  if (!jobsRosterIds(db, pt).empty()) {  // a game with jobs: one row after the conversations
    CampRow jobs;
    jobs.jobs = true;
    jobs.label = "Jobs";
    jobs.extra = "Change, learn, equip";
    rows.push_back(jobs);
  }
  if (rows.empty()) return nullptr;  // nothing to talk about and no jobs to change: no camp
  std::string log;
  for (const CampRow& r : rows) log += (log.empty() ? "" : ", ") + r.label + " (" + r.extra + ")";
  OTE_INFO("camp: %s", log.c_str());
  return std::unique_ptr<Scene>(new CampScene(host, std::move(rows)));
}

}  // namespace ote
