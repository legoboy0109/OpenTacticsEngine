#include "core/battle/save_behaviour.h"

namespace ote {
namespace savejson {

void writeUnitBehaviour(const Unit& u, Json& out) {
  if (!u.behaviour.isDefault()) out.set("behaviour", behaviourToJson(u.behaviour));
  if (u.awake) out.set("awake", Json::makeBool(true));
  // Scripted-style state (docs/AI.md): only when not at its start value.
  if (u.retreating) out.set("retreating", Json::makeBool(true));
  if (u.idleSeen > 0) out.set("idleSeen", Json::makeNumber(u.idleSeen));
  if (u.patrolAt > 0) out.set("patrolAt", Json::makeNumber(u.patrolAt));
}

bool readUnitBehaviour(Reader& rd, const Database& db, const Json& j, const std::string& where, Unit& u) {
  u.behaviour = AiBehaviour();
  u.awake = false;
  u.retreating = false, u.idleSeen = 0, u.patrolAt = 0;
  if (j.has("behaviour")) {
    std::string err;
    if (!parseBehaviour(j["behaviour"], db, u.behaviour, err)) return rd.fail(where + ".behaviour", err);
  }
  if (j.has("awake")) {
    if (!j["awake"].isBool()) return rd.fail(where + ".awake", "not true/false");
    u.awake = j["awake"].asBool();
  }
  if (j.has("retreating")) {
    if (!j["retreating"].isBool()) return rd.fail(where + ".retreating", "not true/false");
    u.retreating = j["retreating"].asBool();
  }
  if (j.has("idleSeen") && !rd.integer(j, "idleSeen", where, 0, 100000, u.idleSeen)) return false;
  if (j.has("patrolAt") && !rd.integer(j, "patrolAt", where, 0, 31, u.patrolAt)) return false;
  return true;
}

}  // namespace savejson
}  // namespace ote
