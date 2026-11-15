// Saves of fuel and ammunition (docs/SAVES.md): a round trip, what the file looks like (and that a game without
// these features writes nothing new), damaged fields one by one, and that the new rule data changes the data hash
// only when it is present.
#include "json_edit.h"
#include "save_helpers.h"
#include "supply_fixture.h"

using namespace ote;
using namespace suptest;
using namespace jsonedit;

namespace {

// plane (0), ranger (1), truck (2), soldier (3) for blue; a red soldier (4). Wounded supplies.
struct Mid {
  SupplyWorld w;
  Mid() : w(field(), {{"plane", 0, 0, 0, false}, {"ranger", 0, 1, 0, false}, {"truck", 0, 2, 0, false},
                      {"soldier", 0, 3, 0, false}, {"soldier", 1, 6, 2, false}}) {}
  bool ok() {
    if (!w.begin()) return false;
    w.battle.units[0].fuel = 7;
    w.battle.units[1].ammo[0] = 1;
    w.battle.units[2].fuel = 12;
    return true;
  }
  Json save() const {
    Json j;
    saveBattleState(w.battle, j);
    return j;
  }
};

std::string restoreError(Mid& m, const Json& saved, Battle* into = nullptr) {
  Battle local;
  Battle& r = into ? *into : local;
  std::string err;
  if (!r.init(m.w.db, m.w.map, m.w.formulas, 1, err)) return "init failed";
  std::string before = savetest::stateText(r);
  bool ok = restoreBattleState(r, saved, err);
  if (!ok && savetest::stateText(r) != before) return "refused but changed the battle: " + err;
  return ok ? "" : err;
}

}  // namespace

TEST(supply_save_round_trip_keeps_fuel_and_ammunition) {
  Mid m;
  CHECK(m.ok());
  Json reread;
  std::string err;
  CHECK(Json::parse(m.save().dump(0), reread, err));   // through text, like a file
  Battle r;
  CHECK_STR(restoreError(m, reread, &r), "");
  CHECK_STR(savetest::stateText(r), savetest::stateText(m.w.battle));
  CHECK_EQ(r.fuel(0), 7);
  CHECK_EQ(r.fuel(2), 12);
  CHECK_EQ(r.ammo(1, Gun), 1);
  CHECK_EQ(r.fuel(3), -1);
  CHECK(r.units[3].ammo.empty());
  // Playing on gives the same game: the plane burns, the truck's neighbours are filled.
  std::vector<Event> a, b;
  for (int i = 0; i < 2; ++i) {
    CHECK(m.w.battle.apply(Action::endTurn(), a));
    CHECK(r.apply(Action::endTurn(), b));
  }
  CHECK(savetest::sameEvents(a, b));
  CHECK_STR(savetest::stateText(r), savetest::stateText(m.w.battle));
}

TEST(supply_save_writes_the_keys_only_for_units_that_have_them) {
  Mid m;
  CHECK(m.ok());
  Json s = m.save();
  const Json& units = s["units"];
  CHECK(units[size_t(0)].has("fuel") && units[size_t(0)].has("ammo"));    // plane: a tank and a gun
  CHECK_EQ((int)units[size_t(0)]["fuel"].asNumber(), 7);
  CHECK(units[size_t(1)].has("ammo") && !units[size_t(1)].has("fuel"));
  CHECK_EQ((int)units[size_t(1)]["ammo"]["gun"].asNumber(), 1);
  CHECK(!units[size_t(1)]["ammo"].has("sword"));                           // unlimited weapons are not listed
  CHECK(units[size_t(2)].has("fuel") && !units[size_t(2)].has("ammo"));
  CHECK(!units[size_t(3)].has("fuel") && !units[size_t(3)].has("ammo"));
}

TEST(supply_save_of_a_game_without_these_features_has_no_new_keys) {
  savetest::Rig rig;
  CHECK(rig.loadDemo("demo-srpg", "bridge"));
  rig.start();
  std::vector<Event> ev;
  savetest::playAi(rig.battle, 40, ev);
  Json j;
  saveBattleState(rig.battle, j);
  std::string text = j.dump(0);
  CHECK(text.find("fuel") == std::string::npos);
  CHECK(text.find("ammo") == std::string::npos);
}

namespace {
struct Damage {
  const char* what;
  std::vector<std::string> path;
  Json value;
  bool erase;
  const char* mention;
};
}  // namespace

TEST(supply_save_refuses_every_kind_of_damaged_supply_field) {
  Mid m;
  CHECK(m.ok());
  Json good = m.save();
  CHECK_STR(restoreError(m, good), "");
  auto unit = [](int i, const char* key) { return std::vector<std::string>{"units", "#" + std::to_string(i), key}; };
  auto gun = [](int i) { return std::vector<std::string>{"units", "#" + std::to_string(i), "ammo", "gun"}; };
  Json obj = Json::makeObject();
  Json sword = Json::makeObject();
  sword.set("sword", num(2));
  Json badName = Json::makeObject();
  badName.set("laser", num(2));
  const Damage damage[] = {
      {"fuel missing for a unit with a tank", unit(0, "fuel"), Json(), true, "units[0].fuel"},
      {"fuel is text", unit(0, "fuel"), str("7"), false, "units[0].fuel"},
      {"fuel is a fraction", unit(0, "fuel"), num(7.5), false, "units[0].fuel"},
      {"fuel is negative", unit(0, "fuel"), num(-1), false, "units[0].fuel"},
      {"fuel is over the tank", unit(0, "fuel"), num(21), false, "units[0].fuel"},
      {"truck fuel over its tank", unit(2, "fuel"), num(31), false, "units[2].fuel"},
      {"fuel on a unit without a tank", unit(3, "fuel"), num(5), false, "this unit type uses no fuel"},
      {"fuel on a ranger", unit(1, "fuel"), num(5), false, "units[1].fuel"},
      {"ammo missing", unit(1, "ammo"), Json(), true, "units[1].ammo"},
      {"ammo is a number", unit(1, "ammo"), num(1), false, "units[1].ammo"},
      {"ammo is a list", unit(1, "ammo"), Json::makeArray(), false, "units[1].ammo"},
      {"ammo lacks the gun", unit(1, "ammo"), obj, false, "units[1].ammo.gun"},
      {"gun is text", gun(1), str("1"), false, "units[1].ammo"},
      {"gun is a fraction", gun(1), num(1.5), false, "units[1].ammo"},
      {"gun is negative", gun(1), num(-1), false, "units[1].ammo"},
      {"gun is over the load", gun(1), num(4), false, "units[1].ammo"},
      {"ammo names an unlimited weapon", unit(1, "ammo"), sword, false, "has no ammunition"},
      {"ammo names an unknown weapon", unit(1, "ammo"), badName, false, "not a weapon of this unit type"},
      {"ammo on a unit without such weapons", unit(3, "ammo"), obj, false, "no weapon with ammunition"},
      {"ammo on a truck", unit(2, "ammo"), obj, false, "no weapon with ammunition"},
  };
  for (const Damage& d : damage) {
    Json bad = d.erase ? withoutAt(good, d.path) : withValue(good, d.path, d.value);
    std::string err = restoreError(m, bad);
    if (err.empty() || err.find(d.mention) == std::string::npos) {
      std::printf("  %s: expected a message with \"%s\", got \"%s\"\n", d.what, d.mention, err.c_str());
      CHECK(false);
    }
  }
}

TEST(supply_save_a_weapon_listed_twice_shares_one_count) {
  SupplyWorld w(field(), {{"ranger", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});
  w.db.unitTypes[Ranger].weapons = {Gun, 0, Gun};
  CHECK(w.begin());
  w.battle.units[0].ammo = {2, -1, 2};
  Json j;
  saveBattleState(w.battle, j);
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, j, err));
  CHECK_EQ(r.units[0].ammo.size(), size_t(3));
  CHECK_EQ(r.units[0].ammo[2], 2);
}

// ---- the data hash ----

namespace {
struct HashGame {
  std::string dir;
  std::string write(const char* name, const std::string& weapons, const std::string& rules,
                    const std::string& unitExtra, const std::string& structures) {
    dir = savetest::tempFolder(std::string("sup_hash_") + name);
    savetest::tempFolder(std::string("sup_hash_") + name + "/data");
    savetest::tempFolder(std::string("sup_hash_") + name + "/maps");
    CHECK(writeTextFile(dir + "/game.json",
                        "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"str\"},{\"id\":\"mov\"}],"
                        " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                        " \"resources\": [{\"id\":\"gold\"}], \"formulas\": {\"damage\":\"a.str\"}," + rules +
                        " \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
    CHECK(writeTextFile(dir + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"} ] }"));
    CHECK(writeTextFile(dir + "/data/weapons.json", weapons));
    CHECK(writeTextFile(dir + "/data/units.json",
                        "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"str\":2,\"mov\":3},\"weapons\":[\"sword\"]" +
                            unitExtra + "}] }"));
    if (!structures.empty()) CHECK(writeTextFile(dir + "/data/structures.json", structures));
    CHECK(writeTextFile(dir + "/maps/m.json",
                        "{ \"grid\": {\"type\":\"square\",\"width\":3,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"...\"],"
                        " \"sides\": [{\"id\":\"a\"},{\"id\":\"b\"}],"
                        " \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0},{\"type\":\"grunt\",\"side\":\"b\",\"x\":2,\"y\":0}] }"));
    Database db;
    std::string err, h;
    if (!loadGame(dir, db, err) || !computeDataHash(db, "m", h, err)) {
      std::printf("  %s\n", err.c_str());
      return "";
    }
    return h;
  }
};
}  // namespace

TEST(supply_hash_changes_only_when_the_new_rule_data_is_present) {
  HashGame g;
  const std::string plain = R"({ "weapons": [ {"id":"sword"} ] })";
  std::string base = g.write("base", plain, "", "", "");
  CHECK_EQ((int)base.size(), 16);
  CHECK_STR(g.write("again", plain, "", "", ""), base);   // stable
  // Each new key changes the hash.
  std::string supply = g.write("supply", plain, R"("rules": { "supply": { "structures": true } },)", "", "");
  std::string repair = g.write("repair", plain, R"("rules": { "supply": { "repair": { "amount": 20 } } },)", "", "");
  std::string ammo = g.write("ammo", R"({ "weapons": [ {"id":"sword","ammo":4} ] })", "", "", "");
  std::string fuel = g.write("fuel", plain, "", R"(,"fuel": 30)", "");
  std::string supplies = g.write("supplies", plain, "", R"(,"supplies": true)", "");
  std::string repairs = g.write("repairs", plain, "", "", R"({ "structures": [ { "id": "depot", "repairs": ["grunt"] } ] })");
  std::string all[] = {supply, repair, ammo, fuel, supplies, repairs};
  for (size_t i = 0; i < 6; ++i) {
    CHECK(all[i] != base);
    for (size_t k = i + 1; k < 6; ++k) CHECK(all[i] != all[k]);
  }
  // A changed value counts too.
  CHECK(g.write("ammo5", R"({ "weapons": [ {"id":"sword","ammo":5} ] })", "", "", "") != ammo);
}
