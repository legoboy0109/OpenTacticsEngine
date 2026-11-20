// The Jobs screen (docs/JOBS.md "Player UI"): who, then one character's Jobs / Learn / Equip tabs, all in the item panel the other list screens use. It works on a
// Playthrough's roster through core/jobs/job_camp.h and has no scene of its own, so the camp (campaign/camp_jobs.cpp) and the preparation screen both host it.
#pragma once
#include <string>
#include <vector>

#include "core/campaign/playthrough.h"
#include "jobs/jobs_text.h"
#include "ui/item_panel.h"

namespace ote {

class JobsScreen {
 public:
  enum Result { None, Closed };
  // `prepMap`: the preparation screen's map (null in the camp): a deployed character may only take a job whose unit type can stand on its slot.
  JobsScreen(const Database& db, Playthrough& pt, const MapDef* prepMap);
  void setArea(int x, int y, int w, int h) { panel_.setArea(x, y, w, h); }
  Result onInput(const InputEvent& e, const Renderer& r);
  void draw(Renderer& r);
  // True once after the roster changed (a job taken, something learned or equipped): the host saves and, in the preparation, rebuilds its battle.
  bool takeChanged();
  // Which list is open now (for logs and tests): "who" or the tab name; the character being edited ("" in the list).
  std::string where() const;
  const std::string& who() const { return who_; }

 private:
  void openWho(int sel);
  void openDetail(int sel);
  void act(int row);
  const RosterUnit* unit() const { return pt_.findUnit(who_); }
  const Database& db_;
  Playthrough& pt_;
  const MapDef* prepMap_;
  ItemPanel panel_;
  bool detail_ = false;
  JobsTab tab_ = JobsTab::Jobs;
  std::string who_;
  std::vector<std::string> ids_;  // the character list's ids
  std::vector<int> ref_;          // the open tab's per-row references (JobRows::ref)
  std::string message_;           // the last result or refusal, shown instead of the summary until the next input
  bool changed_ = false;
};

}  // namespace ote
