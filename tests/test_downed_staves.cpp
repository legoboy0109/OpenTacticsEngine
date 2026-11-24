// A revive staff (docs/SRPG.md "Downed units and revive", section 8): the loader, the use on a downed ally, the refusal on a standing one.
// Game: tests/fixtures/srpg-staves with rules.downed and Wen's mend staff turned into a revive staff (srpg_staves_fixture.h).
#include "srpg_staves_fixture.h"

using namespace ote;
using namespace stavestest;

namespace {

const char* kMend = "\"effects\": [ { \"heal\": \"w.power + a.mag / 2\" } ], \"xp\": 15";

void downedRules(Copy& c) { c.swap("game.json", "\"rules\": { ", "\"rules\": { \"downed\": {}, "); }

void reviveStaff(Copy& c, const char* effects) {
  downedRules(c);
  c.swap("data/items.json", kMend, std::string("\"effects\": [ ") + effects + " ], \"xp\": 15");
}

std::string staffError(const char* name, const char* effects, bool rules = true, const char* target = nullptr) {
  Copy c(name, "srpg-staves");
  if (rules) downedRules(c);
  c.swap("data/items.json", kMend, std::string("\"effects\": [ ") + effects + " ], \"xp\": 15");
  if (target) c.swap("data/items.json", "\"range\": \"1 + a.mag / 3\", \"target\": \"ally\"", std::string("\"target\": \"") + target + "\"");
  return c.gameError();
}

void lay(Clinic& w, const char* who) {
  Unit& u = w.unit(who);
  u.alive = false;
  u.downed = true;
  u.hp = 0;
}

}  // namespace

TEST(downed_staff_loader) {
  CHECK_STR(staffError("ds_ok", "{ \"revive\": 10 }"), "");
  CHECK_STR(staffError("ds_ok2", "{ \"revive\": \"full\" }"), "");
  CHECK(saysIn(staffError("ds_rules", "{ \"revive\": 10 }", false), "\"revive\" needs \"rules\": { \"downed\": {} }"));
  CHECK(saysIn(staffError("ds_foe", "{ \"revive\": 10 }", true, "foe"), "\"revive\" needs \"target\" \"ally\", \"team\" or \"any\""));
  CHECK(saysIn(staffError("ds_self", "{ \"revive\": 10 }", true, "self"), "\"revive\" needs \"target\" \"ally\", \"team\" or \"any\""));
  CHECK(saysIn(staffError("ds_two", "{ \"revive\": 10 }, { \"heal\": 3 }"), "revive"));
  CHECK(saysIn(staffError("ds_zero", "{ \"revive\": 0 }"), "revive"));
}

TEST(downed_staff_revives_a_downed_ally_in_reach) {
  Clinic w("ds_use", "clinic", [](Copy& c) { reviveStaff(c, "{ \"revive\": 10 }"); });
  lay(w, "tarn");
  CHECK(w.use("wen", "mend_staff", "tarn"));
  const Unit& t = w.unit("tarn");
  CHECK(t.alive && !t.downed && t.hp == 10);
  const Event* e = w.first(EventType::UnitRevived);
  CHECK(e && (e->flags & kByStaff) && e->a == w.id("tarn") && e->c == w.id("wen"));
  CHECK_EQ(w.healed("tarn"), 0);  // a revive, not a heal
}

TEST(downed_staff_is_refused_on_a_standing_ally) {
  Clinic w("ds_standing", "clinic", [](Copy& c) { reviveStaff(c, "{ \"revive\": 10 }"); });
  w.hurt("tarn", 3);
  CHECK(w.refusal("wen", "mend_staff", "tarn") != "");
  const StaffOption o = w.option("wen", "mend_staff");
  CHECK(!o.ok && o.noTarget);
  CHECK(saysIn(o.why, "downed"));
}
