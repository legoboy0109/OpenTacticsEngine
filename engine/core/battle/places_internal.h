// PRIVATE members of class Battle for doors, chests and visiting (docs/SRPG.md section 10). Included INSIDE the class body in
// battle.h (private section) and nowhere else.

// places.cpp: one strike's damage and the strike count against a door.
bool placeStrike(const Unit& u, int structure, int weapon, Cell from, int& damage, int& strikes) const;
// places_apply.cpp
bool validatePlace(const Action& action, std::string* why) const;
// Carries OpenPlace / VisitPlace / BreakPlace out; `endsTurn` false for the automatic ones (after a move).
void applyPlace(const Action& action, std::vector<Event>& events, bool endsTurn);
void openPlace(int structure, int unit, int how, std::vector<Event>& events);
void visitPlace(int structure, int unit, bool destroy, std::vector<Event>& events);
void giveLoot(int structure, int unit, std::vector<Event>& events);
void autoPlaces(int unit, std::vector<Event>& events);
int closedKindOf(int structure) const;
bool storyShutDoor(int structure, bool lock, std::vector<Event>& events);
void storyPlace(const Command& c, std::vector<Event>& events);
// places_unlock.cpp
bool unlockTargetOk(const Unit& u, int ability, Cell from, Cell cell, std::string* why) const;
void applyUnlockAbility(const Action& action, std::vector<Event>& events);
void applyStaffUnlock(const Action& action, std::vector<Event>& events);
