// Loader: the "reading a unit's number" form of setVariable / addVariable (docs/EVENTS.md).
#include "core/data/loader_util.h"

namespace ote {

bool readUnitReading(const LoadCtx& c, const Database& db, const Json& e, const std::string& at, const char* number,
                     bool allowHp, UnitReading& out, bool& given) {
  static const char* const kKeys[] = {"level", "hp", "maxHp", "stat"};
  static const UnitReading::Kind kKinds[] = {UnitReading::Kind::Level, UnitReading::Kind::Hp, UnitReading::Kind::MaxHp,
                                             UnitReading::Kind::Stat};
  given = false;
  int found = -1, count = 0;
  for (int i = 0; i < 4; ++i)
    if (e.has(kKeys[i])) {
      found = i;
      ++count;
    }
  if (count == 0) {
    if (e.has("of")) return c.fail(at, "\"of\" goes with \"stat\" (which stat of that unit to read)");
    return true;
  }
  if (e.has(number))
    return c.fail(at, std::string("give \"") + number + "\" (a number) or a reading from a unit, not both");
  if (count > 1) return c.fail(at, "read one thing from the unit: \"level\", \"hp\", \"maxHp\" or \"stat\"");
  const char* key = kKeys[found];
  out.kind = kKinds[found];
  if (out.kind == UnitReading::Kind::Hp && !allowHp)
    return c.fail(at, "\"hp\" cannot be read in a campaign scene (units have no current hit points between battles); "
                      "use \"maxHp\"");
  const Json* idj = &e[key];
  if (out.kind == UnitReading::Kind::Stat) {
    if (!e.has("of")) return c.fail(at, "\"stat\" needs \"of\", the unit to read it from");
    if (!e[key].isString() || (out.stat = db.findStat(e[key].asString())) < 0)
      return c.fail(at, "\"stat\" names unknown stat \"" + (e[key].isString() ? e[key].asString() : std::string()) +
                            "\" (game.json \"stats\")");
    idj = &e["of"];
  } else if (e.has("of")) {
    return c.fail(at, "\"of\" goes with \"stat\" only; \"" + std::string(key) + "\" already names the unit");
  }
  if (!idj->isString() || idj->asString().empty())
    return c.fail(at, "the unit to read from must be a unit id (text that is not empty)");
  out.unit = idj->asString();
  given = true;
  return true;
}

}  // namespace ote
