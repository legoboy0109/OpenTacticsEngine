// A free battle's stocked supply (game.json rules.items.freeBattleStorage; docs/GAME_FORMAT.md "Free battles") and the free
// battle's preparation items: the loader's checks, the supply's order and counts, the screen's trade on the battle's own
// units, and that a campaign chapter never gets the stock. Plays tests/fixtures/srpg-prep (items: potion 2 uses, tonic 1,
// ring and token unlimited; storage "anywhere"; chapter 1's map is meadow, the free prep map is orchard).
#include "core/campaign/free_supply.h"
#include "prep_fixture.h"

using namespace preptest;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

void stock(Game& g, const std::string& block) {
  g.edit("game.json", "\"storage\": \"anywhere\"", "\"storage\": \"anywhere\", \"freeBattleStorage\": " + block);
}

std::string refused(const std::string& block) {
  Game g;
  stock(g, block);
  std::string err;
  return g.load("free_stock_bad", &err) ? "" : err;
}

int countOf(const std::vector<ItemGrant>& v, const Database& db, const char* id) {
  int n = 0;
  for (const ItemGrant& g : v) n += db.items[(size_t)g.item].id == id;
  return n;
}

}  // namespace

TEST(free_stock_absent_means_nothing) {
  Game g;
  CHECK(g.load("free_stock_none"));
  CHECK(!g.db.rules.items.freeStock.given);
  CHECK(freeBattleSupply(g.db).empty());
  MapDef m = g.map("orchard");
  applyFreeSupply(g.db, m);
  CHECK(m.sides[0].storage.empty() && m.sides[0].resources.size() == 1 && m.sides[0].resources[0].amount == 25);
}

TEST(free_stock_each_except_and_counts) {
  Game g;
  stock(g, R"({ "each": 2, "except": ["token"], "items": [ { "item": "potion", "count": 5, "uses": 1 }, "ring", { "item": "tonic", "count": 0 } ] })");
  CHECK(g.load("free_stock_ok"));
  const std::vector<ItemGrant> s = freeBattleSupply(g.db);
  // the game's item order, identical copies together: potion x5 (1 use each), ring x1, no tonic, no token
  CHECK_EQ((int)s.size(), 6);
  CHECK(countOf(s, g.db, "potion") == 5 && countOf(s, g.db, "ring") == 1);
  CHECK(countOf(s, g.db, "tonic") == 0 && countOf(s, g.db, "token") == 0);
  CHECK(g.db.items[(size_t)s[0].item].id == "potion" && s[0].uses == 1 && s[5].uses == -1);
  Game e;  // every item, full uses, an excepted item named in "items" is given after all
  stock(e, R"({ "each": 3, "except": ["token", "ring"], "items": ["ring"] })");
  CHECK(e.load("free_stock_full"));
  const std::vector<ItemGrant> t = freeBattleSupply(e.db);
  CHECK_EQ((int)t.size(), 3 + 3 + 1);
  CHECK(t[0].uses == 2 && t[3].uses == 1);  // potion 2 uses, tonic 1
}

TEST(free_stock_only_with_a_storage_and_only_the_human_sides) {
  Game g;
  stock(g, R"({ "each": 1, "funds": { "gold": 100 } })");
  g.edit("game.json", "\"storage\": \"anywhere\"", "\"storage\": \"off\"");
  CHECK(g.load("free_stock_off"));
  CHECK(freeBattleSupply(g.db).empty());  // no shared storage: nothing to stock
  Game h;
  stock(h, R"({ "each": 1, "funds": { "gold": 100 } })");
  CHECK(h.load("free_stock_sides"));
  MapDef m = h.map("orchard");
  applyFreeSupply(h.db, m);
  CHECK_EQ((int)m.sides[0].storage.size(), 4);
  CHECK_EQ(m.sides[0].resources[0].amount, 125);  // the map's 25 plus the funds
  CHECK(m.sides[1].storage.empty() && m.sides[1].resources.empty());  // the computer side gets nothing
}

TEST(free_stock_refuses_what_it_does_not_understand) {
  CHECK(has(refused(R"({ "each": 3, "all": 1 })"), "unknown key \"all\""));
  CHECK(has(refused(R"({ "each": 3, "except": ["nope"] })"), "unknown item \"nope\""));
  CHECK(has(refused(R"({ "except": ["token"] })"), "\"except\""));
  CHECK(has(refused(R"({ "each": -1 })"), "\"each\""));
  CHECK(has(refused(R"({ "each": 1.5 })"), "\"each\""));
  CHECK(has(refused(R"({ "items": [ "nope" ] })"), "unknown item \"nope\""));
  CHECK(has(refused(R"({ "items": [ { "item": "potion", "count": 100 } ] })"), "\"count\""));
  CHECK(has(refused(R"({ "items": [ { "item": "potion", "cnt": 1 } ] })"), "unknown key \"cnt\""));
  CHECK(has(refused(R"({ "items": [ { "item": "potion", "uses": 3 } ] })"), "\"uses\""));  // a potion has 2
  CHECK(has(refused(R"({ "items": [ { "item": "ring", "uses": 1 } ] })"), "unlimited"));
  CHECK(has(refused(R"({ "items": [ "ring", "ring" ] })"), "listed twice"));
  CHECK(has(refused(R"({ "funds": { "silver": 5 } })"), "unknown resource"));
  CHECK(has(refused(R"({ "funds": { "gold": -5 } })"), "negative"));
  CHECK(has(refused("[]"), "freeBattleStorage"));
  CHECK(refused(R"({})").empty());  // an empty block is allowed and gives nothing
}

TEST(free_battle_items_trade_on_the_battles_own_units) {
  Game g;
  stock(g, R"({ "each": 2, "funds": { "gold": 100 } })");
  CHECK(g.load("free_stock_prep"));
  MapDef map = g.map("orchard");
  applyFreeSupply(g.db, map);
  Playthrough pt;
  prepOpenFree(g.db, pt, map);
  std::string why;
  CHECK(pt.findUnit("ilsa") != nullptr && pt.findUnit("guard") == nullptr);  // the map's own candidates, not its fixed units
  CHECK_EQ((int)prepStorage(pt, map).size(), 8);
  CHECK_EQ(prepResources(g.db, pt, map)[0], 125);
  CHECK_EQ((int)pt.findUnit("ilsa")->items.size(), 1);
  CHECK(prepFetch(g.db, pt, map, "ilsa", 0, why));  // the first stored item: a potion
  CHECK(prepFetch(g.db, pt, map, "ilsa", 0, why));  // another
  CHECK(!prepFetch(g.db, pt, map, "ilsa", 0, why) && has(why.c_str(), "cannot carry more"));  // size 3
  CHECK(prepBuy(g.db, pt, map, "", g.db.findItem("tonic"), why));  // the shop pays from the free battle's funds, into the storage
  CHECK_EQ(prepResources(g.db, pt, map)[0], 125 - 30);
  CHECK(prepBegin(g.db, pt, map, why));
  MapDef start;
  prepApplyToMap(g.db, pt, map, start);
  Battle b;
  std::string err;
  CHECK(b.init(g.db, start, g.formulas, 1, err));
  const Unit& ilsa = b.units[(size_t)b.unitByStoryId("ilsa")];
  CHECK_EQ((int)ilsa.items.size(), 3);
  CHECK(g.db.items[(size_t)ilsa.items[1].item].id == "potion");
  CHECK_EQ((int)b.sides[0].storage.size(), 8 - 2 + 1);
  CHECK_EQ(b.sides[0].resources[0], 95);
  CHECK(pt.prep.scratch && !Playthrough().prep.scratch);
}

TEST(free_stock_never_reaches_a_campaign_chapter) {
  Run r;
  stock(r.g, R"({ "each": 3, "funds": { "gold": 500 } })");
  CHECK(r.begin("free_stock_campaign") && r.before() && r.start());
  CHECK_EQ((int)r.b.sides[0].storage.size(), 1);  // meadow's own single potion
  CHECK(r.b.sides[0].resources.empty() || r.b.sides[0].resources[0] < 500);
}
