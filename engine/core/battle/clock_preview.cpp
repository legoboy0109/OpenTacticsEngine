// The forecast of the turn order (docs/TURN_ORDER.md "The preview"): the clock of clock.cpp run forward on a copy of the numbers, so it changes
// nothing and uses no randomness. It follows exactly the rules runClock follows (a full charged action first, then the best ready unit, else a tick)
// with one assumption: every unit uses its whole turn (cost "both") and stays as it is (speed, conditions, who is alive). So the preview is what
// then happens whenever those hold, and the entries beyond a change are a best guess.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

struct SimCharge {
  int unit, counter, speed, serial, pending;
  bool forecast;
};

}  // namespace

void Battle::turnPreview(int count, std::vector<TurnEntry>& out, const PendingAction* extra) const {
  out.clear();
  if (!speedGame() || over || count <= 0) return;
  const SpeedRules& sp = db->rules.speed;
  std::vector<int> ct(units.size()), gain(units.size());
  std::vector<char> in(units.size());
  for (size_t i = 0; i < units.size(); ++i) {
    in[i] = inClock(units[i]) ? 1 : 0;
    ct[i] = units[i].ct;
    gain[i] = in[i] ? speedOf(units[i]) : 0;
  }
  std::vector<SimCharge> charges;
  for (size_t k = 0; k < pending.size(); ++k) {
    const PendingAction& p = pending[k];
    if (p.unit >= 0 && (size_t)p.unit < units.size() && in[(size_t)p.unit]) charges.push_back({p.unit, p.counter, p.speed, p.serial, (int)k, false});
  }
  int ticks = 0;
  if (activeUnit >= 0 && (size_t)activeUnit < units.size() && in[(size_t)activeUnit]) {
    TurnEntry e;
    e.unit = activeUnit;
    out.push_back(e);
    // What its turn will cost: the whole turn, or (a charged action declared now) the move it made, if any, and the action.
    const Unit& a = units[(size_t)activeUnit];
    ct[(size_t)activeUnit] -= extra ? (a.moved ? sp.costBoth : sp.costOne) : sp.costBoth;
  }
  if (extra) charges.push_back({extra->unit, 0, extra->speed, 1 << 30, -1, true});

  auto unitBefore = [&](size_t x, size_t y) {
    if (ct[x] != ct[y]) return ct[x] > ct[y];
    const int sx = stat(units[x], sp.stat), sy = stat(units[y], sp.stat);
    if (sx != sy) return sx > sy;
    return x < y;
  };
  for (int guard = 0; (int)out.size() < count && guard < 200000; ++guard) {
    int c = -1;  // a full charged action: more counter, then the faster one, then the older
    for (size_t k = 0; k < charges.size(); ++k) {
      if (charges[k].counter < sp.threshold) continue;
      if (c < 0 || charges[k].counter > charges[(size_t)c].counter ||
          (charges[k].counter == charges[(size_t)c].counter &&
           (charges[k].speed > charges[(size_t)c].speed || (charges[k].speed == charges[(size_t)c].speed && charges[k].serial < charges[(size_t)c].serial))))
        c = (int)k;
    }
    if (c >= 0) {
      TurnEntry e;
      e.unit = charges[(size_t)c].unit;
      e.charged = true;
      e.forecast = charges[(size_t)c].forecast;
      e.pending = charges[(size_t)c].pending;
      e.ticks = ticks;
      out.push_back(e);
      charges.erase(charges.begin() + c);
      continue;
    }
    int u = -1;
    for (size_t i = 0; i < units.size(); ++i)
      if (in[i] && ct[i] >= sp.threshold && (u < 0 || unitBefore(i, (size_t)u))) u = (int)i;
    if (u >= 0) {
      TurnEntry e;
      e.unit = u;
      e.ticks = ticks;
      out.push_back(e);
      ct[(size_t)u] -= sp.costBoth;
      continue;
    }
    bool gained = false;
    for (size_t i = 0; i < units.size(); ++i)
      if (gain[i] > 0) {
        ct[i] += gain[i];
        gained = true;
      }
    for (SimCharge& s : charges) {
      s.counter += s.speed;
      gained = true;
    }
    if (!gained) break;  // nobody can ever act
    ++ticks;
  }
}

int Battle::chargeForecast(int speed, int count, int* ticks) const {
  if (!speedGame() || activeUnit < 0 || speed <= 0) return -1;
  PendingAction extra;
  extra.unit = activeUnit;
  extra.speed = speed;
  std::vector<TurnEntry> order;
  turnPreview(count, order, &extra);
  for (size_t i = 0; i < order.size(); ++i)
    if (order[i].forecast) {
      if (ticks) *ticks = order[i].ticks;
      return (int)i;
    }
  return -1;
}

}  // namespace ote
