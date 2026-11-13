#include "battle/battle_view.h"

#include <algorithm>
#include <cmath>

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

void BattleView::cellPos(const Battle& b, Cell c, float& x, float& y) const {
  x = (float)(b.grid.x(c) * tile_);
  y = (float)(b.grid.y(c) * tile_);
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
    v.visible = u.alive;
    v.hp = u.hp;
    if (u.alive) cellPos(b, u.cell, v.px, v.py);
  }
  queue_.clear();
  cur_.active = false;
  banner.clear();
  levelUp.active = false;
  hasFocus = false;
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
  for (const Event& e : events) queue_.push_back(e);
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

bool BattleView::showsResources(const Battle& b, int side) const { return side == viewSide(b, autoplay_); }

void BattleView::begin(const Battle& b) {
  const Event& e = cur_.ev;
  cur_.t = 0;
  cur_.dur = 0;
  cur_.fired = false;
  float cx = tile_ / 2.0f;
  switch (e.type) {
    case EventType::TurnStarted:
      banner = b.sides[e.a].name + " - Turn " + intStr(e.b);
      bannerColor = sideColor(b, e.a);
      cur_.dur = 0.6f;
      break;
    case EventType::UnitMoved:
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
      for (size_t s = 0; s < e.data.size() && s < b.db->stats.size(); ++s)
        if (e.data[s] > 0) levelUp.lines.push_back("+" + intStr(e.data[s]) + " " + b.db->stats[s].name);
      if (levelUp.lines.empty()) levelUp.lines.push_back("No stat gains");
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
    case EventType::CellCaptured: {
      float x, y;
      cellPos(b, e.a, x, y);
      addFloat("Captured!", x + cx, y, Color(255, 240, 140), 1.2f);
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
    case EventType::UnitBuilt: {
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
    case EventType::BattleEnded: break;
  }
}

void BattleView::impact(const Battle& b) {
  const Event& e = cur_.ev;
  cur_.fired = true;
  const UnitView& d = units[e.b];
  float x = d.px + tile_ / 2.0f, y = d.py - 2;
  if (e.flags & kHit) {
    addFloat(intStr(e.d), x, y, Color(255, 245, 210), 0.9f);
    if (e.flags & kCrit) addFloat("CRIT!", x, y - 9, Color(255, 215, 60), 1.0f);
  } else {
    addFloat("MISS", x, y, Color(190, 195, 215), 0.9f);
  }
}

void BattleView::progress(const Battle& b) {
  const Event& e = cur_.ev;
  float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  hasFocus = false;
  switch (e.type) {
    case EventType::UnitMoved: {
      UnitView& v = units[e.a];
      float f = p * (float)(cur_.path.size() - 1);
      size_t i = std::min((size_t)f, cur_.path.size() - 1);
      size_t j = std::min(i + 1, cur_.path.size() - 1);
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
    case EventType::UnitDied: units[e.a].alpha = 1 - p; break;
    case EventType::UnitBuilt: units[e.a].scale = p; break;
    case EventType::LevelUp:
      levelUp.timer = cur_.t;
      if (levelUp.autoContinue && cur_.t >= 1.2f) cur_.dur = 0;  // ends this frame
      break;
    default: break;
  }
}

void BattleView::finish(const Battle& b) {
  const Event& e = cur_.ev;
  switch (e.type) {
    case EventType::TurnStarted:
    case EventType::SideDefeated: banner.clear(); break;
    case EventType::UnitMoved: {
      UnitView& v = units[e.a];
      cellPos(b, cur_.path.back(), v.px, v.py);
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
    case EventType::UnitBuilt: units[e.a].scale = 1; break;
    case EventType::LevelUp: levelUp.active = false; break;
    default: break;
  }
  cur_.active = false;
}

void BattleView::startNext(const Battle& b) {
  while (!cur_.active && !queue_.empty()) {
    cur_.ev = queue_.front();
    queue_.pop_front();
    cur_.active = true;
    begin(b);
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

  bool nowBusy = busy();
  if (wasBusy_ && !nowBusy) {
    std::vector<FloatText> keep;
    keep.swap(floats);
    resync(b);  // safety net: displayed state := rules state
    floats.swap(keep);
  }
  wasBusy_ = nowBusy;
  if (!nowBusy) resFloats_ = 0;
}

}  // namespace ote
