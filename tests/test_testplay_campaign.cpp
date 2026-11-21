// Test playthroughs (docs/SAVES.md "Test playthroughs"): a new playthrough at a chapter with the approximated roster, and the test level
// applied to a roster and to a chapter's battle. On the campaign fixture (campaign_fixture.h): ford -> mill -> (keep | keep) -> skirmish.
#include "campaign_run.h"
#include "core/campaign/testplay.h"

using namespace camptest;

namespace {

bool inRoster(const Roster& r, const char* id) {
  for (const RosterUnit& u : r)
    if (u.id == id) return true;
  return false;
}
const RosterUnit* find(const Roster& r, const char* id) {
  for (const RosterUnit& u : r)
    if (u.id == id) return &u;
  return nullptr;
}

// Two parties: the company plays the ford, the northern road (the mill) belongs to its own party, the keep is the company again.
const char* kTwoParties = R"({
  "name": "Parties", "rules": { "fallen": "gone", "onDefeat": "retry" },
  "parties": [ { "id": "main", "name": "The company" }, { "id": "north", "name": "The northern road" } ],
  "chapters": [
    { "id": "ch1", "name": "One", "map": "ford" },
    { "id": "ch2", "name": "Two", "map": "mill", "before": [ { "switchParty": "north" } ] },
    { "id": "ch3", "name": "Three", "map": "keep", "before": [ { "switchParty": "main" } ] } ] })";

}  // namespace

TEST(testplay_starts_at_a_chapter_with_the_units_the_earlier_maps_place) {
  Game g;
  CHECK(g.load("testplay_roster"));
  Playthrough pt;
  std::string note, err;
  CHECK(beginPlaythroughAt(g.db, pt, "ch3a", "", note, err));
  CHECK_EQ(pt.chapter, g.db.campaign->findChapter("ch3a"));
  CHECK(pt.phase == Phase::Before);
  CHECK(note.empty());  // ch1 -> ch2 -> ch3a is the main path
  // ch1: eirik and lyra (named, on blue), scout (addUnit on blue); "Extra" has no id and the wanderer is the enemy's there.
  // ch2: the wanderer stands on blue for the first time: the recruit of chapter 2.
  CHECK_EQ((int)pt.roster.size(), 4);
  CHECK(inRoster(pt.roster, "eirik") && inRoster(pt.roster, "lyra") && inRoster(pt.roster, "scout") && inRoster(pt.roster, "wanderer"));
  const RosterUnit* eirik = find(pt.roster, "eirik");
  CHECK(eirik && eirik->level == 1 && eirik->name == "Eirik");
  CHECK_EQ((int)eirik->items.size(), 2);  // the items written on the first placement (ring, potion)
  CHECK_EQ(eirik->items[0].item, g.db.findItem("ring"));
  const RosterUnit* scout = find(pt.roster, "scout");
  CHECK(scout && scout->level == 2);  // the addUnit's own level
  // Funds: only what the walked scenes gave (ch1's after scene: 25 gold); the maps' own gold and storage are not carried. The storage is empty.
  const CarriedSide* blue = nullptr;
  for (const CarriedSide& s : pt.sides)
    if (s.id == "blue") blue = &s;
  CHECK(blue && blue->resources[0] == 25 && blue->storage.empty());
  // Switches and variables are unset except what the walked scenes set (chapters + 1 twice).
  CHECK(pt.switches[(size_t)g.db.findSwitch("bridge_cut")] == 0 && pt.switches[(size_t)g.db.findSwitch("met_wanderer")] == 0);
  CHECK_EQ(pt.variables[(size_t)g.db.findVariable("chapters")], 2);
}

TEST(testplay_a_chapter_off_the_main_path_falls_back_to_the_list_order_and_says_so) {
  Game g;
  CHECK(g.load("testplay_branch"));
  Playthrough pt;
  std::string note, err;
  // ch3b is only reached by the mill's "goto" when bridge_cut is set; by default ch2 goes on to ch3a, so ch3b is off the main path.
  CHECK(beginPlaythroughAt(g.db, pt, "ch3b", "", note, err));
  CHECK(!note.empty());
  CHECK_EQ(pt.chapter, g.db.campaign->findChapter("ch3b"));
  CHECK(inRoster(pt.roster, "eirik") && inRoster(pt.roster, "wanderer") && inRoster(pt.roster, "scout"));
  // ch4 is on the main path (ch3a's "next"): no note.
  Playthrough pt4;
  note.clear();
  CHECK(beginPlaythroughAt(g.db, pt4, "ch4", "", note, err));
  CHECK(note.empty());
  CHECK_EQ(pt4.chapter, g.db.campaign->findChapter("ch4"));
}

TEST(testplay_the_first_chapter_needs_no_walk_and_a_wrong_id_is_refused) {
  Game g;
  CHECK(g.load("testplay_first"));
  Playthrough pt;
  std::string note, err;
  CHECK(beginPlaythroughAt(g.db, pt, "ch1", "", note, err));
  CHECK(pt.roster.empty() && pt.chapter == 0 && pt.phase == Phase::Before);
  CHECK(!beginPlaythroughAt(g.db, pt, "nope", "", note, err));
  CHECK(err.find("nope") != std::string::npos && err.find("ch1") != std::string::npos);
  Game none;
  CHECK(none.load("testplay_nocampaign", ""));
  Playthrough p2;
  CHECK(!beginPlaythroughAt(none.db, p2, "ch1", "", note, err));
}

TEST(testplay_characters_join_the_party_that_plays_the_chapter_where_they_first_appear) {
  Game g;
  CHECK(g.load("testplay_parties", kTwoParties));
  Playthrough pt;
  std::string note, err;
  CHECK(beginPlaythroughAt(g.db, pt, "ch3", "", note, err));
  CHECK(note.empty());
  CHECK(pt.party == "north");  // the walk leaves the party ch2 made active; ch3's own scene switches it as the player runs it
  std::vector<SceneLine> lines;
  CHECK(runScene(g.db, pt, SceneKind::Before, lines));
  CHECK(pt.party == "main");
  CHECK(inRoster(pt.roster, "eirik") && inRoster(pt.roster, "lyra") && inRoster(pt.roster, "scout"));
  CHECK(!inRoster(pt.roster, "wanderer"));  // first seen on blue in ch2: the northern road's
  CHECK_EQ((int)pt.parked.size(), 1);
  CHECK(pt.parked[0].id == "north" && inRoster(pt.parked[0].roster, "wanderer") && !inRoster(pt.parked[0].roster, "eirik"));
}

TEST(testplay_levelroster_matches_the_levelling_function_and_leaves_higher_units_alone) {
  Game g;
  g.fordJson = fordMap();
  CHECK(g.load("testplay_levelroster"));
  Playthrough pt;
  std::string note, err;
  CHECK(beginPlaythroughAt(g.db, pt, "ch3a", "", note, err));
  RosterUnit& scout = pt.roster[(size_t)(find(pt.roster, "scout") - &pt.roster[0])];
  scout.level = 9;  // already above
  scout.xp = 17;
  const std::vector<int> scoutStats = scout.stats;
  CHECK_EQ(levelRoster(g.db, pt, 6), 3);  // eirik, lyra, wanderer
  const RosterUnit* eirik = find(pt.roster, "eirik");
  CHECK_EQ(eirik->level, 6);
  CHECK_EQ(eirik->xp, 0);
  // The hero's growths are hp 100 / atk 100: five levels = +5 each, from the map's 20 / 6.
  CHECK_EQ(eirik->stats[(size_t)g.db.findStat("hp")], 25);
  CHECK_EQ(eirik->stats[(size_t)g.db.findStat("atk")], 11);
  const RosterUnit* scout2 = find(pt.roster, "scout");
  CHECK(scout2->level == 9 && scout2->xp == 17 && scout2->stats == scoutStats);
  CHECK_EQ(levelRoster(g.db, pt, 6), 0);  // idempotent
}

TEST(testplay_applychaptertestlevel_auto_uses_the_chapters_hostile_units_and_a_number_is_taken_as_is) {
  Game g;
  g.keepJson = mapJson(6, 3, sides("0", "[]"),
                       heroesOnBlue(R"({"type":"grunt","side":"red","x":5,"y":0,"level":5},{"type":"grunt","side":"red","x":5,"y":1,"level":6})"));
  CHECK(g.load("testplay_auto"));
  Playthrough pt;
  std::string note, err;
  CHECK(beginPlaythroughAt(g.db, pt, "ch3a", "", note, err));
  pt.phase = Phase::Battle;  // the chapter's before scene has run
  CHECK_EQ(applyChapterTestLevel(g.db, pt, g.formulas, 1, -1, err), 8);  // (5 + 6) / 2 = 5.5 -> 6, plus 2
  CHECK_EQ(find(pt.roster, "eirik")->level, 8);
  // The battle built from it: every human unit is at least 6, the computer's are as the map says.
  Battle b;
  b.setTestLevel(8);
  std::vector<Event> ev;
  CHECK(startChapterBattle(g.db, pt, g.formulas, 1, b, ev, err));
  CHECK_EQ(b.units[(size_t)b.unitByStoryId("eirik")].level, 8);
  CHECK_EQ(b.units[(size_t)b.unitByStoryId("wanderer")].level, 8);
  // Off does nothing; a number raises the roster to it and is returned.
  Playthrough pt2;
  CHECK(beginPlaythroughAt(g.db, pt2, "ch3a", "", note, err));
  pt2.phase = Phase::Battle;
  CHECK_EQ(applyChapterTestLevel(g.db, pt2, g.formulas, 1, 0, err), 0);
  CHECK_EQ(find(pt2.roster, "eirik")->level, 1);
  CHECK_EQ(applyChapterTestLevel(g.db, pt2, g.formulas, 1, 4, err), 4);
  CHECK_EQ(find(pt2.roster, "eirik")->level, 4);
  // A later chapter with `auto` never lowers a unit: the earlier level stays.
  CHECK_EQ(applyChapterTestLevel(g.db, pt2, g.formulas, 1, -1, err), 8);
  CHECK_EQ(find(pt2.roster, "eirik")->level, 8);
}

// Weapon ranks follow the roster's level too, once per level actually gained (a later chapter raises only the difference).
TEST(testplay_levelroster_raises_weapon_ranks_by_the_levels_gained_and_never_twice) {
  Game g;
  g.fordJson = fordMap();
  CHECK(g.load("testplay_roster_ranks"));
  // Ladder E 0 .. B 120. The hero wields swords (class value 10), the ranger bows.
  g.db.ranks.ladder = {{"e", "E", 0}, {"d", "D", 30}, {"c", "C", 70}, {"b", "B", 120}};
  g.db.ranks.groups = {"sword", "bow"};
  const int sword = g.db.findWeapon("sword"), bow = g.db.findWeapon("bow");
  g.db.weapons[(size_t)sword].group = "sword";
  g.db.weapons[(size_t)sword].rankGroup = 0;
  g.db.weapons[(size_t)bow].group = "bow";
  g.db.weapons[(size_t)bow].rankGroup = 1;
  g.db.unitTypes[(size_t)g.db.findUnitType("hero")].wields = {"sword"};
  g.db.unitTypes[(size_t)g.db.findUnitType("hero")].rankStart = {10, 0};
  const int perLevel = (120 + 2 * g.db.rules.maxLevel - 1) / (2 * g.db.rules.maxLevel);
  Playthrough pt;
  std::string note, err;
  CHECK(beginPlaythroughAt(g.db, pt, "ch3a", "", note, err));
  CHECK_EQ(levelRoster(g.db, pt, 6), 4);  // eirik, lyra, wanderer, scout
  const RosterUnit* eirik = find(pt.roster, "eirik");
  CHECK_EQ((int)eirik->weaponXp.size(), 2);
  CHECK_EQ(eirik->weaponXp[0], 10 + 5 * perLevel);  // from the class's 10, five levels
  CHECK_EQ(eirik->weaponXp[1], 0);                  // a hero has no bow rank
  CHECK_EQ(levelRoster(g.db, pt, 6), 0);            // again: no levels, no more experience
  CHECK_EQ(find(pt.roster, "eirik")->weaponXp[0], 10 + 5 * perLevel);
  CHECK_EQ(levelRoster(g.db, pt, 9), 4);            // the next chapter: only the 3 new levels
  CHECK_EQ(find(pt.roster, "eirik")->weaponXp[0], 10 + 8 * perLevel);
}

// The roster promotes by the test level too (rules.promotion.testAt) and a promoted unit is "already promoted" in the next chapter.
TEST(testplay_levelroster_promotes_once_and_stays_consistent_across_chapters) {
  Game g;
  g.fordJson = fordMap();
  CHECK(g.load("testplay_roster_promote"));
  const int hero = g.db.findUnitType("hero");
  UnitTypeDef champ = g.db.unitTypes[(size_t)hero];
  champ.id = "champion";
  champ.name = "Champion";
  g.db.unitTypes.push_back(champ);
  PromotionDef up;
  up.to = (int)g.db.unitTypes.size() - 1;
  up.atLevel = PromoteAtLevel::Offer;
  up.fromLevel = 20;
  g.db.unitTypes[(size_t)hero].promotions = {up};
  g.db.rules.promotion.testAt = 3;
  Playthrough pt;
  std::string note, err;
  CHECK(beginPlaythroughAt(g.db, pt, "ch3a", "", note, err));
  CHECK_EQ(levelRoster(g.db, pt, 5), 4);
  const RosterUnit* eirik = find(pt.roster, "eirik");
  CHECK(eirik->type == up.to && eirik->level == 3);  // 1 + (5 - 3)
  CHECK(find(pt.roster, "lyra")->level == 5 && find(pt.roster, "lyra")->type == g.db.findUnitType("ranger"));  // cannot promote: plain
  CHECK_EQ(levelRoster(g.db, pt, 5), 0);  // again: nobody is raised or promoted twice
  CHECK_EQ(levelRoster(g.db, pt, 7), 4);  // the next chapter: already promoted, effective 3 + 3 = 6 -> level 4
  CHECK(find(pt.roster, "eirik")->type == up.to && find(pt.roster, "eirik")->level == 4);
}
