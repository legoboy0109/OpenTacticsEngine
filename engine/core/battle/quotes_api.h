// PUBLIC members of class Battle for battle and defeat quotes (docs/EVENTS.md "Battle and defeat quotes"). Included INSIDE
// the class body in battle.h (public section) and nowhere else. Definitions: quotes.cpp.

// Which battle quotes were said already (a campaign's "once per campaign" flags come in through setStartingQuotes).
QuoteState quotes;
// Hands in a campaign's flags (Playthrough::quotesSaid). Call after init(), before start(). Only the "campaign" quotes'
// flags are taken, and only a list of the right size; anything else is ignored.
void setStartingQuotes(const std::vector<uint8_t>& carried);
// Called by init(): every flag clear (none in a game without quotes).
void quotesInit();
// A fight is about to begin (before the "fight" story events): the defender's quote, then the attacker's, each as
// Dialogue events. Characters only; a quote already said (per its "once") is skipped.
void fightQuotes(int attacker, int defender, std::vector<Event>& events);
// A unit was struck down in a fight (killUnit has run, the "defeated" story events have not): its defeat quote.
void defeatQuote(int unit, std::vector<Event>& events);
