// PRIVATE members of class Battle for map exits (docs/AI.md "Looting and escaping"). Included INSIDE the class body in battle.h (private
// section) and nowhere else.

// exits.cpp. LeaveMap: a unit of a computer side that stands on one of the map's exits and has not acted.
bool validateLeaveMap(const Action& action, std::string* why) const;
// Takes the unit off the map with what it carries, like the story's removeUnit: UnitRemoved (no defeat, no drops, no experience); its
// summons vanish and a unit it carries is put down.
void applyLeaveMap(const Action& action, std::vector<Event>& events);
