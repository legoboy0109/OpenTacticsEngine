// Linux twin of the battle part of main.cpp: same loader, map, seed, loop, summary and hash.
// Host (Linux) side of the battle hash check: links the engine core built on the host (ote_core + lua) and prints the same hash the Xbox diagnostic shows.
#include <cstdio>
#include <string>
#include <vector>
#include "core/ai/ai.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "battle_hash.h"
using namespace ote;
int main(int argc, char** argv) {
  Database db; MapDef map; Formulas formulas; Battle battle; std::vector<Event> events; std::string err;
  if (argc < 2 || !loadGame(argv[1], db, err) || !loadMap(db, "bridge", map, err) || !formulas.init(db, err) ||
      !battle.init(db, map, formulas, 1, err)) { std::printf("load/init failed: %s\n", err.c_str()); return 1; }
  battle.start(events);
  while (!battle.over && battle.round <= 60) {
    long guard = 10L * (long)battle.units.size() + 100, n = 0;
    while (aiTakeStep(battle, events) && ++n <= guard) {}
    events.clear();
  }
  std::string surv;
  for (size_t s = 0; s < battle.sides.size(); ++s) {
    int alive = 0;
    for (const Unit& u : battle.units) if (battle.exists(u) && u.side == (int)s) ++alive;
    surv += (surv.empty() ? "" : ", ") + battle.sides[s].name + "=" + std::to_string(alive);
  }
  std::printf("rounds %d surv %s hash %08x\n", battle.round, surv.c_str(), battleHash(battle));
}
