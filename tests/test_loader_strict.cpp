// How strict the loaders are about the keys and numbers of the game files (docs/GAME_FORMAT.md "Strict files"): every object refuses a key
// it does not know, naming file, entry and key and, when a known key is a slip away, the key that was probably meant; numbers that mean
// nothing (a negative count, a percent of 250) are refused; and every game of the repository that is meant to be valid still loads.
#include <filesystem>
#include <system_error>

#include "core/data/loader_util.h"
#include "srpg_skills_fixture.h"

using namespace ote;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// A writable copy of a whole fixture folder (art included).
std::string copyOf(const char* name, const char* fixture) {
  const std::string dir = savetest::tempFolder(std::string("strict_") + name);
  std::error_code ec;
  std::filesystem::copy(ote_test::sourceDir() + "/tests/fixtures/" + fixture, dir,
                        std::filesystem::copy_options::recursive | std::filesystem::copy_options::overwrite_existing, ec);
  CHECK(!ec);
  return dir;
}

void edit(const std::string& dir, const std::string& rel, const std::string& from, const std::string& to) {
  std::string text;
  readTextFile(dir + "/" + rel, text);
  const size_t at = text.find(from);
  if (at == std::string::npos) {
    std::printf("  the test's text to replace is missing in %s: %s\n", rel.c_str(), from.c_str());
    CHECK(false);
    return;
  }
  writeTextFile(dir + "/" + rel, text.replace(at, from.size(), to));
}

// The load message of the game and then of every map ("" = all load).
std::string loadError(const std::string& dir) {
  Database db;
  std::string err;
  if (!loadGame(dir, db, err)) return err;
  for (const MapRef& m : db.maps) {
    MapDef def;
    if (!loadMap(db, m.id, def, err)) return err;
  }
  return "";
}

// Edits one text of a copy of `fixture` and returns the message the load gives.
std::string refusal(const char* name, const char* fixture, const char* rel, const char* from, const char* to) {
  const std::string dir = copyOf(name, fixture);
  edit(dir, rel, from, to);
  return loadError(dir);
}

}  // namespace

TEST(strict_the_hint_finds_the_key_that_was_meant) {
  CHECK_EQ(keyDistance("rnage", "range"), 1);  // two neighbours swapped is one slip
  CHECK_EQ(keyDistance("rnaeg", "range"), 2);
  CHECK_STR(closestKey("rnage", {"id", "name", "range"}), "range");
  CHECK_EQ(keyDistance("colour", "color"), 1);
  CHECK_EQ(keyDistance("", "abc"), 3);
  CHECK_STR(closestKey("colour", {"id", "name", "color"}), "color");
  CHECK_STR(closestKey("Range", {"range", "rank"}), "range");  // a capital letter is one slip
  CHECK_STR(closestKey("x", {"y"}), "");                       // one letter against one letter is not a hint
  CHECK_STR(closestKey("zzzzzz", {"id", "name", "color"}), "");
  CHECK_STR(closestKey("counterAttack", {"counterAttacks", "defeat"}), "counterAttacks");
  CHECK_STR(unknownKeyText("colour", {"id", "color"}), "unknown key \"colour\" (expected \"id\", \"color\") - did you mean \"color\"?");
  CHECK_STR(unknownKeyText("zzz", {"id", "color"}), "unknown key \"zzz\" (expected \"id\", \"color\")");
  // A long list is not printed (only the hint is).
  const std::vector<std::string> many = {"a1", "b1", "c1", "d1", "e1", "f1", "g1", "h1", "i1", "j1", "k1", "l1", "m1", "n1", "name"};
  CHECK_STR(unknownKeyText("nmae", many), "unknown key \"nmae\" - did you mean \"name\"?");
}

TEST(strict_game_json_refuses_unknown_keys_and_names_the_closest) {
  const char* g = "game.json";
  std::string e = refusal("g_root", "srpg-skills", g, "\"formatVersion\": 1,", "\"formatVersion\": 1, \"descripton\": \"x\",");
  CHECK(has(e, "game.json: unknown key \"descripton\"") && has(e, "did you mean \"description\"?"));
  // "editor" is the web editor's own bookkeeping (formula presets); the game does not read it.
  CHECK_STR(refusal("g_editor", "srpg-skills", g, "\"formatVersion\": 1,", "\"formatVersion\": 1, \"editor\": { \"formulas\": {} },"), "");
  e = refusal("g_rules", "srpg-skills", g, "\"counterAttacks\": true,", "\"counterAttacks\": true, \"defeet\": [],");
  CHECK(has(e, "game.json: rules: unknown key \"defeet\"") && has(e, "did you mean \"defeat\"?"));
  e = refusal("g_items", "srpg-skills", g, "\"inventorySize\": 3", "\"inventorySize\": 3, \"storge\": \"off\"");
  CHECK(has(e, "rules.items: unknown key \"storge\"") && has(e, "did you mean \"storage\"?"));
  e = refusal("g_prog", "srpg-skills", g, "\"maxLevel\": 20,", "\"maxLevel\": 20, \"xpHitt\": \"1\",");
  CHECK(has(e, "rules.progression: unknown key \"xpHitt\"") && has(e, "did you mean \"xpHit\"?"));
  e = refusal("g_formulas", "srpg-skills", g, "\"damage\": \"a.atk - b.def\" }", "\"damage\": \"a.atk - b.def\", \"hitt\": \"90\" }");
  CHECK(has(e, "formulas: unknown key \"hitt\"") && has(e, "did you mean \"hit\"?"));
  e = refusal("g_roles", "srpg-skills", g, "\"move\": \"mov\" }", "\"move\": \"mov\", \"mov\": \"mov\" }");
  CHECK(has(e, "roles: unknown key \"mov\"") && has(e, "did you mean \"move\"?"));
  e = refusal("g_stat", "srpg-skills", g, "{\"id\":\"hp\"}", "{\"id\":\"hp\",\"nam\":\"x\"}");
  CHECK(has(e, "stats \"hp\": unknown key \"nam\"") && has(e, "did you mean \"name\"?"));
  e = refusal("g_map", "srpg-skills", g, "{\"id\":\"field\",\"name\":\"Field\",", "{\"id\":\"field\",\"flie\":\"x\",\"name\":\"Field\",");
  CHECK(has(e, "map \"field\": unknown key \"flie\"") && has(e, "did you mean \"file\"?"));
  // Keys other files read stay known: a move type's sounds, a resource's refill.
  CHECK_STR(refusal("g_sounds", "srpg-skills", g, "{\"id\":\"foot\"}", "{\"id\":\"foot\",\"sounds\":{}}"), "");
  e = refusal("g_style", "plain-economy", g, "{ \"id\": \"classic\", \"name\": \"Classic\" }", "{ \"id\": \"classic\", \"backgrond\": \"#000000\" }");
  CHECK(has(e, "style \"classic\": unknown key \"backgrond\"") && has(e, "did you mean \"background\"?"));
  e = refusal("g_view", "plain-economy", g, "\"tileSize\": 24 }", "\"tileSize\": 24, \"tilesize\": 24 }");
  CHECK(has(e, "view: unknown key \"tilesize\"") && has(e, "did you mean \"tileSize\"?"));
  e = refusal("g_iso", "plain-economy", g, "\"view\": \"isometric\", \"background\"", "\"view\": \"isometric\", \"isometric\": { \"tileWidht\": 64 }, \"background\"");
  CHECK(has(e, "\"isometric\": unknown key \"tileWidht\"") && has(e, "did you mean \"tileWidth\"?"));
  e = refusal("g_cap", "plain-economy", g, "\"capture\": { \"enabled\": true", "\"capture\": { \"enable\": true");
  CHECK(has(e, "rules.capture: unknown key \"enable\"") && has(e, "did you mean \"enabled\"?"));
}

TEST(strict_the_data_files_refuse_unknown_keys_at_the_root_and_in_every_row) {
  std::string e = refusal("d_wroot", "srpg-skills", "data/weapons.json", "{ \"weapons\": [", "{ \"weapon\": [], \"weapons\": [");
  CHECK(has(e, "data/weapons.json: unknown key \"weapon\"") && has(e, "did you mean \"weapons\"?"));
  e = refusal("d_weapon", "srpg-skills", "data/weapons.json", "\"group\": \"blade\", \"skills\"", "\"group\": \"blade\", \"rnage\": [1, 1], \"skills\"");
  CHECK(has(e, "data/weapons.json: weapon \"sword\": unknown key \"rnage\"") && has(e, "did you mean \"range\"?"));
  e = refusal("d_unit", "srpg-skills", "data/units.json", "{ \"id\": \"grunt\", \"name\": \"Grunt\",", "{ \"id\": \"grunt\", \"tag\": [\"x\"], \"name\": \"Grunt\",");
  CHECK(has(e, "data/units.json: unit \"grunt\": unknown key \"tag\"") && has(e, "did you mean \"tags\"?"));
  e = refusal("d_uroot", "srpg-skills", "data/units.json", "{ \"units\": [", "{ \"unit\": [], \"units\": [");
  CHECK(has(e, "data/units.json: unknown key \"unit\""));
  e = refusal("d_terrain", "srpg-skills", "data/terrain.json", "{ \"id\": \"plain\",  \"name\": \"Plain\" }", "{ \"id\": \"plain\", \"colour\": \"#ffffff\", \"name\": \"Plain\" }");
  CHECK(has(e, "data/terrain.json: terrain \"plain\": unknown key \"colour\"") && has(e, "did you mean \"color\"?"));
  e = refusal("d_troot", "srpg-skills", "data/terrain.json", "{ \"terrain\": [", "{ \"terrains\": [], \"terrain\": [");
  CHECK(has(e, "data/terrain.json: unknown key \"terrains\""));
  // The settings that moved to structures keep their own message.
  e = refusal("d_moved", "srpg-skills", "data/terrain.json", "{ \"id\": \"plain\",  \"name\": \"Plain\" }", "{ \"id\": \"plain\", \"capturePoints\": 5, \"name\": \"Plain\" }");
  CHECK(has(e, "now belongs to structures"));
  e = refusal("d_struct", "srpg-skills", "data/structures.json", "\"name\": \"Fort\",", "\"name\": \"Fort\", \"capturePoint\": 3,");
  CHECK(has(e, "data/structures.json: structure \"fort\": unknown key \"capturePoint\"") && has(e, "did you mean \"capturePoints\"?"));
  e = refusal("d_sroot", "srpg-skills", "data/structures.json", "{ \"structures\": [", "{ \"structure\": [], \"structures\": [");
  CHECK(has(e, "data/structures.json: unknown key \"structure\""));
  e = refusal("d_item", "srpg-skills", "data/items.json", "\"kind\": \"usable\", \"uses\": 3,", "\"kind\": \"usable\", \"use\": 3, \"uses\": 3,");
  CHECK(has(e, "data/items.json: item \"potion\": unknown key \"use\"") && has(e, "did you mean \"uses\"?"));
  e = refusal("d_iroot", "srpg-skills", "data/items.json", "{ \"items\": [", "{ \"item\": [], \"items\": [");
  CHECK(has(e, "data/items.json: unknown key \"item\""));
  e = refusal("d_chroot", "srpg-skills", "data/characters.json", "{ \"characters\": [", "{ \"chars\": [], \"characters\": [");
  CHECK(has(e, "data/characters.json: unknown key \"chars\""));
  // Item grants: { "item", "uses", "drop" }.
  skilltest::Copy c("strict_grant", "srpg-skills");
  c.swap("data/units.json", "\"skills\": [\"steady_recovery\"],", "\"skills\": [\"steady_recovery\"], \"items\": [ { \"item\": \"potion\", \"use\": 1 } ],");
  CHECK(has(c.gameError(), "unit \"hero\", \"items\" entry 1: unknown key \"use\"") && has(c.gameError(), "did you mean \"uses\"?"));
}

TEST(strict_capture_points_zero_means_not_capturable_and_only_a_negative_is_refused) {
  const std::string dir = copyOf("capture0", "plain-economy");
  edit(dir, "data/structures.json", "\"capturePoints\": 20, \"income\": { \"funds\": 1000 } },", "\"capturePoints\": 0, \"income\": { \"funds\": 1000 } },");
  Database db;
  std::string err;
  CHECK(loadGame(dir, db, err));
  CHECK_EQ(db.structures[(size_t)db.findStructure("city")].capturePoints, 0);  // the same as leaving it out: not capturable
  const std::string neg = refusal("capture_neg", "plain-economy", "data/structures.json", "\"capturePoints\": 20, \"income\": { \"funds\": 1000 } },",
                                  "\"capturePoints\": -1, \"income\": { \"funds\": 1000 } },");
  CHECK(has(neg, "data/structures.json: structure \"city\": \"capturePoints\" must not be negative"));
}

TEST(strict_a_skill_blocks_percent_chance_is_from_0_to_100) {
  skilltest::Copy c("strict_chance", "srpg-skills");
  for (const char* ok : {"0", "1", "100", "\"a.skl * 2\"", "\"250\""}) {  // a formula is kept within 0 to 100 when it is asked
    c.skills(skilltest::oneBlock(std::string("{ \"when\": \"strike\", \"chance\": ") + ok + ", \"do\": [ { \"hit\": 1 } ] }"));
    CHECK_STR(c.gameError(), "");
  }
  for (const char* bad : {"101", "-1", "250", "1000000"}) {
    c.skills(skilltest::oneBlock(std::string("{ \"when\": \"strike\", \"chance\": ") + bad + ", \"do\": [ { \"hit\": 1 } ] }"));
    const std::string e = c.gameError();
    CHECK(has(e, "data/skills.json: skill \"x\": block 1: \"chance\" must be a percent from 0 to 100"));
  }
  c.skills(skilltest::oneBlock("{ \"when\": \"strike\", \"chance\": 12.5, \"do\": [ { \"hit\": 1 } ] }"));
  CHECK(has(c.gameError(), "\"chance\" must be a percent or a formula"));
}

TEST(strict_support_bonuses_are_from_minus_1000_to_1000_like_the_skill_block_bonuses) {
  const std::string dir = copyOf("supports", "srpg-supports");
  const std::string rel = "data/supports.json";
  edit(dir, rel, "\"bonus\": { \"hit\": 5, \"avoid\": 5 }", "\"bonus\": { \"hit\": 1000, \"avoid\": -1000, \"damage\": 1000, \"stats\": { \"str\": -1000 } }");
  CHECK_STR(loadError(dir), "");
  for (const char* key : {"hit", "avoid", "crit", "critAvoid", "damage", "damageTaken"}) {
    const std::string d2 = copyOf((std::string("supports_") + key).c_str(), "srpg-supports");
    edit(d2, rel, "\"bonus\": { \"hit\": 5, \"avoid\": 5 }", std::string("\"bonus\": { \"") + key + "\": 1001 }");
    const std::string e = loadError(d2);
    CHECK(has(e, "data/supports.json: pair 1") && has(e, (std::string("level 1: \"bonus\": \"") + key + "\" must be a whole number from -1000 to 1000").c_str()));
    edit(d2, rel, "1001", "-1001");
    CHECK(has(loadError(d2), "must be a whole number from -1000 to 1000"));
  }
  const std::string d3 = copyOf("supports_stat", "srpg-supports");
  edit(d3, rel, "\"bonus\": { \"hit\": 5, \"avoid\": 5 }", "\"bonus\": { \"stats\": { \"str\": 1001 } }");
  CHECK(has(loadError(d3), "level 1: \"bonus\": \"stats\".\"str\" must be a whole number from -1000 to 1000"));
}

TEST(strict_numbers_that_mean_nothing_are_refused) {
  const char* g = "game.json";
  CHECK(has(refusal("n_crit", "srpg-skills", g, "\"formulas\": { \"damage\": \"a.atk - b.def\" }", "\"formulas\": { \"damage\": \"a.atk - b.def\", \"critMultiplier\": -1 }"),
            "formulas: \"critMultiplier\" must be a number from 0 to 1000"));
  CHECK_STR(refusal("n_crit_ok", "srpg-skills", g, "\"formulas\": { \"damage\": \"a.atk - b.def\" }", "\"formulas\": { \"damage\": \"a.atk - b.def\", \"critMultiplier\": 0 }"), "");
  CHECK(has(refusal("n_maxlevel", "srpg-skills", g, "\"maxLevel\": 20", "\"maxLevel\": 10000"), "rules.progression: \"maxLevel\" must be at most 9999"));
  CHECK(has(refusal("n_xpper", "srpg-skills", g, "\"xpPerLevel\": 100", "\"xpPerLevel\": 1000001"), "rules.progression: \"xpPerLevel\" must be at most 1000000"));
  CHECK(has(refusal("n_view", "plain-economy", g, "\"width\": 480", "\"width\": 20000"), "view: \"width\" and \"height\" must be at most 16384"));
  CHECK(has(refusal("n_tile", "plain-economy", g, "\"tileSize\": 24", "\"tileSize\": 5000"), "view: \"width\" and \"height\" must be at most 16384 and \"tileSize\" at most 1024"));
  CHECK(has(refusal("n_cost", "srpg-skills", "data/terrain.json", "\"moveCost\": { \"foot\": 3 }", "\"moveCost\": { \"foot\": -1 }"),
            "terrain \"forest\": \"moveCost\".\"foot\" must be a whole number from 0 (cannot enter) to 9999"));
  CHECK_STR(refusal("n_cost_ok", "srpg-skills", "data/terrain.json", "\"moveCost\": { \"foot\": 3 }", "\"moveCost\": { \"foot\": 9999 }"), "");
  CHECK(has(refusal("n_stat", "srpg-skills", "data/units.json", "\"hp\": 20, \"atk\": 6", "\"hp\": 100000, \"atk\": 6"),
            "unit \"hero\": \"stats\".\"hp\" must be a whole number from -99999 to 99999"));
  CHECK(has(refusal("n_struct_cost", "plain-economy", "data/structures.json", "\"capturePoints\": 20, \"income\": { \"funds\": 1000 } },",
                    "\"capturePoints\": 20, \"moveCost\": { \"foot\": 10000 }, \"income\": { \"funds\": 1000 } },"),
            "at most 9999"));
}

TEST(strict_maps_refuse_unknown_keys_in_the_map_its_sides_units_and_structures) {
  const char* m = "maps/field.json";
  std::string e = refusal("m_root", "srpg-skills", m, "\"name\": \"Field\",", "\"name\": \"Field\", \"unitz\": [],");
  CHECK(has(e, "maps/field.json: unknown key \"unitz\"") && has(e, "did you mean \"units\"?"));
  CHECK_STR(refusal("m_music", "srpg-skills", m, "\"name\": \"Field\",", "\"name\": \"Field\", \"music\": \"\", \"prepMusic\": \"\", \"phaseMusic\": {},"), "");
  e = refusal("m_grid", "srpg-skills", m, "\"width\": 6,", "\"widht\": 6, \"width\": 6,");
  CHECK(has(e, "grid: unknown key \"widht\"") && has(e, "did you mean \"width\"?"));
  e = refusal("m_side", "srpg-skills", m, "{ \"id\": \"blue\", \"controller\": \"human\" }", "{ \"id\": \"blue\", \"controler\": \"human\" }");
  CHECK(has(e, "side \"blue\": unknown key \"controler\"") && has(e, "did you mean \"controller\"?"));
  e = refusal("m_unit", "srpg-skills", m, "{ \"type\": \"hero\", \"side\": \"red\", \"x\": 5, \"y\": 2 }", "{ \"type\": \"hero\", \"side\": \"red\", \"x\": 5, \"y\": 2, \"lvl\": 2 }");
  CHECK(has(e, "units[3]: unknown key \"lvl\"") && has(e, "did you mean \"level\"?"));
  e = refusal("m_struct", "plain-economy", "maps/twin_rivers.json", "{ \"type\": \"city\", \"x\": 1, \"y\": 1 }", "{ \"typ\": \"city\", \"type\": \"city\", \"x\": 1, \"y\": 1 }");
  CHECK(has(e, "structures[") && has(e, "unknown key \"typ\"") && has(e, "did you mean \"type\"?"));
}

TEST(strict_graphics_files_refuse_unknown_keys_in_sheets_looks_portraits_and_battle_art) {
  const char* gfx = "data/graphics.json";
  std::string e = refusal("x_sheet", "stage-pres", gfx, "\"cellWidth\": 240,", "\"cellWidth\": 240, \"cellHeigth\": 9,");
  CHECK(has(e, "data/graphics.json: sheet \"bg_forest\": unknown key \"cellHeigth\"") && has(e, "did you mean \"cellHeight\"?"));
  e = refusal("x_portrait", "stage-pres", gfx, "\"cell\": [0, 0], \"faces\": \"left\", \"talk\"", "\"cell\": [0, 0], \"face\": \"left\", \"talk\"");
  CHECK(has(e, "portrait \"eirik\": unknown key \"face\"") && has(e, "did you mean \"faces\"?"));
  e = refusal("x_root", "stage-pres", gfx, "\"backgrounds\": {", "\"background\": {}, \"backgrounds\": {");
  CHECK(has(e, "data/graphics.json: unknown key \"background\"") && has(e, "did you mean \"backgrounds\"?"));
  e = refusal("x_set", "srpg-rename", gfx, "\"plain_set\": { \"animations\"", "\"plain_set\": { \"animation\": {}, \"animations\"");
  CHECK(has(e, "battle set \"plain_set\": unknown key \"animation\"") && has(e, "did you mean \"animations\"?"));
  e = refusal("x_anim", "srpg-rename", gfx, "\"frames\": [ [0, 1] ] }", "\"frames\": [ [0, 1] ], \"frame\": 1 }");
  CHECK(has(e, "animation \"stand\": unknown key \"frame\"") && has(e, "did you mean \"frames\"?"));
  e = refusal("x_assign", "srpg-rename", gfx, "{ \"set\": \"plain_set\", \"type\": \"fighter\" }", "{ \"set\": \"plain_set\", \"typ\": \"fighter\", \"type\": \"fighter\" }");
  CHECK(has(e, "battle assign 1: unknown key \"typ\"") && has(e, "did you mean \"type\"?"));
  e = refusal("x_battle", "srpg-rename", gfx, "\"assign\": [", "\"assigns\": [], \"assign\": [");
  CHECK(has(e, "battle: unknown key \"assigns\"") && has(e, "did you mean \"assign\"?"));
}

// Every game of the repository that is meant to be valid still loads, with all its maps: the demos, the templates of the editor and the
// fixtures of the tests (the ones left out are made invalid on purpose, or need art that a test writes first).
TEST(strict_every_valid_game_of_the_repository_still_loads) {
  const char* games[] = {"games/demo-economy", "games/demo-hex", "games/demo-srpg", "editor/templates/blank", "editor/templates/economy-tactics",
                         "editor/templates/hex-tactics-flat", "editor/templates/hex-tactics-pointy", "editor/templates/strategy-rpg"};
  for (const char* g : games) {
    const std::string e = loadError(ote_test::sourceDir() + "/" + g);
    if (!e.empty()) std::printf("  %s: %s\n", g, e.c_str());
    CHECK_STR(e, "");
  }
  const char* fixtures[] = {"action-points", "ai-scripted", "area-fire", "choice-story", "combo", "commanders", "damage-types", "facing", "factions",
                            "height-rules", "hex-flat", "hex-pointy", "jobs", "mapchange", "merit-ranks", "parties-flow", "plain-economy",
                            "research-branches", "scene-pres", "speed-order", "srpg-abilities", "srpg-carry", "srpg-chars", "srpg-conditions",
                            "srpg-doors", "srpg-hex", "srpg-locks", "srpg-mapweapon", "srpg-pair", "srpg-prep", "srpg-quotes", "srpg-ranks",
                            "srpg-rename", "srpg-skillblocks", "srpg-skillblocks2", "srpg-skills", "srpg-staves", "srpg-staves2", "srpg-supports",
                            "srpg-triangle", "stage-pres", "unit-ranks"};
  for (const char* f : fixtures) {
    const std::string e = loadError(ote_test::sourceDir() + "/tests/fixtures/" + f);
    if (!e.empty()) std::printf("  %s: %s\n", f, e.c_str());
    CHECK_STR(e, "");
  }
}
