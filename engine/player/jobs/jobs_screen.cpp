#include "jobs/jobs_screen.h"

#include <algorithm>
#include <cctype>

#include "core/base/log.h"
#include "core/jobs/job_camp.h"
#include "core/jobs/job_rules.h"
#include "sound/cue_sink.h"
#include "ui/text.h"

namespace ote {

JobsScreen::JobsScreen(const Database& db, Playthrough& pt, const MapDef* prepMap) : db_(db), pt_(pt), prepMap_(prepMap) {
  panel_.setDetailLines(3);  // "Locked: needs Squire level 2. Plays as a Knight." wants the room on a narrow screen
  panel_.setMaxWidth(400);   // the three tabs share the title row with the name: at UI scale 150 the usual width leaves the name no room
  openWho(0);
}

bool JobsScreen::takeChanged() {
  const bool c = changed_;
  changed_ = false;
  return c;
}

std::string JobsScreen::where() const { return detail_ ? jobsTabName(tab_) : "who"; }

void JobsScreen::openWho(int sel) {
  detail_ = false;
  who_.clear();
  message_.clear();
  const std::vector<PanelRow> rows = jobsWhoRows(db_, pt_, ids_);
  panel_.open("Jobs", "", rows, "Nobody here uses jobs", sel);
  panel_.setTabs({}, 0);
  std::string log;
  for (const std::string& id : ids_) log += (log.empty() ? "" : ", ") + id;
  OTE_INFO("jobs who: %s", log.c_str());
}

void JobsScreen::openDetail(int sel) {
  const RosterUnit* r = unit();
  if (!r) {
    openWho(0);
    return;
  }
  detail_ = true;
  JobRows rows;
  const char* empty = "";
  switch (tab_) {
    case JobsTab::Jobs: rows = jobsTabRows(db_, *r); break;
    case JobsTab::Learn: rows = learnTabRows(db_, *r); empty = "Nothing to learn"; break;
    case JobsTab::Equip: rows = equipTabRows(db_, *r); empty = "Nothing learned in other jobs yet"; break;
  }
  ref_ = rows.ref;
  panel_.open(r->name, message_.empty() ? jobsSummary(db_, *r) : message_, rows.rows, empty, sel);
  panel_.setTabs({jobsTabName(JobsTab::Jobs), jobsTabName(JobsTab::Learn), jobsTabName(JobsTab::Equip)}, (int)tab_);
  std::string log;
  for (const PanelRow& row : rows.rows) log += (log.empty() ? "" : ", ") + row.label + (row.header ? ":" : "[" + row.extra + "]");
  OTE_INFO("jobs %s %s: %s", jobsTabName(tab_), who_.c_str(), log.c_str());
}

// Does what the chosen row of the open tab says; a refusal says why in the head line.
void JobsScreen::act(int row) {
  if (row < 0 || row >= (int)ref_.size() || ref_[(size_t)row] < 0) return;
  const int ref = ref_[(size_t)row];
  std::string why;
  bool ok = false;
  switch (tab_) {
    case JobsTab::Jobs:
      ok = prepMap_ ? jobsChangeInPrep(db_, pt_, *prepMap_, who_, ref, why) : jobsChange(db_, pt_, who_, ref, why);
      if (ok) message_ = "Now a " + db_.jobs[(size_t)ref].name + ".";
      break;
    case JobsTab::Learn:
      ok = jobsLearn(db_, pt_, who_, ref, why);
      if (ok) message_ = "Learned " + jobEntryName(db_, ref) + ".";
      break;
    case JobsTab::Equip: {
      const RosterUnit* r = unit();
      const bool on = r && std::find(r->jobs.equipped.begin(), r->jobs.equipped.end(), ref) != r->jobs.equipped.end();
      ok = on ? jobsUnequip(db_, pt_, who_, ref, why) : jobsEquip(db_, pt_, who_, ref, why);
      if (ok) message_ = (on ? "Took off " : "Equipped ") + jobEntryName(db_, ref) + ".";
      break;
    }
  }
  if (!ok) {
    uiCue(Cue::Error);
    message_ = why.empty() ? std::string("Not possible.") : why + ".";
    message_[0] = (char)std::toupper((unsigned char)message_[0]);
    OTE_INFO("jobs refused: %s", message_.c_str());
  } else {
    uiCue(Cue::Confirm);
    changed_ = true;
    OTE_INFO("jobs: %s (%s)", message_.c_str(), who_.c_str());
  }
  openDetail(panel_.sel());
}

JobsScreen::Result JobsScreen::onInput(const InputEvent& e, const Renderer& r) {
  const ItemPanel::Result res = panel_.onInput(e, r);
  if (res == ItemPanel::None) return None;
  if (!detail_) {
    if (res == ItemPanel::Back) return Closed;
    if (res == ItemPanel::Picked) {
      const int row = panel_.sel();
      if (row < 0 || row >= (int)ids_.size()) return None;
      uiCue(Cue::Select);
      who_ = ids_[(size_t)row];
      tab_ = JobsTab::Jobs;
      message_.clear();
      openDetail(0);
    }
    return None;
  }
  if (res == ItemPanel::Back) {
    uiCue(Cue::Cancel);
    int at = 0;
    for (size_t i = 0; i < ids_.size(); ++i)
      if (ids_[i] == who_) at = (int)i;
    openWho(at);
    return None;
  }
  if (res == ItemPanel::Tab) {
    tab_ = (JobsTab)panel_.tab();
    message_.clear();
    openDetail(0);
    return None;
  }
  message_.clear();
  act(panel_.sel());
  return None;
}

void JobsScreen::draw(Renderer& r) { panel_.draw(r); }

}  // namespace ote
