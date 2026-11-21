// The rows of the Unit details pages (step U1): built from the Battle (no renderer needed) and drawn.
#include "battle/unit_info_rows.h"

#include <algorithm>

#include "battle/item_text.h"
#include "battle/rank_text.h"
#include "battle/unit_info_rank.h"
#include "core/jobs/job_rules.h"
#include "ui/text.h"

namespace ote {
namespace {

bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

const Color kText(228, 230, 240), kLabel(150, 158, 190), kSmall(128, 136, 170), kName(255, 235, 130);
const Color kUp(130, 235, 150), kDown(255, 140, 120), kMax(255, 225, 110), kGreyed(110, 114, 140);

void more(Renderer& r, int x, int y, size_t n) { r.drawText(x, y, ("+" + intStr((int)n) + " more").c_str(), kLabel); }

}  // namespace

const char* skillSourceName(SkillSource s) {
  switch (s) {
    case SkillSource::Class: return "Class";
    case SkillSource::Learned: return "Learned";
    case SkillSource::Personal: return "Personal";
    case SkillSource::Item: return "Item";
    case SkillSource::Weapon: return "Weapon";
    case SkillSource::Side: return "Side";
    case SkillSource::Rank: return "Rank";
    case SkillSource::Job: return "Job";
  }
  return "";
}

// Does the unit's rank give it this skill? (skillsOf puts these after the character's skills, before the side's.)
static bool rankGives(const Battle& b, const Unit& u, int skill) {
  const int rank = b.unitRank(u);
  if (rank < 0) return false;
  for (int s : b.db->rules.unitRanks.levels[(size_t)rank].skills)
    if (s == skill) return true;
  return false;
}

// Does one of the unit's side-wide modifiers give it this skill? (skillsOf puts these after the character's skills.)
static bool sideGives(const Battle& b, const Unit& u, int skill) {
  if (u.side < 0 || (size_t)u.side >= b.sides.size()) return false;
  for (const SideModifier& m : b.sides[(size_t)u.side].modifiers)
    if (m.skill == skill && b.modifierApplies(m, u)) return true;
  return false;
}

std::vector<SkillRow> skillRows(const Battle& b, const Unit& u) {
  std::vector<int> list;
  b.skillsOf(u, -1, list);
  const Database& db = *b.db;
  std::vector<SkillRow> out;
  for (int s : list) {
    if (db.skills[(size_t)s].hidden) continue;  // a condition's blocks (docs/SRPG.md section 9) are shown on the Status page
    SkillRow row;
    row.skill = s;
    row.name = db.skills[(size_t)s].name;
    row.description = db.skills[(size_t)s].description;
    // The same order skillsOf uses, so a skill that comes from two places is labelled with the first.
    if (has(b.typeOf(u).skills, s)) row.source = SkillSource::Class;
    else if (has(u.learned, s)) row.source = SkillSource::Learned;
    else if (u.character >= 0 && (size_t)u.character < db.characters.size() &&
             has(db.characters[(size_t)u.character].skills, s))
      row.source = SkillSource::Personal;
    else if (rankGives(b, u, s)) row.source = SkillSource::Rank;
    else if (b.jobGivesSkill(u, s)) row.source = SkillSource::Job;  // the same place skillsOf puts a job's skills: after the rank's
    else if (sideGives(b, u, s)) row.source = SkillSource::Side;
    else {
      bool item = false;
      for (const ItemStack& st : u.items) {
        const ItemDef& d = b.itemOf(st);
        item |= d.kind == ItemKind::Accessory && has(d.skills, s);
      }
      row.source = item ? SkillSource::Item : SkillSource::Weapon;
    }
    out.push_back(row);
  }
  return out;
}

std::vector<InfoRow> statRows(const Battle& b, const Unit& u) {
  const Database& db = *b.db;
  std::vector<InfoRow> rows;
  rows.push_back({"Class", b.typeOf(u).name, "", kText, kLabel});
  if (b.jobsGame() && u.jobs.on()) {  // docs/JOBS.md: "Squire  Lv 2  JP 40" (JP left to spend in this job)
    const int left = jobUnspent(u.jobs, u.jobs.job);
    rows.push_back({"Job", b.db->jobs[(size_t)u.jobs.job].name + "  Lv " + intStr(b.unitJobLevel(u)), "JP " + intStr(left), kText, kSmall});
  }
  if (db.rules.progression && db.rules.unitRanks.replacesLevels())  // docs/SRPG.md "Unit ranks": the rank and the experience towards the next, instead of a level
    rows.push_back({"Rank", unitRankName(b, u) + "  " + unitRankXpText(b, u), "", kText, kLabel});
  else if (db.rules.progression)
    rows.push_back({"Level", intStr(u.level) + "  XP " + intStr(u.xp) + "/" + intStr(db.rules.xpPerLevel), "", kText, kLabel});
  // "rules.ranks": the rank beside the level, with its own counter ("Veteran  Merit 10/30").
  // (short words: this row shares the page with Job and Level at UI scale 150 on the 480x270 canvas)
  if (db.rules.unitRanks.merit) rows.push_back({"Rank", b.unitRank(u) < 0 ? std::string("None") : unitRankName(b, u), unitRankXpText(b, u), kText, kSmall});
  // A unit type with its own top level (units.json "maxLevel") says so; a game without one shows no extra row.
  if (db.rules.progression && b.typeOf(u).maxLevel >= 0)
    rows.push_back({"Level cap", b.typeOf(u).maxLevel == 0 ? std::string("never levels") : intStr(b.typeOf(u).maxLevel), "", kText, kLabel});
  for (size_t s = 0; s < db.stats.size(); ++s) {
    const int own = u.stats[s], now = b.stat(u, (int)s);
    std::string value = intStr(now);
    if ((int)s == db.rules.statHp) value = intStr(u.hp) + "/" + intStr(now);
    InfoRow row{db.stats[s].name, value, "", kText, kLabel};
    if (now != own) {  // what skills, items and partners add or take, in colour
      row.note = "(" + std::string(now > own ? "+" : "") + intStr(now - own) + ")";
      row.noteColor = row.valueColor = now > own ? kUp : kDown;
    }
    const int cap = b.statCap(u, (int)s);  // the highest the permanent stat can grow to (docs/SRPG.md section 7)
    if (cap >= 0) {
      const bool atCap = own >= cap;
      row.cap = atCap ? "MAX" : "cap " + intStr(cap);
      row.capColor = atCap ? kMax : kSmall;
    }
    rows.push_back(row);
  }
  for (const InfoRow& r : rankRows(b, u)) rows.push_back(r);  // weapon ranks: under the stats
  // Damage types (docs/SRPG.md "Damage types"): the non-zero resistances, signed; a game without types has none of these rows.
  for (size_t t = 0; t < db.damageTypes.list.size(); ++t) {
    const int rs = b.resistOf(u, (int)t);
    if (rs == 0) continue;
    InfoRow row{db.damageTypes.list[t].name + " res", std::string(rs > 0 ? "+" : "") + intStr(rs), "", rs > 0 ? kUp : kDown, kLabel};
    rows.push_back(row);
  }
  return rows;
}

std::vector<ItemRow> itemRows(const Battle& b, const Unit& u) {
  std::vector<ItemRow> out;
  const int eq = b.db->rules.items.carriedWeapons ? b.equippedSlot(u) : -1;
  for (size_t i = 0; i < u.items.size(); ++i) {
    ItemRow row;
    row.name = b.itemOf(u.items[i]).name;
    row.pic = itemicon::ofItem(*b.db, u.items[i].item);
    row.uses = usesText(b, u.items[i]);
    row.detail = itemDetail(b, u.items[i]);
    row.equipped = (int)i == eq;
    row.blocked = weaponBlockReason(b, u, u.items[i]);
    if (!row.blocked.empty()) row.detail = row.blocked;
    out.push_back(row);
  }
  return out;
}

// Stats flow into as many columns (up to three) as the height needs: a game with many stats on a small window.
void drawStatRows(Renderer& r, int x, int y, int w, int h, int lineH, const std::vector<InfoRow>& rows) {
  const int rh = lineH + 2, perCol = std::max(1, h / rh);
  const int cols = std::max(1, std::min(3, ((int)rows.size() + perCol - 1) / perCol));
  const int gap = 10, cw = (w - gap * (cols - 1)) / cols;
  for (size_t i = 0; i < rows.size(); ++i) {
    const int col = (int)i / perCol;
    if (col >= cols) { more(r, x + (cols - 1) * (cw + gap), y + (perCol - 1) * rh, rows.size() - i + 1); break; }
    const InfoRow& row = rows[i];
    const int cx = x + col * (cw + gap), cy = y + ((int)i % perCol) * rh;
    const std::string cap = row.cap.empty() ? "" : " " + row.cap;
    const std::string note = row.note.empty() ? "" : " " + row.note;
    const int kw = textW(r, cap), nw = textW(r, note);
    const std::string v = fitText(r, row.value, cw * 3 / 5);
    const int vw = textW(r, v);
    r.drawText(cx, cy, fitText(r, row.label, cw - vw - nw - kw - 6).c_str(), kLabel);
    r.drawText(cx + cw - vw - nw - kw, cy, v.c_str(), row.valueColor);
    if (!note.empty()) r.drawText(cx + cw - nw - kw, cy, note.c_str(), row.noteColor);
    if (!cap.empty()) r.drawText(cx + cw - kw, cy, cap.c_str(), row.capColor);
  }
}

// One entry: the name with its small-print source on the right, then the description wrapped under it.
void drawSkillRows(Renderer& r, int x, int y, int w, int h, int lineH, const std::vector<SkillRow>& rows) {
  if (rows.empty()) { r.drawText(x, y, "No skills", kLabel); return; }
  const int rh = lineH + 2;
  int cy = y;
  for (size_t i = 0; i < rows.size(); ++i) {
    const SkillRow& s = rows[i];
    std::vector<std::string> desc;
    if (!s.description.empty()) desc = wrapText(r, s.description, w - 6);
    int need = rh * (1 + (int)desc.size());
    // Keep a line for "+N more" unless this is the last entry; shorten the description (to at least one line) to fit.
    const int room = y + h - cy - (i + 1 == rows.size() ? 0 : rh);
    while (need > room && desc.size() > 1) { desc.pop_back(); need -= rh; }
    if (need > room) { more(r, x, cy, rows.size() - i); return; }
    const std::string src = skillSourceName(s.source);
    const int sw = textW(r, src);
    r.drawText(x, cy, fitText(r, s.name, w - sw - 8).c_str(), kName);
    r.drawText(x + w - sw, cy, src.c_str(), kSmall);
    for (size_t k = 0; k < desc.size(); ++k) r.drawText(x + 6, cy + rh * (1 + (int)k), desc[k].c_str(), kLabel);
    cy += need + 3;
  }
}

void drawItemRows(Renderer& r, int x, int y, int w, int h, int lineH, const std::vector<ItemRow>& rows) {
  if (rows.empty()) { r.drawText(x, y, "No items", kLabel); return; }
  const int rh = lineH + 2, sq = std::max(3, lineH / 3);
  IconColumn icons;  // one column for the whole list; the name line grows only when an icon is taller than it
  for (const ItemRow& it : rows) icons.add(it.pic);
  const int nameRow = std::max(rh, icons.h), off = (nameRow - rh) / 2;  // (the text keeps its place unless an icon made the rows taller)
  int cy = y;
  for (size_t i = 0; i < rows.size(); ++i) {
    const ItemRow& it = rows[i];
    const int need = nameRow + (it.detail.empty() ? 0 : rh), room = y + h - cy - (i + 1 == rows.size() ? 0 : rh);
    if (need > room) { more(r, x, cy, rows.size() - i); return; }
    if (it.equipped) r.fillRect(x, cy + (lineH - sq) / 2 + off, sq, sq, Color(255, 220, 90));
    const bool blocked = !it.blocked.empty();  // greyed, with the reason in place of the description
    itemicon::drawInRow(r, it.pic, x + sq + 4, cy, nameRow, blocked);
    const int tx = x + sq + 4 + icons.indent(), tw = w - sq - 4 - icons.indent(), uw = textW(r, it.uses);
    r.drawText(tx, cy + off, fitText(r, it.name, tw - uw - 8).c_str(), blocked ? kGreyed : kText);
    if (!it.uses.empty()) r.drawText(x + w - uw, cy + off, it.uses.c_str(), kSmall);
    if (!it.detail.empty()) r.drawText(tx + 6, cy + nameRow, fitText(r, it.detail, tw - 6).c_str(), blocked ? kDown : kLabel);
    cy += need + 3;
  }
}

}  // namespace ote
