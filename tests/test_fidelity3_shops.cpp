// "Fidelity 3", item 5: several preparation shops ("prep": { "shops": [ { "id", "name", "sells", "buys", "music" } ] }) and "music" on the single
// "shop" (docs/EVENTS.md "Preparation"). Plays tests/fixtures/srpg-prep (map "orchard" has the shop).
#include "prep_fixture.h"

using namespace preptest;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

const char* kOne = "\"shop\": { \"sells\": [ \"potion\", \"tonic\" ] }";

// The orchard with another shop setting (the text after "shop":, or a whole replacement of the key).
std::string orchardError(const char* folder, const std::string& shopKey) {
  Game g;
  g.edit("maps/orchard.json", kOne, shopKey);
  std::string err;
  if (!g.load(folder, &err)) return err;
  return g.mapError("orchard");
}

const char* kTwo =
    "\"shops\": [ { \"id\": \"armory\", \"name\": \"Armory\", \"sells\": [ \"tonic\" ], \"music\": \"armory_theme\" },"
    " { \"id\": \"market\", \"name\": \"Market\", \"sells\": [ \"potion\", \"ring\" ], \"buys\": false } ]";

}  // namespace

TEST(fid3_shops_the_single_shop_is_one_unnamed_shop) {
  Run r;
  CHECK(r.begin("f3_shop_single") && r.toChapter2() && r.openChapter2());
  const PrepDef& p = *r.mapDef.prep;
  CHECK(p.hasShop && p.shops.size() == 1);
  CHECK(p.shops[0].id.empty() && p.shops[0].name.empty() && p.shops[0].music.empty() && p.shops[0].buys);
  CHECK(p.shops[0].sells == p.shop);  // the legacy fields are the first shop's
}

TEST(fid3_shops_a_list_loads_and_each_shop_sells_its_own) {
  Run r;
  r.g.edit("maps/orchard.json", kOne, kTwo);
  CHECK(r.begin("f3_shop_list") && r.toChapter2() && r.openChapter2());
  const PrepDef& p = *r.mapDef.prep;
  CHECK(p.hasShop && p.shops.size() == 2);
  CHECK_STR(p.shops[0].id, "armory");
  CHECK_STR(p.shops[0].name, "Armory");
  CHECK_STR(p.shops[0].music, "armory_theme");
  CHECK(p.shops[0].buys && !p.shops[1].buys);
  CHECK_STR(p.shops[1].music, "");
  const int potion = r.g.db.findItem("potion"), tonic = r.g.db.findItem("tonic");
  std::string why;
  // The armory sells the tonic only; the market the potion (and the ring).
  CHECK(!prepBuy(r.g.db, r.pt, r.mapDef, "sera", potion, why, 0) && has(why.c_str(), "not sold here"));
  CHECK(prepBuy(r.g.db, r.pt, r.mapDef, "sera", tonic, why, 0));
  CHECK(prepBuy(r.g.db, r.pt, r.mapDef, "sera", potion, why, 1));
  CHECK(!prepBuy(r.g.db, r.pt, r.mapDef, "sera", tonic, why, 1) && has(why.c_str(), "not sold here"));
  CHECK(!prepBuy(r.g.db, r.pt, r.mapDef, "sera", tonic, why, 2) && has(why.c_str(), "no such shop"));
  // Whether it buys is per shop.
  CHECK(!prepSell(r.g.db, r.pt, r.mapDef, "sera", 0, why, 1) && has(why.c_str(), "does not buy"));
}

TEST(fid3_shops_music_on_the_single_shop) {
  Run r;
  r.g.edit("maps/orchard.json", kOne, "\"shop\": { \"sells\": [ \"potion\" ], \"music\": \"inn\" }");
  CHECK(r.begin("f3_shop_music") && r.toChapter2() && r.openChapter2());
  CHECK_STR(r.mapDef.prep->shops[0].music, "inn");
  CHECK(r.mapDef.prep->shops[0].id.empty());
}

TEST(fid3_shops_loader_refusals) {
  CHECK_STR(orchardError("f3_shop_ok", kTwo), "");
  CHECK(has(orchardError("f3_shop_both", std::string(kOne) + ", " + kTwo).c_str(), "either \"shop\" (one shop) or \"shops\" (several), not both"));
  CHECK(has(orchardError("f3_shop_empty", "\"shops\": []").c_str(), "1 to 8 shops"));
  CHECK(has(orchardError("f3_shop_notlist", "\"shops\": {}").c_str(), "1 to 8 shops"));
  CHECK(has(orchardError("f3_shop_noid", "\"shops\": [ { \"sells\": [ \"potion\" ] } ]").c_str(), "id"));
  CHECK(has(orchardError("f3_shop_badid", "\"shops\": [ { \"id\": \"a b\", \"sells\": [ \"potion\" ] } ]").c_str(), "letters, digits"));
  CHECK(has(orchardError("f3_shop_dupid", "\"shops\": [ { \"id\": \"a\", \"name\": \"A\", \"sells\": [ \"potion\" ] }, { \"id\": \"a\", \"name\": \"B\", \"sells\": [ \"tonic\" ] } ]").c_str(), "used twice"));
  CHECK(has(orchardError("f3_shop_dupname", "\"shops\": [ { \"id\": \"a\", \"name\": \"Same\", \"sells\": [ \"potion\" ] }, { \"id\": \"b\", \"name\": \"Same\", \"sells\": [ \"tonic\" ] } ]").c_str(), "name \"Same\" is used twice"));
  CHECK(has(orchardError("f3_shop_noname", "\"shops\": [ { \"id\": \"a\", \"name\": \"\", \"sells\": [ \"potion\" ] } ]").c_str(), "\"name\" must not be empty"));
  CHECK(has(orchardError("f3_shop_nosells", "\"shops\": [ { \"id\": \"a\", \"sells\": [] } ]").c_str(), "at least one item"));
  CHECK(has(orchardError("f3_shop_unk", "\"shops\": [ { \"id\": \"a\", \"sells\": [ \"nope\" ] } ]").c_str(), "unknown item \"nope\""));
  CHECK(has(orchardError("f3_shop_noprice", "\"shops\": [ { \"id\": \"a\", \"sells\": [ \"token\" ] } ]").c_str(), "no \"price\""));
  CHECK(has(orchardError("f3_shop_key", "\"shops\": [ { \"id\": \"a\", \"sells\": [ \"potion\" ], \"cheap\": true } ]").c_str(), "unknown key \"cheap\""));
  CHECK(has(orchardError("f3_shop_music", "\"shops\": [ { \"id\": \"a\", \"sells\": [ \"potion\" ], \"music\": 5 } ]").c_str(), "\"music\" must be a music id"));
  CHECK(has(orchardError("f3_shop_music2", "\"shop\": { \"sells\": [ \"potion\" ], \"music\": \"\" }").c_str(), "\"music\" must be a music id"));
  CHECK(has(orchardError("f3_shop_single_key", "\"shop\": { \"sells\": [ \"potion\" ], \"id\": \"x\" }").c_str(), "unknown key \"id\""));
  std::string nine = "\"shops\": [";
  for (int i = 0; i < 9; ++i) nine += std::string(i ? "," : "") + "{ \"id\": \"s" + std::to_string(i) + "\", \"sells\": [ \"potion\" ] }";
  CHECK(has(orchardError("f3_shop_nine", nine + "]").c_str(), "1 to 8 shops"));
}

TEST(fid3_shops_a_game_without_the_keys_is_unchanged) {
  // The fixture's own single shop: sells, buys, and the buy / sell rules read exactly as before.
  Run r;
  CHECK(r.begin("f3_shop_plain") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(prepBuy(r.g.db, r.pt, r.mapDef, "sera", r.g.db.findItem("potion"), why));  // the default shop index 0
  CHECK(!prepBuy(r.g.db, r.pt, r.mapDef, "sera", r.g.db.findItem("ring"), why) && has(why.c_str(), "not sold here"));
}
