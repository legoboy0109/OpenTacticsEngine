// View hooks for UnitPromoted (step U2). Empty until then: the dispatcher in battle_view_srpg.cpp logs it.
#include "battle/battle_view.h"

namespace ote {

bool BattleView::beginPromotion(const Battle&) { return false; }
bool BattleView::progressPromotion(const Battle&) { return false; }
bool BattleView::finishPromotion(const Battle&) { return false; }

}  // namespace ote
