// Doors, keys, chests and visiting: the data and its refusals (docs/SRPG.md section 10). Game: tests/fixtures/srpg-doors.
#include "srpg_doors_fixture.h"

using namespace ote;
using namespace doorstest;

namespace {

int counter = 0;

// The load message of the game with `from` replaced by `to` in `rel` ("" = it loads).
std::string gameWith(const char* rel, const std::string& from, const std::string& to) {
  DCopy c("doors_data_" + std::to_string(counter++));
  c.swap(rel, from, to);
  return c.gameError();
}
// The same for a map of the game.
std::string mapWith(const char* map, const std::string& from, const std::string& to) {
  DCopy c("doors_map_" + std::to_string(counter++));
  c.swap(std::string("maps/") + map + ".json", from, to);
  return c.mapError(map);
}

const std::string kS = "data/structures.json: structure \"";
const std::string kI = "data/items.json: item \"";

}  // namespace

TEST(doors_data_structure_kinds_refuse_bad_values) {
  CHECK(saysIn(gameWith("data/structures.json", "\"door\": { \"opens\": \"cell_door_open\", \"locked\": false }", "\"door\": { \"locked\": false }"),
               kS + "latch_door\": \"door\": missing required \"opens\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"opens\": \"gate_open\", \"hp\"", "\"opens\": \"nowhere\", \"hp\""), "\"opens\" names unknown structure \"nowhere\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"opens\": \"gate_open\", \"hp\"", "\"opens\": \"cell_door_open\", \"hp\""), "has a different size"));
  CHECK(saysIn(gameWith("data/structures.json", "\"opens\": \"gate_open\", \"hp\"", "\"opens\": \"gate\", \"hp\""), "names the kind itself"));
  CHECK(saysIn(gameWith("data/structures.json", "\"hp\": 20", "\"hp\": 0"), "\"hp\" must be a whole number from 1"));
  CHECK(saysIn(gameWith("data/structures.json", "\"hp\": 20, \"stats\": { \"def\": 3 }, ", "\"stats\": { \"def\": 3 }, "), "need \"hp\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"stats\": { \"def\": 3 }", "\"stats\": { \"luck\": 3 }"), "unknown stat \"luck\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"keys\": [\"brass_key\", \"master_key\"]", "\"keys\": [\"nope\"]"), "\"keys\" names unknown item \"nope\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"keys\": [\"brass_key\", \"master_key\"]", "\"keys\": [\"potion\"]"), "which opens nothing"));
  CHECK(saysIn(gameWith("data/structures.json", "\"keys\": [\"brass_key\", \"master_key\"]", "\"keys\": [\"strong_key\"]"), "its \"opens\" does not name"));
  CHECK(saysIn(gameWith("data/structures.json", "\"keys\": [\"brass_key\", \"master_key\"]", "\"keys\": [\"brass_key\", \"brass_key\"]"), "lists \"brass_key\" twice"));
  CHECK(saysIn(gameWith("data/structures.json", "\"keys\": [\"brass_key\", \"master_key\"]", "\"keys\": [\"brass_key\"], \"by\": [\"trait\"]"), "\"keys\" are given but \"by\" does not list \"key\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"by\": [\"key\"]", "\"by\": [\"magic\"]"), "\"by\" entry 1 must be \"key\", \"trait\", not \"magic\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"by\": [\"key\"]", "\"by\": [\"break\"]"), "\"by\" entry 1 must be \"key\", \"trait\", not \"break\""));  // chests cannot be broken
  CHECK(saysIn(gameWith("data/structures.json", "\"from\": \"next\"", "\"from\": \"inside\""), "\"from\" must be \"on\", \"next\" or \"either\", not \"inside\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"moveCost\": { \"foot\": 0, \"fly\": 0 },\n    \"door\": { \"opens\": \"gate_open\"", "\"door\": { \"opens\": \"gate_open\""), "a door must stop at least one movement type"));
  CHECK(saysIn(gameWith("data/structures.json", "\"door\": { \"opens\": \"gate_open\"", "\"chest\": { \"opens\": \"gate_open\" }, \"door\": { \"opens\": \"gate_open\""), "has more than one of"));
  CHECK(saysIn(gameWith("data/structures.json", "\"door\": { \"opens\": \"gate_open\", \"hp\": 20,", "\"door\": { \"opens\": \"gate_open\", \"speed\": 1, \"hp\": 20,"), "unknown key \"speed\""));
  CHECK(saysIn(gameWith("data/structures.json", "\"visit\": { \"repeat\": true, \"from\": \"either\" }", "\"visit\": { \"repeat\": true, \"becomes\": \"hamlet_shut\" }"), "contradict each other"));
  CHECK(saysIn(gameWith("data/structures.json", "\"ruin\": \"hamlet_ruin\"", "\"ruin\": \"gate\""), "has a different size"));
  CHECK(saysIn(gameWith("data/structures.json", "\"blocksSight\": true,\n    \"moveCost\": { \"foot\": 0, \"fly\": 0 },\n    \"door\"", "\"blocksSight\": 3,\n    \"moveCost\": { \"foot\": 0, \"fly\": 0 },\n    \"door\""), "blocksSight"));
}

TEST(doors_data_items_and_abilities_refuse_bad_values) {
  CHECK(saysIn(gameWith("data/items.json", "\"opens\": [\"door\"]", "\"opens\": [\"hatch\"]"), "\"opens\" names \"hatch\", which is neither \"door\", \"chest\" nor a structure kind"));
  CHECK(saysIn(gameWith("data/items.json", "\"opens\": [\"door\"]", "\"opens\": []"), "\"opens\" must be a list"));
  CHECK(saysIn(gameWith("data/items.json", "\"kind\": \"weapon\", \"weapon\": \"iron_sword\", \"uses\": 40", "\"kind\": \"weapon\", \"weapon\": \"iron_sword\", \"opens\": [\"door\"]"), "\"opens\" belongs to items of kind \"usable\""));
  CHECK(saysIn(gameWith("data/abilities.json", "\"effect\": { \"unlock\": [\"door\", \"chest\"] }", "\"effect\": { \"unlock\": 3 }"), "\"unlock\" must be true"));
  CHECK(saysIn(gameWith("data/abilities.json", "\"effect\": { \"unlock\": [\"door\", \"chest\"] }", "\"effect\": { \"unlock\": [\"window\"] }"), "\"unlock\" kinds must be \"door\" or \"chest\""));
  CHECK(saysIn(gameWith("data/abilities.json", "\"target\": \"cell\", \"range\": [1, 1]", "\"target\": \"enemy\", \"range\": [1, 1]"), "\"unlock\" needs \"target\": \"cell\""));
  CHECK(saysIn(gameWith("data/abilities.json", "\"needs\": \"lockpick\"", "\"needs\": \"hairpin\""), "\"needs\" names unknown item \"hairpin\""));
  CHECK(saysIn(gameWith("data/abilities.json", "\"needs\": \"lockpick\"", "\"needs\": \"strong_key\""), "unlimited uses"));
  // The staff.
  CHECK(saysIn(gameWith("data/items.json", "\"effects\": [ { \"unlock\": true } ]", "\"effects\": [ { \"unlock\": true }, { \"heal\": 3 } ]"), "cannot be put together with another effect"));
  CHECK(saysIn(gameWith("data/items.json", "\"effects\": [ { \"unlock\": true } ]", "\"target\": \"ally\", \"effects\": [ { \"unlock\": true } ]"), "so the staff has no \"target\""));
  CHECK(saysIn(gameWith("data/items.json", "\"effects\": [ { \"unlock\": true } ]", "\"effects\": [ { \"unlock\": [\"window\"] } ]"), "\"unlock\" kinds must be"));
  CHECK_STR(gameWith("data/items.json", "\"effects\": [ { \"unlock\": true } ]", "\"effects\": [ { \"unlock\": [\"chest\"] } ], \"hit\": 70"), "");
}

TEST(doors_data_a_key_needs_structures_and_the_places_rule_is_checked) {
  DCopy c("doors_nostruct");
  c.drop("data/structures.json");
  CHECK(saysIn(c.gameError(), "\"opens\" names \"strongbox\", which is neither"));
  CHECK(saysIn(gameWith("game.json", "\"rules\": { ", "\"rules\": { \"places\": { \"actions\": \"sometimes\" }, "), "\"actions\" must be \"menu\""));
  CHECK(saysIn(gameWith("game.json", "\"rules\": { ", "\"rules\": { \"places\": { \"mode\": \"menu\" }, "), "unknown key \"mode\""));
  CHECK_STR(gameWith("game.json", "\"rules\": { ", "\"rules\": { \"places\": { \"actions\": \"menu\" }, "), "");
}

TEST(doors_data_map_entries_refuse_bad_values) {
  const std::string p = "structures[";
  CHECK(saysIn(mapWith("hall", "\"contents\": [ \"potion\", { \"resource\": \"gold\", \"amount\": 50 } ]", "\"contents\": []"), "\"contents\" is empty: a chest with nothing in it"));
  CHECK(saysIn(mapWith("hall", ", \"contents\": [ \"potion\", { \"resource\": \"gold\", \"amount\": 50 } ]", ""), "a chest needs \"contents\""));
  CHECK(saysIn(mapWith("hall", "\"contents\": [ \"potion\",", "\"contents\": [ \"elixir\","), "names unknown item \"elixir\""));
  CHECK(saysIn(mapWith("hall", "\"resource\": \"gold\"", "\"resource\": \"silver\""), "names unknown resource \"silver\""));
  CHECK(saysIn(mapWith("hall", "\"amount\": 50", "\"amount\": 0"), "\"amount\" must be a whole number of 1 or more"));
  CHECK(saysIn(mapWith("hall", "\"id\": \"v1\" }", "\"id\": \"v1\", \"contents\": [\"potion\"] }"), "\"contents\" only belongs to a chest"));
  CHECK(saysIn(mapWith("hall", "\"id\": \"v1\" }", "\"id\": \"v1\", \"locked\": true }"), "\"locked\" only belongs to a door or a chest"));
  CHECK(saysIn(mapWith("hall", "\"id\": \"v2\" }", "\"id\": \"v1\" }"), "\"id\" \"v1\" is already used by another structure"));
  CHECK(saysIn(mapWith("hall", "\"id\": \"v2\" }", "\"id\": \"v 2\" }"), "\"id\" must be letters, digits"));
  CHECK(saysIn(mapWith("hall", "\"id\": \"v1\" }", "\"id\": \"v1\", \"visitSides\": [\"green\"] }"), "\"visitSides\" names unknown side \"green\""));
  CHECK(saysIn(mapWith("hall", "\"id\": \"v1\" }", "\"id\": \"v1\", \"visitUnits\": [\"ghost\"] }"), "\"visitUnits\" names \"ghost\""));
  CHECK(saysIn(mapWith("hall", "\"type\": \"chest\", \"x\": 2, \"y\": 0, \"id\": \"c1\"", "\"type\": \"chest\", \"x\": 2, \"y\": 0, \"id\": \"c1\", \"visitSides\": [\"blue\"]"), "only belong to a place that can be visited"));
  // A door onto terrain nothing can enter.
  CHECK(saysIn(mapWith("hall", "\"type\": \"latch_door\", \"x\": 5, \"y\": 3", "\"type\": \"latch_door\", \"x\": 5, \"y\": 2"), "opens onto terrain \"wall\" that no movement type can enter"));
  // The computer opponent's markers.
  CHECK(saysIn(mapWith("hall", "\"id\": \"sneak\"", "\"id\": \"sneak\", \"goesFor\": [\"chests\"]"), "this unit is on a side a person plays"));
  CHECK(saysIn(mapWith("market", "\"goesFor\": [\"chests\"]", "\"goesFor\": [\"bridges\"]"), "must be \"chests\", \"villages\" or \"doors\", not \"bridges\""));
  CHECK(saysIn(mapWith("market", "\"goesFor\": [\"chests\"]", "\"goesFor\": []"), "\"goesFor\" must be a list"));
  CHECK_STR(mapWith("market", "\"goesFor\": [\"chests\"]", "\"goesFor\": [\"chests\", \"doors\", \"villages\"]"), "");
}

TEST(doors_data_events_name_known_doors_and_chests) {
  auto event = [](const std::string& when, const std::string& doo) {
    return mapWith("hall", "\"events\": [", "\"events\": [ { \"id\": \"t\", \"when\": " + when + ", \"do\": " + doo + " },");
  };
  CHECK_STR(event("{ \"place\": \"d1\", \"is\": \"opened\" }", "[ { \"openDoor\": \"d2\" } ]"), "");
  CHECK(saysIn(event("{ \"place\": \"nope\", \"is\": \"opened\" }", "[]"), "\"place\" names \"nope\", which is not the \"id\" of a structure of this map"));
  CHECK(saysIn(event("{ \"place\": \"d1\" }", "[]"), "needs \"is\""));
  CHECK(saysIn(event("{ \"place\": \"d1\", \"is\": \"visited\" }", "[]"), "is not a place that can be visited"));
  CHECK(saysIn(event("{ \"place\": \"v1\", \"is\": \"opened\" }", "[]"), "is not a door or chest"));
  CHECK(saysIn(event("{ \"place\": \"v2\", \"is\": \"destroyed\" }", "[]"), "cannot be destroyed"));
  CHECK(saysIn(event("{ \"place\": \"d1\", \"is\": \"broken\" }", "[]"), "\"is\" must be \"opened\", \"visited\" or \"destroyed\""));
  CHECK(saysIn(event("{ \"place\": \"d1\", \"is\": \"opened\", \"by\": \"ghost\" }", "[]"), "ghost"));
  CHECK(saysIn(event("{ \"battle\": \"start\" }", "[ { \"openDoor\": \"zzz\" } ]"), "\"openDoor\" names \"zzz\""));
  CHECK(saysIn(event("{ \"battle\": \"start\" }", "[ { \"openDoor\": \"c1\" } ]"), "\"c1\" is not a door"));
  CHECK(saysIn(event("{ \"battle\": \"start\" }", "[ { \"openChest\": \"d1\" } ]"), "\"d1\" is not a chest"));
  CHECK(saysIn(event("{ \"battle\": \"start\" }", "[ { \"openChest\": \"c1\", \"by\": \"ghost\" } ]"), "ghost"));
  CHECK(saysIn(event("{ \"battle\": \"start\" }", "[ { \"closeDoor\": \"d1\", \"x\": 1 } ]"), "unknown key"));
  CHECK_STR(event("{ \"battle\": \"start\" }", "[ { \"unlockDoor\": \"c1\" }, { \"lockDoor\": \"d1\" }, { \"closeDoor\": \"d2\" } ]"), "");
}

TEST(doors_data_a_game_without_the_features_hashes_as_before) {
  // The demo games have none of the new keys: their data hash must not move (the compat tests pin demo-hex and the demo-srpg
  // campaign save); this checks the plain fixture the same way, with and without an empty "places" rule.
  Doors a("doors_hash_a", "hall");
  Doors b("doors_hash_b", "hall", [](DCopy& c) { c.swap("game.json", "\"rules\": { ", "\"rules\": { \"places\": { \"actions\": \"menu\" }, "); });
  std::string ha, hb, err;
  CHECK(computeDataHash(a.rig.db, "hall", ha, err));
  CHECK(computeDataHash(b.rig.db, "hall", hb, err));
  CHECK(ha != hb);  // a rule that is written down is part of the rules; a game that does not write it hashes as it always did
}
