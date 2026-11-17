// Rescue and pair up in saves (docs/SRPG.md "Rescue and pair up": Saves, events): who carries whom.
//
// Only the unit that stays on the board writes "carrying": <unit id>. The unit that left is saved as it is (alive
// false, cell -1, its hit points above 0); its `carriedBy` is not saved, checkUnitsRescue derives it.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

namespace {

std::string unitName(int id) { return "unit " + std::to_string(id); }

}  // namespace

void writeUnitRescue(const Battle&, const Unit& u, Json& j) {
  if (u.carrying >= 0) j.set("carrying", Json::makeNumber(u.carrying));
}

bool readUnitRescue(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  if (!j.has("carrying")) return true;
  if (b.db->rules.rescue.mode == RescueMode::Off) return rd.fail(where + ".carrying", "this game has no rescue or pair up");
  // The range of unit ids is only known once every unit is read (checkUnitsRescue looks the unit up).
  return rd.integer(j, "carrying", where, 0, kMaxUnits, u.carrying);
}

bool checkUnitsRescue(Reader& rd, Battle& b) {
  const int count = (int)b.units.size();
  for (Unit& carrier : b.units) {
    if (carrier.carrying < 0) continue;
    const std::string where = "units[" + std::to_string(carrier.id) + "].carrying";
    const int id = carrier.carrying;
    if (id >= count) return rd.fail(where, unitName(id) + " does not exist");
    if (id == carrier.id) return rd.fail(where, "a unit cannot carry itself");
    if (!carrier.alive) return rd.fail(where, "a unit that is not on the board cannot carry anybody");
    Unit& t = b.units[(size_t)id];
    if (t.side != carrier.side) return rd.fail(where, unitName(id) + " is on another side");
    if (t.alive) return rd.fail(where, unitName(id) + " is on the board");
    if (t.hp < 1) return rd.fail(where, unitName(id) + " is defeated (no hit points), not carried");
    if (t.carriedBy >= 0) return rd.fail(where, unitName(id) + " is carried by unit " + std::to_string(t.carriedBy) + " too");
    t.carriedBy = carrier.id;
  }
  // A unit that is not alive but has hit points is carried (a defeated unit has none).
  for (const Unit& u : b.units)
    if (!u.alive && !u.hidden && u.hp > 0 && u.carriedBy < 0)
      return rd.fail("units[" + std::to_string(u.id) + "].hp", "the unit is not alive but has hit points, and nobody carries it");
  return true;
}

}  // namespace savejson
}  // namespace ote
