// FNV-1a hash of a finished battle's state, to compare an Xbox run with a Linux run (battle_host.cpp) of the same map and seed.
#pragma once
#include "core/battle/battle.h"

inline unsigned battleHash(const ote::Battle& b) {
  unsigned h = 2166136261u;
  auto mix = [&](long v) { for (int i = 0; i < 4; ++i) { h ^= (unsigned)((v >> (8 * i)) & 0xff); h *= 16777619u; } };
  mix(b.round); mix(b.winnerTeam); mix((long)b.units.size());
  for (const ote::Unit& u : b.units) { mix(u.id); mix(u.side); mix(u.cell); mix(u.hp); mix(u.xp); mix(u.level); mix(u.alive); }
  return h;
}
