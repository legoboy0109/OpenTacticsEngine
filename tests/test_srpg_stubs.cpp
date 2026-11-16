// Step F1 of docs/SRPG.md: until an area's step lands, a game that uses it is refused with a plain message, in
// its data, in its story and in its saves. Each later step replaces its own refusals by the real rules.
#include <functional>

#include "campaign_fixture.h"
#include "core/campaign/campaign_save.h"
#include "json_edit.h"
#include "story_fixture.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

const char* kNotYet = "is not supported by this build yet";

// The campaign fixture's game with one file replaced or added; the loader's message ("" = it loaded).
std::string loadWith(const char* rel, const std::string& text) {
  camptest::Game g;
  if (!g.load("srpg_stubs")) return "the fixture did not load";
  std::string path = g.dir + "/" + rel, old, err;
  bool had = readTextFile(path, old);
  writeTextFile(path, text);
  Database db;
  bool ok = loadGame(g.dir, db, err);
  if (had) writeTextFile(path, old);
  else removeFile(path);
  return ok ? "" : err;
}

// `text` with the first `from` replaced by `to`.
std::string swapped(std::string text, const std::string& from, const std::string& to) {
  size_t at = text.find(from);
  if (at == std::string::npos) return "the test's text to replace is missing";
  return text.replace(at, from.size(), to);
}

void refused(const std::string& err, const std::string& first, const std::string& second = kNotYet) {
  if (!has(err, first) || !has(err, second)) std::printf("  got: %s\n  wanted: %s ... %s\n", err.c_str(), first.c_str(), second.c_str());
  CHECK(has(err, first) && has(err, second));
}

story::StoryWorld storyWorld() {
  return story::StoryWorld({".........", ".........", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}},
                           {"eirik", "gorm"});
}
std::string eventError(const std::string& event) {
  story::StoryWorld w = storyWorld();
  return w.load("[" + event + "]");
}

}  // namespace

// ---- data files ----

TEST(srpg_stub_a_game_without_the_new_files_loads) { CHECK_STR(loadWith("data/terrain.json", camptest::kTerrain), ""); }

TEST(srpg_stub_refuses_the_new_data_files) {
  refused(loadWith("data/skills.json", R"({ "skills": [] })"), "data/skills.json");
  refused(loadWith("data/abilities.json", R"({ "abilities": [] })"), "data/abilities.json");
  refused(loadWith("data/supports.json", R"({ "pairs": [] })"), "data/supports.json");
}

TEST(srpg_stub_refuses_the_new_rules_blocks) {
  refused(loadWith("game.json", swapped(camptest::kGameJson, "\"counterAttacks\": true", "\"counterAttacks\": true, \"rescue\": {\"mode\":\"rescue\"}")),
          "rules.rescue");
  refused(loadWith("game.json", swapped(camptest::kGameJson, "\"counterAttacks\": true", "\"counterAttacks\": true, \"promotion\": {\"keepLevel\":true}")),
          "rules.promotion");
}

TEST(srpg_stub_refuses_the_new_keys_of_units_weapons_and_items) {
  const std::string hero = "\"id\":\"hero\",\"name\":\"Hero\"";
  for (const char* key : {"skills", "learns", "abilities", "promotions", "pairBonus"})
    refused(loadWith("data/units.json", swapped(camptest::kUnits, hero, hero + ",\"" + key + "\":[]")), "unit \"hero\"",
            std::string("\"") + key + "\" " + kNotYet);
  refused(loadWith("data/weapons.json", swapped(camptest::kWeapons, "\"id\":\"sword\"", "\"id\":\"sword\",\"skills\":[]")),
          "weapon \"sword\"", std::string("\"skills\" ") + kNotYet);
  refused(loadWith("data/items.json", swapped(camptest::kItems, "\"id\":\"ring\"", "\"id\":\"ring\",\"skills\":[]")), "\"ring\"",
          std::string("\"skills\" ") + kNotYet);
  refused(loadWith("data/items.json", swapped(camptest::kItems, "\"id\":\"tonic\"", "\"id\":\"tonic\",\"promotes\":true")), "\"tonic\"",
          std::string("\"promotes\" ") + kNotYet);
  // The keys belong to one kind of item each; that check comes first and stays.
  refused(loadWith("data/items.json", swapped(camptest::kItems, "\"id\":\"ring\"", "\"id\":\"ring\",\"promotes\":true")), "\"ring\"", "usable");
  refused(loadWith("data/items.json", swapped(camptest::kItems, "\"id\":\"tonic\"", "\"id\":\"tonic\",\"skills\":[]")), "\"tonic\"", "accessory");
}

// ---- story ----

TEST(srpg_stub_refuses_the_new_story_commands_conditions_and_triggers) {
  auto command = [](const std::string& c) { return eventError("{\"id\":\"ev\",\"when\":{\"battle\":\"start\"},\"do\":[" + c + "]}"); };
  refused(command(R"({"promote":"eirik"})"), "event \"ev\"", std::string("\"promote\" ") + kNotYet);
  refused(command(R"({"teachSkill":"eirik","skill":"vantage"})"), "event \"ev\"", std::string("\"teachSkill\" ") + kNotYet);
  refused(command(R"({"forgetSkill":"eirik","skill":"vantage"})"), "event \"ev\"", std::string("\"forgetSkill\" ") + kNotYet);
  refused(command(R"({"addSupport":["eirik","gorm"],"points":10})"), "event \"ev\"", std::string("\"addSupport\" ") + kNotYet);
  refused(command(R"({"promote":"eirik","teachSkill":"eirik"})"), "event \"ev\"", "has two commands");
  refused(eventError(R"({"id":"ev","when":{"battle":"start"},"if":[{"hasSkill":"vantage","unit":"eirik"}],"do":[]})"), "event \"ev\"",
          std::string("\"hasSkill\" ") + kNotYet);
  refused(eventError(R"({"id":"ev","when":{"battle":"start"},"if":[{"support":["eirik","gorm"],"level":1}],"do":[]})"), "event \"ev\"",
          std::string("\"support\" ") + kNotYet);
  refused(eventError(R"({"id":"ev","when":{"promoted":"eirik"},"do":[]})"), "event \"ev\"", std::string("\"promoted\" ") + kNotYet);
  refused(eventError(R"({"id":"ev","when":{"support":["eirik","gorm"]},"do":[]})"), "event \"ev\"", std::string("\"support\" ") + kNotYet);
  // What is still not a command, condition or trigger keeps its message.
  refused(command(R"({"dance":1})"), "event \"ev\"", "is not a command");
}

// ---- saves ----

TEST(srpg_stub_a_battle_save_with_the_new_keys_is_refused) {
  savetest::Rig live;
  CHECK(live.loadPlain("twin_rivers"));
  live.start();
  Json good;
  saveBattleState(live.battle, good);
  struct Case { std::vector<std::string> path; Json value; const char* where; const char* why; };
  Json list = Json::makeArray();
  const Case cases[] = {
      {{"units", "#0", "skills"}, list, "units[0].skills", "this game has no skills"},
      {{"units", "#1", "moveSpent"}, Json::makeNumber(1), "units[1].moveSpent", "this game has no skills"},
      {{"units", "#0", "acted"}, Json::makeBool(true), "units[0].acted", "this game has no skills"},
      {{"units", "#0", "refreshed"}, Json::makeBool(true), "units[0].refreshed", "this game has no skills"},
      {{"units", "#0", "abilities"}, list, "units[0].abilities", "this game has no abilities"},
      {{"units", "#0", "summon"}, Json::makeObject(), "units[0].summon", "this game has no abilities"},
      {{"units", "#0", "carrying"}, Json::makeNumber(1), "units[0].carrying", "this game has no rescue or pair up"},
      {{"supports"}, list, "supports", "this game has no supports"},
  };
  for (const Case& c : cases) {
    savetest::Rig fresh;
    CHECK(fresh.loadPlain("twin_rivers"));
    std::string err;
    CHECK(!restoreBattleState(fresh.battle, jsonedit::withValue(good, c.path, c.value), err));
    refused(err, c.where, c.why);
  }
  // The save itself has none of the keys and still restores.
  savetest::Rig fresh;
  CHECK(fresh.loadPlain("twin_rivers"));
  std::string err;
  CHECK(restoreBattleState(fresh.battle, good, err));
  CHECK_STR(savetest::stateText(fresh.battle), savetest::stateText(live.battle));
  for (const char* key : {"skills", "moveSpent", "acted", "refreshed", "abilities", "summon", "carrying"})
    CHECK(!good["units"][0].has(key));
  CHECK(!good.has("supports"));
}

TEST(srpg_stub_a_playthrough_with_the_new_keys_is_refused) {
  camptest::Game g;
  CHECK(g.load("srpg_stubs_playthrough"));
  Playthrough pt;
  CHECK(pt.begin(g.db));
  std::vector<SceneLine> lines;
  runScene(g.db, pt, SceneKind::Before, lines);
  Battle b;
  std::vector<Event> ev;
  std::string err;
  CHECK(startChapterBattle(g.db, pt, g.formulas, 5, b, ev, err));
  b.over = true;
  b.winnerTeam = b.humanTeam();
  CHECK(finishChapterBattle(g.db, pt, b) == ChapterResult::Won);
  CHECK(!pt.roster.empty());
  if (pt.roster.empty()) return;
  Json good;
  savePlaythrough(g.db, pt, good);
  CHECK(!good.has("supports") && !good["roster"][0].has("skills"));
  Playthrough back;
  CHECK(restorePlaythrough(g.db, good, back, err));
  CHECK(!restorePlaythrough(g.db, jsonedit::withValue(good, {"roster", "#0", "skills"}, Json::makeArray()), back, err));
  refused(err, "roster[0].skills", "this game has no skills");
  CHECK(!restorePlaythrough(g.db, jsonedit::withValue(good, {"supports"}, Json::makeArray()), back, err));
  refused(err, "supports", "this game has no supports");
}
