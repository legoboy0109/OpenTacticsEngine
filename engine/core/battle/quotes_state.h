// What changes during play about characters' battle quotes (docs/EVENTS.md "Battle and defeat quotes").
#pragma once
#include <cstdint>
#include <vector>

namespace ote {

// Which battle quotes have been said already, one flag per quote in file order (BattleQuote::flat). Empty in a game
// without quotes. A "map" quote's flag starts clear in every battle; a "campaign" quote's flag is carried from chapter
// to chapter (Playthrough::quotesSaid); an "always" quote's flag is never looked at.
struct QuoteState {
  std::vector<uint8_t> said;
};

}  // namespace ote
