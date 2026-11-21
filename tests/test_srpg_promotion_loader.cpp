// Step P1 of docs/SRPG.md "Promotion", loading: the "promotions" key of unit types, "promotes" on items,
// rules.promotion, and the story command and trigger. Every refusal is checked with its message.
#include "srpg_promo_fixture.h"

using namespace ote;
using promotest::Game;
using promotest::has;

namespace {

// Loads the fixture with `from` replaced by `to` in `file`; the load message ("" = loaded), and says so if the
// message lacks `want`.
void refuses(const char* file, const std::string& from, const std::string& to, const std::string& want) {
  Game g("promo_loader");
  g.swap(file, from, to);
  const std::string err = g.gameError();
  if (!has(err, want)) std::printf("  got: %s\n  wanted: %s\n", err.c_str(), want.c_str());
  CHECK(has(err, want));
}

const std::string kFirst = R"({"to":"captain","fromLevel":3,"items":["writ","elixir"],"atLevel":"offer"})";

void entry(const std::string& body, const std::string& want) {
  refuses("data/units.json", kFirst, body, want);
}

}  // namespace

TEST(promo_the_fixture_loads_and_reads_every_key) {
  Game g("promo_loader");
  CHECK_STR(g.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(g.dir, db, err));
  const UnitTypeDef& r = db.unitTypes[(size_t)db.findUnitType("recruit")];
  CHECK_EQ(r.promotions.size(), (size_t)2);
  const PromotionDef& a = r.promotions[0];
  CHECK_EQ(a.to, db.findUnitType("captain"));
  CHECK_EQ(a.fromLevel, 3);
  CHECK_EQ(a.items.size(), (size_t)2);
  CHECK_EQ(a.items[0], db.findItem("writ"));
  CHECK(a.atLevel == PromoteAtLevel::Offer);
  CHECK(a.levelAfter == LevelAfter::Game);
  CHECK(!a.hasGains);
  const PromotionDef& b = r.promotions[1];
  CHECK(b.levelAfter == LevelAfter::Keep);
  CHECK(b.hasGains);
  CHECK_EQ(b.gains.size(), db.stats.size());
  CHECK_EQ(b.gains[(size_t)db.findStat("hp")], 3);
  CHECK_EQ(b.gains[(size_t)db.findStat("mov")], 1);
  CHECK(b.atLevel == PromoteAtLevel::Off);
  CHECK(db.items[(size_t)db.findItem("writ")].promotes);
  CHECK(!db.items[(size_t)db.findItem("potion")].promotes);
  CHECK(!db.rules.promotion.keepLevel);
  // A type with no key has none; "to" may name a type that comes later in the file (scout is after recruit).
  CHECK(db.unitTypes[(size_t)db.findUnitType("grunt")].promotions.empty());
}

TEST(promo_rules_promotion) {
  Game g("promo_loader");
  g.swap("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"levelAfter\": \"keep\" },");
  Database db;
  std::string err;
  CHECK(loadGame(g.dir, db, err));
  CHECK(db.rules.promotion.keepLevel);
  g.swap("game.json", "\"keep\"", "\"reset\"");
  Database db2;
  CHECK(loadGame(g.dir, db2, err));
  CHECK(!db2.rules.promotion.keepLevel);
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"levelAfter\": \"half\" },",
          "game.json: rules.promotion: \"levelAfter\" must be \"reset\" or \"keep\"");
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"keepLevel\": true },",
          "game.json: rules.promotion: unknown key \"keepLevel\"");
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": 3,", "rules.promotion: must be an object");
}

TEST(promo_testat_is_read_and_checked) {
  Game g("promo_loader");
  g.swap("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"testAt\": 15 },");
  Database db;
  std::string err;
  CHECK(loadGame(g.dir, db, err));
  CHECK_EQ(db.rules.promotion.testAt, 15);
  Database plain;
  Game g2("promo_loader");
  CHECK(loadGame(g2.dir, plain, err));
  CHECK_EQ(plain.rules.promotion.testAt, 0);  // missing = the test level never promotes
  const std::string head = "game.json: rules.promotion: \"testAt\" must be ";
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"testPromotedTag\": \"promoted\" },", "needs \"testAt\"");
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"testAt\": 15, \"testPromotedTag\": 3 },", "must be a tag name");
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"testAt\": 0 },",
          head + "a whole number from 1 up to the game's top level (rules.progression.maxLevel)");
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"testAt\": 2.5 },", "\"testAt\"");
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"testAt\": \"15\" },", "\"testAt\"");
  refuses("game.json", "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"testAt\": 21 },",
          head + "from 1 to the game's top level (20, rules.progression.maxLevel)");
}

TEST(promo_refuses_a_bad_entry) {
  const std::string head = "data/units.json: unit \"recruit\": \"promotions\" entry 1: ";
  entry(R"({"to":"nobody"})", head + "unknown unit type \"nobody\"");
  entry(R"({"to":"recruit"})", head + "\"to\" is the unit type itself");
  entry(R"({"fromLevel":3})", "missing required \"to\"");
  entry(R"({"to":"captain","fromLevel":0})", head + "\"fromLevel\" must be from 1 to 20");
  entry(R"({"to":"captain","fromLevel":21})", head + "\"fromLevel\" must be from 1 to 20");
  entry(R"({"to":"captain","fromLevel":"3"})", head + "\"fromLevel\" must be from 1 to 20");
  entry(R"({"to":"captain","atLevel":"sometimes"})", head + "\"atLevel\" must be \"off\", \"offer\" or \"auto\"");
  entry(R"({"to":"captain","levelAfter":"half"})", head + "\"levelAfter\" must be \"reset\" or \"keep\"");
  entry(R"({"to":"captain","items":["potion"]})",
        head + "\"items\" names \"potion\", which is not a promotion item (it needs \"promotes\": true in data/items.json)");
  entry(R"({"to":"captain","items":["ghost"]})", head + "\"items\" names \"ghost\", which is not a promotion item");
  entry(R"({"to":"captain","speed":1})", head + "unknown key \"speed\"");
  entry(R"({"to":"captain","gains":{"luck":1}})", "\"gains\" names unknown stat \"luck\"");
  entry(R"({"to":"captain","gains":{"hp":"x"}})", "\"gains\".\"hp\" must be a whole number");
  entry(R"(7)", "\"promotions\" entry 1 must be an object");
  // The second entry is numbered 2.
  refuses("data/units.json", R"({"to":"scout","fromLevel":3,"items":["writ"],"levelAfter":"keep","gains":{"hp":3,"mov":1}})",
          R"({"to":"scout","atLevel":"x"})", "\"promotions\" entry 2: \"atLevel\"");
  // Nine entries are too many.
  std::string nine;
  for (int i = 0; i < 9; ++i) nine += std::string(i ? "," : "") + R"({"to":"captain"})";
  refuses("data/units.json", "\"promotions\":[ " + kFirst, "\"promotions\":[ " + nine + "," + kFirst, "too many \"promotions\" (at most 8)");
}

TEST(promo_at_level_needs_experience) {
  Game g("promo_loader");
  // Without rules.progression the level-based triggers have nothing to go by; "off" and the item path are fine.
  g.swap("game.json", R"("progression": { "enabled": true, "xpPerLevel": 100, "maxLevel": 20, "xpHit": "30", "xpKill": "60" })",
         R"("progression": { "enabled": false })");
  const std::string err = g.gameError();
  CHECK(has(err, "\"promotions\" entry 1: \"atLevel\" needs experience and levels (rules.progression)"));
  for (const char* kind : {"offer", "auto", "offer", "auto"})  // two of each in the fixture
    g.swap("data/units.json", std::string("\"atLevel\":\"") + kind + "\"", "\"atLevel\":\"off\"");
  CHECK_STR(g.gameError(), "");
}

TEST(promo_items) {
  // "promotes" belongs to usable items; a usable item needs heal, raise or promotes.
  refuses("data/items.json", R"("id":"blade","name":"Blade","kind":"weapon")", R"("id":"blade","name":"Blade","kind":"weapon","promotes":true)",
          "item \"blade\": \"promotes\" belongs to items of kind \"usable\"");
  refuses("data/items.json", R"("id":"potion","name":"Potion","kind":"usable","uses":1,"heal":5)",
          R"("id":"potion","name":"Potion","kind":"usable","uses":1)", "a usable item needs \"heal\" and/or \"raise\"");
  refuses("data/items.json", R"("id":"potion","name":"Potion","kind":"usable","uses":1,"heal":5)",
          R"("id":"potion","name":"Potion","kind":"usable","uses":1,"promotes":false)", "a usable item needs \"heal\" and/or \"raise\"");
  refuses("data/items.json", R"("promotes":true },
  { "id":"elixir")", R"("promotes":"yes" },
  { "id":"elixir")", "\"promotes\" must be true or false");
}

TEST(promo_story_command_and_trigger) {
  auto eventErr = [](const std::string& ev) {
    Game g("promo_loader");
    g.put("maps/field.json", promotest::map(ev));
    return g.mapError("field");
  };
  auto promote = [&](const std::string& cmd) {
    return eventErr(R"({"id":"ev","when":{"battle":"start"},"do":[)" + cmd + "]}");
  };
  CHECK_STR(promote(R"({"promote":"rec","to":"captain"})"), "");
  CHECK_STR(promote(R"({"promote":"rec","to":"rider"})"), "");  // any unit type, listed or not
  CHECK_STR(promote(R"({"promote":"pwn"})"), "");               // pawn has exactly one promotion
  const std::string at = "event \"ev\": \"do\": command 1 (promote): ";
  std::string err = promote(R"({"promote":"rec"})");
  CHECK(has(err, at + "\"promote\" needs \"to\": \"recruit\" has 2 promotions"));
  err = promote(R"({"promote":"cad","to":"nobody"})");
  CHECK(has(err, at + "\"to\" names unknown unit type \"nobody\""));
  err = promote(R"({"promote":"nobody","to":"captain"})");
  CHECK(has(err, at + "\"promote\" names unknown unit"));
  err = promote(R"({"promote":"rec","to":"captain","by":1})");
  CHECK(has(err, "\"by\""));
  // The trigger takes a unit filter.
  CHECK_STR(eventErr(R"({"id":"ev","when":{"promoted":"rec"},"do":[]})"), "");
  CHECK_STR(eventErr(R"({"id":"ev","when":{"promoted":{"type":"captain"}},"do":[]})"), "");
  CHECK_STR(eventErr(R"({"id":"ev","when":{"promoted":{}},"do":[]})"), "");
  err = eventErr(R"({"id":"ev","when":{"promoted":"nobody"},"do":[]})");
  CHECK(has(err, "\"promoted\" names unknown unit \"nobody\""));
  err = eventErr(R"({"id":"ev","when":{"promoted":"rec","by":"cad"},"do":[]})");
  CHECK(has(err, "\"by\""));
}
