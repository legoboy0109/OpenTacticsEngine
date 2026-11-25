// Guests in a campaign (docs/SRPG.md "Guests", docs/EVENTS.md "Campaign"): a guest is not dressed from the roster and not written to it after the
// battle, unless the story's "guestJoins" made it the player's own during the battle. Fixture: campaign_run.h.
#include "campaign_run.h"

using namespace camptest;

namespace {

std::string guestMap(const char* events) {
  return mapJson(6, 3, sides("0", "[]"),
                 R"([ {"id":"eirik","type":"hero","side":"blue","x":0,"y":1,"name":"Eirik"},
                      {"id":"ritz","type":"ranger","side":"blue","x":0,"y":0,"name":"Ritz","guest":true},
                      {"type":"grunt","side":"red","x":5,"y":1} ])",
                 events);
}

}  // namespace

TEST(guest_campaign_leaves_after_the_battle) {
  Run r;
  r.g.keepJson = guestMap("[]");
  CHECK(r.begin("guest_camp", R"({ "chapters": [ {"id":"a","map":"keep"}, {"id":"b","map":"keep"} ] })"));
  CHECK(r.startBattle());
  CHECK(r.b.units[(size_t)r.unit("ritz")].guest);
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.pt.findUnit("eirik") != nullptr);
  CHECK(r.pt.findUnit("ritz") == nullptr);  // never in the roster
  // The next battle places it again as the map says: still a guest, with its own stats.
  r.scene(SceneKind::After);
  CHECK(r.startBattle());
  CHECK(r.unit("ritz") >= 0 && r.b.units[(size_t)r.unit("ritz")].guest);
}

TEST(guest_campaign_joins_with_guest_joins) {
  Run r;
  r.g.keepJson = guestMap(R"([ {"id":"j","when":{"battle":"start"},"do":[{"guestJoins":"ritz"}]} ])");
  CHECK(r.begin("guest_camp_join", R"({ "chapters": [ {"id":"a","map":"keep"}, {"id":"b","map":"keep"} ] })"));
  CHECK(r.startBattle());
  CHECK(!r.b.units[(size_t)r.unit("ritz")].guest);
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.pt.findUnit("ritz") != nullptr);
}
