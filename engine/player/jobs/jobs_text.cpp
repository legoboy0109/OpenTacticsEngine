#include "jobs/jobs_text.h"

#include <algorithm>
#include <cctype>

#include "core/jobs/job_camp.h"
#include "core/jobs/job_rules.h"
#include "ui/text.h"

namespace ote {

namespace {

std::string slotWord(JobSlot s) {
  std::string w = jobSlotName(s);
  w[0] = (char)(w[0] - 'a' + 'A');
  return w;
}

// A reason from the rules ("costs 30 JP, 10 left in Squire") as a sentence.
std::string sentence(std::string why) {
  if (!why.empty()) why[0] = (char)std::toupper((unsigned char)why[0]);
  return why + ".";
}

// What an entry is and does: "Skill, support slot. <description>".
std::string entryDetail(const Database& db, const JobLearnDef& e) {
  const std::string& desc = e.ability ? db.abilities[(size_t)e.ref].description : db.skills[(size_t)e.ref].description;
  return std::string(e.ability ? "Ability" : "Skill") + ", " + jobSlotName(e.slot) + " slot." + (desc.empty() ? "" : " " + desc);
}

}  // namespace

const char* jobsTabName(JobsTab t) {
  switch (t) {
    case JobsTab::Jobs: return "Jobs";
    case JobsTab::Learn: return "Learn";
    case JobsTab::Equip: return "Equip";
  }
  return "";
}

std::string jobsSummary(const Database& db, const RosterUnit& r) {
  if (!r.jobs.on()) return "";
  return db.jobs[(size_t)r.jobs.job].name + "  Lv " + intStr(jobLevelOf(db, r.jobs, r.jobs.job)) + "  JP " + intStr(jobUnspent(r.jobs, r.jobs.job));
}

// "Bonus: +2 Str, -1 Def." for a job that has a "bonus"; "" otherwise.
static std::string jobBonusText(const Database& db, const JobDef& d) {
  std::string out;
  for (size_t s = 0; s < d.bonus.size() && s < db.stats.size(); ++s) {
    if (d.bonus[s] == 0) continue;
    out += (out.empty() ? "" : ", ") + std::string(d.bonus[s] > 0 ? "+" : "") + intStr(d.bonus[s]) + " " + db.stats[s].name;
  }
  return out.empty() ? out : "Bonus: " + out + ".";
}

JobRows jobsTabRows(const Database& db, const RosterUnit& r) {
  JobRows out;
  for (size_t j = 0; j < db.jobs.size(); ++j) {
    const JobDef& d = db.jobs[j];
    PanelRow row;
    row.label = d.name;
    std::string lock;
    const bool unlocked = jobUnlocked(db, r.jobs, (int)j, &lock);
    row.mark = r.jobs.job == (int)j;
    const int level = jobLevelOf(db, r.jobs, (int)j);
    if (row.mark) row.extra = "Current Lv " + intStr(level);
    else if (!unlocked) row.extra = "Locked";
    else row.extra = "Lv " + intStr(level);
    row.dim = !unlocked;
    row.detail = d.description;
    if (!unlocked) row.detail += (row.detail.empty() ? "" : " ") + std::string("Locked: ") + lock + ".";
    else if (!row.mark && !d.overlay()) row.detail += (row.detail.empty() ? "" : " ") + std::string("Plays as a ") + db.unitTypes[(size_t)d.unitType].name + ".";
    if (unlocked && !jobBonusText(db, d).empty()) row.detail += (row.detail.empty() ? "" : " ") + jobBonusText(db, d);  // an overlay job's whole point, and a class job's extra
    out.rows.push_back(row);
    out.ref.push_back((int)j);
  }
  return out;
}

JobRows learnTabRows(const Database& db, const RosterUnit& r) {
  JobRows out;
  for (size_t j = 0; j < db.jobs.size(); ++j) {
    const JobDef& d = db.jobs[j];
    if (d.learn.empty()) continue;
    std::string lock;
    const bool unlocked = jobUnlocked(db, r.jobs, (int)j, &lock);
    PanelRow head;
    head.header = true;
    head.label = d.name + (unlocked ? "  Lv " + intStr(jobLevelOf(db, r.jobs, (int)j)) + "  JP " + intStr(jobUnspent(r.jobs, (int)j)) : std::string("  (locked)"));
    out.rows.push_back(head);
    out.ref.push_back(-1);
    for (const JobLearnDef& e : d.learn) {
      PanelRow row;
      row.label = jobEntryName(db, e.flat) + (e.ability ? " (A)" : "");
      const bool learned = jobHasLearned(r.jobs, e.flat);
      std::string why;
      const bool can = !learned && jobLearnReason(db, r.jobs, e.flat, why);
      row.extra = learned ? "Learned" : intStr(e.cost) + " JP";
      row.mark = learned;
      row.dim = !can;
      row.detail = entryDetail(db, e);
      if (!learned && !can) row.detail += " " + sentence(why);
      out.rows.push_back(row);
      out.ref.push_back(e.flat);
    }
  }
  return out;
}

JobRows equipTabRows(const Database& db, const RosterUnit& r) {
  JobRows out;
  for (int k = 0; k < kJobSlotKinds; ++k) {
    const JobSlot slot = (JobSlot)k;
    std::vector<int> mine;  // learned in other jobs, of this kind
    for (const JobProgress& p : r.jobs.progress)
      for (int flat : p.learned)
        if (db.jobEntries[(size_t)flat].slot == slot && db.jobEntries[(size_t)flat].job != r.jobs.job) mine.push_back(flat);
    const int room = db.rules.jobs.slots[k];
    if (mine.empty()) continue;  // a kind with nothing learned elsewhere has nothing to choose
    PanelRow head;
    head.header = true;
    head.label = slotWord(slot) + "  " + intStr(jobSlotUsed(db, r.jobs, slot)) + "/" + intStr(room);
    out.rows.push_back(head);
    out.ref.push_back(-1);
    for (int flat : mine) {
      const JobLearnDef& e = db.jobEntries[(size_t)flat];
      PanelRow row;
      row.label = jobEntryName(db, flat) + (e.ability ? " (A)" : "");
      const bool on = std::find(r.jobs.equipped.begin(), r.jobs.equipped.end(), flat) != r.jobs.equipped.end();
      std::string why;
      const bool can = on || jobEquipReason(db, r.jobs, flat, why);
      row.extra = on ? "Equipped" : db.jobs[(size_t)e.job].name;
      row.mark = on;
      row.dim = !can;
      row.detail = entryDetail(db, e);
      if (!on && !can) row.detail += " " + sentence(why);
      out.rows.push_back(row);
      out.ref.push_back(flat);
    }
  }
  return out;
}

std::vector<PanelRow> jobsWhoRows(const Database& db, const Playthrough& pt, std::vector<std::string>& ids) {
  std::vector<PanelRow> rows;
  ids.clear();
  for (const RosterUnit& r : pt.roster) {
    if (!jobsRosterUses(db, r)) continue;
    PanelRow row;
    row.label = r.name;
    row.extra = db.jobs[(size_t)r.jobs.job].name + " " + intStr(jobLevelOf(db, r.jobs, r.jobs.job));
    row.detail = jobsSummary(db, r) + ". " + (r.jobs.equipped.empty() ? std::string("Nothing equipped.") : intStr((int)r.jobs.equipped.size()) + " equipped.");
    rows.push_back(row);
    ids.push_back(r.id);
  }
  return rows;
}

}  // namespace ote
