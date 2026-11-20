// A writable copy of tests/fixtures/combo: levels + promotion + merit ranks + class jobs + overlay jobs in ONE game (docs/JOBS.md "Class jobs and overlay
// jobs", docs/SRPG.md "Unit ranks"). Same shape as the jobs fixture (jobs_fixture.h) so jobtest::Game plays it.
//   squire hp 30 str 6 def 2 mov 4 (promotes to captain at level 2, automatically) | captain hp 36 str 8 def 3 mov 4 | knight hp 36 str 7 def 5 mov 3 | mage hp 22 str 3 def 1 mov 4
//   class jobs: squire, knight, mage (as in the jobs fixture).  overlay jobs: scout (bonus str +1, mov +1; teaches eagle, 20 JP) and warden (bonus def +2; teaches bulwark, 30 JP)
//   characters: aldric (squire, class job squire, 30 JP), bryn (squire, overlay job scout, 25 JP), cora (mage, class job mage); maps first (aldric has merit 8), second, duel
//   (the red raider is a grunt with the overlay job scout). Merit: 5 per hit, 15 per defeat; Veteran at 10 (str +1), Elite at 30 (str +2, hp +4, steady).
//   Experience: 20 per level, a hit pays 10.
#pragma once
#include "jobs_fixture.h"

namespace combotest {

using namespace ote;

struct Copy : skilltest::Copy {
  explicit Copy(const std::string& name) : skilltest::Copy(name, "combo") {
    static const char* const extra[] = {"data/jobs.json", "data/campaign.json", "maps/first.json", "maps/second.json"};
    for (const char* f : extra) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/combo/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
};

}  // namespace combotest
