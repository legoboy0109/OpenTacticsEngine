// Supports in a campaign (docs/SRPG.md "Supports": state, "Story and campaign"): the closeness a playthrough
// carries between chapters, and the conversations of the camp between chapters.
// STUB (step F1): no game has support pairs yet (data/supports.json is refused by the loader), so the carried
// state stays empty and nothing is ever pending. Step S1 fills this file.
#include "core/campaign/playthrough.h"

namespace ote {

void beginSupports(const Database&, Playthrough& pt) { pt.supports = SupportState(); }

void takeSupports(const Database&, Playthrough& pt, const Battle& battle) { pt.supports = battle.supports; }

void pendingSupports(const Database&, const Playthrough&, std::vector<int>& pairs) { pairs.clear(); }

bool playSupport(const Database&, Playthrough&, int, std::vector<SceneLine>&) { return false; }

}  // namespace ote
