// Research and the campaign and the data hash (docs/ECONOMY.md, docs/EVENTS.md "Campaign", docs/SAVES.md):
// finished technologies of human sides carry from chapter to chapter and into campaign saves, research
// under way does not, and data/research.json is part of the hash only when present (names do not count).
#include "campaign_run.h"
#include "json_edit.h"

using namespace camptest;
using namespace jsonedit;

namespace {

// drill: hero units get +3 hp after 2 turns (cost 10); plans: free, instant, a prerequisite for nothing.
const char* kResearch = R"({ "technologies": [
  { "id": "drill", "name": "Drill", "description": "Harder training.", "cost": {"gold": 10}, "turns": 2,
    "bonus": { "types": ["hero"], "stats": { "hp": 3 } } },
  { "id": "plans", "name": "Plans", "cost": {}, "turns": 0 } ] })";
enum { Drill, Plans };

bool begin(Run& r, const char* folder, const char* research = kResearch) {
  r.g.researchJson = research;
  return r.begin(folder);
}

std::string hashOf(const std::string& research, const char* map = "ford") {
  Game g;
  g.researchJson = research;
  if (!g.load("camp_res_hash")) return "load failed";
  std::string h, err;
  if (!computeDataHash(g.db, map, h, err)) return "error: " + err;
  return h;
}
std::string campaignHashOf(const std::string& research) {
  Game g;
  g.researchJson = research;
  if (!g.load("camp_res_hash")) return "load failed";
  std::string h, err;
  if (!computeCampaignHash(g.db, h, err)) return "error: " + err;
  return h;
}

std::string replaced(const char* from, const char* to) {
  std::string s = kResearch;
  size_t at = s.find(from);
  if (at == std::string::npos) return "";
  return s.replace(at, std::string(from).size(), to);
}

Json saveOf(Run& r, bool withBattle) {
  Json out;
  std::string err;
  if (!buildCampaignSave(r.g.db, "res_save", r.pt, withBattle ? &r.b : nullptr, 1000, out, err)) std::printf("  build: %s\n", err.c_str());
  Json again;
  Json::parse(out.dump(0), again, err);
  return again;
}

std::string restore(Run& r, const Json& file, Playthrough& pt, Battle& b, bool& hasBattle) {
  std::string err;
  return restoreCampaignSave(r.g.db, r.g.formulas, "res_save", file, pt, b, hasBattle, err) ? "" : err;
}

}  // namespace

// ---- hash ----

TEST(research_hash_is_mixed_in_only_when_the_file_exists) {
  std::string without = hashOf(""), with = hashOf(kResearch);
  CHECK(without.size() == 16 && without.find("error") == std::string::npos);
  CHECK(with.size() == 16 && with != without);
  // The campaign's own hash follows the same rule.
  CHECK(campaignHashOf(kResearch) != campaignHashOf(""));
  // And a game without the file hashes the same as ever: nothing about research is mixed in
  // (the pinned demo hashes in the demo tests check this against real history).
  CHECK_STR(hashOf(""), hashOf(""));
}

TEST(research_hash_ignores_names_and_descriptions) {
  std::string base = hashOf(kResearch);
  CHECK_STR(hashOf(replaced("\"name\": \"Drill\"", "\"name\": \"Exercise\"")), base);
  CHECK_STR(hashOf(replaced("Harder training.", "Tougher training.")), base);
  CHECK_STR(hashOf(replaced("\"description\": \"Harder training.\", ", "")), base);  // the key may even go
  CHECK_STR(campaignHashOf(replaced("Plans", "Blueprints")), campaignHashOf(kResearch));
}

TEST(research_hash_counts_every_rule) {
  std::string base = hashOf(kResearch);
  const char* changes[][2] = {
      {"\"gold\": 10", "\"gold\": 11"},                     // cost
      {"\"turns\": 2", "\"turns\": 3"},                     // turns
      {"\"hp\": 3", "\"hp\": 4"},                           // bonus
      {"[\"hero\"]", "[\"ranger\"]"},                       // bonus types
      {"\"id\": \"plans\"", "\"id\": \"plan\""},            // an id
      {"\"turns\": 0 }", "\"turns\": 0, \"unlocks\": {\"units\": [\"grunt\"]} }"},
      {"\"turns\": 0 }", "\"turns\": 0, \"requires\": [\"drill\"] }"},
  };
  for (const auto& c : changes) {
    std::string h = hashOf(replaced(c[0], c[1]));
    if (h == base || h.find("error") != std::string::npos || h == "load failed") {
      std::printf("  change of %s did not change the hash (%s)\n", c[0], h.c_str());
      CHECK(false);
    }
  }
}

// ---- carry-over ----

TEST(research_finished_technologies_carry_into_the_next_chapter_and_progress_does_not) {
  Run r;
  CHECK(begin(r, "camp_res_carry"));
  CHECK(r.startBattle());
  Battle& b = r.b;
  CHECK_EQ(b.sides[0].researched.size(), size_t(2));
  b.sides[0].researched[Plans] = 1;       // finished
  b.sides[0].researching = Drill;         // under way: lost
  b.sides[0].researchLeft = 2;
  b.sides[1].researched[Drill] = 1;       // the computer side: never carried
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK_EQ(r.pt.sides.size(), size_t(1));
  CHECK_STR(r.pt.sides[0].id, "blue");
  CHECK_EQ(r.pt.sides[0].research.size(), size_t(1));
  CHECK_EQ(r.pt.sides[0].research[0], (int)Plans);
  CHECK(r.scene(SceneKind::After));
  CHECK(r.startBattle());  // chapter 2
  const Battle& n = r.b;
  CHECK(n.hasTechnology(0, Plans));
  CHECK(!n.hasTechnology(0, Drill));
  CHECK_EQ(n.researching(0), -1);
  CHECK(!n.hasTechnology(1, Drill) && !n.hasTechnology(1, Plans));
}

TEST(research_carried_bonuses_reach_units_at_full_strength_in_the_next_chapter) {
  Run r;
  CHECK(begin(r, "camp_res_bonus"));
  CHECK(r.startBattle());
  r.b.sides[0].researched[Drill] = 1;
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.scene(SceneKind::After));
  CHECK(r.startBattle());
  const Battle& n = r.b;
  const Unit& eirik = n.units[(size_t)r.unit("eirik")];
  CHECK_EQ(n.maxHp(eirik), 27);          // 20 + 4 (his ring) + 3 (drill)
  CHECK_EQ(eirik.hp, 27);                // the chapter starts at full strength, bonus included
  CHECK_EQ(eirik.stats[0], 20);          // the unit's own value is untouched (it carries on without the bonus in the roster)
  const Unit& lyra = n.units[(size_t)r.unit("lyra")];
  CHECK_EQ(n.maxHp(lyra), 12);           // a ranger: not a hero
}

TEST(research_a_lost_chapter_carries_nothing) {
  Run r;
  CHECK(begin(r, "camp_res_retry"));
  CHECK(r.startBattle());
  r.b.sides[0].researched[Plans] = 1;
  r.decide(false);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Retry);
  CHECK(r.pt.sides.empty());
}

TEST(research_carry_over_is_absent_in_games_without_research) {
  Run r;
  CHECK(r.begin("camp_res_none"));
  CHECK(r.startBattle());
  CHECK(r.b.sides[0].researched.empty());
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.pt.sides[0].research.empty());
  Json j;
  savePlaythrough(r.g.db, r.pt, j);
  CHECK(j.dump(0).find("research") == std::string::npos);
}

// ---- saves ----

TEST(research_campaign_save_round_trips_carried_technologies_and_research_under_way) {
  Run r;
  CHECK(begin(r, "camp_res_save"));
  CHECK(r.startBattle());
  r.b.sides[0].researched[Plans] = 1;
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.scene(SceneKind::After));
  CHECK(r.startBattle());  // chapter 2, plans carried
  // Research in progress inside the battle is part of the battle save.
  r.b.sides[0].researching = Drill;
  r.b.sides[0].researchLeft = 2;
  Json f = saveOf(r, true);
  CHECK(f["playthrough"]["sides"][size_t(0)].has("research"));
  CHECK(f["battle"]["sides"][size_t(0)].has("research"));
  Playthrough pt;
  Battle b;
  bool has = false;
  CHECK_STR(restore(r, f, pt, b, has), "");
  CHECK(has);
  CHECK_EQ(pt.sides[0].research.size(), size_t(1));
  CHECK(b.hasTechnology(0, Plans));
  CHECK_EQ(b.researching(0), (int)Drill);
  CHECK_EQ(b.researchTurnsLeft(0), 2);
  Json again;
  std::string err;
  CHECK(buildCampaignSave(r.g.db, "res_save", pt, &b, 1000, again, err));
  CHECK_STR(again.dump(0), f.dump(0));
}

TEST(research_campaign_save_refuses_damaged_carried_technologies) {
  Run r;
  CHECK(begin(r, "camp_res_dmg"));
  CHECK(r.startBattle());
  r.b.sides[0].researched[Drill] = 1;
  r.b.sides[0].researched[Plans] = 1;
  CHECK(r.winAndFinish() == ChapterResult::Won);
  Json good = saveOf(r, false);
  Playthrough pt;
  Battle b;
  bool has = false;
  CHECK_STR(restore(r, good, pt, b, has), "");
  const std::vector<std::string> key = {"playthrough", "sides", "#0", "research"};
  struct Case {
    const char* what;
    Json value;
    const char* mention;
  } cases[] = {
      {"not a list", num(1), "playthrough.sides[0].research"},
      {"an entry that is text", [] { Json a = Json::makeArray(); a.push(Json::makeString("drill")); return a; }(), "research[0]"},
      {"an unknown technology", list({0, 5}), "research[1]"},
      {"a negative one", list({-1}), "research[0]"},
      {"a fraction", list({0.5}), "research[0]"},
      {"out of order", list({1, 0}), "ascending"},
      {"listed twice", list({0, 0}), "ascending"},
  };
  for (const Case& c : cases) {
    std::string err = restore(r, withValue(good, key, c.value), pt, b, has);
    if (err.find(c.mention) == std::string::npos) {
      std::printf("  %s: expected \"%s\", got \"%s\"\n", c.what, c.mention, err.c_str());
      CHECK(false);
    }
  }
  // A game without research refuses the key.
  Run plain;
  CHECK(plain.begin("camp_res_dmg_plain"));
  CHECK(plain.startBattle() && plain.winAndFinish() == ChapterResult::Won);
  Json f = saveOf(plain, false);
  Playthrough p2;
  Battle b2;
  std::string err = restore(plain, withValue(f, key, list({0})), p2, b2, has);
  CHECK(err.find("the game has no research") != std::string::npos);
}
