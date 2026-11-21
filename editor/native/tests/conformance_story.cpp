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

void addStoryExtraSkips(std::map<QString, QString>&) {}

}  // namespace conformance
