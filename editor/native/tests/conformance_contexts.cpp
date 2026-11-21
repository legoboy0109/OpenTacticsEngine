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
      {"characters:job", "the equipped skills must belong to the job and be learned (cross-key rule on the rows around it)"},
      {"characters:jobs", "the equipped skills must have been learned in one of the jobs (cross-key rule on the rows around it)"},
      {"maps:structures[].contents*", "needs a chest placed on the fixture's own map geometry; the loader side is covered by the engine's place tests (tests/test_srpg_places*)"},
      {"maps:structures[].visitSides", "needs a visit place placed on the fixture's own map geometry; covered by the engine's place tests"},
      {"maps:structures[].visitUnits", "needs a visit place placed on the fixture's own map geometry; covered by the engine's place tests"},
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
      {"structures:by", "it must name at least one unit type or tag, which Enable alone cannot know (cross-key rule)"},
      {"structures:door", "the kind it opens into must have the same footprint as the structure, and the first kind of a game may not (cross-file rule)"},
      {"structures:chest", "the kind it opens into must have the same footprint as the structure, and the first kind of a game may not (cross-file rule)"},
      {"game:grid", "the keys are tried in a hexagon fixture; Enable alone writes a square grid, which its structures (rings) refuse"},
  };
  return s;
}

// Excused ONLY where the schema is partial by design; a key missing from inside a described block is always a failure.
bool excusedKey(const QString& schema, const QString& path, QString* why) {
  const QStringList seg = path.split('.');
  auto yes = [&](const char* w) { *why = w; return true; };
  if (schema == "maps") {
    // What a map keeps raw for now, each with the reason. Everything else of a map is described.
    static const char* const kRaw[][2] = {
        {"tiles", "the tile grid: a map painter is a later milestone"},
        {"legend", "the tile grid's legend: goes with the tile grid"},
        {"tileLayer", "the second tile layer: goes with the tile grid"},
        {"heights", "the height grid: goes with the tile grid"},
        {"events", "story events: triggers and commands are a larger language (docs/EVENTS.md), milestone 4"},
        {"changes", "named map changes (cells, structures) that story commands apply: milestone 4"},
        {"prep", "the preparation screen (slots, required characters, shop): milestone 4"},
        {"objectives", "win / lose rules with unit filters (docs/EVENTS.md): milestone 4"},
    };
    for (const auto& r : kRaw)
      if (seg[0] == r[0]) return yes(r[1]);
    if (seg.size() == 2 && seg[0] == "sides[]" && seg[1] == "storage") return yes("a side's item storage: milestone 4");
    if (seg.size() == 2 && seg[0] == "units[]") {
      static const char* const kUnit[][2] = {
          {"behaviour", "the computer behaviour of a unit (mode, route, retreat ...): milestone 4"},
          {"character", "unit placement keys that name characters, jobs and learned skills: milestone 4"},
          {"job", "unit placement keys that name characters, jobs and learned skills: milestone 4"},
          {"learned", "unit placement keys that name characters, jobs and learned skills: milestone 4"},
          {"equipped", "unit placement keys that name characters, jobs and learned skills: milestone 4"},
          {"xp", "experience, merit and weapon ranks of a placed unit: milestone 4"},
          {"merit", "experience, merit and weapon ranks of a placed unit: milestone 4"},
          {"ranks", "experience, merit and weapon ranks of a placed unit: milestone 4"},
          {"goesFor", "what a computer unit goes for (chests, villages ...): milestone 4"},
      };
      for (const auto& r : kUnit)
        if (seg[1] == r[0]) return yes(r[1]);
    }
    return false;
  }
  if (schema == "units" || schema == "weapons" || schema == "items" || schema == "terrain")
    if (seg.size() == 1) return yes("older keys of a core file (abilities, promotions, ammo, skills, inflict, vision ...): not described yet");
  return false;
}

}  // namespace conformance
