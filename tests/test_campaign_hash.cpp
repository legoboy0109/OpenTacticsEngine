// What the data hash covers for campaigns (docs/SAVES.md): campaign.json counts except its wording, a campaign
// save's hash covers every chapter's map, and games without a campaign.json keep their hashes.
#include "campaign_run.h"

using namespace camptest;

namespace {

std::string campaignHash(const std::string& campaign, const std::string& keep = "") {
  Game g;
  g.keepJson = keep;
  if (!g.load("camp_hash", campaign)) return "load failed";
  std::string h, err;
  if (!computeCampaignHash(g.db, h, err)) return "error: " + err;
  return h;
}
std::string mapHash(const std::string& campaign, const char* map = "ford") {
  Game g;
  if (!g.load("camp_hash", campaign)) return "load failed";
  std::string h, err;
  if (!computeDataHash(g.db, map, h, err)) return "error: " + err;
  return h;
}

// The default campaign with one piece of text replaced.
std::string edited(const char* from, const char* to) {
  std::string s = kCampaignJson;
  size_t at = s.find(from);
  if (at == std::string::npos) return "";
  return s.replace(at, std::string(from).size(), to);
}

}  // namespace

TEST(campaign_hash_ignores_wording) {
  std::string base = campaignHash(kCampaignJson);
  CHECK(base.size() == 16 && base.find("error") == std::string::npos && base != "load failed");
  CHECK_STR(campaignHash(edited("The Ford War", "A Quite Different Name")), base);
  CHECK_STR(campaignHash(edited("\"name\": \"The Ford\"", "\"name\": \"Der Furt\"")), base);
  CHECK_STR(campaignHash(edited("We hold the ford.", "Haltet die Furt.")), base);
  CHECK_STR(campaignHash(edited("\"speakerName\":\"Narrator\"", "\"speakerName\":\"Erzaehler\"")), base);
  CHECK_STR(campaignHash(edited("Alone again.", "")), base);
}

TEST(campaign_hash_counts_every_rule_of_campaign_json) {
  std::string base = campaignHash(kCampaignJson);
  const char* changes[][2] = {
      {"\"fallen\": \"gone\"", "\"fallen\": \"return\""},
      {"\"onDefeat\": \"retry\"", "\"onDefeat\": \"end\""},
      {"[\"skirmish\"]", "\"all\""},
      {"\"next\": \"ch4\"", "\"next\": \"ch3b\""},
      {"\"goto\":\"ch3b\"", "\"goto\":\"ch3a\""},
      {"\"gold\":25", "\"gold\":26"},
      {"\"portrait\":\"narr\"", "\"portrait\":\"other\""},
      {"\"speaker\":\"lyra\"", "\"speaker\":\"eirik\""},
      {"{\"addVariable\":\"chapters\",\"by\":1} ],\n      \"after\":  [ {\"say\":\"It is done", "{\"addVariable\":\"chapters\",\"by\":2} ],\n      \"after\":  [ {\"say\":\"It is done"},
      {"\"alive\":\"wanderer\"", "\"dead\":\"wanderer\""},
      {"{\"say\":\"The end.\"}", ""},  // a line removed
  };
  for (const auto& c : changes) {
    std::string h = campaignHash(edited(c[0], c[1]));
    if (h == base || h == "load failed") std::printf("  change of %s did not change the hash (%s)\n", c[0], h.c_str());
    CHECK(h != base && h != "load failed");
  }
  // Chapter order and the map a chapter uses.
  CHECK(campaignHash(edited("\"id\": \"ch2\", \"name\": \"The Mill\", \"map\": \"mill\"", "\"id\": \"ch2\", \"name\": \"The Mill\", \"map\": \"keep\"")) != base);
}

TEST(campaign_hash_covers_the_map_of_every_chapter) {
  std::string base = campaignHash(kCampaignJson);
  // A rules change in the keep (used by chapters 3 and 4 only) changes the playthrough's hash ...
  std::string keep = mapJson(6, 3, sides("0", "[]"), heroesOnBlue(R"({"type":"grunt","side":"red","x":4,"y":1})"));
  CHECK(campaignHash(kCampaignJson, keep) != base);
  // ... but not its wording (event text in a map is not a rule either; names are cosmetic).
  CHECK_STR(campaignHash(kCampaignJson, keepMap()), base);
  // The hash is for the whole game: the battle hash of the ford is a different number.
  CHECK(mapHash(kCampaignJson) != base);
}

TEST(campaign_hash_of_a_single_battle_includes_the_file_only_when_it_exists) {
  Game with, without;
  CHECK(with.load("camp_hash_a") && without.load("camp_hash_b", ""));
  std::string a, b, c, err;
  CHECK(computeDataHash(with.db, "ford", a, err) && computeDataHash(without.db, "ford", b, err));
  CHECK(a != b);  // the file counts when it exists ...
  CHECK(computeDataHash(without.db, "ford", c, err) && b == c);
  // ... and its wording does not.
  Game worded;
  CHECK(worded.load("camp_hash_c", edited("We hold the ford.", "Haltet die Furt.")));
  CHECK(computeDataHash(worded.db, "ford", c, err) && c == a);
  CHECK(!computeCampaignHash(without.db, c, err));  // no campaign, no campaign hash
}

TEST(campaign_the_demo_games_have_no_campaign) {
  for (const char* demo : {"demo-srpg", "demo-economy"}) {
    Database db;
    std::string err;
    CHECK(loadGame(ote_test::sourceDir() + "/games/" + demo, db, err));
    CHECK(db.campaign == nullptr);
    std::string h;
    CHECK(computeDataHash(db, db.maps[0].id, h, err));  // pinned in test_story_save.cpp: unchanged
  }
}
