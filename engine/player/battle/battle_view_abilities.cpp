// View hooks for AbilityUsed (step U3). Empty until then: the dispatcher in battle_view_srpg.cpp logs it.
// (UnitRelocated, UnitAdded and UnitRemoved of a push or a summon are already played by the story / build code.)
#include "battle/battle_view.h"

namespace ote {

bool BattleView::beginAbilities(const Battle&) { return false; }
bool BattleView::progressAbilities(const Battle&) { return false; }
bool BattleView::finishAbilities(const Battle&) { return false; }

}  // namespace ote
