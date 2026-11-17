// Parties and the preparation screen (docs/EVENTS.md "Parties", "Preparation"): the candidates, the storage and the funds the
// screen shows are the active party's; the other party's roster comes back untouched.
#include "prep_fixture.h"

using namespace preptest;

namespace {

// The srpg-prep campaign with chapter 2 played by a second party, chapter 3 by the first again.
void withParties(Run& r) {
  r.g.edit("data/campaign.json", R"("rules": { "fallen": "gone", "onDefeat": "retry" },)",
           R"("rules": { "fallen": "gone", "onDefeat": "retry" }, "parties": [ { "id": "main" }, { "id": "north", "name": "North" } ],)");
  r.g.edit("data/campaign.json", R"({ "id": "ch2", "name": "The Orchard", "map": "orchard",)",
           R"({ "id": "ch2", "name": "The Orchard", "map": "orchard", "party": "north",)");
  r.g.edit("data/campaign.json", R"({ "id": "ch3", "name": "The Hilltop", "map": "hilltop" })",
           R"({ "id": "ch3", "name": "The Hilltop", "map": "hilltop", "party": "main" })");
}

std::string ids(const std::vector<PrepEntry>& e) {
  std::string out;
  for (const PrepEntry& p : e) out += (out.empty() ? "" : " ") + p.id;
  return out;
}

std::string rosterIds(const Roster& r) {
  std::string out;
  for (const RosterUnit& u : r) out += (out.empty() ? "" : " ") + u.id;
  return out;
}

}  // namespace

TEST(parties_prep_candidates_funds_and_storage_belong_to_the_active_party) {
  Run r;
  withParties(r);
  CHECK(r.begin("parties_prep") && r.toChapter2());
  CHECK_STR(rosterIds(r.pt.roster), "sera tomas wren bram");  // the main party's company after the meadow
  const std::vector<int> mainFunds = [&] {
    r.loadChapterMap();  // (chapter 2's map; the funds are read below from the carried side)
    return r.pt.sides.empty() ? std::vector<int>() : r.pt.sides[0].resources;
  }();
  CHECK(r.before());  // chapter 2's "party": north
  CHECK(r.pt.party == "north" && r.pt.roster.empty() && r.pt.sides.empty());
  r.loadChapterMap();
  const std::vector<PrepEntry> entries = prepEntries(r.g.db, r.pt, r.mapDef);
  for (const PrepEntry& p : entries) CHECK(!p.inRoster);  // nobody of the main party: only what the orchard itself places
  CHECK(ids(entries).find("tomas") == std::string::npos && ids(entries).find("bram") == std::string::npos);
  prepOpen(r.g.db, r.pt, r.mapDef, true);
  CHECK(r.pt.prep.open);
  CHECK(rosterIds(r.pt.roster).find("tomas") == std::string::npos);
  // What the screen shows to spend is the north party's: the orchard's own allowance, not the meadow's gold carried by main.
  const std::vector<int> funds = prepResources(r.g.db, r.pt, r.mapDef);
  CHECK(!funds.empty() && !mainFunds.empty() && funds[0] < mainFunds[0] + 1000 && funds[0] != mainFunds[0] + 40);
  CHECK(r.pt.parked.size() == 1 && r.pt.parked[0].id == "main" && rosterIds(r.pt.parked[0].roster) == "sera tomas wren bram");
  CHECK_EQ(r.pt.parked[0].sides[0].resources[0], mainFunds[0]);

  // Play it out: the north party carries its own result; main is untouched. Chapter 3 then goes back to main's four.
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why));
  CHECK(r.start());
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(r.pt.party == "north" && !r.pt.roster.empty() && !r.pt.lastDeployed.empty());
  CHECK(rosterIds(r.pt.parked[0].roster) == "sera tomas wren bram" && r.pt.parked[0].lastDeployed.empty());
  r.lines.clear();
  CHECK(runScene(r.g.db, r.pt, SceneKind::After, r.lines));
  CHECK(r.before());  // chapter 3's "party": main
  CHECK(r.pt.party == "main" && r.pt.lastDeployed.empty());
  CHECK_STR(rosterIds(r.pt.roster), "sera tomas wren bram");
  r.loadChapterMap();
  const std::vector<PrepEntry> back = prepEntries(r.g.db, r.pt, r.mapDef);
  for (const std::string id : {"sera", "tomas", "bram"}) CHECK(ids(back).find(id) != std::string::npos);
  CHECK_EQ(prepResources(r.g.db, r.pt, r.mapDef).empty() ? -1 : 0, 0);
  // The north party's deployment memory did not leak into main's default pick.
  for (const PrepDeploy& d : prepDefaultDeployment(r.g.db, r.pt, r.mapDef)) CHECK(r.pt.findUnit(d.id) || d.id == "guard" || d.id == "ilsa");
}
