// How the battle view plays the events of commander powers (docs/COMMANDERS.md "Player"): PowerUsed shows the commander's name with the power's name in
// the banner (the turn banner's presentation: it stays up for a moment, then goes with the entry); PowerEnded a short line. Everything the power does
// (healing, hurting, gold, units that may act again) arrives as the usual entries after PowerUsed and plays as they always do. A computer side's banner
// is as long as a person's: a power is a thing to see. Each entry writes one [info] line (the windowless tests read them).
#include "battle/battle_view.h"
#include "battle/commander_text.h"
#include "core/base/log.h"

namespace ote {
namespace {
constexpr float kUsedSeconds = 1.5f, kEndedOwn = 0.7f, kEndedOther = 0.25f;
}  // namespace

bool BattleView::beginCommander(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::PowerUsed && e.type != EventType::PowerEnded) return false;
  const bool ok = e.b >= 0 && (size_t)e.b < b.db->commanders.size() && e.c >= 0 && (size_t)e.c < b.db->commanders[(size_t)e.b].powers.size();
  if (!ok) {
    cur_.dur = 0;
    return true;
  }
  const CommanderDef& d = b.db->commanders[(size_t)e.b];
  const CommanderPower& p = d.powers[(size_t)e.c];
  const Color tint = e.a >= 0 && (size_t)e.a < b.sides.size() ? Color::rgb(b.sides[(size_t)e.a].color) : Color(255, 255, 255);
  if (e.type == EventType::PowerUsed) {
    banner = d.name;
    bannerSub = p.name;
    bannerColor = tint;
    cur_.dur = skipping_ ? 0.3f : kUsedSeconds;
    OTE_INFO("power used: %s - %s (cost %d, meter now %d)", d.name.c_str(), p.name.c_str(), e.d, e.data.empty() ? 0 : e.data[0]);
  } else {
    banner = d.name + ": " + p.name + CommanderWords::kEnded;
    bannerSub.clear();
    bannerColor = tint;
    cur_.dur = showsResources(b, e.a) ? kEndedOwn : kEndedOther;
    OTE_INFO("power ended: %s - %s", d.name.c_str(), p.name.c_str());
  }
  return true;
}

}  // namespace ote
