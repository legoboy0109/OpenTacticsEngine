// Character-locked weapons (docs/SRPG.md "Character-locked weapons"): the loader, the one wield test and everything
// that follows it, the words the player shows, and the rules fingerprint. Game: tests/fixtures/srpg-locks (duel map).
//
// Weapons: dawn_blade (sword, wieldedBy kell), pair_blade (sword, rank B, kell + ser), trio_blade (kell, ser, mara).
// Units: kell (fighter, sword D) and ser (knight, sword C) and mara (archer, bow only); g1/g2 are generic grunts (sword).
#include <algorithm>
#include <cstring>

#include "battle/rank_text.h"
#include "battle/unit_info_rows.h"
#include "srpg_ranks_fixture.h"

using namespace ote;
using namespace rankstest;

namespace {
struct Locks : World {
  explicit Locks(const char* name, const std::function<void(Copy&)>& tweak = nullptr) : World(name, "duel", tweak, "srpg-locks") {}
  std::string refusal(const char* story, const char* weapon) {
    std::string why;
    return b().canWieldNow(unit(story), this->weapon(weapon), &why) ? "" : why;
  }
};

// The load message when the dawn blade's "wieldedBy" is `value` (JSON text) instead of ["kell"].
std::string loadError(const char* name, const std::string& value) {
  Copy c(name, "srpg-locks");
  c.swap("data/weapons.json", "\"wieldedBy\": [\"kell\"], \"props\": { \"might\": 9 }",
         "\"wieldedBy\": " + value + ", \"props\": { \"might\": 9 }");
  return c.gameError();
}

std::string fingerprint(const std::string& dir) {
  Database db;
  std::string err, out;
  if (!loadGame(dir, db, err) || !computeDataHash(db, "duel", out, err)) return "error: " + err;
  return out;
}
}  // namespace

TEST(locks_the_fixture_loads_and_the_ids_become_indices) {
  Copy c("locks_load", "srpg-locks");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  const WeaponDef& trio = db.weapons[(size_t)db.findWeapon("trio_blade")];
  CHECK_EQ((int)trio.wielders.size(), 3);
  CHECK_EQ(trio.wielders[1], db.findCharacter("ser"));
  CHECK(db.weapons[(size_t)db.findWeapon("bronze_sword")].wielders.empty());
}

TEST(locks_loader_refusals) {
  CHECK(saysIn(loadError("locks_l1", R"(["nobody"])"), "weapon \"dawn_blade\": \"wieldedBy\" names unknown character \"nobody\""));
  CHECK(saysIn(loadError("locks_l2", R"("kell")"), "\"wieldedBy\" must be a list of character ids"));
  CHECK(saysIn(loadError("locks_l3", "[3]"), "\"wieldedBy\" entry 1 must be a string"));
  CHECK(saysIn(loadError("locks_l4", R"(["kell","kell"])"), "names character \"kell\" twice"));
  CHECK_STR(loadError("locks_l5", R"(["kell"])"), "");
  // An empty list is no lock; a game without characters cannot name one.
  CHECK_STR(loadError("locks_l6", "[]"), "");
  Copy none("locks_l7", "srpg-locks");
  none.put("data/characters.json", R"({"characters":[]})");
  CHECK(saysIn(none.gameError(), "\"wieldedBy\" is given but this game has no characters (data/characters.json)"));
  none.drop("data/characters.json");
  CHECK(saysIn(none.gameError(), "this game has no characters"));
}

TEST(locks_the_truth_table) {
  Locks w("locks_table");
  // class cannot wield (mara is an archer): no, whatever the list says, and the class's own refusal wins.
  CHECK_STR(w.refusal("mara", "dawn_blade"), "the unit cannot wield that");
  CHECK_STR(w.refusal("mara", "trio_blade"), "the unit cannot wield that");  // on the list, still no: a lock never grants
  // class can wield, no list: as before.
  CHECK_STR(w.refusal("kell", "bronze_sword"), "");
  CHECK_STR(w.refusal("g1", "bronze_sword"), "");
  // class can wield, on the list: yes (no rank on these).
  CHECK_STR(w.refusal("kell", "dawn_blade"), "");
  CHECK_STR(w.refusal("ser", "trio_blade"), "");
  // class can wield, not on the list: no, with the words; a generic unit is never on a list.
  CHECK_STR(w.refusal("ser", "dawn_blade"), "Only Kell can wield this");
  CHECK_STR(w.refusal("g1", "dawn_blade"), "Only Kell can wield this");
  CHECK_EQ(w.unit("g1").character, -1);
  CHECK(w.unit("kell").character >= 0);
}

TEST(locks_the_words_list_up_to_two_names) {
  Locks w("locks_words");
  CHECK_STR(w.refusal("g1", "dawn_blade"), "Only Kell can wield this");
  // Two names: the pair blade, refused to a generic unit (lock first, although it also lacks the rank).
  CHECK_STR(w.refusal("g1", "pair_blade"), "Only Kell and Ser Ovan can wield this");
  // Three names: two, then "and others".
  CHECK_STR(w.refusal("g1", "trio_blade"), "Only Kell, Ser Ovan and others can wield this");
}

TEST(locks_do_not_bypass_ranks) {
  Locks w("locks_rank");
  // kell and ser are both named on the pair blade, which needs rank B: kell has D, ser C.
  CHECK_STR(w.refusal("kell", "pair_blade"), "Needs rank B");
  CHECK_STR(w.refusal("ser", "pair_blade"), "Needs rank B");
  w.unit("ser").weaponXp[(size_t)w.group("sword")] = 12;  // B
  CHECK_STR(w.refusal("ser", "pair_blade"), "");
  CHECK_STR(w.refusal("kell", "pair_blade"), "Needs rank B");
}

TEST(locks_equip_attack_forecast_and_counters_follow_the_one_test) {
  Locks w("locks_follow");
  Unit& ser = w.unit("ser");
  // ser carries the dawn blade first, but it is not his: the equipped weapon is the bronze sword after it.
  CHECK_EQ(w.b().equippedSlot(ser), w.slotOf("ser", "bronze_sword"));
  CHECK_STR(w.why(Action::equip(ser.id, w.slotOf("ser", "dawn_blade"))), "Only Kell can wield this");
  CHECK_STR(w.why(Action::equip(ser.id, w.slotOf("ser", "bronze_sword"))), "that is already equipped");
  std::vector<int> weapons;
  w.b().weaponsOf(ser, weapons);
  CHECK(std::find(weapons.begin(), weapons.end(), w.weapon("dawn_blade")) == weapons.end());
  CHECK(std::find(weapons.begin(), weapons.end(), w.weapon("bronze_sword")) != weapons.end());
  Forecast fc;
  CHECK(!w.b().forecast(ser.id, w.id("g1"), w.weapon("dawn_blade"), ser.cell, fc));
  // kell can: equipped dawn blade after moving it to the front, forecast and equip are fine.
  Unit& kell = w.unit("kell");
  CHECK_STR(w.why(Action::equip(kell.id, w.slotOf("kell", "dawn_blade"))), "");
  CHECK(w.b().forecast(kell.id, w.id("g1"), w.weapon("dawn_blade"), kell.cell, fc));
  // A carried locked weapon can still be handed to its owner (trading never checks it).
  CHECK_STR(w.why(Action::giveItem(ser.id, w.slotOf("ser", "dawn_blade"), w.id("kell"))), "");
  // The grunt g1 holds a dawn blade (not its to wield) and a bronze sword: it fights with the bronze sword, and
  // g2, with only the dawn blade, has no weapon at all (so the computer never picks it).
  CHECK_EQ(w.b().equippedSlot(w.unit("g1")), 1);
  std::vector<int> g2;
  w.b().weaponsOf(w.unit("g2"), g2);
  CHECK(g2.empty());
  // The counterattack in the forecast is made with the bronze sword, never the dawn blade.
  CHECK(w.b().forecast(kell.id, w.id("g1"), w.weapon("dawn_blade"), kell.cell, fc));
  CHECK(fc.counter.can);
  CHECK_EQ(fc.counter.weapon, w.weapon("bronze_sword"));
}

TEST(locks_the_player_greys_the_weapon_with_the_reason) {
  Locks w("locks_player");
  std::vector<ItemRow> rows = itemRows(w.b(), w.unit("ser"));
  CHECK_EQ((int)rows.size(), 3);
  CHECK_STR(rows[0].blocked, "Only Kell can wield this");  // dawn blade
  CHECK_STR(rows[0].detail, "Only Kell can wield this");
  CHECK_STR(rows[1].blocked, "");                           // bronze sword
  CHECK(rows[1].equipped);
  CHECK_STR(rows[2].blocked, "Needs rank B");               // pair blade: his to wield, but the rank is short
  rows = itemRows(w.b(), w.unit("kell"));
  CHECK_STR(rows[1].blocked, "");                           // the dawn blade is his
  // An archer's class cannot wield swords at all: not a "blocked" row (the class refusal is a different thing).
  CHECK_STR(weaponBlockReason(w.b(), w.unit("mara"), w.unit("mara").items[1]), "");
}

TEST(locks_the_fingerprint_changes_only_when_the_key_is_used) {
  Copy c("locks_fp", "srpg-locks");
  const std::string used = fingerprint(c.dir);
  CHECK(used.rfind("error", 0) != 0);
  // Strip every lock: the game without the key. An empty list is the same game as none.
  std::string text = c.read("data/weapons.json");
  for (const char* k : {", \"wieldedBy\": [\"kell\"]", ", \"wieldedBy\": [\"kell\", \"ser\"]", ", \"wieldedBy\": [\"kell\", \"ser\", \"mara\"]"}) {
    const size_t at = text.find(k);
    CHECK(at != std::string::npos);
    if (at != std::string::npos) text.erase(at, std::strlen(k));
  }
  c.put("data/weapons.json", text);
  const std::string without = fingerprint(c.dir);
  CHECK(without != used);
  c.swap("data/weapons.json", "\"id\": \"dawn_blade\",", "\"id\": \"dawn_blade\", \"wieldedBy\": [],");
  CHECK_STR(fingerprint(c.dir), without);
  c.swap("data/weapons.json", "\"wieldedBy\": []", "\"wieldedBy\": [\"kell\"]");
  CHECK(fingerprint(c.dir) != without);
}

TEST(locks_a_game_without_the_key_is_untouched) {
  // The ranks fixture has no lock anywhere: nothing is locked and nothing changes in its words.
  World w("locks_plain", "duel");
  for (const WeaponDef& d : w.rig.db.weapons) CHECK(d.wielders.empty() && d.wielderIds.empty());
  std::string why;
  CHECK(w.b().meetsCharacterLock(w.unit("g1"), w.weapon("steel_sword"), &why));
  CHECK_STR(why, "");
}
