#include "battle/battle_view.h"

#include <algorithm>
#include <cmath>

#include "battle/item_text.h"
#include "battle/rank_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr float kMoveSecPerCell = 0.035f;
constexpr float kAttackDur = 0.4f, kImpactAt = 0.35f, kLungePx = 8;

Color sideColor(const Battle& b, int side) {
  return side >= 0 && side < (int)b.sides.size() ? Color::rgb(b.sides[side].color) : Color(255, 255, 255);
}

}  // namespace

int viewSide(const Battle& b, bool autoplay) {
  if (autoplay || !b.sides[b.activeSide].ai) return b.activeSide;
  for (size_t s = 0; s < b.sides.size(); ++s)
    if (!b.sides[s].ai) return (int)s;
  return b.activeSide;
}

// Where a unit's tile box is for a cell (squares: the square; hexagons: centred on the hexagon's middle).
void BattleView::cellPos(const Battle& b, Cell c, float& x, float& y) const { cellTopLeft(b.grid, tile_, c, x, y); }

// Middle of the top edge of a structure's picture box: where its floating texts start.
void BattleView::structureTop(const Battle& b, const StructureState& st, float& x, float& y) const {
  int bx, by, bw, bh;
  structureBox(b.grid, tile_, st.x, st.y, b.defOf(st), bx, by, bw, bh);
  x = bx + bw / 2.0f;
  y = (float)by;
}

void BattleView::init(const Battle& b, int tileSize, bool autoplay) {
  tile_ = tileSize;
  autoplay_ = autoplay;
  floats.clear();
  resync(b);
}

void BattleView::resync(const Battle& b) {
  units.assign(b.units.size(), UnitView());
  for (const Unit& u : b.units) {
    UnitView& v = units[u.id];
    v.visible = u.alive && u.carriedBy < 0;  // a carried unit (rescue / pair up) is off the board: not drawn
    v.hp = u.hp;
    for (const ConditionState& cs : u.conditions) v.conds.push_back(cs.condition);
    if (u.alive) cellPos(b, u.cell, v.px, v.py);
  }
  queue_.clear();
  cur_.active = false;
  updateShown(b);  // fog: what the viewer cannot see is not drawn (after the queue is empty: the view is idle)
  banner.clear();
  bannerSub.clear();
  levelUp.active = false;
  structFlash.clear();
  dialogue.active = false;
  hasFocus = false;
  stickyFocus_ = false;
  skipping_ = false;
  wasBusy_ = false;
}

void BattleView::ensureUnits(const Battle& b) {
  // Units created during play stay hidden until their UnitBuilt event plays.
  while (units.size() < b.units.size()) {
    UnitView v;
    v.visible = false;
    units.push_back(v);
  }
}

void BattleView::push(const std::vector<Event>& events) {
  for (const Event& e : events) {
    queue_.push_back(e);
    // The rules already show the unit on its new side; the view keeps the old colour until the
    // SideChanged entry plays (a line like "Join us!" comes first).
    if (e.type == EventType::SideChanged && e.a >= 0 && (size_t)e.a < units.size() && units[(size_t)e.a].side < 0)
      units[(size_t)e.a].side = e.b;
    pushPromotion(e);  // likewise the old unit type until UnitPromoted plays (battle_view_promotion.cpp)
  }
}

void BattleView::addFloat(const std::string& text, float x, float y, Color c, float life, bool screen) {
  FloatText f;
  f.text = text;
  f.x = x;
  f.y = y;
  f.color = c;
  f.life = life;
  f.screen = screen;
  floats.push_back(f);
}

std::string BattleView::itemName(const Battle& b, int item) const {
  return item >= 0 && item < (int)b.db->items.size() ? b.db->items[(size_t)item].name : "item";
}

bool BattleView::showsResources(const Battle& b, int side) const { return side == shownSide(b, autoplay_); }

void BattleView::begin(const Battle& b) {
  const Event& e = cur_.ev;
  cur_.t = 0;
  cur_.dur = 0;
  cur_.fired = false;
  cur_.hideAtEnd = false;
  float cx = tile_ / 2.0f;
  if (beginStory(b) || beginEconomy(b) || beginFog(b) || beginSrpg(b)) return;
  switch (e.type) {
    case EventType::TurnStarted:
      banner = b.sides[e.a].name + " - Turn " + intStr(e.b);
      bannerColor = sideColor(b, e.a);
      cur_.dur = 0.6f;
      if (announce_) {  // the very first turn of a new battle: say what the battle is about, long enough to read
        announce_ = false;
        bannerSub = b.objectiveText();
        if (!bannerSub.empty()) {
          bannerSub = "Objective: " + bannerSub;
          cur_.dur = 2.4f;
        }
      }
      break;
    case EventType::UnitMoved:  // (with fog, beginFog took the ones it changes)
      cur_.path.assign(e.data.begin(), e.data.end());
      cur_.dur = std::max(0.02f, kMoveSecPerCell * (float)(cur_.path.size() - 1));
      break;
    case EventType::Attack: {
      UnitView& d = units[e.b];
      const UnitView& a = units[e.a];
      cur_.hpFrom = d.hp;
      cur_.hpTo = std::max(0, d.hp - e.d);
      float dx = d.px - a.px, dy = d.py - a.py, len = std::sqrt(dx * dx + dy * dy);
      cur_.dirX = len > 0 ? dx / len : 0;
      cur_.dirY = len > 0 ? dy / len : 0;
      cur_.dur = kAttackDur;
      break;
    }
    case EventType::UnitDied: cur_.dur = 0.3f; break;
    case EventType::XpGained:
      addFloat("+" + intStr(e.b) + " XP", units[e.a].px + cx, units[e.a].py - 2, Color(120, 220, 255), 1.1f);
      cur_.dur = 0.2f;
      break;
    case EventType::LevelUp: {
      const Unit& u = b.units[e.a];
      levelUp.active = true;
      levelUp.timer = 0;
      levelUp.autoContinue = autoplay_ || b.sides[u.side].ai;
      levelUp.title = u.name + " reached Lv " + intStr(e.b);
      levelUp.lines.clear();
      for (size_t s = 0; s < e.data.size() && s < b.db->stats.size(); ++s) {
        const std::string capped = capLine(b, u, (int)s, e.data[s]);  // a stat at its cap says so (docs/SRPG.md section 7)
        if (!capped.empty()) {
          levelUp.lines.push_back(capped);
          OTE_INFO("level-up box: %s", capped.c_str());
        }
        else if (e.data[s] > 0) levelUp.lines.push_back("+" + intStr(e.data[s]) + " " + b.db->stats[s].name);
      }
      if (levelUp.lines.empty()) levelUp.lines.push_back("No stat gains");
      addLearnedToBox(b, e.a);  // "New skill: ..." (battle_view_skills.cpp)
      cur_.dur = 1e9f;  // until Confirm, or the auto-continue timer
      break;
    }
    case EventType::CaptureProgress: {
      float x, y;
      cellPos(b, e.b, x, y);
      addFloat("Capture: " + intStr(e.c) + " left", x + cx, y, Color(255, 230, 120), 1.2f);
      cur_.dur = 0.3f;
      break;
    }
    case EventType::StructureCaptured: {
      // Once per structure, over the middle of its footprint's top edge.
      const StructureState& st = b.structures[(size_t)e.a];
      float x, y;
      structureTop(b, st, x, y);
      addFloat("Captured!", x, y, Color(255, 240, 140), 1.2f);
      cur_.dur = 0.35f;
      break;
    }
    case EventType::ResourceChanged:
      if (showsResources(b, e.a) && e.c != 0 && e.b >= 0 && e.b < (int)b.db->resources.size()) {
        std::string t = (e.c > 0 ? "+" : "") + intStr(e.c) + " " + b.db->resources[e.b].name;
        addFloat(t, (float)resX_, (float)(resY_ + 11 * (resFloats_++ % 3)), e.c > 0 ? Color(120, 255, 140) : Color(255, 130, 110), 1.3f, true);
        cur_.dur = 0.12f;
      }
      break;
    case EventType::UnitBuilt:
    case EventType::UnitAdded: {  // same layout: a=unit, b=cell
      UnitView& v = units[e.a];
      v = UnitView();
      v.hp = b.units[e.a].hp;
      v.scale = 0;
      cellPos(b, e.b, v.px, v.py);
      cur_.dur = 0.25f;
      break;
    }
    case EventType::SideDefeated:
      banner = b.sides[e.a].name + " defeated";
      bannerColor = sideColor(b, e.a);
      cur_.dur = 0.8f;
      break;
    case EventType::UnitHealed: {
      UnitView& v = units[e.a];
      cur_.hpFrom = v.hp;
      cur_.hpTo = std::min(v.hp + e.b, b.maxHp(b.units[e.a]));
      addFloat("+" + intStr(e.b), v.px + cx, v.py - 2, Color(120, 255, 140), 1.0f);
      cur_.dur = 0.35f;
      break;
    }
    case EventType::StatsRaised: {
      std::string t = gainsText(*b.db, e.data);
      if (!t.empty()) addFloat(t, units[e.a].px + cx, units[e.a].py - 2, Color(255, 235, 130), 1.2f);
      cur_.dur = t.empty() ? 0 : 0.3f;
      break;
    }
    case EventType::ItemBroke:
      // A potion is used up rather than broken.
      addFloat(itemName(b, e.b) + (b.db->items[(size_t)e.b].kind == ItemKind::Usable ? " used up" : " broke"), units[e.a].px + cx, units[e.a].py - 2, Color(255, 170, 110), 1.2f);
      cur_.dur = 0.3f;
      break;
    case EventType::ItemGained:
      if (e.a >= 0) {
        addFloat("Got " + itemName(b, e.b), units[e.a].px + cx, units[e.a].py - 2, Color(170, 235, 255), 1.1f);
        cur_.dur = 0.25f;
      } else if (showsResources(b, e.c)) {  // into the storage: nobody to float over, say it in the hint line
        notice = itemName(b, e.b) + " sent to storage";
      }
      break;
    case EventType::ItemLost:
      addFloat(itemName(b, e.b) + " lost (no room)", units[e.a].px + cx, units[e.a].py - 2, Color(255, 140, 120), 1.2f);
      cur_.dur = 0.3f;
      break;
    case EventType::ItemUsed:    // nothing to show beyond what its effects show
    case EventType::ItemMoved:
    case EventType::ItemEquipped:
    case EventType::MusicChanged:  // sound only (battle_view_sound.cpp)
    case EventType::SoundRequested:
    case EventType::BattleEnded: break;
    // Story entries are handled by beginStory() above (battle_view_story.cpp).
    case EventType::Dialogue:
    case EventType::Focus:
    case EventType::SideChanged:
    case EventType::ObjectiveChanged:
    case EventType::UnitRemoved:
    case EventType::UnitRelocated:
    // Upgrades and research (docs/ECONOMY.md): no player UI yet; the new structure kind shows by itself.
    case EventType::StructureUpgraded:
    case EventType::ResearchStarted:
    case EventType::ResearchFinished:
    // Repair and supply (docs/ECONOMY.md): no player UI yet; the unit's HP shows by itself next time it is drawn.
    case EventType::UnitRepaired:  // (its hit points are set when the event ends)
    case EventType::UnitResupplied:
    case EventType::MoveBlocked: break;  // beginFog (battle_view_fog.cpp) plays it for a viewer
    // Strategy-RPG entries: beginSrpg (battle_view_srpg.cpp) took them above.
    case EventType::SkillLearned:
    case EventType::SkillActivated:
    case EventType::UnitHurt:
    case EventType::UnitPromoted:
    case EventType::AbilityUsed:
    case EventType::UnitRescued:
    case EventType::UnitDropped:
    case EventType::UnitHanded:
    case EventType::PairSwitched:
    case EventType::SupportPoints:
    case EventType::SupportReached:
    case EventType::RankUp:
    case EventType::StaffUsed:
    case EventType::StaffMissed:
    case EventType::ItemRepaired:
    case EventType::AreaLit:
    // Conditions (battle_view_conditions.cpp).
    case EventType::ConditionApplied:
    case EventType::ConditionEnded:
    case EventType::ConditionResisted:
    case EventType::ConditionTick: break;
  }
}

void BattleView::impact(const Battle& b) {
  const Event& e = cur_.ev;
  cur_.fired = true;
  soundImpact(b);
  const UnitView& d = units[e.b];
  float x = d.px + tile_ / 2.0f, y = d.py - 2;
  if (e.flags & kHit) {
    addFloat(intStr(e.d), x, y, Color(255, 245, 210), 0.9f);
    if (e.flags & kCrit) addFloat("CRIT!", x, y - 9, Color(255, 215, 60), 1.0f);
    if (e.flags & kSurvived) {  // a skill kept it at 1 HP
      addFloat("Survived!", x, y - 18, Color(255, 150, 120), 1.2f);
      OTE_INFO("Survived! %s", b.units[(size_t)e.b].name.c_str());
    }
  } else {
    addFloat("MISS", x, y, Color(190, 195, 215), 0.9f);
  }
}

void BattleView::progress(const Battle& b) {
  const Event& e = cur_.ev;
  float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  hasFocus = false;
  if (progressStory(b) || progressEconomy(b) || progressSrpg(b)) return;
  switch (e.type) {
    case EventType::UnitMoved: {
      UnitView& v = units[e.a];
      float f = p * (float)(cur_.path.size() - 1);
      size_t i = std::min((size_t)f, cur_.path.size() - 1);
      size_t j = std::min(i + 1, cur_.path.size() - 1);
      soundStep(b, i);
      float t = f - (float)i, x0, y0, x1, y1;
      cellPos(b, cur_.path[i], x0, y0);
      cellPos(b, cur_.path[j], x1, y1);
      v.px = x0 + (x1 - x0) * t;
      v.py = y0 + (y1 - y0) * t;
      hasFocus = true;
      focusX = v.px + tile_ / 2.0f;
      focusY = v.py + tile_ / 2.0f;
      break;
    }
    case EventType::Attack: {
      UnitView& a = units[e.a];
      UnitView& d = units[e.b];
      float off = p < kImpactAt ? p / kImpactAt : std::max(0.0f, 1 - (p - kImpactAt) / 0.25f);
      a.ox = cur_.dirX * kLungePx * off;
      a.oy = cur_.dirY * kLungePx * off;
      if (p >= kImpactAt && !cur_.fired) impact(b);
      if (p >= kImpactAt) {
        float k = std::min(1.0f, (p - kImpactAt) / 0.55f);
        d.hp = cur_.hpFrom - (int)std::lround((cur_.hpFrom - cur_.hpTo) * k);
      }
      hasFocus = true;
      focusX = d.px + tile_ / 2.0f;
      focusY = d.py + tile_ / 2.0f;
      break;
    }
    case EventType::UnitHealed:
      units[e.a].hp = cur_.hpFrom + (int)std::lround((cur_.hpTo - cur_.hpFrom) * p);
      hasFocus = true;
      focusX = units[e.a].px + tile_ / 2.0f;
      focusY = units[e.a].py + tile_ / 2.0f;
      break;
    case EventType::UnitDied: units[e.a].alpha = 1 - p; break;
    case EventType::MoveBlocked:  // the camera goes to the revealed enemy
      hasFocus = true;
      focusX = units[(size_t)e.b].px + tile_ / 2.0f;
      focusY = units[(size_t)e.b].py + tile_ / 2.0f;
      break;
    case EventType::UnitBuilt:
    case EventType::UnitAdded: units[e.a].scale = p; break;
    case EventType::LevelUp:
      levelUp.timer = cur_.t;
      if (levelUp.autoContinue && cur_.t >= 1.2f) cur_.dur = 0;  // ends this frame
      break;
    default: break;
  }
}

void BattleView::finish(const Battle& b) {
  const Event& e = cur_.ev;
  if (finishStory(b) || finishEconomy(b) || finishSrpg(b)) {
    cur_.active = false;
    return;
  }
  switch (e.type) {
    case EventType::TurnStarted:
    case EventType::SideDefeated:
      banner.clear();
      bannerSub.clear();
      break;
    case EventType::UnitMoved: {
      UnitView& v = units[e.a];
      cellPos(b, cur_.path.back(), v.px, v.py);
      if (cur_.hideAtEnd) v.visible = false;  // fog: it walked on out of sight
      break;
    }
    case EventType::Attack: {
      if (!cur_.fired) impact(b);  // skipped before the hit landed
      units[e.a].ox = units[e.a].oy = 0;
      units[e.b].hp = cur_.hpTo;
      break;
    }
    case EventType::UnitDied: {
      UnitView& v = units[e.a];
      v.visible = false;
      v.hp = 0;
      v.alpha = 1;
      break;
    }
    case EventType::UnitBuilt:
    case EventType::UnitAdded: units[e.a].scale = 1; break;
    case EventType::LevelUp: levelUp.active = false; break;
    case EventType::UnitHealed: units[e.a].hp = cur_.hpTo; break;
    // Repair at the start of a turn (no animation yet): the displayed hit points just follow the rules.
    case EventType::UnitRepaired: units[e.a].hp = e.c; break;
    case EventType::StatsRaised: {
      // New maximum hit points arrive filled (the rules do the same); other stats show in the panel.
      int hpGain = e.data.size() > (size_t)b.db->rules.statHp ? e.data[(size_t)b.db->rules.statHp] : 0;
      if (hpGain > 0) units[e.a].hp = std::min(units[e.a].hp + hpGain, b.maxHp(b.units[e.a]));
      clampHpTo(b, e.a);
      break;
    }
    // Moving an accessory (or throwing it away) can lower someone's maximum: the displayed hit points follow.
    case EventType::ItemMoved:
      clampHpTo(b, e.a);
      clampHpTo(b, e.b);
      break;
    case EventType::ItemBroke: clampHpTo(b, e.a); break;
    default: break;
  }
  cur_.active = false;
}

// The displayed hit points never exceed the unit's maximum (a lowered maximum cuts them, as in the rules).
void BattleView::clampHpTo(const Battle& b, int unit) {
  if (unit < 0 || unit >= (int)units.size() || unit >= (int)b.units.size() || !b.units[(size_t)unit].alive) return;
  units[(size_t)unit].hp = std::min(units[(size_t)unit].hp, b.maxHp(b.units[(size_t)unit]));
}

void BattleView::startNext(const Battle& b) {
  while (!cur_.active && !queue_.empty()) {
    cur_.ev = queue_.front();
    queue_.pop_front();
    if (unseenByViewer(cur_.ev)) {  // fog: nothing to show, nothing to wait for
      applyUnseen(b, cur_.ev);
      continue;
    }
    cur_.active = true;
    begin(b);
    soundStarted(b);
    if (cur_.dur <= 0) finish(b);
  }
}

void BattleView::skip(const Battle& b) {
  if (cur_.active) finish(b);
  startNext(b);
}

void BattleView::update(double dt, const Battle& b) {
  ensureUnits(b);
  for (FloatText& f : floats) f.age += (float)dt;
  floats.erase(std::remove_if(floats.begin(), floats.end(), [](const FloatText& f) { return f.age >= f.life; }),
               floats.end());

  if (cur_.active) {
    cur_.t += (float)dt;
    progress(b);
    if (cur_.t >= cur_.dur) finish(b);
  }
  startNext(b);
  if (cur_.active) progress(b);  // so a freshly started event shows frame 0
  if (!cur_.active) hasFocus = false;
  if (!hasFocus && stickyFocus_) {  // a Focus entry's spot, until the run is over
    hasFocus = true;
    focusX = stickyX_;
    focusY = stickyY_;
  }

  bool nowBusy = busy();
  if (wasBusy_ && !nowBusy) {
    std::vector<FloatText> keep;
    keep.swap(floats);
    resync(b);  // safety net: displayed state := rules state
    floats.swap(keep);
  }
  wasBusy_ = nowBusy;
  updateShown(b);
  if (!nowBusy) {
    resFloats_ = 0;
    skipping_ = false;
  }
}

}  // namespace ote
