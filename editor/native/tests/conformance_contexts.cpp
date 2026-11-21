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
    addMapContexts(v);
    addEventContexts(v);
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
      {"maps:changes.*.cells[].tile", "needs a tile sheet image inside the game folder (graphics.json, tileLayer); the test only writes JSON"},
      {"maps:prep.shop.music", "the track must exist in data/audio.json; covered with audio.json"},
      {"maps:prep.shops[].music", "the track must exist in data/audio.json; covered with audio.json"},
      {"audio:music", "a free map: its entries are tried one by one (replacing the whole map would drop the tracks the rest of the file names)"},
      {"audio:sounds", "a free map: its entries are tried one by one (replacing the whole map would drop the sounds the rest of the file names)"},
      {"campaign:chapters[].after*", "an after scene holds the same commands as a before scene: tried under campaign:chapters[].before{commands}[]"},
      {"campaign:chapters[].before{music and commands}.commands*", "the commands of the scene form with music are the same as the plain list: tried under campaign:chapters[].before{commands}[]"},
      {"campaign:chapters[].before{commands}[]{if}.then[]{*", "the commands in a nested list are the same definition as the scene's own list: tried there"},
      {"campaign:chapters[].before{commands}[]{if}.else[]{*", "the commands in a nested list are the same definition as the scene's own list: tried there"},
      {"campaign:chapters[].before{commands}[]{choice}.answers[].do[]{*", "the commands in a nested list are the same definition as the scene's own list: tried there"},
      {"campaign:chapters[].before{commands}[]{choice}.answers[].if[]{*", "the conditions are the same definition as the if command's own list: tried there"},
      {"maps:events[].do[]{addUnit}.addUnit.*", "a unit an addUnit command adds has the keys of a map's unit (without facing): tried under maps:units[]"},
      {"maps:events[].do[]{setBehaviour}.setBehaviour{*", "the behaviour is the same block as a unit's: tried under maps:units[].behaviour"},
      {"maps:events[].do[]{objective}.objective.*", "the objectives are the same block as the map's own: tried under maps:objectives"},
      {"maps:events[].do[]{if}.then[]{*", "the commands in a nested list are the same definition as the event's do list: tried there"},
      {"maps:events[].do[]{if}.else[]{*", "the commands in a nested list are the same definition as the event's do list: tried there"},
      {"maps:events[].do[]{if}.if[]{*", "the conditions are the same definition as the event's own if: tried there"},
      {"maps:events[].do[]{choice}.answers[].do[]{*", "the commands in a nested list are the same definition as the event's do list: tried there"},
      {"maps:events[].do[]{choice}.answers[].if[]{*", "the conditions are the same definition as the event's own if: tried there"},
      {"maps:events[].do[]{setTerrain}.tile", "needs a tile sheet image inside the game folder (graphics.json, tileLayer); the test only writes JSON"},
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
    addMapExtraSkips(m);
    addEventExtraSkips(m);
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
      {"maps:prep", "it needs at least one slot on a free square, which Enable alone cannot know"},
      {"maps:events[].do[]{fight}.fight.weapon", "it must name at least one weapon, which Enable alone cannot know"},
      {"maps:prep.shop", "it must sell at least one item, which Enable alone cannot know"},
      {"maps:units[].behaviour{behaviour block}.retreat.to", "it needs at least one terrain id, which Enable alone cannot know"},
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
        {"tileLayer", "the second tile layer: goes with the tile grid"},
        {"heights", "the height grid: goes with the tile grid"},
    };
    for (const auto& r : kRaw)
      if (seg[0] == r[0]) return yes(r[1]);
    return false;
  }
  if (schema == "units" || schema == "weapons" || schema == "items" || schema == "terrain")
    if (seg.size() == 1) return yes("older keys of a core file (abilities, promotions, ammo, skills, inflict, vision ...): not described yet");
  return false;
}

}  // namespace conformance
