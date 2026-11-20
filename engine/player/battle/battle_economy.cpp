// The player's part of docs/ECONOMY.md "Player" for upgrades, research and supply: the menu of an own structure
// (Build / Upgrade), the Upgrade list, the research panel (opened from the battle menu, battle_menus.cpp), the
// Supply highlight and the log line for
// units with fuel or ammunition. All changes go through submit(), like every other action, so the view animates
// the events and the Continue save is written. Wording: economy_text.h.
#include "battle/battle_scene.h"

#include <algorithm>

#include "battle/economy_text.h"
#include "battle/item_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

// What an own structure offers. A lone entry opens its screen at once (a factory with nothing else to do shows
// the build menu directly, as it always did); two or more open the structure menu first.
bool BattleScene::openStructureAt(Cell cell) {
  const int si = battle_.grid.valid(cell) ? battle_.structureAt[(size_t)cell] : -1;
  if (si < 0 || battle_.structures[(size_t)si].owner != battle_.activeSide || battle_.over) return false;
  const StructureDef& def = battle_.defOf(battle_.structures[(size_t)si]);
  std::vector<int> types;
  battle_.buildOptions(cell, types, true);  // locked unit types too: they are shown greyed with what they need
  const bool build = !types.empty(), upgrade = !def.upgrades.empty();
  const int count = (int)build + (int)upgrade;  // research is in the battle menu, not here
  if (count == 0) return false;
  buildCell_ = cell;
  structureMenuBack_ = count > 1;
  if (count > 1) openStructureMenu(cell);
  else if (build) openBuildMenu(cell);
  else openUpgradeMenu(si);
  return true;
}

void BattleScene::openStructureMenu(Cell cell) {
  mode_ = Mode::StructureMenu;
  buildCell_ = cell;
  const int si = battle_.structureAt[(size_t)cell];
  const StructureDef& def = battle_.defOf(battle_.structures[(size_t)si]);
  std::vector<int> types;
  battle_.buildOptions(cell, types, true);
  std::vector<MenuItem> items;
  std::string log;
  auto add = [&](const char* label, int id) {
    items.push_back({label, "", true, id});
    log += (log.empty() ? "" : ", ") + std::string(label);
  };
  if (!types.empty()) add(EconomyWords::kBuild, kBuildEntry);
  if (!def.upgrades.empty()) add(EconomyWords::kUpgrade, kUpgradeEntry);
  OTE_INFO("structure menu: %s", log.c_str());
  int sx, sy, bw, bh;
  cellScreenBox(cell, sx, sy, bw, bh);
  menu_.open(host_.renderer(), def.name, items, sx + bw + 2, sy, layout_.vw, layout_.vh);
  menu_.reanchor(sx + bw + 2, sy, sx - 2, layout_.vw, layout_.vh);
}

void BattleScene::openUpgradeMenu(int structure) {
  mode_ = Mode::UpgradeMenu;
  upgradeStructure_ = structure;
  const Database& db = session_.db;
  const StructureState& st = battle_.structures[(size_t)structure];
  std::vector<UpgradeOption> opts;
  battle_.upgradeOptions(structure, opts);
  std::vector<MenuItem> items;
  std::string log;
  for (const UpgradeOption& o : opts) {
    MenuItem it;
    it.label = db.structures[(size_t)o.to].name;
    std::string cost = priceText(db, o.def->cost);
    // A greyed entry says why in the place of the price: a disabled row cannot be selected, so a hint line could not.
    it.extra = o.ok ? (cost.empty() ? std::string(EconomyWords::kFree) : cost) : o.why;
    it.enabled = o.ok;
    it.id = o.to;
    items.push_back(it);
    log += (log.empty() ? "" : "; ") + it.label + " (" + it.extra + ")" + (o.ok ? "" : " [greyed]");
  }
  OTE_INFO("upgrade menu: %s", log.c_str());
  int sx, sy, bw, bh;
  cellScreenBox(battle_.grid.at(st.x, st.y), sx, sy, bw, bh);
  menu_.open(host_.renderer(), std::string(EconomyWords::kUpgrade) + " " + battle_.defOf(st).name, items, sx + bw + 2, sy,
             layout_.vw, layout_.vh);
  menu_.reanchor(sx + bw + 2, sy, sx - 2, layout_.vw, layout_.vh);
}

void BattleScene::openResearchPanel() {
  mode_ = Mode::Research;
  menu_.close();
  const Database& db = session_.db;
  const int side = battle_.activeSide;
  std::vector<PanelRow> rows = researchRows(battle_, side, techRows_);
  std::string head;
  for (size_t i = 0; i < db.resources.size(); ++i)
    head += (head.empty() ? "" : "  ") + db.resources[i].name + " " + intStr(battle_.sides[(size_t)side].resources[i]);
  researchPanel_.setArea(0, 0, layout_.vw < 300 ? layout_.W : layout_.vw, layout_.vh);  // narrow (UI scale 150): use the whole screen width
  // "1000 Funds, 2 turns" next to a long name; a bigger font (UI scale 150) takes the whole width, and one explanation line less so more rows show.
  researchPanel_.setMaxWidth(layout_.vw < 300 ? layout_.W - 8 : 340);
  researchPanel_.setDetailLines(layout_.lh > 9 ? 3 : 4);  // description + what it gives + the reason it is greyed
  researchPanel_.open(EconomyWords::kResearch, head, rows, EconomyWords::kNoTechnologies);
  std::string log;
  for (const PanelRow& r : rows) log += (log.empty() ? "" : "; ") + rowLogText(r);
  OTE_INFO("research panel: %s", log.c_str());
  std::vector<ResearchOption> opts;
  battle_.researchOptions(side, opts);
  if (!battle_.canResearch(side) && !opts.empty()) OTE_INFO("research reason: %s", opts[0].why.c_str());  // needsStructure and none owned
  std::string why;  // why each greyed row is greyed (the panel shows it under the list when the row is selected)
  for (const ResearchOption& o : opts)
    if (!o.ok) why += (why.empty() ? "" : "; ") + db.research[(size_t)o.tech].name + ": " + o.why;
  if (!why.empty()) OTE_INFO("research reasons: %s", why.c_str());
}

void BattleScene::backToStructure() {
  menu_.close();
  if (structureMenuBack_) openStructureMenu(buildCell_);
  else mode_ = Mode::Idle;
}

void BattleScene::economyMenuChosen(int id) {
  if (mode_ == Mode::StructureMenu) {
    const int si = battle_.structureAt[(size_t)buildCell_];
    if (id == kBuildEntry) openBuildMenu(buildCell_);
    else if (id == kUpgradeEntry) openUpgradeMenu(si);
    return;
  }
  // Mode::UpgradeMenu: `id` is the kind it becomes.
  menu_.close();
  mode_ = Mode::Idle;
  submit(Action::upgrade(upgradeStructure_, id));
}

// Two entries beside the panel's place: give the research up (with what comes back), or keep it. Cancel = keep.
void BattleScene::openResearchCancel() {
  const int side = battle_.activeSide;
  std::string why;
  if (!battle_.canCancelResearch(&why)) {
    say(why);
    return;
  }
  std::vector<Amount> back;
  battle_.cancelRefund(side, back);
  const std::string refund = back.empty() ? std::string(EconomyWords::kNoRefund) : EconomyWords::kRefund + priceText(session_.db, back);
  researchPanel_.close();
  mode_ = Mode::ResearchCancel;
  std::vector<MenuItem> items = {{EconomyWords::kCancelResearch, refund, true, kCancelResearchEntry},
                                 {EconomyWords::kKeepResearching, "", true, kKeepResearchEntry}};
  menu_.open(host_.renderer(), session_.db.research[(size_t)battle_.researching(side)].name, items, layout_.W / 4, layout_.vh / 3,
             layout_.W, layout_.vh);  // (the whole screen width: the label and the refund must both fit at UI scale 150)
  OTE_INFO("research cancel menu: %s (%s)", EconomyWords::kCancelResearch, refund.c_str());
}

void BattleScene::researchInput(const InputEvent& e) {
  researchPanel_.setArea(0, 0, layout_.vw < 300 ? layout_.W : layout_.vw, layout_.vh);  // (as the weapon list: the info column may be covered when narrow)
  ItemPanel::Result res = researchPanel_.onInput(e, host_.renderer());
  if (res == ItemPanel::Back) {
    researchPanel_.close();
    openEndMenu();  // the panel is opened from the battle menu: Cancel goes back to it
  } else if (res == ItemPanel::Picked) {
    const int tech = techRows_[(size_t)researchPanel_.sel()];
    std::string why;
    if (tech < 0) return;  // a heading (the selection never rests on one; belt and braces)
    if (tech == battle_.researching(battle_.activeSide)) openResearchCancel();  // choosing the one under way offers to cancel it
    else if (!battle_.validate(Action::research(tech), &why)) say(why.empty() ? "Cannot research that" : why);
    else {
      researchPanel_.close();
      mode_ = Mode::Idle;
      submit(Action::research(tech));
    }
  }
}

// While Supply is the highlighted entry of the action menu, the units it would fill are tinted on the map. This
// reuses the overlay the move range uses (blue squares), which every kind of view already draws.
Overlay BattleScene::supplyOverlay() const {
  Overlay ov;
  if (mode_ != Mode::ActionMenu || !menu_.isOpen() || menu_.selectedId() != kSupply || sel_ < 0) return ov;
  std::vector<int> targets;
  battle_.supplyTargetsFrom(sel_, battle_.units[(size_t)sel_].cell, targets);
  const size_t n = (size_t)battle_.grid.cellCount();
  ov.reach.assign(n, 0);
  ov.attack.assign(n, 0);
  for (int t : targets) ov.reach[(size_t)battle_.units[(size_t)t].cell] = 1;
  return ov;
}

// The unit under the cursor, when it has fuel or ammunition: one [info] line whenever what the panel shows changes.
void BattleScene::logInspected() {
  std::string line;
  if (battle_.grid.valid(cursor_) && humanTurn()) {
    const int uid = battle_.unitAt(cursor_);
    if (uid >= 0 && battle_.units[(size_t)uid].alive && (!view_.fogged() || view_.units[(size_t)uid].visible))  // (fog: only what the panel shows)
      line = supplyLogLine(battle_, battle_.units[(size_t)uid]);
  }
  if (line == inspectedLine_) return;
  inspectedLine_ = line;
  if (!line.empty()) OTE_INFO("info: %s", line.c_str());
}

// The flash of an upgraded structure: a white wash over each of its squares, fading out. A box per square works the
// same in every kind of view (like the flash of a unit that changed sides).
void BattleScene::drawEconomy(Renderer& r) {
  for (size_t i = 0; i < view_.structFlash.size() && i < battle_.structures.size(); ++i) {
    const float f = view_.structFlash[i];
    if (f <= 0) continue;
    std::vector<Cell> cells;
    battle_.structureCells((int)i, cells);
    for (Cell c : cells) {
      int x, y, w, h;
      cellScreenBox(c, x, y, w, h);
      fillCellBox(r, x, y, w, h, Color(255, 255, 255, (uint8_t)(170 * std::min(1.0f, f))));
    }
  }
}

std::string BattleScene::economyHint() const {
  switch (mode_) {
    case Mode::StructureMenu: return EconomyWords::kHintStructure;
    case Mode::UpgradeMenu: return EconomyWords::kHintUpgrade;
    case Mode::Research: return EconomyWords::kHintResearch;
    case Mode::ResearchCancel: return EconomyWords::kHintCancelResearch;
    default: return "";
  }
}

}  // namespace ote
