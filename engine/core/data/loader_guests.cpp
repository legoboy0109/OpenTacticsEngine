// "guest" on a map unit or an addUnit (docs/SRPG.md "Guests"). A map that never writes the key loads exactly as before.
#include "core/data/loader_guests.h"

namespace ote {

bool readUnitGuest(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u) {
  u.guest = false;
  if (!e.has("guest")) return true;
  if (!c.boolean(e, "guest", where, u.guest)) return false;
  if (!u.guest) return true;
  if (u.side >= 0 && u.side < (int)m.sides.size() && m.sides[(size_t)u.side].ai)
    return c.fail(where, "\"guest\" is for a unit on a side a person plays; side \"" + m.sides[(size_t)u.side].id +
                             "\" is the computer's, so its units are the computer's already");
  if (db.rules.ap.on)
    return c.fail(where, "\"guest\" cannot be used with \"rules\": { \"actionPoints\" } (the side's shared points pay for the player's own units)");
  return true;
}

}  // namespace ote
