// How the battle view shows SupportPoints and SupportReached (docs/SRPG.md "Player UI > Supports", step U5).
//   SupportPoints with kReady: a small "Support!" label over both units (the pair's conversation can be played).
//                   Other gains show nothing.
//   SupportReached: a banner "Eirik and Lyra: support level 1" for a moment. (The conversation lines before it are
//                   ordinary Dialogue events, handled by battle_view_story.cpp.)
#include "battle/battle_view.h"
#include "battle/support_text.h"
#include "core/base/log.h"

namespace ote {
namespace {
constexpr float kLabelSeconds = 0.35f;   // the entry waits this long so the label is seen before the next event
constexpr float kBannerSeconds = 1.4f;
}  // namespace

bool BattleView::beginSupports(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.c < 0 || (size_t)e.c >= b.db->supports.pairs.size()) return false;  // not a pair of this game: a logged no-op
  if (e.type == EventType::SupportPoints) {
    cur_.dur = 0;
    if (!(e.flags & kReady)) return true;  // a plain gain: nothing to show
    OTE_INFO("support ready: %s", supportPairName(*b.db, e.c).c_str());
    for (int id : {e.a, e.b}) {
      // A unit that is off the board or hidden (fog, carried) gets no label.
      if (id < 0 || (size_t)id >= units.size() || !units[(size_t)id].visible) continue;
      const UnitView& v = units[(size_t)id];
      addFloat(supportReadyLabel(), v.px + tile_ / 2.0f, v.py - 2, Color(255, 220, 90), 1.2f);
    }
    cur_.dur = kLabelSeconds;
    return true;
  }
  if (e.type == EventType::SupportReached) {
    banner = supportBannerText(*b.db, e.c, e.d);
    bannerColor = Color(70, 110, 190);
    OTE_INFO("support level: %s", banner.c_str());
    cur_.dur = kBannerSeconds;
    return true;
  }
  return false;
}

bool BattleView::progressSupports(const Battle& b) {
  if (cur_.ev.c < 0 || (size_t)cur_.ev.c >= b.db->supports.pairs.size()) return false;
  return cur_.ev.type == EventType::SupportPoints || cur_.ev.type == EventType::SupportReached;
}

bool BattleView::finishSupports(const Battle& b) {
  if (cur_.ev.c < 0 || (size_t)cur_.ev.c >= b.db->supports.pairs.size()) return false;
  if (cur_.ev.type == EventType::SupportReached) {
    banner.clear();
    bannerSub.clear();
    return true;
  }
  return cur_.ev.type == EventType::SupportPoints;
}

}  // namespace ote
