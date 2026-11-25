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
    addBattleContexts(v);
    return v;
  }();
  return c;
}

const std::map<QString, QString>& skipped() {
  static const std::map<QString, QString> s = {
      {"maps:events[].do[]{returnMap}*", "a returnMap needs a showMap before it in the same event (cross-key rule); the test replaces the event's first command, so the pair is covered by the engine's show-map tests and the editor's unit tests"},
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
      {"game:rules.promotion.testPromotedTag", "needs testAt beside it (cross-key rule); the test level is covered by the engine's promotion tests"},
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
      {"graphics:battle.sets", "a free map: its entries are tried one by one (replacing it would drop the set the assignments name)"},
      {"graphics:battle.backgrounds", "a free map: its entries are tried one by one (replacing it would drop the backgrounds the terrain table names)"},
      {"graphics:battle.platforms", "a free map: its entries are tried one by one (replacing it would drop the platforms the terrain table names)"},
      {"graphics:sheets", "a list of sheets: its entries are tried one by one (replacing the whole list would drop the sheets the rest of the file names)"},
      {"graphics:backgrounds", "a free map: its entries are tried one by one (replacing it would drop the backgrounds the story names)"},
      {"graphics:terrain", "a map by terrain id: its entries are tried one by one"},
      {"graphics:units", "a map by unit type id: its entries are tried one by one"},
      {"graphics:characters", "a map by character id: its entries are tried one by one"},
      {"graphics:structures", "a map by structure id: its entries are tried one by one"},
      {"graphics:portraits", "a free map: its entries are tried one by one"},
      {"graphics:icons", "a free map: its entries are tried one by one"},
      // { "actor": true } is a filter only inside the commands of an event that has an acting unit (docs/EVENTS.md "The actor as a target"): the loader
      // refuses it in a trigger's own filters and in the objectives, which these keys are tried in.
      {"maps:events[].do[]{addCondition}.to{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{changeSide}.changeSide{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{forgetSkill}.from{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:units[].guest*", "a guest must stand on a side a person plays, and both sides of the core fixture are the computer's (cross-key rule); covered by tests/test_guests.cpp and tests/editor_api_rule_gaps_checks.py"},
      {"maps:abilityPoints*", "needs items that teach (data/items.json \"teaches\"), which the core fixture has none of (cross-file rule); covered by tests/test_map_ability_points.cpp and tests/editor_api_rule_gaps_checks.py"},
      {"maps:events[].do[]{guestJoins}.guestJoins{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{giveItem}.to{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{removeCondition}.from{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{removeUnit}.removeUnit{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{setBehaviour}.units{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{storeItems}.storeItems{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{takeItem}.from{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].do[]{teachSkill}.to{the acting unit}*", "the acting unit needs an event with a unit that sets it off; these keys are tried in a battle-start event (cross-key rule); covered by tests/test_story_before_prep.cpp"},
      {"maps:events[].if[]{alive}.alive{the acting unit}*", "the acting unit is for commands, not for an event's own condition (cross-key rule)"},
      {"maps:events[].if[]{dead}.dead{the acting unit}*", "the acting unit is for commands, not for an event's own condition (cross-key rule)"},
      {"maps:events[].if[]{carries}.unit{the acting unit}*", "the acting unit is for commands, not for an event's own condition (cross-key rule)"},
      {"maps:events[].if[]{sees}.sees{the acting unit}*", "the acting unit is for commands, not for an event's own condition (cross-key rule)"},
      {"maps:events[].if[]{hasSkill}.unit{the acting unit}*", "the acting unit is for commands, not for an event's own condition (cross-key rule)"},
      {"maps:events[].if[]{hasCondition}.unit{the acting unit}*", "the acting unit is for commands, not for an event's own condition (cross-key rule)"},
      {"maps:events[].if[]{actor}.actor{the acting unit}*", "the acting unit is for commands, not for an event's own condition (cross-key rule)"},
      {"maps:events[].marker", "only an event whose trigger is a reach area takes a marker, and the marker must be in graphics.json (cross-file rule); covered by tests/test_badges.cpp"},
      {"maps:events[].when{defeated}.by{the acting unit}*", "the acting unit is for commands, not for triggers (cross-key rule)"},
      {"maps:events[].when{defeated}.defeated{the acting unit}*", "the acting unit is for commands, not for triggers (cross-key rule)"},
      {"maps:events[].when{fight}.fight{the acting unit}*", "the acting unit is for commands, not for triggers (cross-key rule)"},
      {"maps:events[].when{fight}.with{the acting unit}*", "the acting unit is for commands, not for triggers (cross-key rule)"},
      {"maps:events[].when{promoted}.promoted{the acting unit}*", "the acting unit is for commands, not for triggers (cross-key rule)"},
      {"maps:events[].when{reach}.by{the acting unit}*", "the acting unit is for commands, not for triggers (cross-key rule)"},
      {"maps:events[].when{place}.by{the acting unit}*", "the acting unit is for commands, not for triggers (cross-key rule)"},
      {"maps:objectives.lose[]{defeated}.defeated{the acting unit}*", "the acting unit is for commands, not for objectives (cross-key rule)"},
      {"maps:objectives.lose[]{reach}.by{the acting unit}*", "the acting unit is for commands, not for objectives (cross-key rule)"},
      {"maps:objectives.win[]{defeated}.defeated{the acting unit}*", "the acting unit is for commands, not for objectives (cross-key rule)"},
      {"maps:objectives.win[]{reach}.by{the acting unit}*", "the acting unit is for commands, not for objectives (cross-key rule)"},
      {"graphics:styles", "a map by style id: its entries are tried one by one"},
      {"graphics:tilesets", "a free map: its entries are tried one by one"},
      {"graphics:terrain.*{picture}.stand*", "a standing thing needs a 3D style: tried under graphics:styles"},
      {"graphics:units.*{3D model}*", "a model needs a 3D style: tried under graphics:styles"},
      {"graphics:characters.*{3D model}*", "a model needs a 3D style: tried under graphics:styles"},
      {"graphics:structures.*{3D model}*", "a model needs a 3D style: tried under graphics:styles"},
      {"graphics:styles.*.terrain", "a map by terrain id: its entries are tried one by one"},
      {"graphics:styles.*.units", "a map by unit type id: its entries are tried one by one"},
      {"graphics:styles.*.characters", "a map by character id: its entries are tried one by one"},
      {"graphics:styles.*.structures", "a map by structure id: its entries are tried one by one"},
      {"graphics:styles.*.terrain.*{picture}.join{one block}", "a joined picture needs a 4 x 4 block of cells; tried with the flat art at graphics:terrain"},
      {"graphics:styles.*.terrain.*{picture}.join{animation*", "a joined picture needs a 4 x 4 block of cells; tried with the flat art at graphics:terrain"},
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
    addBattleExtraSkips(m);
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
      {"maps:backdrop", "it must name a sheet of data/graphics.json, which Enable alone cannot know"},
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
