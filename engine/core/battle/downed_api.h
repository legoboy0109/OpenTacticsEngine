// PUBLIC members of class Battle for downed units and revive (contract: docs/SRPG.md "Downed units and revive"). Included INSIDE the class body in
// battle.h (public section) and nowhere else. Definitions: core/battle/downed.cpp.

// Does the game keep defeated units on the board, downed (game.json rules.downed)?
bool downedGame() const { return db->rules.downed.on; }
// Does `team` know that the square of unit `occ` is taken: it sees that unit, or the unit lies downed there (a body is always known, fog or not).
// The test every "is that square free for me" question uses, so a downed unit's square is never offered to stand on.
bool occupantSeen(int team, int occ) const;
// Can a revive act on `t`: it lies downed on its square and its side is still in the battle.
bool revivable(const Unit& t) const;
// The HP a revive gives `t`: the amount (a number, "full", a percent of its maximum, a formula with a = `user`), at least 1, at most its maximum.
int reviveHp(const Unit& t, const ReviveAmount& r, int user) const;
// The HP the revive ability `d` of `user` gives `t` (its heal fields), at least 1, at most its maximum.
int abilityReviveHp(const AbilityDef& d, int user, const Unit& t) const;
