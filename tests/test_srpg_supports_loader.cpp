// data/supports.json (docs/SRPG.md section 6 "Data"): what loads, and every refusal with its message; the hash
// treatment of the file; campaign scenes that use supports.
#include "campaign_run.h"
#include "core/battle/support_rules.h"
#include "srpg_supports_fixture.h"

using namespace camptest;

namespace {

bool has(const std::string& hay, const std::string& needle) {
  if (hay.find(needle) == std::string::npos) std::printf("  got: %s\n  wanted: %s\n", hay.c_str(), needle.c_str());
  return hay.find(needle) != std::string::npos;
}

// The campaign fixture with the given supports.json (and the standard characters); the loader's message.
std::string loadSupports(const std::string& supports, const char* characters = suptest::kCharacters) {
  Game g;
  g.charactersJson = characters;
  g.supportsJson = supports;
  std::string err;
  return g.load("sup_loader", kCampaignJson, &err) ? "" : err;
}

// The standard supports.json with one piece of text replaced.
std::string edited(const char* from, const char* to) {
  std::string s = suptest::kSupports;
  const size_t at = s.find(from);
  if (at == std::string::npos) return "the test's text to replace is missing: " + std::string(from);
  return s.replace(at, std::string(from).size(), to);
}

const char* kFile = "data/supports.json: ";

}  // namespace

TEST(supports_loader_reads_the_file) {
  Game g;
  g.charactersJson = suptest::kCharacters;
  g.supportsJson = suptest::kSupports;
  CHECK(g.load("sup_loader"));
  const SupportData& s = g.db.supports;
  CHECK_EQ(s.pairs.size(), (size_t)3);
  CHECK(s.rules.endTurnAdjacent == 5 && s.rules.fightTogether == 3 && s.rules.heal == 4 && s.rules.range == 3);
  CHECK(s.rules.conversations == SupportTalks::Both && s.rules.maxLevels == 0);
  CHECK_STR(ote::supportLevelName(s.rules, 2), "B");
  const SupportPairDef& p = s.pairs[0];
  CHECK(p.a == g.db.findCharacter("eirik") && p.b == g.db.findCharacter("lyra"));
  CHECK(p.levels.size() == 2 && p.levels[0].points == 10 && p.levels[1].points == 30);
  CHECK(p.levels[0].bonus.hit == 10 && p.levels[0].bonus.avoid == 5 && p.levels[1].bonus.damage == 1);
  CHECK(p.levels[1].bonus.stats.size() == g.db.stats.size() && p.levels[1].bonus.stats[(size_t)g.db.findStat("def")] == 1);
  CHECK_EQ(p.levels[0].talk.size(), (size_t)2);
  CHECK_STR(s.texts[(size_t)p.levels[0].talk[0].text], "You never miss.");
  CHECK_STR(p.levels[0].talk[0].speaker, "eirik");
  CHECK_STR(s.texts[(size_t)p.levels[0].talk[0].speakerName], "Eirik");  // kept for a speaker who is not on the board
  CHECK_EQ(s.pairs[2].start, 40);
  CHECK(g.db.findSupportPair(g.db.findCharacter("lyra"), g.db.findCharacter("eirik")) == 0);
  // A game without the file loads as before, with no pairs.
  Game none;
  CHECK(none.load("sup_loader_none"));
  CHECK(none.db.supports.pairs.empty() && none.db.supports.texts.empty());
}

TEST(supports_loader_defaults) {
  Game g;
  g.charactersJson = suptest::kCharacters;
  g.supportsJson = R"({ "pairs": [ { "a": "eirik", "b": "lyra", "levels": [ { "points": 1 } ] } ] })";
  CHECK(g.load("sup_loader"));
  const SupportRules& r = g.db.supports.rules;
  CHECK(r.range == 3 && r.maxLevels == 0 && r.conversations == SupportTalks::Both && r.levelNames.empty());
  CHECK(r.endTurnAdjacent == 0 && r.fightTogether == 0 && r.heal == 0);
  CHECK_STR(ote::supportLevelName(r, 2), "2");
}

TEST(supports_loader_refuses_the_file_without_characters_and_unknown_keys) {
  CHECK(has(loadSupports(suptest::kSupports, ""), std::string(kFile) + "the game has no characters (data/characters.json)"));
  CHECK(has(loadSupports(R"({ "pairs": [], "x": 1 })"), std::string(kFile) + "unknown key \"x\""));
  CHECK(has(loadSupports(R"({ "rules": { "x": 1 }, "pairs": [] })"), std::string(kFile) + "rules: unknown key \"x\""));
  CHECK(has(loadSupports(R"({ "rules": { "points": { "x": 1 } }, "pairs": [] })"), "unknown key \"x\""));
  CHECK(has(loadSupports(R"({ "rules": {} })"), "missing required \"pairs\""));
  CHECK(has(loadSupports(edited("\"start\": 0", "\"start\": 0, \"x\": 1")), "pair 1: unknown key \"x\""));
}

TEST(supports_loader_refuses_bad_rules) {
  auto rules = [](const char* body) { return loadSupports(std::string("{ \"rules\": {") + body + "}, \"pairs\": [] }"); };
  CHECK(has(rules(R"("conversations": "sometimes")"), "rules: \"conversations\" must be \"battle\", \"camp\" or \"both\""));
  CHECK(has(rules(R"("range": 0)"), "rules: \"range\" must be a whole number from 1 to 10"));
  CHECK(has(rules(R"("range": 11)"), "\"range\" must be a whole number from 1 to 10"));
  CHECK(has(rules(R"("maxLevels": 0)"), "\"maxLevels\" must be a whole number from 1 to 99"));
  CHECK(has(rules(R"("points": { "heal": -1 })"), "\"heal\" must be a whole number from 0 to 1000000"));
  CHECK(has(rules(R"("levelNames": [1])"), "\"levelNames\" entry 1 must be a string"));
  CHECK(has(rules(R"("levelNames": ["a","b","c","d","e","f"])"), "\"levelNames\" has more than 5 names"));
  CHECK(has(loadSupports(edited("[\"C\", \"B\", \"A\"]", "[\"C\"]")), std::string(kFile) + "rules: \"levelNames\" has 1 names but pair 1 has 2 levels"));
}

TEST(supports_loader_refuses_bad_pairs) {
  auto pair = [](const char* body) { return loadSupports(std::string("{ \"pairs\": [") + body + "] }"); };
  const char* ok = R"({ "a": "eirik", "b": "lyra", "levels": [ { "points": 20 } ] })";
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra", "levels": [ { "points": 20 } ] }, { "a": "eirik", "b": "x", "levels": [] })"),
            std::string(kFile) + "pair 2: unknown character \"x\""));
  CHECK(has(pair(R"({ "a": "eirik", "b": "eirik", "levels": [ { "points": 20 } ] })"), "pair 1: \"eirik\" and \"eirik\" are the same character"));
  CHECK(has(pair((std::string(ok) + R"(, { "a": "eirik", "b": "wanderer", "levels": [ { "points": 1 } ] }, { "a": "lyra", "b": "eirik", "levels": [ { "points": 1 } ] })").c_str()),
            "pair 3: \"lyra\" and \"eirik\" are already a pair (pair 1)"));
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra", "levels": [] })"), "pair 1 (\"eirik\", \"lyra\"): \"levels\" must have from 1 to 5 entries"));
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra", "levels": [ {"points":1},{"points":2},{"points":3},{"points":4},{"points":5},{"points":6} ] })"),
            "\"levels\" must have from 1 to 5 entries"));
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra" })"), "\"levels\" must have from 1 to 5 entries"));
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra", "levels": [ {"points":20}, {"points":20} ] })"),
            "pair 1 (\"eirik\", \"lyra\"): level 2: \"points\" must be more than level 1's (20)"));
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra", "start": -1, "levels": [ {"points":20} ] })"), "\"start\" must be a whole number from 0 to 1000000"));
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra", "levels": [ {} ] })"), "level 1: missing required \"points\""));
  CHECK(has(pair(R"({ "a": "eirik", "b": "lyra", "levels": [ {"points":1,"x":1} ] })"), "level 1: unknown key \"x\""));
  CHECK(has(pair(R"({ "a": "eirik", "levels": [ {"points":1} ] })"), "pair 1: missing required \"b\""));
}

TEST(supports_loader_refuses_bad_bonuses_and_conversations) {
  auto level = [](const char* body) {
    return loadSupports(std::string(R"({ "pairs": [ { "a": "eirik", "b": "lyra", "levels": [ { "points": 5, )") + body + " } ] } ] }");
  };
  CHECK(has(level(R"("bonus": { "x": 1 })"), "level 1: \"bonus\": unknown key \"x\""));
  CHECK(has(level(R"("bonus": { "hit": 1.5 })"), "\"hit\" must be a whole number"));
  CHECK(has(level(R"("bonus": { "stats": { "nope": 1 } })"), "\"stats\" names unknown stat \"nope\""));
  CHECK(has(level(R"("bonus": { "stats": { "hp": 1 } })"), "\"stats\" cannot hold \"hp\""));
  CHECK(has(level(R"("bonus": { "stats": { "mov": 1 } })"), "\"stats\" cannot hold \"mov\""));
  CHECK(has(level(R"("talk": [ { "say": "Hi", "speaker": "eirik" }, { "setSwitch": "met_wanderer", "to": true } ])"),
            "level 1: \"talk\": command 2 (setSwitch): only \"say\" is allowed in a support conversation"));
  CHECK(has(level(R"("talk": [ { "say": "Hi", "speaker": "nobody" } ])"), "command 1 (say): \"speaker\" names unknown character \"nobody\""));
  CHECK(has(level(R"("talk": [ { "say": "Hi", "speaker": "eirik", "speakerName": "E" } ])"), "give \"speaker\" or \"speakerName\", not both"));
  CHECK(has(level(R"("talk": [ { "say": 5 } ])"), "\"say\" must be the text to show"));
  CHECK(has(level(R"("talk": [ { "say": "x", "y": 1 } ])"), "command 1 (say): unknown key \"y\""));
  CHECK(has(level(R"("talk": { "say": "x" })"), "\"talk\": must be a list of say commands"));
  CHECK_STR(level(R"("talk": [])"), "");  // a level may have no conversation
  std::string many = "\"talk\": [";
  for (int i = 0; i < 201; ++i) many += std::string(i ? "," : "") + "{ \"say\": \"x\" }";
  CHECK(has(level((many + "]").c_str()), "\"talk\": has more than 200 lines"));
}

TEST(supports_loader_refuses_more_than_the_limits) {
  std::string pairs;
  for (int i = 0; i < 501; ++i)
    pairs += std::string(i ? "," : "") + R"({ "a": "eirik", "b": "lyra", "levels": [ { "points": 1 } ] })";
  CHECK(has(loadSupports("{ \"pairs\": [" + pairs + "] }"), "too many \"pairs\" (at most 500)"));
}

// ---- the hash (docs/SRPG.md "Saves and hash") ----

namespace {
std::string hashWith(const std::string& supports) {
  Game g;
  g.charactersJson = suptest::kCharacters;
  g.supportsJson = supports;
  if (!g.load("sup_hash")) return "load failed";
  std::string h, err;
  return computeDataHash(g.db, "ford", h, err) ? h : "error: " + err;
}
}  // namespace

TEST(supports_hash_ignores_conversation_wording_and_level_names_but_counts_rules) {
  const std::string base = hashWith(suptest::kSupports);
  CHECK(base.size() == 16 && base != "load failed");
  CHECK_STR(hashWith(edited("You never miss.", "Du triffst immer.")), base);
  CHECK_STR(hashWith(edited("Old friend", "Alter Freund")), base);
  CHECK_STR(hashWith(edited("[\"C\", \"B\", \"A\"]", "[\"x\", \"y\", \"z\"]")), base);
  CHECK(hashWith(edited("\"points\": 10,", "\"points\": 11,")) != base);
  CHECK(hashWith(edited("\"hit\": 10", "\"hit\": 11")) != base);
  CHECK(hashWith(edited("\"range\": 3", "\"range\": 4")) != base);
  CHECK(hashWith(edited("{ \"say\": \"Hello again.\", \"speakerName\": \"Old friend\" }", "")) != base);  // a line fewer
}

TEST(supports_hash_of_a_game_without_the_file_is_unchanged_by_the_feature) {
  Game g;
  CHECK(g.load("sup_hash_none"));
  std::string a, b, err;
  CHECK(computeDataHash(g.db, "ford", a, err));
  Game h;
  h.charactersJson = suptest::kCharacters;  // characters alone do not make supports
  CHECK(h.load("sup_hash_none2"));
  CHECK(computeDataHash(h.db, "ford", b, err));
  CHECK(a != b);  // characters are part of the hash (F1), supports.json is not mixed in by their presence
  Game k;
  k.charactersJson = suptest::kCharacters;
  k.supportsJson = R"({ "pairs": [] })";
  CHECK(k.load("sup_hash_none3"));
  std::string c;
  CHECK(computeDataHash(k.db, "ford", c, err));
  CHECK(c != b);  // the file, once present, is mixed in
}

// ---- campaign scenes ----

namespace {
std::string sceneError(const char* chapter1Json) {
  const std::string campaign = std::string(R"({ "chapters": [ { "id": "ch1", "map": "ford", )") + chapter1Json + R"( } ] })";
  Game g;
  g.charactersJson = suptest::kCharacters;
  g.supportsJson = suptest::kSupports;
  std::string err;
  return g.load("sup_scene", campaign, &err) ? "" : err;
}
}  // namespace

TEST(supports_loader_scene_commands_and_conditions) {
  CHECK_STR(sceneError(R"("after": [ { "addSupport": ["eirik", "lyra"], "by": 4 },
        { "if": [ { "support": ["lyra", "eirik"], "op": ">", "value": 0 } ], "then": [ { "say": "x" } ] } ])"), "");
  CHECK(has(sceneError(R"("after": [ { "addSupport": ["eirik", "scout"], "by": 1 } ])"),
            "chapter \"ch1\": \"after\": command 1 (addSupport): \"eirik\" and \"scout\" are not a support pair (data/supports.json)"));
  CHECK(has(sceneError(R"("before": [ { "say": "a" }, { "if": [ { "alive": "eirik" } ], "then": [ { "addSupport": ["eirik", "scout"], "by": 1 } ] } ])"),
            "\"before\": command 2 (if): \"then\": command 1 (addSupport): \"eirik\" and \"scout\" are not a support pair"));
  CHECK(has(sceneError(R"("after": [ { "if": [ { "support": ["eirik", "scout"], "value": 1 } ], "then": [] } ])"),
            "command 1 (if): condition 1: \"eirik\" and \"scout\" are not a support pair"));
  CHECK(has(sceneError(R"("after": [ { "addSupport": ["eirik", "nobody"], "by": 1 } ])"), "unknown character \"nobody\""));
  CHECK(has(sceneError(R"("after": [ { "addSupport": ["eirik", "lyra"] } ])"), "missing required \"by\""));
  CHECK(has(sceneError(R"("after": [ { "if": [ { "support": ["eirik", "lyra"], "op": "~", "value": 1 } ], "then": [] } ])"), "\"op\" must be one of"));
  CHECK(has(sceneError(R"("after": [ { "setSwitch": "met_wanderer", "to": true, "addSupport": ["eirik","lyra"], "by": 1 } ])"), "has two commands"));
  // What is still no scene command keeps its message.
  CHECK(has(sceneError(R"("after": [ { "dance": 1 } ])"), "is not a scene command; it needs one of say, setSwitch"));
}
