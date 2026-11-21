#include "conformance_contexts.h"
#include <QStringList>

namespace conformance {

const std::vector<Context>& contexts() {
  static const std::vector<Context> c = [] {
    std::vector<Context> v;
    addCoreContexts(v);
    addGameContexts(v);
    addDataContexts(v);
    addSrpgContexts(v);
    addStoryContexts(v);
    return v;
  }();
  return c;
}

const std::map<QString, QString>& skipped() {
  static const std::map<QString, QString> s = {
      {"game:font.file", "needs a font file inside the game folder; the test only writes JSON"},
      {"abilities:if[]*", "the conditions are the shared definition _skills#condition, every shape of it is tried under skills:blocks[].if; an ability judges only the target unit (no fight: initiating, distance, arc, strike, marked, counterattack are refused) and this game has none of the ids many shapes name"},
      {"items:if[]*", "the conditions are the shared definition _skills#condition, every shape of it is tried under skills:blocks[].if; a staff judges only the target unit (no fight) and this game has none of the ids many shapes name"},
      {"conditions:blocks[].if[]{arc}*", "needs rules.facing; tried under skills:blocks[].if[]{arc}"},
      {"conditions:blocks[].if[]{damageType}*", "needs damage types; tried under skills:blocks[].if[]{damageType}"},
      {"conditions:blocks[].if[]{structure}*", "this fixture has no structures; every structure shape is tried under skills:blocks[].if[]{structure}"},
      {"conditions:blocks[].if[]{weaponKind}*", "needs weapons with a damageKind; tried under skills:blocks[].if[]{weaponKind}"},
      {"conditions:blocks[].aura.weaponKind", "needs weapons with a damageKind; tried under skills:blocks[].aura.weaponKind"},
      {"conditions:blocks[].do[]{ability}*", "needs abilities.json; tried under skills:blocks[].do[]{ability}"},
      {"skills:blocks[].do[]{cancelEffective}.cancelEffective{these tags}", "the tags must be keys of some weapon's versus, and this fixture has none"},
      {"conditions:blocks[].do[]{cancelEffective}.cancelEffective{these tags}", "the tags must be keys of some weapon's versus, and this fixture has none"},
      {"units:promotions[].items", "needs a promotion item (promotes: true), which these fixtures' items are not (cross-file rule)"},
      {"abilities:spendsItem", "needs an item whose skill gives the ability (cross-file rule); the probe ability has none"},
      {"weapons:sounds", "sound ids need data/audio.json with sounds, and the keys are cue names (docs/SOUND.md); covered with audio.json"},
      {"units:sounds", "sound ids need data/audio.json with sounds, and the keys are cue names (docs/SOUND.md); covered with audio.json"},
      {"items:sounds", "sound ids need data/audio.json with sounds, and the keys are cue names (docs/SOUND.md); covered with audio.json"},
      {"moveTypes:sounds", "sound ids need data/audio.json with sounds, and the keys are cue names (docs/SOUND.md); covered with audio.json"},
      {"game:moveTypes[].sounds", "sound ids need data/audio.json with sounds, and the keys are cue names (docs/SOUND.md); covered with audio.json"},
  };
  return s;
}

const std::map<QString, QString>& extraSkips() {
  static const std::map<QString, QString> s = [] {
    std::map<QString, QString> m;
    addCoreExtraSkips(m);
    addGameExtraSkips(m);
    addDataExtraSkips(m);
    addSrpgExtraSkips(m);
    addStoryExtraSkips(m);
    return m;
  }();
  return s;
}

const std::map<QString, QString>& blockExcuses() {
  static const std::map<QString, QString> s = {
      {"game:grid", "the keys are tried in a hexagon fixture; Enable alone writes a square grid, which its structures (rings) refuse"},
  };
  return s;
}

// Excused ONLY where the schema is partial by design; a key missing from inside a described block is always a failure.
bool excusedKey(const QString& schema, const QString& path, QString* why) {
  const QStringList seg = path.split('.');
  auto yes = [&](const char* w) { *why = w; return true; };
  if (schema == "maps") {
    if (seg.size() == 1) return yes("map structure (tiles, legend, events, structures, objectives ...): not described yet");
    if (seg.size() == 2 && (seg[0] == "units[]" || seg[0] == "sides[]"))
      return yes("older side / unit placement keys (behaviour, items, xp, merit, ...): not described yet");
    return false;
  }
  if (schema == "units" || schema == "weapons" || schema == "items" || schema == "terrain")
    if (seg.size() == 1) return yes("older keys of a core file (abilities, promotions, ammo, skills, inflict, vision ...): not described yet");
  return false;
}

}  // namespace conformance
