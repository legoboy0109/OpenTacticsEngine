// Staves, the data (docs/SRPG.md section 8): the loader and its refusals, the groups, the one wield test, the range
// formula and the words of the reasons. Game: tests/fixtures/srpg-staves (clinic map).
#include <algorithm>

#include "sound/audio_data.h"
#include "srpg_staves_fixture.h"

using namespace ote;
using namespace stavestest;

namespace {
// The load message when `from` (a piece of the fixture's items.json) is replaced by `to`.
std::string itemsError(const char* name, const std::string& from, const std::string& to) {
  Copy c(name, "srpg-staves");
  c.swap("data/items.json", from, to);
  return c.gameError();
}
}  // namespace

TEST(staves_the_fixture_loads_and_a_staff_is_an_item_with_its_own_data) {
  Copy c("staves_load", "srpg-staves");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK_EQ((int)db.staves.size(), 8);
  const ItemDef& mend = db.items[(size_t)db.findItem("mend_staff")];
  CHECK(mend.kind == ItemKind::Staff && mend.uses == 6 && mend.staff >= 0);
  const StaffDef& m = db.staves[(size_t)mend.staff];
  CHECK_STR(m.gear.group, "staff");
  CHECK_EQ(m.rangeExpr >= 0, true);
  CHECK(m.target == StaffTarget::Ally && m.area == StaffArea::Single && !m.canMiss());
  CHECK_EQ(m.gear.rankXp, 2);
  CHECK_EQ(m.xp, 15);
  const StaffDef& rally = db.staves[(size_t)db.items[(size_t)db.findItem("rally_staff")].staff];
  CHECK(rally.area == StaffArea::Radius && rally.radius == 1 && rally.maxRange == 3 && rally.minRange == 1);
  CHECK_EQ(rally.gear.rankNeed, db.ranks.findRank("c"));
  const StaffDef& chorus = db.staves[(size_t)db.items[(size_t)db.findItem("chorus_staff")].staff];
  CHECK(chorus.area == StaffArea::All && chorus.target == StaffTarget::Team && chorus.minRange == 1 && chorus.maxRange == 4);
  CHECK(chorus.xpExpr >= 0 && !chorus.needsPick());
  CHECK(db.staves[(size_t)db.items[(size_t)db.findItem("fortune_staff")].staff].canMiss());
  CHECK(db.staves[(size_t)db.items[(size_t)db.findItem("rest_staff")].staff].target == StaffTarget::Self);
  // The staff group comes after the weapon groups: no index of an existing group moves.
  CHECK_EQ(db.ranks.findGroup("sword"), 0);
  CHECK_EQ(db.ranks.findGroup("staff"), 1);
  CHECK_EQ(m.gear.rankGroup, 1);
  // A staff is not a weapon: the weapon list is untouched.
  CHECK_EQ((int)db.weapons.size(), 2);
  // The lock resolved to the character's index.
  CHECK_EQ(db.staves[(size_t)db.items[(size_t)db.findItem("kin_staff")].staff].gear.wielders[0], db.findCharacter("wen"));
}

TEST(staves_loader_refusals_name_the_file_the_item_and_the_key) {
  const std::string p = "data/items.json: item \"mend_staff\": ";
  CHECK(saysIn(itemsError("staves_l1", "\"group\": \"staff\", \"rankXp\": 2", "\"rankXp\": 2"), p + "\"group\" is required for a staff"));
  CHECK(saysIn(itemsError("staves_l2", "\"range\": \"1 + a.mag / 3\"", "\"range\": 0"), "\"range\" must be a whole number from 1 to 20"));
  CHECK(saysIn(itemsError("staves_l3", "\"range\": \"1 + a.mag / 3\"", "\"range\": [3, 1]"), "min not above max"));
  CHECK(saysIn(itemsError("staves_l4", "\"target\": \"ally\"", "\"target\": \"friend\""), "\"target\" must be \"ally\", \"self\", \"team\", \"foe\" or \"any\""));
  CHECK(saysIn(itemsError("staves_l5", "\"target\": \"ally\"", "\"target\": \"foe\""), "\"heal\" cannot target \"foe\""));
  CHECK(saysIn(itemsError("staves_l6", "\"area\": { \"radius\": 1 }", "\"area\": { \"radius\": 11 }"), "\"area\": \"radius\" must be a whole number from 1 to 10"));
  CHECK(saysIn(itemsError("staves_l7", "\"area\": { \"radius\": 1 }", "\"area\": \"wide\""), "\"area\" must be \"single\", \"all\" or { \"radius\": n }"));
  CHECK(saysIn(itemsError("staves_l8", "[ { \"heal\": \"w.power + a.mag / 2\" } ]", "[]"), "\"effects\" needs 1 to 4 effects"));
  CHECK(saysIn(itemsError("staves_l9", "{ \"heal\": \"w.power + a.mag / 2\" }", "{ \"teleport\": 3 }"), "\"effects\" entry 1: unknown effect \"teleport\""));
  CHECK(saysIn(itemsError("staves_l10", "{ \"heal\": \"w.power + a.mag / 2\" }", "{ \"heal\": 0 }"), "\"heal\" must be from 1 to 1000000"));
  CHECK(saysIn(itemsError("staves_l11", "\"xp\": 15", "\"xp\": -3"), "\"xp\" must be a whole number from 0 to 1000 or a formula"));
  CHECK(saysIn(itemsError("staves_l12", "\"props\": { \"power\": 5 }", "\"props\": { \"power\": \"x\" }"), "\"props\".\"power\" must be a number"));
  CHECK(saysIn(itemsError("staves_l13", "\"xp\": 15", "\"xp\": 15, \"heals\": 3"), "unknown key \"heals\" of a staff"));
  CHECK(saysIn(itemsError("staves_l14", "\"xp\": 15 },", "\"xp\": 15, \"heal\": 3 },"), "\"heal\" belongs to items of kind \"usable\""));
  CHECK(saysIn(itemsError("staves_l15", "\"wieldedBy\": [\"wen\"]", "\"wieldedBy\": [\"nobody\"]"), "\"wieldedBy\" names unknown character \"nobody\""));
  CHECK(saysIn(itemsError("staves_l16", "\"rank\": \"b\"", "\"rank\": \"z\""), "\"rank\" names unknown rank \"z\""));
  CHECK(saysIn(itemsError("staves_l17", "\"hit\": \"60\"", "\"hit\": [1]"), "\"hit\" must be a percent"));
  CHECK(saysIn(itemsError("staves_l18", "\"kind\": \"staff\", \"description\": \"Heals its user.\"", "\"kind\": \"wand\""), "\"kind\" must be \"usable\", \"weapon\", \"accessory\" or \"staff\""));
  CHECK_STR(itemsError("staves_l19", "\"range\": \"1 + a.mag / 3\"", "\"range\": 3"), "");
  // A condition is read like an ability's.
  CHECK_STR(itemsError("staves_l20", "\"xp\": 15 },", "\"xp\": 15, \"if\": [ { \"hp\": { \"op\": \"<=\", \"percent\": 50 }, \"who\": \"foe\" } ] },"), "");
  CHECK(saysIn(itemsError("staves_l21", "\"xp\": 15 },", "\"xp\": 15, \"if\": [ { \"hp\": 5 } ] },"), "mend_staff"));
  // "self" has no range and no area.
  CHECK(saysIn(itemsError("staves_l22", "\"target\": \"self\"", "\"target\": \"self\", \"range\": 2"), "\"range\" cannot be used with \"target\": \"self\""));
  CHECK(saysIn(itemsError("staves_l23", "\"target\": \"self\"", "\"target\": \"self\", \"area\": \"all\""), "\"area\" has no meaning with \"target\": \"self\""));
  // Experience and ranks need their features.
  Copy noProgress("staves_l24", "srpg-staves");
  noProgress.swap("game.json", "\"enabled\": true, \"xpPerLevel\"", "\"enabled\": false, \"xpPerLevel\"");
  CHECK(saysIn(noProgress.gameError(), "\"xp\" needs experience"));
  Copy noRanks("staves_l25", "srpg-staves");
  noRanks.drop("data/ranks.json");
  CHECK(saysIn(noRanks.gameError(), "item \"mend_staff\": \"rankXp\" is given but this game has no weapon ranks (data/ranks.json)"));
  Copy noChars("staves_l26", "srpg-staves");
  noChars.drop("data/characters.json");
  CHECK(saysIn(noChars.gameError(), "item \"kin_staff\": \"wieldedBy\" is given but this game has no characters (data/characters.json)"));
}

// "sounds": { "use": ... } is the same key as on a usable item (docs/SOUND.md): the core loader lets it through, the player's audio loader reads it.
TEST(staves_a_staff_accepts_the_sounds_key_like_a_usable_item) {
  const char* audio = R"({ "sounds": { "chime": { "file": "audio/chime.wav" } }, "use": {} })";
  Copy c("staves_sounds", "srpg-staves");
  c.swap("data/items.json", "\"xp\": 15 },", "\"xp\": 15, \"sounds\": { \"use\": \"chime\" } },");
  c.put("data/audio.json", audio);
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  AudioData d;
  CHECK(loadAudioFile(c.dir, d, err));
  CHECK(loadAudioOverrides(db, d, err));
  CHECK_EQ(d.items[(size_t)db.findItem("mend_staff")].sound[(int)Cue::ItemUse], d.findSound("chime"));
  CHECK_EQ(d.items[(size_t)db.findItem("rally_staff")].sound[(int)Cue::ItemUse], kNone);  // the others keep the game's sound
  // A sound the game does not have is refused with the item named.
  Copy bad("staves_sounds_bad", "srpg-staves");
  bad.swap("data/items.json", "\"xp\": 15 },", "\"xp\": 15, \"sounds\": { \"use\": \"nope\" } },");
  bad.put("data/audio.json", audio);
  Database db2;
  CHECK(loadGame(bad.dir, db2, err));
  AudioData d2;
  CHECK(loadAudioFile(bad.dir, d2, err));
  CHECK(!loadAudioOverrides(db2, d2, err) && saysIn(err, "\"use\" names unknown sound \"nope\""));
}

TEST(staves_a_class_lists_the_staff_group_to_wield_it) {
  Clinic w("staves_wield");
  std::string why;
  // Wen (cleric, rank C by character): mend and rally yes; the high staff needs B; the kin staff is hers.
  CHECK(w.b().canWieldStaff(w.unit("wen"), w.item("mend_staff"), &why));
  CHECK(w.b().canWieldStaff(w.unit("wen"), w.item("rally_staff"), &why));
  CHECK(w.b().canWieldStaff(w.unit("wen"), w.item("kin_staff"), &why));
  CHECK(!w.b().canWieldStaff(w.unit("wen"), w.item("high_staff"), &why));
  CHECK_STR(why, "Needs rank B");
  // Mira (a generic cleric, rank D): the lock says whose it is; rally needs C.
  CHECK(!w.b().canWieldStaff(w.unit("mira"), w.item("kin_staff"), &why));
  CHECK_STR(why, "Only Wen can wield this");
  CHECK(!w.b().canWieldStaff(w.unit("mira"), w.item("rally_staff"), &why));
  CHECK_STR(why, "Needs rank C");
  CHECK(w.b().canWieldStaff(w.unit("mira"), w.item("mend_staff"), &why));
  // A fighter's class does not list "staff": the class decides first, whatever its rank or character.
  CHECK(!w.b().canWieldStaff(w.unit("tarn"), w.item("mend_staff"), &why));
  CHECK_STR(why, "the unit cannot wield that");
  CHECK(!w.b().canWieldStaff(w.unit("tarn"), w.item("kin_staff"), &why));
  CHECK_STR(why, "the unit cannot wield that");
  // Not a staff at all.
  CHECK(!w.b().canWieldStaff(w.unit("wen"), w.item("short_sword"), &why));
  CHECK_STR(why, "that is not a staff");
}

TEST(staves_the_rank_follows_experience_at_once) {
  Clinic w("staves_rank");
  w.hurt("tarn", 5);
  Unit& mira = w.unit("mira");
  const int staff = w.rig.db.ranks.findGroup("staff");
  CHECK_EQ(mira.weaponXp[(size_t)staff], 10);  // class "d"
  mira.items.push_back({w.item("rally_staff"), 3, false});
  CHECK_STR(w.refusal("mira", "rally_staff", "tarn"), "Needs rank C");
  mira.weaponXp[(size_t)staff] = 25;
  CHECK_STR(w.option("mira", "rally_staff").why, "nobody in reach needs that");  // wieldable now; tarn is 4 away
}

TEST(staves_the_range_is_a_number_a_list_or_a_formula_of_the_users_stats) {
  Clinic w("staves_range");
  int lo = 0, hi = 0;
  Unit& wen = w.unit("wen");
  w.b().staffRange(wen, w.item("mend_staff"), lo, hi);  // 1 + mag / 3 = 3
  CHECK(lo == 1 && hi == 3);
  wen.stats[(size_t)w.stat("mag")] = 12;  // a stronger mage reaches further
  w.b().staffRange(wen, w.item("mend_staff"), lo, hi);
  CHECK(lo == 1 && hi == 5);
  wen.stats[(size_t)w.stat("mag")] = 0;
  w.b().staffRange(wen, w.item("mend_staff"), lo, hi);
  CHECK(lo == 1 && hi == 1);
  w.b().staffRange(wen, w.item("rally_staff"), lo, hi);  // a number 3 = reach 1 to 3
  CHECK(lo == 1 && hi == 3);
  w.b().staffRange(wen, w.item("chorus_staff"), lo, hi);  // a list [1, 4]
  CHECK(lo == 1 && hi == 4);
  // The reach decides who may be picked: with mag 0 only the neighbours (none here) are in reach of a mend.
  w.hurt("tarn", 5);
  w.hurt("ola", 5);
  std::vector<int> targets;
  w.b().staffTargets(w.id("wen"), w.slot("wen", "mend_staff"), wen.cell, targets);
  CHECK(targets.empty());
  wen.stats[(size_t)w.stat("mag")] = 6;  // reach 3: tarn (2 away) and ola (2 away)
  w.b().staffTargets(w.id("wen"), w.slot("wen", "mend_staff"), wen.cell, targets);
  CHECK_EQ((int)targets.size(), 2);
  CHECK(targets[0] == w.id("tarn") && targets[1] == w.id("ola"));
  // A square the unit would stand on after a move changes who is in reach.
  const Cell nearTarn = w.b().grid.at(4, 2);
  w.b().staffTargets(w.id("wen"), w.slot("wen", "mend_staff"), nearTarn, targets);
  CHECK(std::find(targets.begin(), targets.end(), w.id("tarn")) != targets.end());
}
