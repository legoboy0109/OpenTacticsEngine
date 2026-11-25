// docs/JOBS.md "Learning from equipment": a map's own "abilityPoints": { "perBattle": n } replaces rules.abilityPoints.perBattle for a battle on that
// map; the loader's refusals. Fixture: teaching_fixture.h (rules.abilityPoints: 10 a won battle, 5 an action, 3 a defeat).
#include <algorithm>

#include "core/battle/save.h"
#include "core/jobs/mastery.h"
#include "teaching_fixture.h"

using namespace ote;
using skilltest::saysIn;
using teachtest::Copy;

namespace {

const char* kMapHead = R"({ "name": "Duel", )";

// Aldric (with the charm) defeats the foe, which ends the battle: the points brave got, sorted (the defeat, the action, the win).
std::vector<int> winPoints(Copy& c) {
  c.swap("game.json", R"("defeat": ["noUnits"])", R"("defeat": ["leaderDead"])");
  skilltest::World w("duel", c.dir);
  ItemStack s;
  s.item = w.db().findItem("charm");
  w.unit("aldric").items.push_back(s);
  w.unit("foe").hp = 1;
  CHECK(w.apply(Action::attack(w.id("aldric"), w.id("foe"), w.db().findWeapon("sword"))));
  CHECK(w.b().over);
  std::vector<int> amounts;
  for (const Event& e : w.events)
    if (e.type == EventType::PointsGained && e.c == 0) amounts.push_back(e.b);
  std::sort(amounts.begin(), amounts.end());
  return amounts;
}

std::string mapError(const char* name, const std::string& key) {
  Copy c(name);
  c.swap("maps/duel.json", kMapHead, std::string(kMapHead) + key + ", ");
  Database db;
  std::string err;
  if (!loadGame(c.dir, db, err)) return err;
  MapDef m;
  return loadMap(db, "duel", m, err) ? "" : err;
}

}  // namespace

TEST(map_ap_the_maps_own_per_battle_replaces_the_games) {
  {
    Copy c("mapap_game");
    const std::vector<int> a = winPoints(c);  // the game's 10
    CHECK(a.size() == 3 && a[0] == 3 && a[1] == 5 && a[2] == 10);
  }
  {
    Copy c("mapap_map");
    c.swap("maps/duel.json", kMapHead, std::string(kMapHead) + R"("abilityPoints": { "perBattle": 40 }, )");
    const std::vector<int> a = winPoints(c);  // the map's 40, not the game's 10
    CHECK(a.size() == 3 && a[0] == 3 && a[1] == 5 && a[2] == 40);
    MapDef m;
    Database db;
    std::string err;
    CHECK(loadGame(c.dir, db, err) && loadMap(db, "duel", m, err));
    CHECK_EQ(m.apPerBattle, 40);
  }
  {  // 0 on the map: no points for the win at all, the rest as before
    Copy c("mapap_zero");
    c.swap("maps/duel.json", kMapHead, std::string(kMapHead) + R"("abilityPoints": { "perBattle": 0 }, )");
    const std::vector<int> a = winPoints(c);
    CHECK(a.size() == 2 && a[0] == 3 && a[1] == 5);
  }
}

TEST(map_ap_win_amount_is_the_maps_in_the_event) {
  Copy c("mapap_event");
  c.swap("maps/duel.json", kMapHead, std::string(kMapHead) + R"("abilityPoints": { "perBattle": 7 }, )");
  const std::vector<int> a = winPoints(c);  // the map's 7
  CHECK(a.size() == 3 && a[0] == 3 && a[1] == 5 && a[2] == 7);
  skilltest::World w("duel", c.dir);
  CHECK_EQ(w.b().mapApPerBattle, 7);
  Copy plain("mapap_event_none");
  skilltest::World v("duel", plain.dir);
  CHECK_EQ(v.b().mapApPerBattle, -1);  // no key: the game's rule
}

TEST(map_ap_a_save_starts_from_the_map_again) {
  Copy c("mapap_save");
  c.swap("maps/duel.json", kMapHead, std::string(kMapHead) + R"("abilityPoints": { "perBattle": 33 }, )");
  skilltest::World w("duel", c.dir);
  Json saved;
  saveBattleState(w.b(), saved);
  CHECK(!saved.has("abilityPoints"));  // map data: nothing is saved for it
  skilltest::World again("duel", c.dir, false);
  std::string err;
  CHECK(restoreBattleState(again.b(), saved, err));
  CHECK_EQ(again.b().mapApPerBattle, 33);
}

TEST(map_ap_loader_refusals) {
  CHECK(saysIn(mapError("mapap_r1", R"("abilityPoints": { "perBattle": -1 })"), "\"perBattle\""));
  CHECK(saysIn(mapError("mapap_r2", R"("abilityPoints": { "perBattle": 10000 })"), "\"perBattle\""));
  CHECK(saysIn(mapError("mapap_r3", R"("abilityPoints": { "perAction": 3 })"), "unknown key \"perAction\""));
  CHECK(saysIn(mapError("mapap_r4", R"("abilityPoints": {})"), "needs \"perBattle\""));
  CHECK(saysIn(mapError("mapap_r5", R"("abilityPoints": 40)"), "abilityPoints"));
  // A game where no item teaches: nobody could earn the points.
  Copy c("mapap_r6");
  c.put("data/items.json", R"({ "items": [ { "id": "charm", "name": "Charm", "kind": "accessory", "bonus": { "str": 1 } } ] })");
  c.swap("game.json", "\"changeIn\": \"camp+prep\" },\n             \"abilityPoints\": { \"name\": \"AP\", \"perBattle\": 10, \"perAction\": 5, \"perDefeat\": 3 } },",
         "\"changeIn\": \"camp+prep\" } },");
  c.swap("data/units.json", R"(, "abilities": ["fire", "flame", "poke", "patch", "salve"])", "");
  c.swap("maps/duel.json", kMapHead, std::string(kMapHead) + R"("abilityPoints": { "perBattle": 40 }, )");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK_STR(err, "");
  MapDef m;
  CHECK(!loadMap(db, "duel", m, err));
  CHECK(saysIn(err, "\"abilityPoints\" needs items that teach"));
}
