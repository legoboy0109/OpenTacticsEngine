#include "conformance_contexts.h"
#include <QStringList>

namespace conformance {

const std::vector<Context>& contexts() {
  static const std::vector<Context> c = [] {
    std::vector<Context> v;
    addCoreContexts(v);
    addGameContexts(v);
    addSrpgContexts(v);
    addStoryContexts(v);
    return v;
  }();
  return c;
}

const std::map<QString, QString>& skipped() {
  static const std::map<QString, QString> s = {
      {"game:font.file", "needs a font file inside the game folder; the test only writes JSON"},
  };
  return s;
}

const std::map<QString, QString>& extraSkips() {
  static const std::map<QString, QString> s = [] {
    std::map<QString, QString> m;
    addCoreExtraSkips(m);
    addGameExtraSkips(m);
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
