// Pieces of the data hash (save_hash.cpp), shared with the campaign's hash (core/campaign/campaign_hash.cpp).
#pragma once
#include <cstdint>
#include <string>

#include "core/battle/save.h"

namespace ote {
namespace savehash {

struct Fnv {
  uint64_t h = 1469598103934665603ULL;
  void byte(uint8_t b) { h ^= b; h *= 1099511628211ULL; }
  void bytes(const std::string& s) { for (char c : s) byte((uint8_t)c); }
  void number(uint64_t v) { for (int i = 0; i < 8; ++i) byte((uint8_t)(v >> (8 * i))); }
  // 16 lowercase hex digits.
  std::string hex() const;
};

// Mixes everything that defines the game's rules except the map: game.json's rule keys, the data
// files, the scripts. (computeDataHash adds the map file; the campaign hash adds every chapter's.)
bool mixRules(Fnv& f, const Database& db, std::string& err);
// Mixes one map file (rules content only: names and the wording of events do not count).
bool mixMapFile(Fnv& f, const Database& db, const std::string& mapFile, std::string& err);
// Mixes data/campaign.json when the game has one (nothing at all otherwise, so games without a
// campaign keep their hash). Names and the wording of scenes do not count.
bool mixCampaignFile(Fnv& f, const Database& db, std::string& err);

}  // namespace savehash
}  // namespace ote
