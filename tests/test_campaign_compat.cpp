// Games without a campaign are untouched by it (docs/SAVES.md): their battle saves stay byte for byte what they
// were before campaigns existed. The goldens below were taken from the build before the campaign work (main at
// the merge of the story events) and must never change by accident.
#include "save_helpers.h"

using namespace ote;

namespace {

uint64_t fnv(const std::string& s) {
  uint64_t h = 1469598103934665603ULL;
  for (char c : s) {
    h ^= (uint8_t)c;
    h *= 1099511628211ULL;
  }
  return h;
}

std::string hex(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v);
  return buf;
}

// A battle played 60 AI steps, saved as text.
std::string playedSave(const char* game, const char* map, unsigned seed) {
  savetest::Rig rig;
  if (!rig.loadDemo(game, map, seed)) return "load failed";
  rig.start();
  std::vector<Event> ev;
  savetest::playAi(rig.battle, 60, ev);
  return savetest::stateText(rig.battle);
}

}  // namespace

TEST(campaign_games_without_one_keep_their_battle_saves_byte_for_byte) {
  CHECK_STR(hex(fnv(playedSave("demo-srpg", "bridge", 7))), "1af1975f86935fd8");
  CHECK_STR(hex(fnv(playedSave("demo-economy", "twin_rivers", 7))), "7bd8c1c0aa74081e");
}
