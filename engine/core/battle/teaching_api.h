// PUBLIC members of class Battle for learning from equipment (contract: docs/JOBS.md "Learning from equipment (ability points)"). Included INSIDE the
// class body in battle.h (public section) and nowhere else. Definitions: core/battle/teaching.cpp. In a game where no item teaches, every answer is
// "nothing".

// The taught entries (indices into db->teachEntries) the unit may use now because an equipped item teaches them and they are not mastered yet: the weapon,
// then the accessories it carries, each entry once. An entry a job teaches too counts only while the unit's current job is that job. `weapon` as in
// skillsOf: -1 = the weapon in hand, -2 = no weapon, else that weapon (index into db->weapons; its item if the unit carries one, so a forecast with
// another weapon or a strike ability's weapon gets nothing from the one in hand).
void teachFromItems(const Unit& u, std::vector<int>& out, int weapon = -1) const;
// The taught entries it keeps for good (mastered, and no job teaches them; a mastered job entry is one of its job's learned entries instead).
void teachMasteredOf(const Unit& u, std::vector<int>& out) const;
// The entry of the unit's usable item-taught ability / skill `ref` (-1 = none comes from an item now): for the unit details' "(AP 40/100)".
int teachEntryFromItem(const Unit& u, bool ability, int ref) const;
