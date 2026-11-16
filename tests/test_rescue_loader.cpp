// Step R1 of docs/SRPG.md: game.json "rules.rescue" and "pairBonus" on unit types, with every refusal message of
// the contract; and a rescued unit going to the campaign roster as living.
#include "campaign_fixture.h"
#include "json_edit.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

// The fixture game with `rescue` (the text of the "rescue" value, "" = none) in game.json and `units` as
// data/units.json. Returns the loader's message ("" = it loaded); `out` receives the database when given.
std::string loadRescue(const std::string& rescue, const std::string& units = camptest::kUnits, Database* out = nullptr) {
  camptest::Game g;
  if (!g.load("rescue_loader")) return "the fixture did not load";
  std::string game = camptest::kGameJson;
  if (!rescue.empty()) {
    const std::string at = "\"counterAttacks\": true,";
    game.replace(game.find(at), at.size(), at + " \"rescue\": " + rescue + ",");
  }
  writeTextFile(g.dir + "/game.json", game);
  writeTextFile(g.dir + "/data/units.json", units);
  Database db;
  std::string err;
  const bool ok = loadGame(g.dir, db, err);
  if (ok && out) *out = db;
  return ok ? "" : err;
}

void refusedWith(const std::string& err, const std::string& needle) {
  if (!has(err, needle)) std::printf("  got: %s\n  wanted: %s\n", err.c_str(), needle.c_str());
  CHECK(has(err, needle));
}

std::string withHeroBonus(const std::string& bonus) {
  std::string units = camptest::kUnits;
  const std::string hero = "\"id\":\"hero\",\"name\":\"Hero\"";
  units.replace(units.find(hero), hero.size(), hero + ",\"pairBonus\":" + bonus);
  return units;
}

}  // namespace

TEST(rescue_loader_a_game_without_the_block_has_neither_mode) {
  Database db;
  CHECK_STR(loadRescue("", camptest::kUnits, &db), "");
  CHECK(db.rules.rescue.mode == RescueMode::Off);
  CHECK(db.rules.rescue.canCarry == -1 && db.rules.rescue.carrierStats.empty() && db.rules.rescue.pairBonus.empty());
  CHECK(db.unitTypes[0].pairBonus.empty());
}

TEST(rescue_loader_reads_carry_mode) {
  Database db;
  CHECK_STR(loadRescue(R"({ "mode": "carry", "canCarry": "a.atk >= b.atk",
                           "carrierStats": { "atk": { "percent": 50 }, "mov": -1 } })", camptest::kUnits, &db), "");
  const RescueRules& r = db.rules.rescue;
  CHECK(r.mode == RescueMode::Carry);
  CHECK(r.canCarry >= 0 && db.exprs[(size_t)r.canCarry].text == "a.atk >= b.atk");
  CHECK(has(db.exprs[(size_t)r.canCarry].label, "rules.rescue.canCarry"));
  CHECK_EQ(r.carrierStats.size(), (size_t)4);
  const int atk = db.findStat("atk"), mov = db.findStat("mov"), def = db.findStat("def");
  CHECK(r.carrierStats[(size_t)atk].percent == 50 && r.carrierStats[(size_t)atk].add == 0);
  CHECK(r.carrierStats[(size_t)mov].percent == 100 && r.carrierStats[(size_t)mov].add == -1);
  CHECK(r.carrierStats[(size_t)def].percent == 100 && r.carrierStats[(size_t)def].add == 0);
  CHECK(r.pairBonus.empty());
}

TEST(rescue_loader_reads_pair_up_mode_and_pair_bonuses) {
  Database db;
  CHECK_STR(loadRescue(R"({ "mode": "pairUp", "pairBonus": { "atk": 2, "def": 2 } })", withHeroBonus(R"({ "atk": 3, "mov": 1 })"), &db), "");
  const RescueRules& r = db.rules.rescue;
  CHECK(r.mode == RescueMode::PairUp && r.canCarry == -1);
  const int atk = db.findStat("atk"), def = db.findStat("def"), mov = db.findStat("mov");
  CHECK_EQ(r.pairBonus.size(), (size_t)4);
  CHECK(r.pairBonus[(size_t)atk] == 2 && r.pairBonus[(size_t)def] == 2 && r.pairBonus[(size_t)mov] == 0);
  const UnitTypeDef& hero = db.unitTypes[(size_t)db.findUnitType("hero")];
  CHECK_EQ(hero.pairBonus.size(), (size_t)4);
  CHECK(hero.pairBonus[(size_t)atk] == 3 && hero.pairBonus[(size_t)mov] == 1 && hero.pairBonus[(size_t)def] == 0);
  CHECK(db.unitTypes[(size_t)db.findUnitType("ranger")].pairBonus.empty());  // none: the game's applies
  // An empty object is a real, zero bonus (it replaces the game's), not "none".
  CHECK_STR(loadRescue(R"({ "mode": "pairUp", "pairBonus": { "atk": 2 } })", withHeroBonus("{}"), &db), "");
  CHECK_EQ(db.unitTypes[(size_t)db.findUnitType("hero")].pairBonus.size(), (size_t)4);
}

TEST(rescue_loader_refuses_with_the_contract_messages) {
  const std::string head = "game.json: rules.rescue: ";
  refusedWith(loadRescue(R"({ "canCarry": "true" })"), head + "\"mode\" must be \"carry\" or \"pairUp\"");
  refusedWith(loadRescue(R"({ "mode": "rescue" })"), head + "\"mode\" must be \"carry\" or \"pairUp\"");
  refusedWith(loadRescue(R"({ "mode": 3 })"), head + "\"mode\" must be \"carry\" or \"pairUp\"");
  refusedWith(loadRescue(R"({ "mode": "carry", "carrierStats": { "xp": -1 } })"), head + "\"carrierStats\" names unknown stat \"xp\"");
  refusedWith(loadRescue(R"({ "mode": "carry", "carrierStats": { "hp": -1 } })"),
              head + "\"carrierStats\" cannot change \"hp\" (the health stat)");
  refusedWith(loadRescue(R"({ "mode": "carry", "pairBonus": { "atk": 1 } })"), head + "\"pairBonus\" belongs to \"mode\": \"pairUp\"");
  refusedWith(loadRescue(R"({ "mode": "carry", "dance": true })"), head + "unknown key \"dance\"");
  refusedWith(loadRescue(R"([])"), head + "must be an object");
  // Shapes of the values.
  refusedWith(loadRescue(R"({ "mode": "carry", "carrierStats": { "atk": { "percent": 1001 } } })"), "\"percent\" must be a whole number from 0 to 1000");
  refusedWith(loadRescue(R"({ "mode": "carry", "carrierStats": { "atk": { "percent": -1 } } })"), "\"percent\" must be a whole number from 0 to 1000");
  refusedWith(loadRescue(R"({ "mode": "carry", "carrierStats": { "atk": { "adds": 1 } } })"), "unknown key \"adds\"");
  refusedWith(loadRescue(R"({ "mode": "carry", "carrierStats": { "atk": "half" } })"), "must be a whole number or { \"percent\": n }");
  refusedWith(loadRescue(R"({ "mode": "carry", "carrierStats": 5 })"), "\"carrierStats\" must be an object");
  refusedWith(loadRescue(R"({ "mode": "carry", "canCarry": 5 })"), "\"canCarry\" must be a string");
  refusedWith(loadRescue(R"({ "mode": "pairUp", "pairBonus": { "zz": 1 } })"), "\"pairBonus\" names unknown stat \"zz\"");
  refusedWith(loadRescue(R"({ "mode": "pairUp", "pairBonus": { "hp": 1 } })"), "\"pairBonus\" cannot change \"hp\" (the health stat)");
}

TEST(rescue_loader_refuses_pair_bonus_on_a_unit_type_without_pair_up) {
  const std::string want = "unit \"hero\": \"pairBonus\" needs \"rules\": { \"rescue\": { \"mode\": \"pairUp\" } } in game.json";
  refusedWith(loadRescue("", withHeroBonus(R"({ "atk": 1 })")), want);
  refusedWith(loadRescue(R"({ "mode": "carry" })", withHeroBonus(R"({ "atk": 1 })")), want);
  refusedWith(loadRescue(R"({ "mode": "pairUp" })", withHeroBonus(R"({ "zz": 1 })")), "\"pairBonus\" names unknown stat \"zz\"");
  refusedWith(loadRescue(R"({ "mode": "pairUp" })", withHeroBonus(R"({ "hp": 1 })")), "\"pairBonus\" cannot change \"hp\"");
}

TEST(rescue_loader_a_canCarry_syntax_error_names_the_place) {
  Database db;
  CHECK_STR(loadRescue(R"({ "mode": "carry", "canCarry": "a.atk >=" })", camptest::kUnits, &db), "");  // compiled later
  Formulas f;
  std::string err;
  CHECK(!f.init(db, err));
  CHECK(err.rfind("game.json: rules.rescue.canCarry: ", 0) == 0);
}

// ---- the campaign ----

TEST(rescue_a_carried_unit_goes_to_the_roster_as_living) {
  camptest::Game g;
  CHECK(g.load("rescue_campaign"));
  std::string game = camptest::kGameJson;
  const std::string at = "\"counterAttacks\": true,";
  game.replace(game.find(at), at.size(), at + " \"rescue\": { \"mode\": \"carry\" },");
  writeTextFile(g.dir + "/game.json", game);
  Database db;
  Formulas formulas;
  std::string err;
  CHECK(loadGame(g.dir, db, err) && formulas.init(db, err));
  Playthrough pt;
  CHECK(pt.begin(db));
  std::vector<SceneLine> lines;
  runScene(db, pt, SceneKind::Before, lines);
  Battle b;
  std::vector<Event> ev;
  CHECK(startChapterBattle(db, pt, formulas, 5, b, ev, err));
  const int eirik = b.unitByStoryId("eirik"), lyra = b.unitByStoryId("lyra");
  CHECK(eirik >= 0 && lyra >= 0);
  CHECK(b.apply(Action::rescue(eirik, lyra), ev, &err));
  CHECK(!b.units[(size_t)lyra].alive && b.exists(b.units[(size_t)lyra]));
  b.over = true;
  b.winnerTeam = b.humanTeam();
  CHECK(finishChapterBattle(db, pt, b) == ChapterResult::Won);
  bool found = false;
  for (const RosterUnit& r : pt.roster)
    if (r.id == "lyra") {
      found = true;
      CHECK(!r.gone);  // carried at the end: not fallen
    }
  CHECK(found);
  // The playthrough with it still saves and restores.
  Json saved;
  savePlaythrough(db, pt, saved);
  Playthrough back;
  CHECK(restorePlaythrough(db, saved, back, err));
}
