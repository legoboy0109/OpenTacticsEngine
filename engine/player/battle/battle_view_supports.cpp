// View hooks for SupportPoints and SupportReached (step U5). Empty until then: the dispatcher in
// battle_view_srpg.cpp logs them.
#include "battle/battle_view.h"

namespace ote {

bool BattleView::beginSupports(const Battle&) { return false; }
bool BattleView::progressSupports(const Battle&) { return false; }
bool BattleView::finishSupports(const Battle&) { return false; }

}  // namespace ote
