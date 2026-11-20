// How the battle view plays the events of upgrades, research, repair and supply (docs/ECONOMY.md "Player"):
// StructureUpgraded, ResearchStarted / ResearchFinished, UnitRepaired and UnitResupplied. Each hook returns true
// when the entry was one of these (and so fully handled here), like the story hooks of battle_view_story.cpp.
//
// Everything here is drawn with the things every kind of view already draws (floating texts, the banner, the
// displayed hit points, a flash box on a structure's squares), so it looks the same in all four.
// Events of sides the human does not watch stay short, so the AI's turns never wait on them. Each event also writes
// one [info] line to the log (the windowless tests read those).
#include <algorithm>

#include "battle/battle_view.h"
#include "battle/economy_text.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {

constexpr float kUpgradeFlash = 0.45f;
constexpr float kBannerOwn = 1.3f, kBannerOther = 0.5f;  // seconds: your own research is worth reading, theirs is a glance
constexpr float kRepairOwn = 0.32f, kRepairOther = 0.1f;
constexpr float kSupplyOwn = 0.18f, kSupplyOther = 0.05f;

Color sideTint(const Battle& b, int side) {
  return side >= 0 && side < (int)b.sides.size() ? Color::rgb(b.sides[(size_t)side].color) : Color(255, 255, 255);
}

}  // namespace

bool BattleView::beginEconomy(const Battle& b) {
  const Event& e = cur_.ev;
  const float cx = tile_ / 2.0f;
  switch (e.type) {
    case EventType::StructureUpgraded: {
      const StructureState& st = b.structures[(size_t)e.a];
      if (structFlash.size() < b.structures.size()) structFlash.resize(b.structures.size(), 0.0f);
      structFlash[(size_t)e.a] = 1;
      const StructureDef& to = b.db->structures[(size_t)e.c];
      float fx, fy;
      structureTop(b, st, fx, fy);  // (the upgraded kind keeps the size of the hexagons it covers)
      addFloat(EconomyWords::kFloatUpgraded + to.name, fx, fy, Color(255, 240, 140), 1.2f);
      OTE_INFO("upgrade: %s %s at %d,%d -> %s", st.owner >= 0 ? b.sides[(size_t)st.owner].name.c_str() : "neutral"  /* a neutral structure changes kind too (map weapons, map changes) */, b.db->structures[(size_t)e.b].name.c_str(),
               st.x, st.y, to.name.c_str());
      cur_.dur = kUpgradeFlash;
      return true;
    }
    case EventType::ResearchStarted: {
      const TechnologyDef& t = b.db->research[(size_t)e.b];
      OTE_INFO("research started: %s %s (%s)", b.sides[(size_t)e.a].name.c_str(), t.name.c_str(),
               e.c == 0 ? EconomyWords::kInstant : turnsText(e.c).c_str());
      if (e.c == 0) {  // finished inside the same action: the ResearchFinished banner says it
        cur_.dur = 0;
        return true;
      }
      banner = b.sides[(size_t)e.a].name + ": " + EconomyWords::kBannerResearching + t.name;
      bannerSub = turnsText(e.c);
      bannerColor = sideTint(b, e.a);
      cur_.dur = showsResources(b, e.a) ? kBannerOwn : kBannerOther;
      return true;
    }
    case EventType::ResearchFinished: {
      const TechnologyDef& t = b.db->research[(size_t)e.b];
      OTE_INFO("research finished: %s %s", b.sides[(size_t)e.a].name.c_str(), t.name.c_str());
      banner = b.sides[(size_t)e.a].name + ": " + t.name + EconomyWords::kBannerResearched;
      bannerSub = techGives(*b.db, e.b);
      bannerColor = sideTint(b, e.a);
      cur_.dur = showsResources(b, e.a) ? kBannerOwn : kBannerOther;
      return true;
    }
    case EventType::ResearchCancelled: {
      const TechnologyDef& t = b.db->research[(size_t)e.b];
      OTE_INFO("research cancelled: %s %s (%d turns were left)", b.sides[(size_t)e.a].name.c_str(), t.name.c_str(), e.c);
      banner = b.sides[(size_t)e.a].name + ": " + EconomyWords::kBannerCancelled + t.name;
      bannerSub.clear();
      bannerColor = sideTint(b, e.a);
      cur_.dur = showsResources(b, e.a) ? kBannerOwn : kBannerOther;
      return true;
    }
    case EventType::UnitRepaired: {
      const UnitView& v = units[(size_t)e.a];
      const bool own = showsResources(b, b.units[(size_t)e.a].side);
      cur_.hpFrom = v.hp;
      cur_.hpTo = e.c;
      addFloat("+" + intStr(e.b), v.px + cx, v.py - 2, Color(120, 255, 140), own ? 1.0f : 0.5f);  // the same number as a healing item
      OTE_INFO("repair: %s +%d HP (now %d)", b.units[(size_t)e.a].name.c_str(), e.b, e.c);
      cur_.dur = own ? kRepairOwn : kRepairOther;
      return true;
    }
    case EventType::UnitResupplied: {
      const UnitView& v = units[(size_t)e.a];
      const bool own = showsResources(b, b.units[(size_t)e.a].side);
      addFloat(EconomyWords::kFloatResupplied, v.px + cx, v.py - 2, Color(150, 220, 255), own ? 1.0f : 0.5f);
      OTE_INFO("resupply: %s", b.units[(size_t)e.a].name.c_str());
      cur_.dur = own ? kSupplyOwn : kSupplyOther;
      return true;
    }
    default: return false;
  }
}

bool BattleView::progressEconomy(const Battle& b) {
  const Event& e = cur_.ev;
  float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  switch (e.type) {
    case EventType::StructureUpgraded:
      if ((size_t)e.a < structFlash.size()) structFlash[(size_t)e.a] = 1 - p;
      return true;
    case EventType::UnitRepaired:
      units[(size_t)e.a].hp = cur_.hpFrom + (int)((cur_.hpTo - cur_.hpFrom) * p + 0.5f);
      if (showsResources(b, b.units[(size_t)e.a].side)) {
        hasFocus = true;
        focusX = units[(size_t)e.a].px + tile_ / 2.0f;
        focusY = units[(size_t)e.a].py + tile_ / 2.0f;
      }
      return true;
    case EventType::ResearchStarted:
    case EventType::ResearchFinished:
    case EventType::ResearchCancelled:
    case EventType::UnitResupplied: return true;
    default: return false;
  }
}

bool BattleView::finishEconomy(const Battle&) {
  const Event& e = cur_.ev;
  switch (e.type) {
    case EventType::StructureUpgraded:
      if ((size_t)e.a < structFlash.size()) structFlash[(size_t)e.a] = 0;
      return true;
    case EventType::UnitRepaired: units[(size_t)e.a].hp = e.c; return true;
    case EventType::ResearchStarted:
    case EventType::ResearchFinished:
    case EventType::ResearchCancelled:
      banner.clear();
      bannerSub.clear();
      return true;
    case EventType::UnitResupplied: return true;
    default: return false;
  }
}

}  // namespace ote
