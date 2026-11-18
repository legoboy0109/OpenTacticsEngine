// "freeBattles" object form and the story-complete unlock rule (docs/EVENTS.md "Campaign", docs/SAVES.md "Progress record").
#include "campaign_run.h"
#include "player/progress.h"
#include "player/saves.h"

using namespace camptest;

namespace {

std::string refuse(const std::string& top) {
  Game g;
  return g.loadError("camp_loader", "{ " + top + " \"chapters\": [ { \"id\": \"a\", \"map\": \"ford\" } ] }");
}
bool has(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }
std::string withFree(const std::string& free) {
  return "{ \"freeBattles\": " + free + ", \"chapters\": [ { \"id\": \"a\", \"map\": \"ford\" } ] }";
}

SaveStore freshStore(const char* name) {
  SaveStore s(savetest::tempFolder(std::string("free_battles/") + name));
  std::string err;
  for (int i = 0; i <= SaveStore::kSlots; ++i) s.remove(i, err);
  std::remove(s.progressPath().c_str());
  return s;
}

}  // namespace

TEST(free_battles_object_form_loads) {
  Game g;
  CHECK(g.load("camp_loader", withFree(R"({"maps":"all","when":"storyComplete"})")));
  const CampaignDef& c = *g.db.campaign;
  CHECK(c.freeBattles == CampaignDef::FreeBattles::All && c.freeAfterStory && c.lockedNote.empty());
  CHECK(g.load("camp_loader", withFree(R"({"maps":["skirmish","keep"],"when":"storyComplete","lockedNote":"Finish the story first."})")));
  CHECK(g.db.campaign->freeBattles == CampaignDef::FreeBattles::List && g.db.campaign->freeMaps.size() == 2);
  CHECK(g.db.campaign->freeAfterStory);
  CHECK_STR(g.db.campaign->lockedNote, "Finish the story first.");
  CHECK(g.load("camp_loader", withFree(R"({"maps":["skirmish"],"when":"always"})")));
  CHECK(!g.db.campaign->freeAfterStory);
  CHECK(g.load("camp_loader", withFree(R"({"maps":"all"})")));  // when defaults to always
  CHECK(!g.db.campaign->freeAfterStory);
  // The old forms mean "always".
  CHECK(g.load("camp_loader", withFree(R"(["skirmish"])")) && !g.db.campaign->freeAfterStory);
  CHECK(g.load("camp_loader", withFree(R"("all")")) && !g.db.campaign->freeAfterStory);
  CHECK(g.load("camp_loader") && !g.db.campaign->freeAfterStory);
}

TEST(free_battles_waiting_for_the_story_still_allow_every_start) {
  // freeBattleAllowed is about which maps may be started on their own; --map and the editor's Play use it, never the unlock.
  Game g;
  CHECK(g.load("camp_loader", withFree(R"({"maps":["skirmish"],"when":"storyComplete"})")));
  CHECK(freeBattleAllowed(g.db, "skirmish") && !freeBattleAllowed(g.db, "ford"));
}

TEST(free_battles_object_form_refusals) {
  CHECK(has(refuse(R"("freeBattles": {"when":"always"},)"), "needs \"maps\""));
  CHECK(has(refuse(R"("freeBattles": {"maps":"all","when":"later"},)"), "\"when\" must be \"always\" or \"storyComplete\""));
  CHECK(has(refuse(R"("freeBattles": {"maps":"all","when":3},)"), "\"when\""));
  CHECK(has(refuse(R"("freeBattles": {"maps":["nowhere"]},)"), "unknown map \"nowhere\""));
  CHECK(has(refuse(R"("freeBattles": {"maps":[3]},)"), "must be a map id"));
  CHECK(has(refuse(R"("freeBattles": {"maps":"some"},)"), "must be \"none\", \"all\" or a list"));
  CHECK(has(refuse(R"("freeBattles": {"maps":5},)"), "\"maps\" must be"));
  CHECK(has(refuse(R"("freeBattles": {"maps":"all","lockedNote":4},)"), "\"lockedNote\""));
  CHECK(has(refuse(R"("freeBattles": {"maps":"all","then":"x"},)"), "unknown key \"then\""));
  // The old refusals are unchanged.
  CHECK(has(refuse(R"("freeBattles":"some",)"), "must be \"none\", \"all\" or a list"));
  CHECK(has(refuse(R"("freeBattles":3,)"), "\"freeBattles\" must be"));
}

TEST(free_battles_object_form_does_not_change_the_data_hash) {
  // Moving a game to the object form (or changing when / the note) must keep its saves usable.
  auto hash = [](const std::string& free) {
    Game g;
    std::string h, err;
    CHECK(g.load("camp_loader", withFree(free)));
    CHECK(computeDataHash(g.db, "ford", h, err));
    return h;
  };
  const std::string plain = hash(R"("all")");
  CHECK_STR(hash(R"({"maps":"all"})"), plain);
  CHECK_STR(hash(R"({"maps":"all","when":"storyComplete","lockedNote":"Not yet."})"), plain);
  CHECK(hash(R"(["skirmish"])") != plain);  // the maps still count
}

TEST(free_battles_unlock_rule) {
  Run r;
  CHECK(r.begin("camp_player"));
  std::string err;
  const std::string game = "camp_player";

  // No saves at all, and a disabled store: locked.
  SaveStore store = freshStore("rule");
  CHECK(!storyUnlocked(store, game));
  CHECK(!storyUnlocked(SaveStore(), game));
  CHECK(!recordStoryComplete(SaveStore(), 1794651000, err));

  // Ordinary saves (a chapter under way, a battle) do not count.
  Json file;
  CHECK(buildCampaignSave(r.g.db, game, r.pt, nullptr, 1794651000, file, err));
  CHECK(store.write(0, file, err));
  CHECK(!storyUnlocked(store, game));

  // A story that ended by defeat ("Story ended") does not count, in the Continue file or a slot.
  r.pt.phase = Phase::Over;
  CHECK(buildCampaignSave(r.g.db, game, r.pt, nullptr, 1794651000, file, err));
  CHECK(store.write(0, file, err));
  CHECK(store.write(3, file, err));
  CHECK_STR(store.inspect(3, nullptr, game).phase, "over");
  CHECK(!storyUnlocked(store, game));

  // A completed slot ("Story complete") counts; so does a completed Continue save, even for a game edited since
  // (judged by the file alone) and after the other save is gone.
  r.pt.phase = Phase::Finished;
  CHECK(buildCampaignSave(r.g.db, game, r.pt, nullptr, 1794651000, file, err));
  CHECK(store.write(4, file, err));
  CHECK(storyUnlocked(store, game));
  CHECK(store.remove(4, err));
  CHECK(!storyUnlocked(store, game));
  CHECK(store.write(0, file, err));
  CHECK(storyUnlocked(store, game));
  CHECK(!storyUnlocked(store, "another_game"));  // a save belongs to its game
  CHECK(store.remove(0, err) && store.remove(3, err));
  CHECK(!storyUnlocked(store, game));

  // The record: written once, survives every save being deleted or replaced, keeps the first date.
  CHECK(!storyCompleteRecorded(store));
  CHECK(recordStoryComplete(store, 1794651000, err));
  CHECK(storyCompleteRecorded(store));
  CHECK(storyUnlocked(store, game));
  std::string text;
  CHECK(readTextFile(store.progressPath(), text));
  CHECK(has(text, "\"storyComplete\": true") || has(text, "\"storyComplete\":true"));
  CHECK(has(text, "2026-"));  // an ISO date
  CHECK(recordStoryComplete(store, 1894651000, err));
  std::string again;
  CHECK(readTextFile(store.progressPath(), again));
  CHECK_STR(again, text);
  for (int s = 0; s <= SaveStore::kSlots; ++s) CHECK(store.remove(s, err));
  CHECK(storyUnlocked(store, game));
  // A record that says nothing useful does not unlock.
  CHECK(store.writeAt(store.progressPath(), Json::makeObject(), err));
  CHECK(!storyUnlocked(store, game));
}

TEST(free_battles_listed_only_when_the_campaign_waits_for_the_story) {
  Game g;
  SaveStore store = freshStore("listed");
  CHECK(g.load("camp_loader", withFree(R"({"maps":"all","when":"storyComplete"})")));
  CHECK(!freeBattlesListed(g.db, store, "camp_loader"));
  std::string err;
  CHECK(recordStoryComplete(store, 1794651000, err));
  CHECK(freeBattlesListed(g.db, store, "camp_loader"));
  SaveStore none = freshStore("listed_none");
  CHECK(g.load("camp_loader", withFree(R"({"maps":"all","when":"always"})")));
  CHECK(freeBattlesListed(g.db, none, "camp_loader"));
  CHECK(g.load("camp_loader", withFree(R"(["skirmish"])")));
  CHECK(freeBattlesListed(g.db, none, "camp_loader") && freeBattlesListed(g.db, SaveStore(), "camp_loader"));
  CHECK(g.load("camp_loader", ""));  // no campaign
  CHECK(freeBattlesListed(g.db, none, "camp_loader"));
}
