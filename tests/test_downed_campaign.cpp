// Downed units after a campaign battle (docs/SRPG.md "Downed units and revive", "After the battle"): rules.downed.afterBattle "return" brings a
// unit still downed back as a survivor, "fallen" leaves it to the campaign's fallen rule (here: gone). Game: tests/campaign_fixture.h.
#include <string>

#include "campaign_run.h"

using namespace camptest;

namespace {

// fodder (1 HP) next to a grunt: the grunt downs it, the hero finishes the grunt and the battle is won with fodder still lying downed.
bool fodderAfterBattle(const char* folder, const char* downed) {
  Run r;
  std::string game = kGameJson;
  const std::string at = "\"rules\": { ";
  game.replace(game.find(at), at.size(), at + "\"downed\": " + downed + ", ");
  r.g.gameJson = game;
  r.g.keepJson = mapJson(4, 2, sides("0", "[]"),
                         R"([ {"id":"fodder","type":"ranger","side":"blue","x":0,"y":0,"stats":{"hp":1}},
                              {"id":"eirik","type":"hero","side":"blue","x":1,"y":1},
                              {"type":"grunt","side":"red","x":1,"y":0} ])");
  CHECK(r.begin(folder, R"({ "chapters": [ {"id":"a","map":"keep"}, {"id":"b","map":"keep"} ] })"));
  CHECK(r.startBattle());
  int grunt = -1;
  for (const Unit& u : r.b.units)
    if (u.side == 1) grunt = u.id;
  CHECK(r.b.apply(Action::endTurn(), r.ev));
  CHECK(r.b.apply(Action::attack(grunt, r.unit("fodder"), 0), r.ev));
  CHECK(r.b.units[(size_t)r.unit("fodder")].downed);
  CHECK(r.b.apply(Action::endTurn(), r.ev));
  r.b.units[(size_t)grunt].hp = 1;
  CHECK(r.b.apply(Action::attack(r.unit("eirik"), grunt, 0), r.ev));
  CHECK(r.b.over && r.b.winnerTeam == r.b.humanTeam());
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(r.pt.findUnit("fodder") != nullptr);
  return r.pt.findUnit("fodder") && !r.pt.findUnit("fodder")->gone;
}

}  // namespace

TEST(downed_campaign_return_brings_a_downed_unit_back) { CHECK(fodderAfterBattle("downed_camp_return", "{}")); }

TEST(downed_campaign_fallen_leaves_it_to_the_fallen_rule) { CHECK(!fodderAfterBattle("downed_camp_fallen", "{\"afterBattle\":\"fallen\"}")); }
