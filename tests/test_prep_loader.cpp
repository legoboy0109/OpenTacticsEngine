// A map's "prep" block: what the loader reads and what it refuses, in plain words (docs/EVENTS.md "Preparation").
#include "prep_fixture.h"

using namespace preptest;

namespace {

// Loads the fixture with `from` replaced by `to` in the orchard map and returns the loader's message for that map.
std::string orchardError(const std::string& tag, const std::string& from, const std::string& to) {
  Game g;
  g.edit("maps/orchard.json", from, to);
  std::string err;
  if (!g.load("prep_loader_" + tag, &err)) return "GAME: " + err;
  return g.mapError("orchard");
}

bool mentions(const std::string& err, const char* text) { return err.find(text) != std::string::npos; }

}  // namespace

TEST(prep_loader_reads_the_block) {
  Game g;
  CHECK(g.load("prep_loader_read"));
  MapDef m = g.map("orchard");
  CHECK(m.prep != nullptr);
  CHECK_EQ((int)m.prep->slots.size(), 4);
  CHECK_EQ(m.prep->limit, 3);
  CHECK_EQ(m.prep->slots[3].x, 1);
  CHECK_EQ((int)m.prep->required.size(), 1);
  CHECK(m.prep->required[0].id == "sera" && m.prep->required[0].slot == 0);
  CHECK(m.prep->fixed.size() == 1 && m.prep->fixed[0] == "guard");
  CHECK(m.prep->hasShop && m.prep->shop.size() == 2);
  CHECK_EQ(m.prep->side, 0);
  MapDef hill = g.map("hilltop");
  CHECK(hill.prep && hill.prep->forbidden.size() == 1 && !hill.prep->hasShop && hill.prep->limit == 4);
  CHECK(g.map("meadow").prep == nullptr);  // a map without the block is an ordinary map
}

TEST(prep_loader_limit_defaults_to_the_slot_count) {
  Game g;
  g.edit("maps/orchard.json", "    \"limit\": 3,\n", "");
  CHECK(g.load("prep_loader_limit_default"));
  CHECK_EQ(g.map("orchard").prep->limit, 4);
}

TEST(prep_loader_refuses_bad_values_in_plain_words) {
  std::string e;
  e = orchardError("key", "\"limit\": 3,", "\"limit\": 3, \"party\": 2,");
  CHECK(mentions(e, "unknown key \"party\""));
  e = orchardError("noslots", "\"slots\": [ { \"x\": 0, \"y\": 1 }, { \"x\": 0, \"y\": 2 }, { \"x\": 0, \"y\": 3 }, { \"x\": 1, \"y\": 0 } ],", "\"slots\": [],");
  CHECK(mentions(e, "\"slots\" must be a list of squares"));
  e = orchardError("outside", "{ \"x\": 0, \"y\": 3 }", "{ \"x\": 9, \"y\": 3 }");
  CHECK(mentions(e, "slot 3: square 9,3 is outside the map"));
  e = orchardError("dup", "{ \"x\": 0, \"y\": 3 }", "{ \"x\": 0, \"y\": 2 }");
  CHECK(mentions(e, "already another slot"));
  e = orchardError("overlimit", "\"limit\": 3", "\"limit\": 5");
  CHECK(mentions(e, "\"limit\" 5 is more than the map's 4 slots"));
  e = orchardError("zerolimit", "\"limit\": 3", "\"limit\": 0");
  CHECK(mentions(e, "\"limit\" must be at least 1"));
  e = orchardError("reqslot", "\"slot\": 1", "\"slot\": 7");
  CHECK(mentions(e, "slot\" 7 does not exist"));
  e = orchardError("reqtoomany", "\"required\": [ { \"character\": \"sera\", \"slot\": 1 } ]",
                   "\"required\": [ \"sera\", \"tomas\", \"wren\", \"bram\" ]");
  CHECK(mentions(e, "4 required characters do not fit the limit of 3"));
  e = orchardError("reqdupslot", "\"required\": [ { \"character\": \"sera\", \"slot\": 1 } ]",
                   "\"required\": [ { \"character\": \"sera\", \"slot\": 1 }, { \"character\": \"tomas\", \"slot\": 1 } ]");
  CHECK(mentions(e, "slot 1 is already fixed"));
  e = orchardError("reqforbid", "\"fixed\": [ \"guard\" ],", "\"fixed\": [ \"guard\" ], \"forbidden\": [ \"sera\" ],");
  CHECK(mentions(e, "also forbidden"));
  e = orchardError("reqbad", "{ \"character\": \"sera\", \"slot\": 1 }", "7");
  CHECK(mentions(e, "required entry 1: must be a character id"));
  e = orchardError("fixedgone", "\"fixed\": [ \"guard\" ]", "\"fixed\": [ \"nobody\" ]");
  CHECK(mentions(e, "\"fixed\" names \"nobody\", but the map does not place"));
  e = orchardError("forbidplaced", "\"fixed\": [ \"guard\" ],", "\"fixed\": [ \"guard\" ], \"forbidden\": [ \"ilsa\" ],");
  CHECK(mentions(e, "\"ilsa\" is forbidden, but the map places that unit"));
  e = orchardError("shopitem", "\"potion\", \"tonic\"", "\"potion\", \"nothing\"");
  CHECK(mentions(e, "unknown item \"nothing\""));
  e = orchardError("shopprice", "\"potion\", \"tonic\"", "\"potion\", \"token\"");
  CHECK(mentions(e, "\"token\", which has no \"price\""));
  e = orchardError("shopempty", "\"sells\": [ \"potion\", \"tonic\" ]", "\"sells\": []");
  CHECK(mentions(e, "must list at least one item id"));
  e = orchardError("shopshape", "\"shop\": { \"sells\": [ \"potion\", \"tonic\" ] }", "\"shop\": [ \"potion\" ]");
  CHECK(mentions(e, "\"shop\" must be an object"));
  e = orchardError("side", "\"limit\": 3,", "\"limit\": 3, \"side\": \"red\",");
  CHECK(mentions(e, "computer side"));
  e = orchardError("side2", "\"limit\": 3,", "\"limit\": 3, \"side\": \"green\",");
  CHECK(mentions(e, "unknown side \"green\""));
  e = orchardError("shape", "\"prep\": {", "\"prep\": 5, \"x\": {");
  CHECK(mentions(e, "\"prep\" must be an object"));
}

TEST(prep_loader_refuses_slots_on_squares_that_stay_occupied) {
  // The fixed unit's square, an enemy's square: the unit there stays, so nobody could be deployed.
  std::string e = orchardError("onfixed", "{ \"x\": 0, \"y\": 3 }", "{ \"x\": 2, \"y\": 5 }");
  CHECK(mentions(e, "slot 3: is the square of a unit that stays where it is placed"));
  e = orchardError("onenemy", "{ \"x\": 0, \"y\": 3 }", "{ \"x\": 7, \"y\": 1 }");
  CHECK(mentions(e, "an unnamed unit"));
  // A choosable unit's own square is fine: the slots decide where it starts.
  e = orchardError("onchoosable", "{ \"x\": 0, \"y\": 3 }", "{ \"x\": 3, \"y\": 4 }");
  CHECK_STR(e, "");
}

TEST(prep_loader_refuses_a_slot_nobody_can_stand_on) {
  Game g;
  g.edit("data/terrain.json", "\"moveCost\": { \"foot\": 0 } }", "\"moveCost\": { \"foot\": 0, \"wing\": 0 } }");
  std::string err;
  CHECK(g.load("prep_loader_rock", &err));
  // The pond is now impassable for every kind of unit and the orchard's fourth slot is on it.
  CHECK(mentions(g.mapError("orchard"), "slot 4: square 1,0 is terrain \"pond\", which no kind of unit can stand on"));
}

TEST(prep_loader_a_prep_without_a_human_side_is_refused) {
  std::string e = orchardError("noside", "\"id\": \"blue\", \"name\": \"Company\", \"controller\": \"human\"",
                               "\"id\": \"blue\", \"name\": \"Company\", \"controller\": \"ai\"");
  CHECK(mentions(e, "no human side"));
}
