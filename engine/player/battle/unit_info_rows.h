// What the Unit details pages of step U1 (Stats, Skills, Items) show, as plain rows, plus the drawing helpers they
// share. The rows are built from the Battle with no renderer, so a unit test can read them; the page files only copy the
// rows into their draw function (a page must not hold on to the Battle, see unit_info.h).
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "platform/platform.h"
#include "ui/item_icon.h"

namespace ote {

// "Strength   9 (+2)": label left, value, a small note after the value (what skills, items and partners add).
struct InfoRow {
  std::string label, value, note;
  Color valueColor, noteColor;
  // A stat cap (docs/SRPG.md section 7): "cap 12" below the cap, "MAX" at it; empty = no cap.
  std::string cap = "";
  Color capColor = Color(150, 158, 190);
};
std::vector<InfoRow> statRows(const Battle& b, const Unit& u);

// Where a skill on the unit comes from (the first place in the order of Battle::skillsOf).
enum class SkillSource { Class, Learned, Personal, Item, Weapon, Side, Rank, Job };
const char* skillSourceName(SkillSource s);  // "Class", "Learned", "Personal", "Item", "Weapon", "Side" (a side-wide modifier, e.g. research), "Rank" (the unit's rank table entry), "Job" (learned in its job or equipped from another: docs/JOBS.md)
struct SkillRow {
  int skill = -1;
  std::string name, description;
  SkillSource source = SkillSource::Class;
};
std::vector<SkillRow> skillRows(const Battle& b, const Unit& u);

struct ItemRow {
  std::string name, uses, detail;
  bool equipped = false;  // the weapon in hand (carried weapons)
  IconRef pic;            // the item's icon (graphics.json "icons"); empty = none
  std::string blocked;    // "Needs rank B" / "Only Kell can wield this": a weapon the unit may not wield now (shown greyed; also in `detail`)
};
std::vector<ItemRow> itemRows(const Battle& b, const Unit& u);

// Drawing inside the page box (x, y, w, h); lineH is the height of a text row. Entries that do not fit the height are
// replaced by a last line "+N more".
void drawStatRows(Renderer& r, int x, int y, int w, int h, int lineH, const std::vector<InfoRow>& rows);
void drawSkillRows(Renderer& r, int x, int y, int w, int h, int lineH, const std::vector<SkillRow>& rows);
void drawItemRows(Renderer& r, int x, int y, int w, int h, int lineH, const std::vector<ItemRow>& rows);

}  // namespace ote
