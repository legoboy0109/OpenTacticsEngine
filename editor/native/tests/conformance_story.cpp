// Conformance tables, part 5: the campaign (chapters and their scenes), data/audio.json with the sound keys elsewhere, and the map
// keys that name music. A scene is tried in the first chapter's `before` list (the sample of a command replaces its first entry);
// `after` and the scene form with music hold the same commands, so only their own keys are tried and the rest is skipped with that
// reason (conformance_contexts.cpp, skipped()).
#include "conformance_contexts.h"

namespace conformance {

namespace {
#define SC "campaign:chapters[].before{commands}[]"
}  // namespace

void addStoryContexts(std::vector<Context>& c) {
  // --- the campaign file ---
  c.push_back({"campaign-base", "parties-flow", {},
               {"campaign:name", "campaign:rules", "campaign:freeBattles", "campaign:parties", "campaign:chapters", SC "{say}", SC "{narrate}", SC "{giveResource}",
                SC "{switchParty}", SC "{mergeParties}", SC "{goto}", SC "{wait}", SC "{shake}", SC "{clearStage}", SC "{leave}", SC "{enter}",
                SC "{move}", SC "{fade}", SC "{storeItems}"},
               {{"campaign:chapters[].next", "\"ch2\""},
                {"campaign:parties", R"([{"id":"main"},{"id":"north"}])"},
                {SC "{mergeParties}", R"({"mergeParties":["north"],"into":"main"})"},
                {SC "{mergeParties}.mergeParties", R"(["north"])"},
                {SC "{mergeParties}.into", "\"main\""},
                {SC "{goto}.goto", "\"ch2\""},
                {SC "{fade}.color", "\"#112233\""}}});
  c.push_back({"campaign-story", "srpg-rename",
               {{"game.json", "difficulties", R"({"default":"normal","list":[{"id":"normal"},{"id":"hard"}]})"}},
               {SC "{setSwitch}", SC "{setVariable", SC "{addVariable", SC "{takeItem}", SC "{addSupport}", SC "{choice}", SC "{if}"},
               {{SC "{addSupport}", R"({"addSupport":["hero","ally"],"by":2})"},
                {SC "{addSupport}.addSupport", R"(["hero","ally"])"},
                {SC "{choice}.answers", R"([{"text":"Yes"},{"text":"No"}])"},
                {SC "{if}.if[]{support}", R"({"support":["hero","ally"],"value":1})"},
                {SC "{if}.if[]{support}.support", R"(["hero","ally"])"}}});
  c.push_back({"campaign-audio", "demo-srpg", {},
               {SC "{playMusic}", SC "{stopMusic}", SC "{playSound}", "campaign:chapters[].before{music and commands}.music"}, {}});
  c.push_back({"campaign-background", "scene-pres", {}, {SC "{background}"}, {}});

  // --- data/graphics.json (and the style fragments art/*.style.json, which the engine itself never reads: same keys, tried here) ---
  c.push_back({"gfx-main", "demo-economy", {},
               {"graphics:sheets", "graphics:unitMarker", "graphics:terrain", "graphics:units", "graphics:structures"},
               {{"graphics:terrain.*{picture}", R"({"sheet":"tiles","cell":[0,0]})"},
                {"graphics:terrain.*{picture}.join{one block}", "[0,2]"},
                {"graphics:terrain.*{picture}.join{animation (several blocks)}", "[[0,2]]"},
                {"graphics:terrain.*{picture}.join{animation (several blocks)}[]", "[0,2]"},
                {"graphics:units.*{picture}", R"({"sheet":"units","cell":[0,0]})"},
                {"graphics:units.*{picture}.walk", R"({"down":[[0,0]]})"},
                {"graphics:units.*{picture}.sides", R"({"blue":{"sheet":"units","cell":[0,0]}})"},
                {"graphics:structures.*{picture}", R"({"sheet":"structures","cell":[0,0]})"},
                {"graphics:structures.*{picture}.sides", R"({"blue":{"sheet":"structures","cell":[0,0]}})"},
                {"graphics:sheets[].file", "\"art/tiles.png\""}}});
  c.push_back({"gfx-tilesets", "demo-economy",
               {{"data/graphics.json", "tilesets", R"({"t":{"sheet":"tiles","animations":{"0":{"frames":[0,1],"ms":200}}}})"}},
               {"graphics:tilesets"},
               {{"graphics:tilesets.*.animations", R"({"0":{"frames":[0,1],"ms":200}})"}, {"graphics:tilesets.*.animations.*.frames", "[0,1]"}}});
  c.push_back({"gfx-extra", "scene-pres", {}, {"graphics:portraits", "graphics:backgrounds"}, {}});
  // Item and weapon icons (docs/GRAPHICS.md "Icons"): by weapon or item id; demo-economy has weapons and a sheet "tiles" of cells.
  c.push_back({"gfx-icons", "demo-economy",
               {{"data/graphics.json", "icons", R"({"rifle":{"sheet":"tiles","cell":[0,0]}})"}},
               {"graphics:icons"},
               {}});

  // A 3D style of demo-srpg (the first entry of `styles` is what the keys are tried in): sprites, models and standing things.
  c.push_back({"gfx-style", "demo-srpg",
               {{"data/graphics.json", "styles",
                 R"({"isometric-3d":{"terrain":{"plain":{"sheet":"iso_tiles","cell":[0,0]}},"units":{"lord":{"sheet":"iso_units","cell":[0,0]}},)"
                 R"("structures":{"fort":{"model":"models/fort.glb"}},"portraits":{"eirik":{"sheet":"units","cell":[0,0]}}}})"}},
               {"graphics:styles"},
               {{"graphics:styles.*.terrain.*{picture}", R"({"sheet":"iso_tiles","cell":[0,0]})"},

                {"graphics:styles.*.terrain.*{picture}.stand", R"({"sheet":"stand","cell":[0,0]})"},
                {"graphics:styles.*.terrain.*{picture}.stand.variants{cells of the sheet}", "[[0,0]]"},
                {"graphics:styles.*.terrain.*{picture}.stand.variants{cells of the sheet}[]", "[0,0]"},
                {"graphics:styles.*.terrain.*{picture}.stand.model", "\"models/fighter.glb\""},
                {"graphics:styles.*.units.*{picture}", R"({"sheet":"iso_units","cell":[0,0]})"},
                {"graphics:styles.*.units.*{picture}.walk", R"({"down":[[0,0]]})"},
                {"graphics:styles.*.units.*{picture}.sides", R"({"blue":{"sheet":"iso_units","cell":[0,0]}})"},
                {"graphics:styles.*.units.*{3D model}", R"({"model":"models/fighter.glb"})"},
                {"graphics:styles.*.units.*{3D model}.model", "\"models/fighter.glb\""},
                {"graphics:styles.*.structures.*{picture}", R"({"sheet":"structures","cell":[0,0]})"},
                {"graphics:styles.*.structures.*{picture}.sides", R"({"blue":{"sheet":"structures","cell":[0,0]}})"},
                {"graphics:styles.*.structures.*{3D model}", R"({"model":"models/fort.glb"})"},
                {"graphics:styles.*.structures.*{3D model}.model", "\"models/fort.glb\""}}});

  // A standing thing is a picture (sheet, cell, width) or a model (model, height, turn); the two never mix.
  c.push_back({"gfx-style-stand-model", "demo-srpg",
               {{"data/graphics.json", "styles", R"({"isometric-3d":{"terrain":{"plain":{"sheet":"iso_tiles","cell":[0,0],"stand":{"model":"models/fighter.glb"}}}}})"}},
               {"graphics:styles.*.terrain.*{picture}.stand.model", "graphics:styles.*.terrain.*{picture}.stand.height", "graphics:styles.*.terrain.*{picture}.stand.turn",
                "graphics:styles.*.terrain.*{picture}.stand.variants{model files}"},
               {{"graphics:styles.*.terrain.*{picture}", R"({"sheet":"iso_tiles","cell":[0,0]})"},
                // (a list of model files is only read when the standing thing has no sheet: the loader picks the picture kind first)
                {"graphics:styles.*.terrain.*{picture}.stand.variants{model files}[]", "\"models/fighter.glb\""},
                {"graphics:styles.*.terrain.*{picture}.stand", R"({"model":"models/fighter.glb"})"},
                {"graphics:styles.*.terrain.*{picture}.stand.model", "\"models/fighter.glb\""}}});

  // --- data/audio.json (demo-economy has the tracks and sound files) and the sound keys of other files ---
  c.push_back({"audio", "demo-economy", {}, {"audio:"},
               {{"audio:music.*.file", "\"audio/title.ogg\""},
                {"audio:sounds.*.file", "\"audio/cursor.wav\""},
                {"audio:sounds.*.files", R"(["audio/cursor.wav"])"},
                // A jingle plays once: the track must have loop false.
                {"audio:use.victory", "\"victory\""}, {"audio:use.defeat", "\"defeat\""}, {"audio:use.levelUp", "\"levelup\""},
                {"audio:use.promotion", "\"victory\""}, {"audio:use.chapterCard", "\"victory\""}, {"audio:use.bootJingle", "\"victory\""}}});
  c.push_back({"audio-sounds", "demo-economy", {}, {"weapons:sounds", "units:sounds", "game:moveTypes[].sounds"}, {}});
  c.push_back({"audio-item-sounds", "demo-srpg", {}, {"items:sounds"}, {}});
  c.push_back({"map-audio", "demo-economy", {}, {"maps:music", "maps:playerTurnMusic", "maps:enemyTurnMusic", "maps:prepMusic", "maps:phaseMusic", "maps:fewEnemies"}, {}});
}

void addStoryExtraSkips(std::map<QString, QString>& s) {
  const char* kJoin = "the lists are read only for a joined picture (join); a plain picture ignores them";
  for (const char* k : {"graphics:terrain.*{picture}.joinWith", "graphics:terrain.*{picture}.joinExcept", "graphics:styles.*.terrain.*{picture}.joinWith",
                        "graphics:styles.*.terrain.*{picture}.joinExcept"})
    s.insert({QString(k) + " = [\"zz_no_such_id\"]", kJoin});
}

}  // namespace conformance
