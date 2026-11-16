// Supports in a campaign (docs/SRPG.md section 6 "State", "Story and campaign"): the closeness carried from
// chapter to chapter, a lost battle changing nothing, the camp between chapters, scenes, and the saves.
#include "campaign_run.h"
#include "json_edit.h"
#include "srpg_supports_fixture.h"

using namespace camptest;

namespace {

// ch1 ford: eirik (0,1) and lyra (0,0) stand side by side on blue; wanderer joins blue at the start, scout is added.
const char* kCampaign = R"({
  "name": "Supports", "rules": { "fallen": "gone", "onDefeat": "retry" },
  "chapters": [
    { "id": "ch1", "map": "ford",
      "after": [ { "addSupport": ["eirik", "wanderer"], "by": 6 },
                 { "if": [ { "support": ["eirik", "wanderer"], "op": ">=", "value": 1 } ], "then": [ { "say": "Bonded.", "speaker": "eirik" } ] } ] },
    { "id": "ch2", "map": "mill", "before": [ { "say": "Next." } ] },
    { "id": "ch3", "map": "keep" } ] })";

// The fixture's ford and mill with characters in place of plain named units (a unit whose id is a character's must
// say `character`). Ford: eirik (0,1) and lyra (0,0) side by side; wanderer joins blue at the start, scout is added.
std::string fordWithCharacters() {
  return mapJson(6, 3, sides("100", "[]"),
                 R"([ {"character":"eirik","side":"blue","x":0,"y":1}, {"character":"lyra","side":"blue","x":0,"y":0},
                      {"character":"wanderer","side":"red","x":5,"y":1}, {"type":"grunt","side":"red","x":5,"y":0} ])",
                 R"([ {"id":"recruit","when":{"battle":"start"},"do":[ {"changeSide":"wanderer","side":"blue"},
                      {"addUnit":{"character":"scout","side":"blue","x":1,"y":1}} ]} ])");
}
std::string millWithCharacters() {
  return mapJson(6, 3, sides("0", "[]"),
                 R"([ {"character":"eirik","side":"blue","x":0,"y":1}, {"character":"lyra","side":"blue","x":0,"y":0},
                      {"character":"wanderer","side":"blue","x":0,"y":2}, {"character":"scout","side":"blue","x":1,"y":2},
                      {"type":"brute","side":"red","x":5,"y":1} ])");
}

struct SupRun : Run {
  bool start(const char* folder, const char* conversations = "both") {
    g.fordJson = fordWithCharacters();
    g.millJson = millWithCharacters();
    g.charactersJson = suptest::kCharacters;
    std::string s = suptest::kSupports;
    const size_t at = s.find("\"both\"");
    s.replace(at, 6, std::string("\"") + conversations + "\"");
    g.supportsJson = s;
    return begin(folder, kCampaign);
  }
  // Wins chapter 1 after blue ended its turn once (eirik and lyra side by side: +5 for that pair).
  bool winChapterOne() {
    if (!startBattle()) return false;
    b.apply(Action::endTurn(), ev);
    b.apply(Action::endTurn(), ev);
    return winAndFinish() == ChapterResult::Won;
  }
  std::vector<int> pending() const {
    std::vector<int> p;
    pendingSupports(g.db, pt, p);
    return p;
  }
};

}  // namespace

TEST(supports_campaign_starts_every_pair_at_its_start_points) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK((r.pt.supports.points == std::vector<int>{0, 0, 40}));
  CHECK((r.pt.supports.level == std::vector<uint8_t>{0, 0, 0}));  // pair 3's level has a conversation to play
}

TEST(supports_campaign_carries_closeness_from_chapter_to_chapter) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.startBattle());
  CHECK((r.b.supports.points == r.pt.supports.points));  // the battle starts from what the playthrough carries
  CHECK(r.winChapterOne());
  CHECK_EQ(r.pt.supports.points[0], 5);  // eirik and lyra ended a turn next to each other
  CHECK_EQ(r.pt.supports.level[0], 0);
  CHECK(r.scene(SceneKind::After));  // the scene adds 6 to eirik and wanderer: their level has no conversation
  CHECK_EQ(r.pt.supports.points[1], 6);
  CHECK_EQ(r.pt.supports.level[1], 1);
  CHECK_STR(r.said(), "Eirik|Bonded.;");
  // Chapter 2 starts from it.
  CHECK(r.startBattle());
  CHECK((r.b.supports.points == r.pt.supports.points) && (r.b.supports.level == r.pt.supports.level));
}

TEST(supports_campaign_a_lost_battle_changes_nothing) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.startBattle());
  r.b.apply(Action::endTurn(), r.ev);
  r.b.apply(Action::endTurn(), r.ev);
  CHECK_EQ(r.b.supports.points[0], 5);
  const SupportState before = r.pt.supports;
  r.decide(false);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Retry);
  CHECK(r.pt.supports.points == before.points && r.pt.supports.level == before.level);
  CHECK(r.startBattle());  // the retry starts from the same closeness
  CHECK_EQ(r.b.supports.points[0], 0);
}

TEST(supports_camp_offers_ready_conversations_of_pairs_in_the_party_and_plays_them) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.pending().empty());  // nobody is in the roster yet
  CHECK(r.winChapterOne());
  CHECK(r.scene(SceneKind::After));
  CHECK((r.pending() == std::vector<int>{2}));  // lyra and scout: 40 points from the start, a conversation waits
  std::vector<SceneLine> lines;
  CHECK(!playSupport(r.g.db, r.pt, 0, lines));  // not ready: not pending
  CHECK(lines.empty());
  CHECK(playSupport(r.g.db, r.pt, 2, lines));
  CHECK_EQ(lines.size(), (size_t)1);
  if (lines.size() == 1) {
    CHECK_STR(lines[0].text, "Hello again.");
    CHECK_STR(lines[0].speaker, "Old friend");
  }
  CHECK_EQ(r.pt.supports.level[2], 1);
  CHECK(r.pending().empty());
  CHECK(!playSupport(r.g.db, r.pt, 2, lines));
}

TEST(supports_camp_names_the_roster_member_who_speaks) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.winChapterOne());
  r.pt.supports.points[0] = 12;  // eirik and lyra: a conversation with both of them speaking
  std::vector<int> p = r.pending();
  CHECK((p == std::vector<int>{0, 2}));
  std::vector<SceneLine> lines;
  CHECK(playSupport(r.g.db, r.pt, 0, lines));
  CHECK_EQ(lines.size(), (size_t)2);
  if (lines.size() == 2) {
    CHECK_STR(lines[0].speakerId, "eirik");
    CHECK_STR(lines[0].speaker, "Eirik");  // the roster's own name
    CHECK_STR(lines[1].speaker, "Lyra");
  }
  // The next level (30 points, no conversation) comes at once when its points are reached.
  r.pt.supports.points[0] = 30;
  CHECK(r.pending().size() == 1);
  CHECK_EQ(r.pt.supports.level[0], 1);
}

TEST(supports_camp_skips_pairs_with_someone_who_fell_and_games_that_play_in_battle_only) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.winChapterOne());
  for (RosterUnit& u : r.pt.roster)
    if (u.id == "scout") u.gone = true;
  CHECK(r.pending().empty());
  SupRun q;
  CHECK(q.start("sup_camp_battle", "battle"));
  CHECK(q.winChapterOne());
  CHECK(q.pending().empty());  // conversations "battle": the camp has none
}

TEST(supports_scene_points_do_not_reach_a_character_who_fell) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.winChapterOne());
  for (RosterUnit& u : r.pt.roster)
    if (u.id == "wanderer") u.gone = true;
  CHECK(r.scene(SceneKind::After));
  CHECK_EQ(r.pt.supports.points[1], 0);
  CHECK(r.said().empty());
}

TEST(supports_a_conversation_only_in_camp_waits_for_the_camp_in_a_campaign_battle) {
  SupRun r;
  CHECK(r.start("sup_camp_only", "camp"));
  CHECK(r.startBattle());
  // lyra and scout have 40 points and a conversation; in battle it is refused, in the camp it is offered.
  const int lyra = r.unit("lyra"), scout = r.unit("scout");
  std::string why;
  r.b.validate(Action::support(lyra, scout), &why);
  CHECK_STR(why, "support conversations are played between chapters in this game");
  r.b.apply(Action::endTurn(), r.ev);
  r.b.apply(Action::endTurn(), r.ev);
  CHECK(r.winAndFinish() == ChapterResult::Won);
  CHECK(r.scene(SceneKind::After));
  CHECK_EQ(r.pending().size(), (size_t)1);
}

// ---- saves ----

TEST(supports_campaign_save_round_trips_and_writes_nothing_until_something_happened) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  Json j;
  savePlaythrough(r.g.db, r.pt, j);
  CHECK(!j.has("supports"));  // the start state: no key
  CHECK(r.winChapterOne());
  savePlaythrough(r.g.db, r.pt, j);
  CHECK(j.has("supports"));
  CHECK_EQ(j["supports"]["points"].size(), (size_t)3);
  CHECK_EQ(j["supports"]["levels"].size(), (size_t)3);
  Playthrough back;
  std::string err;
  CHECK(restorePlaythrough(r.g.db, j, back, err));
  CHECK(back.supports.points == r.pt.supports.points && back.supports.level == r.pt.supports.level);
  Json again;
  savePlaythrough(r.g.db, back, again);
  CHECK_STR(again.dump(0), j.dump(0));
  // A save without the key restores the starting closeness.
  CHECK(restorePlaythrough(r.g.db, jsonedit::without(j, "supports"), back, err));
  CHECK((back.supports.points == std::vector<int>{0, 0, 40}));
}

TEST(supports_campaign_save_refuses_damaged_closeness) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.winChapterOne());
  Json j;
  savePlaythrough(r.g.db, r.pt, j);
  Playthrough back;
  std::string err;
  struct Case { std::vector<std::string> path; Json value; const char* where; };
  Json two = Json::makeArray();
  two.push(Json::makeNumber(1));
  two.push(Json::makeNumber(2));
  const Case cases[] = {
      {{"supports", "points"}, two, "playthrough.supports.points: has 2 entries, expected 3"},
      {{"supports", "levels"}, two, "playthrough.supports.levels: has 2 entries, expected 3"},
      {{"supports", "points", "#0"}, Json::makeNumber(-1), "playthrough.supports.points[0]"},
      {{"supports", "points", "#0"}, Json::makeNumber(1000001), "playthrough.supports.points[0]"},
      {{"supports", "points", "#0"}, Json::makeString("x"), "playthrough.supports.points[0]"},
      {{"supports", "levels", "#0"}, Json::makeNumber(3), "playthrough.supports.levels[0]"},
      {{"supports", "levels", "#0"}, Json::makeNumber(1.5), "playthrough.supports.levels[0]"},
      {{"supports", "levels", "#0"}, Json::makeNumber(2), "playthrough.supports.levels[0]: level 2 needs more points"},  // 5 points
      {{"supports", "x"}, Json::makeNumber(1), "playthrough.supports: unknown key \"x\""},
      {{"supports"}, Json::makeNumber(1), "playthrough.supports"},
  };
  for (const Case& c : cases) {
    CHECK(!restorePlaythrough(r.g.db, jsonedit::withValue(j, c.path, c.value), back, err));
    if (err.find(c.where) == std::string::npos) std::printf("  got: %s\n  wanted: %s\n", err.c_str(), c.where);
    CHECK(err.find(c.where) != std::string::npos);
  }
}

TEST(supports_battle_save_round_trips_and_refuses_damage) {
  SupRun r;
  CHECK(r.start("sup_camp"));
  CHECK(r.startBattle());
  Json j;
  saveBattleState(r.b, j);
  CHECK(!j.has("supports"));  // nothing has happened yet
  r.b.apply(Action::endTurn(), r.ev);
  r.b.apply(Action::endTurn(), r.ev);
  saveBattleState(r.b, j);
  CHECK(j.has("supports"));
  Battle b2;
  std::string err;
  CHECK(prepareChapterBattle(r.g.db, r.pt, r.g.formulas, 1, b2, err));
  CHECK(restoreBattleState(b2, j, err));
  CHECK(b2.supports.points == r.b.supports.points && b2.supports.level == r.b.supports.level);
  CHECK_EQ(b2.supports.points[0], 5);
  const char* bad[][2] = {{"points", "3"}, {"levels", "[0,0]"}};
  for (const auto& c : bad) {
    Json parsed;
    Json::parse(c[1][0] == '[' ? c[1] : "7", parsed, err);
    Battle b3;
    CHECK(prepareChapterBattle(r.g.db, r.pt, r.g.formulas, 1, b3, err));
    CHECK(!restoreBattleState(b3, jsonedit::withValue(j, {"supports", c[0]}, parsed), err));
    CHECK(err.find("battle.supports." + std::string(c[0])) != std::string::npos);
  }
  Battle b4;
  CHECK(prepareChapterBattle(r.g.db, r.pt, r.g.formulas, 1, b4, err));
  CHECK(!restoreBattleState(b4, jsonedit::withValue(j, {"supports", "levels", "#1"}, Json::makeNumber(1)), err));  // 0 points
  CHECK(err.find("battle.supports.levels[1]") != std::string::npos);
}
