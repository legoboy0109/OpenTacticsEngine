#include "core/battle/save_behaviour.h"

namespace ote {
namespace savejson {

void writeUnitBehaviour(const Unit& u, Json& out) {
  if (!u.behaviour.isDefault()) out.set("behaviour", behaviourToJson(u.behaviour));
  if (u.awake) out.set("awake", Json::makeBool(true));
}

bool readUnitBehaviour(Reader& rd, const Database& db, const Json& j, const std::string& where, Unit& u) {
  u.behaviour = AiBehaviour();
  u.awake = false;
  if (j.has("behaviour")) {
    std::string err;
    if (!parseBehaviour(j["behaviour"], db, u.behaviour, err)) return rd.fail(where + ".behaviour", err);
  }
  if (j.has("awake")) {
    if (!j["awake"].isBool()) return rd.fail(where + ".awake", "not true/false");
    u.awake = j["awake"].asBool();
  }
  return true;
}

}  // namespace savejson
}  // namespace ote
