// Action menu: the unit's abilities, ids kAbilityBase + n (docs/SRPG.md "Player UI", step U3).
//
// One entry per ability the unit has, in the core's order (abilityOptions). One that cannot be used now stays in the
// menu greyed with the plain reason on the right (the menu cannot select it); a usable one with a limit shows its
// uses left. Choosing one asks for the target with the generic pick modes (battle_srpg_target.cpp): a unit or a
// square, the end squares of what it would do marked red; a "self" ability is used at once. The hint line says what
// the ability does while a target is picked (a swap's end squares are both under units, so the marks alone are not
// enough). The id is the position in abilityOptions, which is recomputed on choosing (the state cannot change while
// the menu is open).
#include "battle/ability_preview.h"
#include "battle/ability_text.h"
#include "battle/condition_text.h"
#include "battle/item_text.h"
#include "battle/srpg_menu_ids.h"
#include "battle/strike_preview.h"
#include "battle/battle_scene.h"
#include "battle/menu_hide.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {
// The last "what it does" line this file put in the hint line. The pick's Cancel goes through the generic code, so the
// notice would stay up to six seconds over the menu's own hint; the next menu (or the use) takes it away again.
std::string gPickHint;
}  // namespace

void BattleScene::addAbilityEntries(std::vector<MenuItem>& items) {
  if (sel_ < 0) return;
  if (!gPickHint.empty() && notice_ == gPickHint) noticeTime_ = 0;
  gPickHint.clear();
  const Unit& u = battle_.units[(size_t)sel_];
  std::vector<AbilityOption> opts;
  battle_.abilityOptions(sel_, u.cell, opts);
  const bool hide = host_.menuHide();  // "hide": an ability with nothing to use it on is left out (menu_hide.h); the others keep their id = position
  // A reason must not push the menu off a small screen: it gets what is left of the viewport next to the widest
  // label (the menu's own padding and arrow are about 36 px). Too long for that: the short form, then cut off.
  Renderer& r = host_.renderer();
  int labelW = 0;
  for (const AbilityOption& o : opts)
    if (!abilityEntryHidden(hide, o)) labelW = std::max(labelW, textW(r, battle_.db->abilities[(size_t)o.ability].name));
  const int maxExtra = std::max(40, layout_.vw - 36 - textW(r, "  ") - labelW);
  std::string log;  // for the logs: what the menu offered and the reason of the greyed ones
  for (size_t i = 0; i < opts.size(); ++i) {
    const AbilityOption& o = opts[i];
    if (abilityEntryHidden(hide, o)) continue;
    std::string extra = abilityMenuExtra(battle_, u, o);
    if (textW(r, extra) > maxExtra) extra = abilityShortReason(extra);
    // Still too long: cut off with "..", but a stump of a word ("no..") says nothing, so with no room at all the
    // entry stays greyed without a reason (the phone-sized screen; the Abilities page of Unit details has the words).
    if (textW(r, extra) > maxExtra) extra = maxExtra >= textW(r, "xxxxxx") ? fitText(r, extra, maxExtra) : "";
    items.push_back({battle_.db->abilities[(size_t)o.ability].name, extra, o.ok, kAbilityBase + (int)i});
    log += (log.empty() ? "" : ", ") + battle_.db->abilities[(size_t)o.ability].name + (o.ok ? "" : " [" + o.why + "]");
  }
  if (!log.empty()) OTE_INFO("ability entries of %s: %s", u.name.c_str(), log.c_str());
}

// "Steal which?": the items of the foe that was picked, the ones that cannot be taken greyed with the reason on the right (the long
// form when it fits, else a short one, else none). Choosing one uses the ability; Cancel steps back to the unit pick.
void BattleScene::stealMenu(int user, int ability, int target, std::function<void()> back) {
  const Unit& t = battle_.units[(size_t)target];
  std::vector<StealOption> opts;
  battle_.stealOptions(user, ability, battle_.units[(size_t)user].cell, target, opts);
  Renderer& r = host_.renderer();
  std::vector<MenuItem> items;
  std::string log;
  for (const StealOption& o : opts) {
    const ItemStack& st = t.items[(size_t)o.slot];
    const ItemDef& d = battle_.db->items[(size_t)st.item];
    const int room = menuRoomAfter(r, layout_.vw, d.name);  // as the staff lists do
    std::string extra = o.ok ? usesText(battle_, st) : stealReasonText(o.why, false);
    if (textW(r, extra) > room && !o.ok) extra = stealReasonText(o.why, true);
    if (textW(r, extra) > room) extra = "";  // a narrow screen: greyed without words (the unit details say why)
    items.push_back({d.name, extra, o.ok, kStealBase + o.slot, itemicon::ofItem(*battle_.db, st.item)});
    log += (log.empty() ? "" : ", ") + d.name + (o.ok ? "" : " [" + stealReasonText(o.why, false) + "]");
  }
  OTE_INFO("steal list from %s: %s", t.name.c_str(), log.c_str());
  mode_ = Mode::ActionMenu;
  overlay_ = Overlay();
  staffFcOn_ = false;
  pickBack_ = nullptr;
  staffBack_ = std::move(back);
  staffItemUser_ = user;
  staffItemTarget_ = target;
  stealAbility_ = ability;
  menu_.open(r, AbilityWords::kStealWhich, items, 0, 0, layout_.vw, layout_.vh);
  anchorActionMenu();
  say(AbilityWords::kStealWhich);
}

bool BattleScene::abilityChosen(int id) {
  if (sel_ < 0) return false;
  if (id >= kStealBase && id < kSupportBase) {  // an item picked in the "Steal which?" list
    if (stealAbility_ < 0 || staffItemUser_ < 0 || staffItemTarget_ < 0) return false;
    const int user = staffItemUser_, ability = stealAbility_;
    Action a = Action::useAbility(user, ability, staffItemTarget_);
    a.slot = id - kStealBase;
    stealAbility_ = -1;
    staffBack_ = nullptr;
    if (!submit(a)) return true;
    const Unit& me = battle_.units[(size_t)user];
    if (battle_.over || !me.alive || me.done) finishOrder();
    else itemActionDone();
    return true;
  }
  const Unit& u = battle_.units[(size_t)sel_];
  std::vector<AbilityOption> opts;
  battle_.abilityOptions(sel_, u.cell, opts);
  const int n = id - kAbilityBase;
  if (n < 0 || n >= (int)opts.size() || !opts[(size_t)n].ok) return false;
  const int ability = opts[(size_t)n].ability;
  const AbilityDef& def = battle_.db->abilities[(size_t)ability];
  const int user = sel_;
  aimedChargeSpeed_ = def.charge.on() ? def.charge.speed : 0;  // a charged ability: the turn strip shows where it would land while it is aimed

  auto clearHint = [this] {
    if (!gPickHint.empty() && notice_ == gPickHint) noticeTime_ = 0;
    gPickHint.clear();
  };
  // The order is given; what happens next is the same as after Talk: an ability that ends the turn finishes the
  // order, a free one leaves the menu open for the rest of the turn.
  auto use = [this, user, ability, clearHint](int target, Cell cell) {
    clearHint();
    aimedChargeSpeed_ = 0;
    if (!submit(Action::useAbility(user, ability, target, cell))) return;
    const Unit& me = battle_.units[(size_t)user];
    if (battle_.over || !me.alive || me.done) finishOrder();
    else itemActionDone();
  };
  // "Pushes it 2 squares away  Z:use  X:back" while the highlight is on a candidate.
  const std::string effect = abilityEffectText(*battle_.db, def);
  const std::string hint = effect + "  {ok}:use  {cancel}:back";
  auto sayHint = [this, hint] {
    gPickHint = hint;
    say(hint);
  };
  // The chance of the ability's condition on the unit under the cursor ("Sleep 60%"; docs/SRPG.md "Ability conditions"), only for one that rolls.
  auto chanceOn = [this, user, ability](int target) -> std::string {
    if (!battle_.abilityInflictRolls(ability) || target < 0) return "";
    const int p = battle_.abilityInflictChance(user, ability, target, battle_.units[(size_t)user].cell);
    const std::string t = conditionChanceText(*battle_.db, battle_.db->abilities[(size_t)ability].inflictCond, p);
    OTE_INFO("condition chance: %s", t.c_str());  // (the player tests read it)
    return t;
  };
  // A steal's forecast: how many of the highlighted foe's items can be taken.
  auto stealHint = [this, user, ability](int target) {
    std::vector<StealOption> o;
    battle_.stealOptions(user, ability, battle_.units[(size_t)user].cell, target, o);
    int ok = 0;
    for (const StealOption& e : o) ok += e.ok;
    gPickHint = "Steal: " + std::to_string(ok) + " of " + std::to_string(o.size()) + " items can be taken  {ok}:choose  {cancel}:back";
    say(gPickHint);
  };

  std::vector<int> units;
  std::vector<Cell> cells;
  battle_.abilityTargets(user, ability, u.cell, units, cells);
  if (def.effect == AbilityEffect::Strike) {
    // A strike (docs/SRPG.md "Abilities that strike"): the hint line is the forecast of the highlighted target; an area weapon marks every square
    // it would cover. The fight itself plays like an attack (Attack events).
    auto sayLine = [this](const std::string& line) {
      gPickHint = (line.empty() ? std::string("?") : line) + "  {ok}:use  {cancel}:back";
      say(gPickHint);
    };
    if (def.target == AbilityTarget::Cell) {
      if (cells.empty()) { say(AbilityWords::kNothingToTarget); return true; }
      beginPickCell(cells, [use](Cell c) { use(-1, c); }, [this, user, ability, sayLine](int cand, Overlay& ov) {
        std::vector<Cell> covered;
        const std::string line = strikeAreaLine(battle_, user, ability, cand, covered);
        for (Cell c : covered) { ov.reach[(size_t)c] = 0; ov.attack[(size_t)c] = 1; }
        sayLine(line);
      });
    } else {
      if (units.empty()) { say(AbilityWords::kNothingToTarget); return true; }
      std::vector<Cell> candidateCells;
      for (int t : units) candidateCells.push_back(battle_.units[(size_t)t].cell);
      beginPickUnit(units, [use](int t) { use(t, kNoCell); }, [this, user, ability, candidateCells, sayLine, chanceOn](int target, Overlay& ov) {
        for (Cell c : candidateCells) ov.reach[(size_t)c] = 1;
        ov.reach[(size_t)battle_.units[(size_t)target].cell] = 0;
        ov.attack[(size_t)battle_.units[(size_t)target].cell] = 1;
        std::string line = strikeUnitLine(battle_, user, ability, target);
        const std::string chance = chanceOn(target);
        if (!line.empty() && !chance.empty()) line += ", " + chance;
        OTE_INFO("strike forecast: %s", line.c_str());  // (the player tests read it)
        sayLine(line);
      });
    }
  } else if (def.target == AbilityTarget::Self) {
    menu_.close();
    use(-1, kNoCell);
  } else if (def.target == AbilityTarget::Cell) {
    if (cells.empty()) { say(AbilityWords::kNothingToTarget); return true; }
    // The new unit's square is the one under the cursor: mark it like an end square.
    beginPickCell(cells, [use](Cell c) { use(-1, c); }, [sayHint](int cand, Overlay& ov) {
      ov.reach[(size_t)cand] = 0;  // (the hexagon drawing lets blue win over red on one square)
      ov.attack[(size_t)cand] = 1;
      sayHint();
    });
  } else {
    if (units.empty()) { say(AbilityWords::kNothingToTarget); return true; }
    std::vector<Cell> candidateCells;  // blue: who can be picked
    for (int t : units) candidateCells.push_back(battle_.units[(size_t)t].cell);
    const bool steals = def.effect == AbilityEffect::Steal;
    auto mark = [this, user, ability, candidateCells, sayHint, stealHint, steals, effect, chanceOn](int target, Overlay& ov) {
      for (Cell c : candidateCells) ov.reach[(size_t)c] = 1;
      for (const AbilityMovePreview& m : previewAbilityMoves(battle_, user, ability, target))
        for (Cell c : m.path) { ov.reach[(size_t)c] = 0; ov.attack[(size_t)c] = 1; }  // where the units would end up (and pass, for a push)
      const std::string chance = steals ? std::string() : chanceOn(target);
      if (steals) stealHint(target);
      else if (!chance.empty()) {  // "Inflicts Sleep for 3 turns  Sleep 60%  Z:use  X:back"
        gPickHint = effect + "  " + chance + "  {ok}:use  {cancel}:back";
        say(gPickHint);
      } else sayHint();
    };
    if (steals) {  // the foe first, then which of its items
      beginPickUnit(units, [this, user, ability, id, clearHint](int t) {
        clearHint();
        stealMenu(user, ability, t, [this, id] { abilityChosen(id); });
      }, mark);
    } else {
      beginPickUnit(units, [use](int t) { use(t, kNoCell); }, mark);
    }
  }
  OTE_INFO("ability chosen: %s by %s", def.name.c_str(), u.name.c_str());
  return true;
}

}  // namespace ote
