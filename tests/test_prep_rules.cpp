// The preparation rules: the default pick, required / forbidden / limit, swaps and movement types, the roster across
// chapters, trading, the shop, and the order of scene, screen and battle start (docs/EVENTS.md "Preparation").
#include "prep_fixture.h"

using namespace preptest;

namespace {

bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

}  // namespace

TEST(prep_default_pick_is_required_then_roster_order_up_to_the_limit) {
  Run r;
  CHECK(r.begin("prep_default") && r.toChapter2());
  CHECK_EQ((int)r.pt.roster.size(), 4);  // chapter 1 had no preparation: it ran as before
  CHECK(r.before());
  r.loadChapterMap();
  CHECK_EQ((int)prepEntries(r.g.db, r.pt, r.mapDef).size(), 5);
  CHECK(!prepEntries(r.g.db, r.pt, r.mapDef)[4].inRoster);  // ilsa: only the map knows her so far
  prepOpen(r.g.db, r.pt, r.mapDef, true);
  CHECK_STR(r.slots(), "sera tomas wren -");  // sera on her own slot 1, then roster order; limit 3
  std::vector<PrepEntry> e = prepEntries(r.g.db, r.pt, r.mapDef);
  CHECK_EQ((int)e.size(), 5);  // sera tomas wren bram + ilsa (only the map knows her); guard is fixed, not a candidate
  CHECK(e[4].id == "ilsa" && e[4].inRoster && e[4].slot == -1);  // opening the screen took her into the roster
  CHECK(e[0].lock == PrepLock::Required && e[0].fixedSlot == 0);
  CHECK(!prepLockReason(PrepLock::Required).empty() && prepLockReason(PrepLock::None).empty());
  CHECK_EQ(prepDeployedCount(r.pt), 3);
}

TEST(prep_default_pick_prefers_the_last_chapters_party) {
  Run r;
  CHECK(r.begin("prep_default_last") && r.toChapter2());
  CHECK(r.openChapter2());
  CHECK(r.bench("tomas") && r.deploy("bram"));  // sera, wren, bram go to chapter 2's battle
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why));
  CHECK(r.start());
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK_EQ((int)r.pt.lastDeployed.size(), 4);  // the fixed guard too, in battle order, then the deployed ones
  r.lines.clear();
  CHECK(runScene(r.g.db, r.pt, SceneKind::After, r.lines));
  CHECK(r.before());
  r.loadChapterMap();
  // Chapter 3 (limit 4, wren forbidden): last chapter's party first, then the rest in roster order.
  std::vector<PrepDeploy> pick = prepDefaultDeployment(r.g.db, r.pt, r.mapDef);
  CHECK_EQ((int)pick.size(), 4);
  std::string ids;
  for (const PrepDeploy& d : pick) ids += d.id + " ";
  CHECK_STR(ids, "guard sera bram tomas ");
}

TEST(prep_required_forbidden_and_limit) {
  Run r;
  CHECK(r.begin("prep_rules") && r.toChapter2() && r.openChapter2());
  CHECK(!r.bench("sera") && has(r.why, "must fight in this battle"));
  CHECK(r.bench("tomas") && r.bench("wren"));
  CHECK(r.deploy("bram") && r.deploy("ilsa"));  // sera + bram + ilsa = the limit of 3
  CHECK(!r.deploy("tomas") && has(r.why, "limit of 3 is reached"));
  CHECK(!r.deploy("nobody") && has(r.why, "not in your party"));
  CHECK(!r.deploy("bram") && has(r.why, "already deployed"));
  CHECK(!r.bench("tomas") && has(r.why, "not deployed"));
  CHECK_EQ(prepDeployedCount(r.pt), 3);
  // The hilltop forbids wren.
  Run h;
  CHECK(h.begin("prep_rules_hill") && h.toChapter2() && h.openChapter2());
  std::string why;
  CHECK(prepBegin(h.g.db, h.pt, h.mapDef, why) && h.start());
  h.decide(true);
  CHECK(finishChapterBattle(h.g.db, h.pt, h.b) == ChapterResult::Won);
  h.lines.clear();
  CHECK(runScene(h.g.db, h.pt, SceneKind::After, h.lines) && h.before());
  h.loadChapterMap();
  prepOpen(h.g.db, h.pt, h.mapDef, true);
  CHECK(!h.deploy("wren") && has(h.why, "cannot take part in this battle"));
  std::vector<PrepEntry> e = prepEntries(h.g.db, h.pt, h.mapDef);
  bool seen = false;
  for (const PrepEntry& x : e)
    if (x.id == "wren") seen = x.lock == PrepLock::Forbidden;
  CHECK(seen);
}

TEST(prep_swaps_follow_the_movement_types) {
  Run r;
  CHECK(r.begin("prep_swap") && r.toChapter2() && r.openChapter2());
  CHECK_STR(r.slots(), "sera tomas wren -");
  CHECK(r.move("wren", 3));  // the glider flies: the pond is fine; the square it left is empty now
  CHECK_STR(r.slots(), "sera tomas - wren");
  CHECK(!r.move("tomas", 3) && has(r.why, "Tomas cannot stand on slot 4"));  // swapping would put the archer on the pond
  CHECK(!r.move("wren", 1) && has(r.why, "Tomas cannot stand on slot 4"));  // ... and so would wren going to tomas' square
  CHECK_STR(r.slots(), "sera tomas - wren");
  CHECK(r.move("tomas", 2));  // to an empty square
  CHECK_STR(r.slots(), "sera - tomas wren");
  // Required with a slot of its own: it cannot be moved, and nobody can be swapped onto its square.
  CHECK(!r.move("sera", 1) && has(r.why, "must start on slot 1"));
  CHECK(!r.move("tomas", 0) && has(r.why, "Sera must start on slot 1"));
  CHECK(!r.move("tomas", 9) && has(r.why, "not a deployment square"));
  // A character deployed on a square its movement type cannot stand on.
  CHECK(r.bench("wren"));
  CHECK(!r.deploy("bram", 3) && has(r.why, "Bram cannot stand on slot 4"));
  CHECK(r.deploy("bram"));  // the first free slot it can stand on
  CHECK_STR(r.slots(), "sera bram tomas -");
}

TEST(prep_roster_keeps_benched_characters_with_items_and_levels) {
  Run r;
  CHECK(r.begin("prep_roster") && r.toChapter2() && r.openChapter2());
  const RosterUnit before = *r.roster("bram");
  CHECK(before.items.size() == 1 && r.g.db.items[(size_t)before.items[0].item].id == "tonic");
  CHECK(r.roster("ilsa") != nullptr);  // opening the screen took the map's own character into the roster
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why) && r.start());
  CHECK_EQ(r.unit("bram"), -1);   // benched: not on the board
  CHECK_EQ(r.unit("ilsa"), -1);
  CHECK(r.unit("guard") >= 0 && r.unit("sera") >= 0);
  r.b.units[(size_t)r.unit("tomas")].level = 3;
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK_EQ(r.roster("tomas")->level, 3);        // the deployed one took its chapter in
  CHECK_EQ(r.roster("bram")->level, before.level);  // the benched one is exactly as it was
  CHECK(r.roster("bram")->items.size() == 1 && r.roster("bram")->xp == before.xp);
  CHECK(r.roster("ilsa") && !r.roster("ilsa")->gone && r.roster("ilsa")->items.size() == 1);
  CHECK(r.roster("guard") != nullptr);  // the fixed unit is dressed from the roster and written back as usual
}

TEST(prep_fallen_and_joins_keep_the_roster_consistent) {
  Run r;
  CHECK(r.begin("prep_fallen") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why) && r.start());
  r.kill("tomas");  // fallen: gone
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(r.roster("tomas")->gone);
  CHECK(!r.roster("bram")->gone);  // the benched one did not fall
  r.lines.clear();
  CHECK(runScene(r.g.db, r.pt, SceneKind::After, r.lines) && r.before());
  r.loadChapterMap();
  prepOpen(r.g.db, r.pt, r.mapDef, true);
  CHECK(!r.deploy("tomas") && has(r.why, "has fallen"));
  bool locked = false;
  for (const PrepEntry& e : prepEntries(r.g.db, r.pt, r.mapDef))
    if (e.id == "tomas") locked = e.lock == PrepLock::Fallen;
  CHECK(locked);
  for (const PrepDeploy& d : prepDefaultDeployment(r.g.db, r.pt, r.mapDef)) CHECK(d.id != "tomas");
}

TEST(prep_fallen_return_brings_a_deployed_unit_back) {
  Run r;
  r.g.edit("data/campaign.json", "\"fallen\": \"gone\"", "\"fallen\": \"return\"");
  CHECK(r.begin("prep_fallen_return") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why) && r.start());
  r.kill("tomas");
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(!r.roster("tomas")->gone);
}

TEST(prep_unit_leaving_the_party_comes_out_of_the_roster) {
  // A unit that changes to the computer's side during the battle leaves the roster (docs/EVENTS.md); benched ones stay.
  Run r;
  r.g.edit("maps/orchard.json", "\"id\": \"muster\"",
           "\"id\": \"defect\", \"when\": { \"battle\": \"start\" }, \"do\": [ { \"changeSide\": \"ilsa\", \"side\": \"red\" } ] }, { \"id\": \"muster\"");
  CHECK(r.begin("prep_leaves") && r.toChapter2() && r.openChapter2());
  CHECK(r.bench("tomas") && r.deploy("ilsa"));
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why) && r.start());
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(r.roster("ilsa") == nullptr);  // she left the party
  CHECK(r.roster("bram") != nullptr && r.roster("tomas") != nullptr);
}

TEST(prep_the_battle_starts_with_the_chosen_units_on_their_squares) {
  Run r;
  CHECK(r.begin("prep_start") && r.toChapter2() && r.openChapter2());
  CHECK(r.bench("tomas") && r.deploy("bram", 1) && r.move("wren", 3));
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why));
  CHECK(!r.pt.prep.open);
  CHECK(r.start());
  auto at = [&](const char* id) { Unit& u = r.b.units[(size_t)r.unit(id)]; return std::to_string(r.b.grid.x(u.cell)) + "," + std::to_string(r.b.grid.y(u.cell)); };
  CHECK_STR(at("sera"), "0,1");
  CHECK_STR(at("bram"), "0,2");
  CHECK_STR(at("wren"), "1,0");
  CHECK_STR(at("guard"), "2,5");  // fixed: where the map put it
  CHECK_EQ(r.unit("tomas"), -1);
  CHECK_EQ(r.unit("ilsa"), -1);
  // They are dressed from the roster as always: Sera still carries her potion and ring.
  CHECK_EQ((int)r.b.units[(size_t)r.unit("sera")].items.size(), 2);
}

TEST(prep_order_is_before_scene_then_screen_then_battle_start_events) {
  Run r;
  CHECK(r.begin("prep_order") && r.toChapter2());
  CHECK(r.pt.phase == Phase::Before);
  CHECK(r.before());  // 1. the chapter's before scene
  CHECK(r.pt.phase == Phase::Battle);
  r.loadChapterMap();
  prepOpen(r.g.db, r.pt, r.mapDef, true);  // 2. the screen
  Battle shown;
  std::string err;
  CHECK(prepareChapterBattle(r.g.db, r.pt, r.g.formulas, 1, shown, err));  // what the screen draws: nothing has fired
  CHECK_EQ((int)shown.storyState.switches[0], 0);
  std::string why;
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why));
  CHECK(r.start());  // 3. the battle-start events
  CHECK_EQ((int)r.b.storyState.switches[0], 1);
}

TEST(prep_simulator_default_needs_no_screen_and_matches_the_default_pick) {
  Run r;
  CHECK(r.begin("prep_sim") && r.toChapter2() && r.before());
  CHECK(r.start());  // no prepOpen: what ote-sim, --autoplay and Play-without-interaction do
  CHECK(r.unit("sera") >= 0 && r.unit("tomas") >= 0 && r.unit("wren") >= 0 && r.unit("guard") >= 0);
  CHECK_EQ(r.unit("bram"), -1);
  CHECK_EQ(r.unit("ilsa"), -1);
  Run s;  // the screen opened and Begin pressed at once: the same battle
  CHECK(s.begin("prep_sim2") && s.toChapter2() && s.openChapter2());
  std::string why;
  CHECK(prepBegin(s.g.db, s.pt, s.mapDef, why) && s.start());
  CHECK_EQ((int)r.b.units.size(), (int)s.b.units.size());
  for (size_t i = 0; i < r.b.units.size(); ++i) {
    CHECK(r.b.units[i].storyId == s.b.units[i].storyId);
    CHECK_EQ((int)r.b.units[i].cell, (int)s.b.units[i].cell);
  }
  CHECK_EQ(r.b.sides[0].resources[0], s.b.sides[0].resources[0]);  // the map's allowance counted once either way
  CHECK_EQ(r.b.sides[0].resources[0], 125);
  // The simulator plays the chapter out.
  CHECK(r.pt.prep.deployed.empty());
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  CHECK(r.roster("ilsa") != nullptr);  // benched map characters join the party in this path too
}

TEST(prep_trade_between_roster_characters_and_the_storage) {
  Run r;
  CHECK(r.begin("prep_trade") && r.toChapter2() && r.openChapter2());
  const Database& db = r.g.db;
  std::string why;
  // sera carries potion + ring; tomas nothing; bram (benched) a tonic.
  CHECK(prepGive(db, r.pt, r.mapDef, "sera", 0, "bram", why));  // to a benched character: any two roster characters
  CHECK_EQ((int)r.roster("bram")->items.size(), 2);
  CHECK_EQ((int)r.roster("sera")->items.size(), 1);
  CHECK(prepGive(db, r.pt, r.mapDef, "sera", 0, "bram", why));
  CHECK(!prepGive(db, r.pt, r.mapDef, "tomas", 0, "bram", why) && has(why, "does not carry"));
  CHECK(prepGive(db, r.pt, r.mapDef, "bram", 0, "tomas", why));
  CHECK(prepGive(db, r.pt, r.mapDef, "bram", 0, "tomas", why));
  CHECK(prepGive(db, r.pt, r.mapDef, "bram", 0, "tomas", why));
  CHECK_EQ((int)r.roster("tomas")->items.size(), 3);
  CHECK(!prepGive(db, r.pt, r.mapDef, "bram", 0, "tomas", why) && has(why, "does not carry"));  // bram has given everything
  CHECK(!prepGive(db, r.pt, r.mapDef, "wren", 0, "tomas", why) && has(why, "Tomas cannot carry more"));  // inventory size 3
  CHECK(!prepGive(db, r.pt, r.mapDef, "tomas", 0, "tomas", why));
  CHECK(!prepGive(db, r.pt, r.mapDef, "tomas", 0, "nobody", why));
  // The storage: chapter 1's storage came along (a potion), plus what the map gives.
  CHECK_EQ((int)prepStorage(r.pt, r.mapDef).size(), 1);
  CHECK(prepStore(db, r.pt, r.mapDef, "tomas", 0, why));
  CHECK_EQ((int)prepStorage(r.pt, r.mapDef).size(), 2);
  CHECK(prepFetch(db, r.pt, r.mapDef, "bram", 0, why));
  CHECK(!prepFetch(db, r.pt, r.mapDef, "bram", 99, why) && has(why, "not in the storage"));
  CHECK(!prepEquip(db, r.pt, r.mapDef, "bram", 0, why));  // not a weapon
  // What was traded is what the battle starts with.
  CHECK(prepBegin(db, r.pt, r.mapDef, why) && r.start());
  CHECK_EQ((int)r.b.units[(size_t)r.unit("tomas")].items.size(), 2);
  CHECK_EQ((int)r.b.sides[0].storage.size(), 1);
}

TEST(prep_shop_buys_with_the_campaigns_resources) {
  Run r;
  CHECK(r.begin("prep_shop") && r.toChapter2() && r.openChapter2());
  const Database& db = r.g.db;
  std::string why;
  const int potion = db.findItem("potion"), ring = db.findItem("ring"), tonic = db.findItem("tonic");
  CHECK_EQ(prepResources(db, r.pt, r.mapDef)[0], 125);  // 60 from chapter 1, 40 the scene gave, 25 from this map
  CHECK(prepBuy(db, r.pt, r.mapDef, "tomas", potion, why));
  CHECK_EQ(prepResources(db, r.pt, r.mapDef)[0], 105);
  CHECK_EQ((int)r.roster("tomas")->items.size(), 1);
  CHECK(!prepBuy(db, r.pt, r.mapDef, "tomas", ring, why) && has(why, "not sold here"));
  CHECK(prepBuy(db, r.pt, r.mapDef, "tomas", tonic, why));
  CHECK(prepBuy(db, r.pt, r.mapDef, "tomas", potion, why));
  // Full inventory: it goes to the storage (the game has one).
  CHECK(prepBuy(db, r.pt, r.mapDef, "tomas", potion, why));
  CHECK_EQ((int)r.roster("tomas")->items.size(), 3);
  CHECK_EQ((int)prepStorage(r.pt, r.mapDef).size(), 2);
  CHECK(prepBuy(db, r.pt, r.mapDef, "", potion, why));  // straight into the storage
  CHECK_EQ(prepResources(db, r.pt, r.mapDef)[0], 125 - 20 - 30 - 20 - 20 - 20);
  CHECK(!prepBuy(db, r.pt, r.mapDef, "", tonic, why) && has(why, "Not enough Gold"));  // 15 left, a tonic is 30
  // The battle starts with what is left, once.
  CHECK(prepBegin(db, r.pt, r.mapDef, why) && r.start());
  CHECK_EQ(r.b.sides[0].resources[0], 125 - 20 - 30 - 20 - 20 - 20);
  CHECK_EQ((int)r.b.units[(size_t)r.unit("tomas")].items.size(), 3);
}

TEST(prep_a_lost_battle_retries_with_the_same_preparation) {
  Run r;
  CHECK(r.begin("prep_retry") && r.toChapter2() && r.openChapter2());
  std::string why;
  CHECK(r.bench("tomas") && r.deploy("bram") && prepBuy(r.g.db, r.pt, r.mapDef, "sera", r.g.db.findItem("potion"), why));
  CHECK(prepBegin(r.g.db, r.pt, r.mapDef, why) && r.start());
  const std::string state = r.state();
  const int gold = r.b.sides[0].resources[0];
  r.decide(false);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Retry);
  CHECK_STR(r.state(), state);  // nothing of the lost battle, nothing lost of the preparation
  CHECK(r.start(2));
  CHECK_EQ(r.b.sides[0].resources[0], gold);
  CHECK(r.unit("bram") >= 0 && r.unit("tomas") < 0);
  // The screen can be opened again for the retry and keeps the choice.
  r.loadChapterMap();
  prepOpen(r.g.db, r.pt, r.mapDef, true);
  CHECK(r.pt.prep.open);
  CHECK_STR(r.slots(), "sera bram wren -");
}

TEST(prep_chapters_without_it_start_exactly_as_before) {
  Run r;
  CHECK(r.begin("prep_none"));
  CHECK(r.before());
  r.loadChapterMap();
  CHECK(!prepOn(r.mapDef));
  MapDef out;
  prepApplyToMap(r.g.db, r.pt, r.mapDef, out);
  CHECK_EQ((int)out.units.size(), (int)r.mapDef.units.size());
  for (size_t i = 0; i < out.units.size(); ++i) CHECK(out.units[i].id == r.mapDef.units[i].id && out.units[i].x == r.mapDef.units[i].x);
  CHECK(r.start());
  CHECK(r.unit("sera") == 0 && r.unit("bram") >= 0);  // placed in the map's order
  r.decide(true);
  CHECK(finishChapterBattle(r.g.db, r.pt, r.b) == ChapterResult::Won);
  const std::string s = r.state();
  CHECK(s.find("\"prep\"") == std::string::npos && s.find("lastDeployed") == std::string::npos);  // nothing new in the save
  CHECK(r.pt.lastDeployed.empty() && !r.pt.prep.any());
}

TEST(prep_free_battle_uses_the_placed_units_as_the_pool) {
  Game g;
  CHECK(g.load("prep_free"));
  MapDef map = g.map("orchard");
  Playthrough scratch;  // no campaign, no roster
  prepOpen(g.db, scratch, map, false);
  std::vector<PrepEntry> e = prepEntries(g.db, scratch, map);
  CHECK_EQ((int)e.size(), 1);  // ilsa (guard is fixed); sera is required but not here, so nothing forces her
  CHECK_STR(e[0].id, "ilsa");
  CHECK_EQ((int)scratch.prep.deployed.size(), 1);
  CHECK(scratch.prep.deployed[0].slot == 0);
  std::string why;
  CHECK(prepMove(g.db, scratch, map, "ilsa", 2, why));
  CHECK(!scratch.prep.folded);
  MapDef out;
  prepApplyToMap(g.db, scratch, map, out);
  Formulas f;
  Battle b;
  std::string err;
  CHECK(f.init(g.db, err) && b.init(g.db, out, f, 1, err));
  CHECK(b.unitByStoryId("ilsa") >= 0 && b.unitByStoryId("guard") >= 0);
  Unit& ilsa = b.units[(size_t)b.unitByStoryId("ilsa")];
  CHECK_EQ(b.grid.x(ilsa.cell), 0);
  CHECK_EQ(b.grid.y(ilsa.cell), 3);
  CHECK_EQ(b.sides[0].resources[0], 25);  // a free battle keeps the map's own allowance
}
