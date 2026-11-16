// Human-readable event log lines for ote-sim --verbose.
#pragma once
#include <string>

#include "core/battle/battle.h"

namespace simtool {

// One line describing `e`, using names/coordinates from `b` (state after
// the event, which is fine for names and positions that never change).
std::string describeEvent(const ote::Battle& b, const ote::Event& e);
// The strategy-RPG entries (docs/SRPG.md "Events"; event_print_srpg.cpp). False = not one of them.
bool describeSrpgEvent(const ote::Battle& b, const ote::Event& e, std::string& out);

}  // namespace simtool
