// The preparation shop buys items back, and a weapon can be equipped on the screen (docs/EVENTS.md "Preparation"):
// sell values and refusals, selling out of the storage, the game's sell fraction, a shop that does not buy, a save
// round trip of the result, and prepEquip's wield checks. Plays tests/fixtures/srpg-prep with carried weapons.
#include "prep_fixture.h"

using namespace preptest;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

// The fixture with weapons carried: sera [sword_item, axe_item (5 of 10 uses), potion], tomas [sword_item, bow_item],
// bram [tonic, relic], and a storage of [potion, token, relic]. The soldier wields sword and axe, the archer the bow.
void carry(Game& g, const std::string& sellPercent = "") {
  g.edit("game.json", "\"weapons\": \"fixed\"", "\"weapons\": \"carried\"");
  if (!sellPercent.empty()) g.edit("game.json", "\"inventorySize\": 3,", "\"inventorySize\": 3, \"sellPercent\": " + sellPercent + ",");
  g.edit("data/weapons.json", "{ \"id\": \"bow\"", "{ \"id\": \"axe\", \"name\": \"Axe\", \"props\": { \"might\": 3 } },\n  { \"id\": \"bow\"");
  g.edit("data/units.json", "\"weapons\": [\"sword\"] },\n  { \"id\": \"archer\"", "\"weapons\": [\"sword\", \"axe\"] },\n  { \"id\": \"archer\"");
  g.edit("data/items.json", "{ \"id\": \"token\"",
         "{ \"id\": \"sword_item\", \"name\": \"Sword\", \"kind\": \"weapon\", \"weapon\": \"sword\", \"uses\": 10, \"price\": { \"gold\": 100 } },\n"
         "  { \"id\": \"axe_item\", \"name\": \"Axe\", \"kind\": \"weapon\", \"weapon\": \"axe\", \"uses\": 10, \"price\": { \"gold\": 60 } },\n"
         "  { \"id\": \"bow_item\", \"name\": \"Bow\", \"kind\": \"weapon\", \"weapon\": \"bow\", \"uses\": 10, \"price\": { \"gold\": 80 } },\n"
         "  { \"id\": \"relic\", \"name\": \"Relic\", \"kind\": \"accessory\", \"bonus\": { \"def\": 1 }, \"price\": { \"gold\": 40 }, \"sellable\": false },\n"
         "  { \"id\": \"token\"");
  g.edit("maps/meadow.json", "\"name\": \"Sera\", \"items\": [\"potion\", \"ring\"]",
         "\"name\": \"Sera\", \"items\": [\"sword_item\", { \"item\": \"axe_item\", \"uses\": 5 }, \"potion\"]");
  g.edit("maps/meadow.json", "\"name\": \"Tomas\" }", "\"name\": \"Tomas\", \"items\": [\"sword_item\", \"bow_item\"] }");
  g.edit("maps/meadow.json", "\"items\": [\"tonic\"]", "\"items\": [\"tonic\", \"relic\"]");
  g.edit("maps/meadow.json", "\"storage\": [\"potion\"]", "\"storage\": [\"potion\", \"token\", \"relic\"]");
}

int gold(const Run& r) { return prepResources(r.g.db, r.pt, r.mapDef)[0]; }

}  // namespace

TEST(prep_sell_value_is_half_the_price_scaled_by_the_uses_left) {
  Run r;
  carry(r.g);
  CHECK(r.begin("prep_sell_value") && r.toChapter2() && r.openChapter2());
  const Database& db = r.g.db;
  std::vector<Amount> v;
  std::string why;
  ItemGrant g;
  g.item = db.findItem("sword_item");
  g.uses = 10;
  CHECK(prepSellValue(db, g, v, why) && v.size() == 1 && v[0].amount == 50);  // 100 * 50% * 10/10
  g.uses = 3;
  CHECK(prepSellValue(db, g, v, why) && v[0].amount == 15);  // 100 * 50% * 3/10
  g.uses = 0;
  CHECK(prepSellValue(db, g, v, why) && v[0].amount == 0);  // worn out: 0, and still sellable (the minimum is 0)
  g.item = db.findItem("ring");  // unlimited uses: no scaling
  g.uses = -1;
  CHECK(prepSellValue(db, g, v, why) && v[0].amount == 25);
  g.item = db.findItem("potion");  // 20 gold, 2 uses: one left = 5
  g.uses = 1;
  CHECK(prepSellValue(db, g, v, why) && v[0].amount == 5);
  g.item = db.findItem("token");
  CHECK(!prepSellValue(db, g, v, why) && has(why.c_str(), "no price") && v.empty());
  g.item = db.findItem("relic");
  CHECK(!prepSellValue(db, g, v, why) && has(why.c_str(), "cannot be sold"));
}

TEST(prep_sell_pays_the_resources_and_takes_the_item) {
  Run r;
  carry(r.g);
  CHECK(r.begin("prep_sell_pays") && r.toChapter2() && r.openChapter2());
  const Database& db = r.g.db;
  std::string why;
  CHECK_EQ(gold(r), 125);
  // sera: [sword_item 10/10, axe_item 5/10, potion 2/2]
  CHECK(prepSell(db, r.pt, r.mapDef, "sera", 1, why));  // the axe: 60 * 50% * 5/10 = 15
  CHECK_EQ(gold(r), 140);
  CHECK_EQ((int)r.roster("sera")->items.size(), 2);
  CHECK(db.items[(size_t)r.roster("sera")->items[1].item].id == "potion");  // the rest slid up
  CHECK(prepSell(db, r.pt, r.mapDef, "sera", 0, why));  // the sword: 50
  CHECK_EQ(gold(r), 190);
  CHECK(!prepSell(db, r.pt, r.mapDef, "sera", 5, why) && has(why.c_str(), "does not carry"));
  CHECK_EQ(gold(r), 190);
  // bram: [tonic, relic]: the relic is marked unsellable, the tonic fetches 15
  CHECK(!prepSell(db, r.pt, r.mapDef, "bram", 1, why) && has(why.c_str(), "cannot be sold"));
  CHECK_EQ((int)r.roster("bram")->items.size(), 2);
  CHECK(prepSell(db, r.pt, r.mapDef, "bram", 0, why));
  CHECK_EQ(gold(r), 205);
  CHECK(!prepSell(db, r.pt, r.mapDef, "nobody", 0, why));
  // the storage: [potion, token, relic], then what chapter 2's map added (nothing)
  const size_t before = prepStorage(r.pt, r.mapDef).size();
  CHECK(!prepSell(db, r.pt, r.mapDef, "", 1, why) && has(why.c_str(), "no price"));  // the token
  CHECK(!prepSell(db, r.pt, r.mapDef, "", 2, why) && has(why.c_str(), "cannot be sold"));
  CHECK(!prepSell(db, r.pt, r.mapDef, "", 9, why) && has(why.c_str(), "not in the storage"));
  CHECK_EQ((int)prepStorage(r.pt, r.mapDef).size(), (int)before);
  CHECK(prepSell(db, r.pt, r.mapDef, "", 0, why));  // the potion: 10
  CHECK_EQ(gold(r), 215);
  CHECK_EQ((int)prepStorage(r.pt, r.mapDef).size(), (int)before - 1);
  // The shop can pay for a purchase with the proceeds, and the battle starts with the new total.
  CHECK(prepBuy(db, r.pt, r.mapDef, "tomas", db.findItem("tonic"), why));
  CHECK(prepBegin(db, r.pt, r.mapDef, why) && r.start());
  CHECK_EQ(r.b.sides[0].resources[0], 215 - 30);
}

TEST(prep_sell_uses_the_games_fraction) {
  Run r;
  carry(r.g, "33");
  CHECK(r.begin("prep_sell_fraction") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(prepSell(r.g.db, r.pt, r.mapDef, "sera", 0, why));  // 100 * 33% = 33
  CHECK_EQ(gold(r), 125 + 33);
  CHECK(prepSell(r.g.db, r.pt, r.mapDef, "sera", 0, why));  // axe 5/10: 60 * 33 * 5 / 10 / 100 = 9.9 -> 9
  CHECK_EQ(gold(r), 125 + 33 + 9);
  Game zero;
  carry(zero, "0");
  CHECK(zero.load("prep_sell_zero"));
  CHECK_EQ(zero.db.rules.items.sellPercent, 0);
  Game bad;
  carry(bad, "101");
  std::string err;
  CHECK(!bad.load("prep_sell_bad", &err) && has(err.c_str(), "sellPercent"));
  Game dflt;
  CHECK(dflt.load("prep_sell_default"));
  CHECK_EQ(dflt.db.rules.items.sellPercent, 50);
}

TEST(prep_a_shop_that_does_not_buy_refuses_and_a_map_without_a_shop_too) {
  {
    Run r;
    carry(r.g);
    r.g.edit("maps/orchard.json", "\"shop\": { \"sells\": [ \"potion\", \"tonic\" ] }", "\"shop\": { \"sells\": [ \"potion\", \"tonic\" ], \"buys\": false }");
    CHECK(r.begin("prep_sell_nobuy") && r.toChapter2() && r.openChapter2());
    CHECK(r.mapDef.prep->hasShop && !r.mapDef.prep->buys);
    std::string why;
    CHECK(!prepSell(r.g.db, r.pt, r.mapDef, "sera", 0, why) && has(why.c_str(), "does not buy"));
    CHECK_EQ((int)r.roster("sera")->items.size(), 3);
    CHECK(prepBuy(r.g.db, r.pt, r.mapDef, "tomas", r.g.db.findItem("potion"), why));  // buying is unaffected
  }
  {
    Run r;
    carry(r.g);
    r.g.edit("maps/orchard.json", ",\n    \"shop\": { \"sells\": [ \"potion\", \"tonic\" ] } }", " }");
    CHECK(r.begin("prep_sell_noshop") && r.toChapter2() && r.openChapter2());
    CHECK(!r.mapDef.prep->hasShop);
    std::string why;
    CHECK(!prepSell(r.g.db, r.pt, r.mapDef, "sera", 0, why) && has(why.c_str(), "no shop"));
  }
  Run d;  // the default: a shop buys
  carry(d.g);
  CHECK(d.begin("prep_sell_default_buys") && d.toChapter2() && d.openChapter2());
  CHECK(d.mapDef.prep->buys);
  Game bad;
  bad.edit("maps/orchard.json", "\"sells\": [ \"potion\", \"tonic\" ]", "\"sells\": [ \"potion\", \"tonic\" ], \"buys\": \"yes\"");
  std::string err;
  CHECK(bad.load("prep_sell_badbuys", &err) && has(bad.mapError("orchard").c_str(), "buys"));
}

TEST(prep_sell_survives_a_save_round_trip) {
  Run r;
  carry(r.g);
  CHECK(r.begin("prep_sell_save") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(prepSell(r.g.db, r.pt, r.mapDef, "sera", 1, why) && prepSell(r.g.db, r.pt, r.mapDef, "", 0, why));
  CHECK(!prepEquip(r.g.db, r.pt, r.mapDef, "sera", 1, why));  // sera is [sword, potion] now: the potion is no weapon
  Json file;
  std::string err;
  CHECK(buildCampaignSave(r.g.db, "muster", r.pt, nullptr, 1000, file, err));
  Json again;
  CHECK(Json::parse(file.dump(0), again, err));
  Playthrough back;
  Battle b;
  bool hasBattle = true;
  CHECK(restoreCampaignSave(r.g.db, r.g.formulas, "muster", again, back, b, hasBattle, err));
  CHECK(!hasBattle && back.prep.open);
  CHECK_EQ(prepResources(r.g.db, back, r.mapDef)[0], 125 + 15 + 10);
  CHECK_EQ((int)back.findUnit("sera")->items.size(), 2);
  CHECK_EQ((int)prepStorage(back, r.mapDef).size(), 2);
  Json a, c;
  savePlaythrough(r.g.db, r.pt, a);
  savePlaythrough(r.g.db, back, c);
  CHECK_STR(a.dump(0), c.dump(0));
}

TEST(prep_equip_follows_the_wield_rules_and_changes_the_battles_weapon) {
  Run r;
  carry(r.g);
  CHECK(r.begin("prep_equip") && r.toChapter2() && r.openChapter2());
  const Database& db = r.g.db;
  std::string why;
  // sera [sword_item, axe_item, potion]: the sword is in hand.
  CHECK_EQ(prepEquippedAt(db, r.pt, "sera"), 0);
  CHECK(!prepEquipReason(db, r.pt, "sera", 0, why) && has(why.c_str(), "already equipped"));
  CHECK(!prepEquipReason(db, r.pt, "sera", 2, why) && has(why.c_str(), "cannot be wielded"));  // a potion
  CHECK(!prepEquip(db, r.pt, r.mapDef, "sera", 9, why) && has(why.c_str(), "does not carry"));
  // tomas [sword_item, bow_item]: an archer cannot wield the sword; the bow is his weapon in hand, though second.
  CHECK_EQ(prepEquippedAt(db, r.pt, "tomas"), 1);
  CHECK(!prepEquip(db, r.pt, r.mapDef, "tomas", 0, why) && has(why.c_str(), "Tomas cannot wield"));
  CHECK(!prepEquip(db, r.pt, r.mapDef, "tomas", 1, why) && has(why.c_str(), "already equipped"));
  CHECK(db.items[(size_t)r.roster("tomas")->items[0].item].id == "sword_item");  // refusals change nothing
  // the axe to the front
  CHECK(prepEquipReason(db, r.pt, "sera", 1, why));
  CHECK(prepEquip(db, r.pt, r.mapDef, "sera", 1, why));
  CHECK(db.items[(size_t)r.roster("sera")->items[0].item].id == "axe_item");
  CHECK_EQ(r.roster("sera")->items[0].uses, 5);  // its wear travelled with it
  CHECK_EQ(prepEquippedAt(db, r.pt, "sera"), 0);
  // the battle that follows swings the axe
  CHECK(r.bench("tomas") && prepBegin(db, r.pt, r.mapDef, why) && r.start());
  const Unit& sera = r.b.units[(size_t)r.unit("sera")];
  const int slot = r.b.equippedSlot(sera);
  CHECK(slot >= 0 && db.items[(size_t)sera.items[(size_t)slot].item].id == "axe_item");
  CHECK_EQ(slot, 0);
}

TEST(prep_equip_is_refused_in_a_game_with_fixed_weapons) {
  Run r;
  CHECK(r.begin("prep_equip_fixed") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(!prepEquip(r.g.db, r.pt, r.mapDef, "sera", 0, why) && has(why.c_str(), "not carried"));
  CHECK_EQ(prepEquippedAt(r.g.db, r.pt, "sera"), -1);
}
