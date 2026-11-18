// The setPhaseMusic story command (docs/SOUND.md "Music by situation"): { "setPhaseMusic": { "side": "red", "music": "calm" } }.
// The music id is opaque here (it goes into the text table, so the save hash ignores which track it is); the player checks it
// against data/audio.json, like playMusic. `"music": null` takes the override away, `""` = no phase music for that side.
#include "core/data/loader_events.h"

namespace ote {

bool StoryLoader::phaseMusicCommand(const Json& e, const std::string& at, Command& out) {
  if (!onlyKeys(e, {"setPhaseMusic"}, at)) return false;
  const Json& v = e["setPhaseMusic"];
  if (!v.isObject()) return fail(at, "\"setPhaseMusic\" must be { \"side\": \"red\", \"music\": \"calm\" } (music null takes the change away)");
  if (!onlyKeys(v, {"side", "music"}, at + ": \"setPhaseMusic\"")) return false;
  std::string side;
  if (!c_.str(v, "side", at, side, true) || !sideIndex(side, at, "\"side\"", out.side)) return false;
  if (!v.has("music")) return fail(at, "missing required \"music\" (a music id, \"\" for none, or null to take the change away)");
  const Json& m = v["music"];
  if (m.isNull()) {
    out.text = -1;
    return true;
  }
  if (!m.isString() || (!m.asString().empty() && !isStoryId(m.asString())))
    return fail(at, "\"music\" must name a music track of data/audio.json (letters, digits, \"_\" and \"-\"), \"\" for no phase music, or be null");
  out.text = text(m.asString());
  return true;
}

}  // namespace ote
