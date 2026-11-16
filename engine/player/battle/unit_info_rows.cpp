// The rows of the Unit details pages (step U1): built from the Battle (no renderer needed) and drawn.
#include "battle/unit_info_rows.h"

#include <algorithm>

#include "battle/item_text.h"
#include "ui/text.h"

namespace ote {
namespace {

bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

const Color kText(228, 230, 240), kLabel(150, 158, 190), kSmall(128, 136, 170), kName(255, 235, 130);
const Color kUp(130, 235, 150), kDown(255, 140, 120);

void more(Renderer& r, int x, int y, size_t n) { r.drawText(x, y, ("+" + intStr((int)n) + " more").c_str(), kLabel); }

}  // namespace

const char* skillSourceName(SkillSource s) {
  switch (s) {
    case SkillSource::Class: return "Class";
    case SkillSource::Learned: return "Learned";
    case SkillSource::Personal: return "Personal";
    case SkillSource::Item: return "Item";
    case SkillSource::Weapon: return "Weapon";
  }
  return "";
}

std::vector<SkillRow> skillRows(const Battle& b, const Unit& u) {
  std::vector<int> list;
  b.skillsOf(u, -1, list);
  const Database& db = *b.db;
  std::vector<SkillRow> out;
  for (int s : list) {
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
  if (db.rules.progression)
    rows.push_back({"Level", intStr(u.level) + "  XP " + intStr(u.xp) + "/" + intStr(db.rules.xpPerLevel), "", kText, kLabel});
  for (size_t s = 0; s < db.stats.size(); ++s) {
    const int own = u.stats[s], now = b.stat(u, (int)s);
    std::string value = intStr(now);
    if ((int)s == db.rules.statHp) value = intStr(u.hp) + "/" + intStr(now);
    InfoRow row{db.stats[s].name, value, "", kText, kLabel};
    if (now != own) {  // what skills, items and partners add or take, in colour
      row.note = "(" + std::string(now > own ? "+" : "") + intStr(now - own) + ")";
      row.noteColor = row.valueColor = now > own ? kUp : kDown;
    }
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
    row.uses = usesText(b, u.items[i]);
    row.detail = itemDetail(b, u.items[i]);
    row.equipped = (int)i == eq;
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
    const std::string note = row.note.empty() ? "" : " " + row.note;
    const int nw = textW(r, note);
    const std::string v = fitText(r, row.value, cw * 3 / 5);
    const int vw = textW(r, v);
    r.drawText(cx, cy, fitText(r, row.label, cw - vw - nw - 6).c_str(), kLabel);
    r.drawText(cx + cw - vw - nw, cy, v.c_str(), row.valueColor);
    if (!note.empty()) r.drawText(cx + cw - nw, cy, note.c_str(), row.noteColor);
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
  int cy = y;
  for (size_t i = 0; i < rows.size(); ++i) {
    const ItemRow& it = rows[i];
    const int need = rh * (it.detail.empty() ? 1 : 2), room = y + h - cy - (i + 1 == rows.size() ? 0 : rh);
    if (need > room) { more(r, x, cy, rows.size() - i); return; }
    if (it.equipped) r.fillRect(x, cy + (lineH - sq) / 2, sq, sq, Color(255, 220, 90));
    const int tx = x + sq + 4, tw = w - sq - 4, uw = textW(r, it.uses);
    r.drawText(tx, cy, fitText(r, it.name, tw - uw - 8).c_str(), kText);
    if (!it.uses.empty()) r.drawText(x + w - uw, cy, it.uses.c_str(), kSmall);
    if (!it.detail.empty()) r.drawText(tx + 6, cy + rh, fitText(r, it.detail, tw - 6).c_str(), kLabel);
    cy += need + 3;
  }
}

}  // namespace ote
