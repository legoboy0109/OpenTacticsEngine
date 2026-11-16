// View hooks for UnitRescued, UnitDropped, UnitHanded and PairSwitched (step U4). Empty until then: the dispatcher
// in battle_view_srpg.cpp logs them and only hides / shows the unit that left / came back onto the board.
#include "battle/battle_view.h"

namespace ote {

bool BattleView::beginRescue(const Battle&) { return false; }
bool BattleView::progressRescue(const Battle&) { return false; }
bool BattleView::finishRescue(const Battle&) { return false; }

}  // namespace ote
