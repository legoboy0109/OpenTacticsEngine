// Weapons locked to unit types (docs/SRPG.md "Character-locked weapons", key "wieldedByTypes"): the loader, the OR rule with
// "wieldedBy", the words, ranks and class on top, staves, and the rules fingerprint. Game: tests/fixtures/srpg-locks (duel
// map; units fighter "Fighter", knight "Knight", archer, grunt "Grunt") and tests/fixtures/srpg-staves.
#include "battle/rank_text.h"
#include "srpg_staves_fixture.h"

using namespace ote;
using namespace rankstest;

namespace {
// A locks game where the dawn blade's `"wieldedBy": ["kell"]` is replaced by `lock` (JSON members text).
struct Typed : World {
  Typed(const char* name, const std::string& lock)
      : World(name, "duel", [&](Copy& c) { c.swap("data/weapons.json", "\"wieldedBy\": [\"kell\"],", lock + ","); }, "srpg-locks") {}
  std::string refusal(const char* story, const char* weapon) {
    std::string why;
    return b().canWieldNow(unit(story), this->weapon(weapon), &why) ? "" : why;
  }
};

std::string typeError(const char* name, const std::string& lock) {
  Copy c(name, "srpg-locks");
  c.swap("data/weapons.json", "\"wieldedBy\": [\"kell\"],", lock + ",");
  return c.gameError();
}

std::string fingerprint(const std::string& dir) {
  Database db;
  std::string err, out;
  if (!loadGame(dir, db, err) || !computeDataHash(db, "duel", out, err)) return "error: " + err;
  return out;
}
}  // namespace

TEST(lock_types_loader_reads_and_refuses) {
  Typed w("lt_load", "\"wieldedByTypes\": [\"knight\", \"grunt\"]");
  const WeaponDef& d = w.rig.db.weapons[(size_t)w.weapon("dawn_blade")];
  CHECK(d.wielders.empty());
  CHECK_EQ((int)d.wielderTypes.size(), 2);
  CHECK_EQ(d.wielderTypes[0], w.rig.db.findUnitType("knight"));
  CHECK(saysIn(typeError("lt_l1", "\"wieldedByTypes\": [\"ghost\"]"), "weapon \"dawn_blade\": \"wieldedByTypes\" names unknown unit type \"ghost\" (data/units.json)"));
  CHECK(saysIn(typeError("lt_l2", "\"wieldedByTypes\": \"knight\""), "\"wieldedByTypes\" must be a list of unit type ids"));
  CHECK(saysIn(typeError("lt_l3", "\"wieldedByTypes\": [3]"), "\"wieldedByTypes\" entry 1 must be a string"));
  CHECK(saysIn(typeError("lt_l4", "\"wieldedByTypes\": [\"knight\", \"knight\"]"), "names unit type \"knight\" twice"));
  CHECK_STR(typeError("lt_l5", "\"wieldedByTypes\": []"), "");
  // A type list needs no characters in the game; only "wieldedBy" does.
  Copy none("lt_l6", "srpg-locks");
  none.put("data/characters.json", R"({"characters":[]})");
  none.swap("data/weapons.json", "\"wieldedBy\": [\"kell\"], ", "");
  none.swap("data/weapons.json", "\"wieldedBy\": [\"kell\", \"ser\"], ", "");
  none.swap("data/weapons.json", "\"wieldedBy\": [\"kell\", \"ser\", \"mara\"], ", "");
  none.swap("data/weapons.json", "\"id\": \"dawn_blade\",", "\"id\": \"dawn_blade\", \"wieldedByTypes\": [\"grunt\"],");
  CHECK_STR(none.gameError(), "");
}

TEST(lock_types_the_rule_is_character_or_type) {
  // Characters kell (fighter) and a type: knight. ser is a knight (not named), g1 a grunt, mara an archer (class cannot).
  Typed w("lt_rule", "\"wieldedBy\": [\"kell\"], \"wieldedByTypes\": [\"knight\"]");
  CHECK_STR(w.refusal("kell", "dawn_blade"), "");   // on the character list
  CHECK_STR(w.refusal("ser", "dawn_blade"), "");    // on the type list
  CHECK_STR(w.refusal("g1", "dawn_blade"), "Only Kell and Knight can wield this");
  CHECK_STR(w.refusal("mara", "dawn_blade"), "the unit cannot wield that");  // never grants: the class decides first
  // Types only: a character of another type is out, a generic unit of the type is in.
  Typed t("lt_types_only", "\"wieldedByTypes\": [\"grunt\"]");
  CHECK_STR(t.refusal("g1", "dawn_blade"), "");
  CHECK_STR(t.refusal("kell", "dawn_blade"), "Only Grunt can wield this");
  CHECK_STR(t.refusal("ser", "dawn_blade"), "Only Grunt can wield this");
}

TEST(lock_types_the_words_name_up_to_two_then_others) {
  Typed a("lt_w1", "\"wieldedByTypes\": [\"knight\", \"fighter\"]");
  CHECK_STR(a.refusal("g1", "dawn_blade"), "Only Knight and Fighter can wield this");
  Typed b("lt_w2", "\"wieldedBy\": [\"kell\", \"ser\"], \"wieldedByTypes\": [\"fighter\"]");
  CHECK_STR(b.refusal("g1", "dawn_blade"), "Only Kell, Ser Ovan and others can wield this");
  Typed c("lt_w3", "\"wieldedByTypes\": [\"knight\", \"fighter\", \"grunt\"]");
  CHECK_STR(c.refusal("mara", "dawn_blade"), "the unit cannot wield that");
  Typed d("lt_w4", "\"wieldedByTypes\": [\"knight\", \"archer\", \"grunt\"]");
  // The player reads the same words (a narrow screen shortens them in its own places, which key on "Only ... can wield this").
  CHECK_STR(weaponBlockReason(d.b(), d.unit("kell"), d.unit("kell").items[1]), "Only Knight, Archer and others can wield this");
}

TEST(lock_types_ranks_and_class_still_apply) {
  Typed w("lt_rank", "\"wieldedByTypes\": [\"fighter\", \"knight\"]");
  w.rig.db.weapons[(size_t)w.weapon("dawn_blade")].rankNeed = 3;  // B
  CHECK_STR(w.refusal("kell", "dawn_blade"), "Needs rank B");
  w.unit("kell").weaponXp[(size_t)w.group("sword")] = 12;
  CHECK_STR(w.refusal("kell", "dawn_blade"), "");
  CHECK_STR(w.refusal("g1", "dawn_blade"), "Only Fighter and Knight can wield this");  // the lock is said first
}

TEST(lock_types_follow_the_unit_type_after_promotion) {
  Typed w("lt_promo", "\"wieldedByTypes\": [\"knight\"]");
  Unit& kell = w.unit("kell");  // a fighter: not allowed, then promoted to knight: allowed
  CHECK_STR(w.refusal("kell", "dawn_blade"), "Only Knight can wield this");
  kell.type = w.rig.db.findUnitType("knight");
  CHECK_STR(w.refusal("kell", "dawn_blade"), "");
}

TEST(lock_types_work_for_staves_too) {
  stavestest::Clinic s("lt_staff", "clinic", [](Copy& c) { c.swap("data/items.json", "\"wieldedBy\": [\"wen\"]", "\"wieldedByTypes\": [\"cleric\"]"); });
  std::string why;
  CHECK(s.b().canWieldStaff(s.unit("wen"), s.item("kin_staff"), &why));
  CHECK(s.b().canWieldStaff(s.unit("mira"), s.item("kin_staff"), &why));  // a generic cleric now may
  stavestest::Clinic t("lt_staff2", "clinic", [](Copy& c) { c.swap("data/items.json", "\"wieldedBy\": [\"wen\"]", "\"wieldedBy\": [\"wen\"], \"wieldedByTypes\": [\"knight\"]"); });
  CHECK(!t.b().canWieldStaff(t.unit("mira"), t.item("kin_staff"), &why));
  CHECK_STR(why, "Only Wen and Knight can wield this");
  Copy bad("lt_staff3", "srpg-staves");
  bad.swap("data/items.json", "\"wieldedBy\": [\"wen\"]", "\"wieldedByTypes\": [\"ghost\"]");
  CHECK(saysIn(bad.gameError(), "item \"kin_staff\": \"wieldedByTypes\" names unknown unit type \"ghost\""));
}

TEST(lock_types_the_fingerprint_changes_only_when_the_key_is_used) {
  Copy c("lt_fp", "srpg-locks");
  const std::string base = fingerprint(c.dir);
  CHECK(base.rfind("error", 0) != 0);
  c.swap("data/weapons.json", "\"id\": \"dawn_blade\",", "\"id\": \"dawn_blade\", \"wieldedByTypes\": [],");
  CHECK_STR(fingerprint(c.dir), base);
  c.swap("data/weapons.json", "\"wieldedByTypes\": []", "\"wieldedByTypes\": [\"knight\"]");
  CHECK(fingerprint(c.dir) != base);
}
