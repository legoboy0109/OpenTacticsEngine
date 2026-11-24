// PUBLIC members of class Battle for guests (contract: docs/SRPG.md "Guests"). Included INSIDE the class body in battle.h (public section) and
// nowhere else. Definitions: core/battle/guests.cpp.
//
// A guest is a unit on a side a person plays that the computer moves (Unit::guest). The rules core does not move it: whoever drives the turns
// (the player, a test) asks guestToAct() and lets the computer opponent play that unit (aiGuestStep, ai.h) before the person gets the turn.

// The guest that should act now, or -1: side by side, the lowest-id guest of the side whose turn it is (a side a person plays) that is on the board
// and has not finished; in the speed turn order, the unit whose turn it is when it is a guest. -1 once the battle is over.
int guestToAct() const;
