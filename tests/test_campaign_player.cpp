// What the player decides around the campaign without a window (engine/player/campaign/campaign_logic.*
// and the save files of a campaign game): which step follows which phase, when the Continue file is
// written, the words of the save lists, and SaveStore reading campaign saves.
#include "campaign_run.h"
#include "player/campaign/campaign_logic.h"
#include "player/saves.h"

using namespace camptest;

namespace {

SaveInfo infoOf(SaveStore& store, int slot, Run& r) { return store.inspect(slot, &r.g.db, "camp_player"); }

// A store in a fresh folder.
SaveStore freshStore(const char* name) {
  std::string folder = savetest::tempFolder(std::string("campaign_player/") + name);
  SaveStore s(folder);
  for (int i = 0; i <= SaveStore::kSlots; ++i) {
    std::string err;
    s.remove(i, err);
  }
  return s;
}

}  // namespace

TEST(campaign_player_next_step_by_phase) {
  CHECK(nextForPhase(Phase::Before, false) == Next::BeforeScene);
  CHECK(nextForPhase(Phase::Before, true) == Next::BeforeScene);  // a battle in hand only matters in phase Battle
  CHECK(nextForPhase(Phase::Battle, false) == Next::StartBattle);
  CHECK(nextForPhase(Phase::Battle, true) == Next::ResumeBattle);
  CHECK(nextForPhase(Phase::After, false) == Next::AfterScene);
  CHECK(nextForPhase(Phase::Finished, false) == Next::EndFinished);
  CHECK(nextForPhase(Phase::Over, false) == Next::EndOver);
}

TEST(campaign_player_next_step_after_a_battle) {
  CHECK(nextAfterBattle(ChapterResult::Won) == Next::AfterScene);
  CHECK(nextAfterBattle(ChapterResult::Retry) == Next::RetryChoice);
  CHECK(nextAfterBattle(ChapterResult::Ended) == Next::EndOver);
  CHECK(nextAfterBattle(ChapterResult::NotOver) == Next::None);
  CHECK(nextAfterBattle(ChapterResult::WrongPhase) == Next::None);
}

TEST(campaign_player_continue_file_policy) {
  // Plain game: whenever saving is on. Campaign game: only the story's own battles.
  CHECK(continueIsWritten(true, false, false));
  CHECK(!continueIsWritten(false, false, false));
  CHECK(continueIsWritten(true, true, true));
  CHECK(!continueIsWritten(true, true, false));  // a free battle of a campaign game never touches it
  CHECK(!continueIsWritten(false, true, true));
}

TEST(campaign_player_words) {
  CHECK_STR(chapterHeading(0), "Chapter 1");
  CHECK_STR(chapterHeading(4), "Chapter 5");
  CHECK_STR(phaseWords("before"), "Chapter begins");
  CHECK_STR(phaseWords("battle"), "Ready to fight");
  CHECK_STR(phaseWords("after"), "Battle won");
  CHECK_STR(phaseWords("nonsense"), "");

  SaveInfo plain;
  plain.state = SaveInfo::Ok;
  plain.mapId = "bridge";
  plain.round = 3;
  CHECK_STR(saveTitle(plain), "bridge");  // no name: the id
  plain.mapName = "The Ford";
  CHECK_STR(saveTitle(plain), "The Ford");
  CHECK_STR(saveDetail(plain), "Turn 3");

  SaveInfo camp = plain;
  camp.campaign = true;
  camp.chapterName = "The Mill";
  camp.phase = "battle";
  camp.inBattle = true;
  camp.round = 5;
  CHECK_STR(saveTitle(camp), "The Mill");
  CHECK_STR(saveDetail(camp), "Turn 5");  // mid-battle: the turn
  camp.inBattle = false;
  camp.round = 0;
  CHECK_STR(saveDetail(camp), "Ready to fight");  // between battles: where the story stands
  camp.phase = "after";
  CHECK_STR(saveDetail(camp), "Battle won");
  camp.chapterName = "";
  CHECK_STR(saveTitle(camp), "The Ford");  // no chapter name in the file: the map's
}

TEST(campaign_player_store_reads_campaign_saves) {
  Run r;
  CHECK(r.begin("camp_player"));
  SaveStore store = freshStore("read");
  std::string err;

  // Phase Before, no battle.
  Json file;
  CHECK(buildCampaignSave(r.g.db, "camp_player", r.pt, nullptr, 1794651000, file, err));
  CHECK(store.write(0, file, err));
  SaveInfo i = infoOf(store, 0, r);
  CHECK(i.state == SaveInfo::Ok);
  CHECK(i.campaign);
  CHECK(!i.inBattle);
  CHECK_EQ(i.round, 0);
  CHECK_STR(i.phase, "before");
  CHECK_STR(i.chapterName, "The Ford");
  CHECK_STR(saveDetail(i), "Chapter begins");
  CHECK_EQ(i.savedAt, 1794651000);

  // Mid-battle: the turn is shown.
  CHECK(r.startBattle());
  CHECK(buildCampaignSave(r.g.db, "camp_player", r.pt, &r.b, 1794651100, file, err));
  CHECK(store.write(2, file, err));
  i = infoOf(store, 2, r);
  CHECK(i.state == SaveInfo::Ok && i.inBattle);
  CHECK_EQ(i.round, r.b.round);
  CHECK_STR(i.phase, "battle");
  CHECK_STR(saveDetail(i), "Turn " + std::to_string(r.b.round));

  // read() hands the whole file back, which restoreCampaignSave takes.
  SaveFile sf;
  CHECK(store.read(2, r.g.db, "camp_player", sf, err));
  CHECK(sf.info.campaign);
  Playthrough pt;
  Battle b;
  bool has = false;
  CHECK(restoreCampaignSave(r.g.db, r.g.formulas, "camp_player", sf.root, pt, b, has, err));
  CHECK(has);
  CHECK_EQ(b.round, r.b.round);
  CHECK(pt.phase == Phase::Battle);

  // A plain save of the same game (a free battle) is listed as a plain save.
  Battle skirm;
  MapDef map;
  CHECK(loadMap(r.g.db, "skirmish", map, err));
  CHECK(skirm.init(r.g.db, map, r.g.formulas, 1, err));
  std::vector<Event> ev;
  skirm.start(ev);
  CHECK(buildSaveFile(skirm, r.g.db, "camp_player", "skirmish", 1794651200, file, err));
  CHECK(store.write(3, file, err));
  i = infoOf(store, 3, r);
  CHECK(i.state == SaveInfo::Ok && !i.campaign);
  CHECK_STR(saveTitle(i), "Skirmish");
  CHECK_STR(saveDetail(i), "Turn 1");
}

TEST(campaign_player_store_refuses_bad_campaign_saves) {
  Run r;
  CHECK(r.begin("camp_player"));
  SaveStore store = freshStore("refuse");
  std::string err;
  Json file;
  CHECK(buildCampaignSave(r.g.db, "camp_player", r.pt, nullptr, 1, file, err));

  // Another game.
  CHECK(store.write(1, file, err));
  SaveInfo i = store.inspect(1, &r.g.db, "other_game");
  CHECK(i.state == SaveInfo::Unusable);
  CHECK_STR(i.reason, "This save belongs to another game");

  // The game changed its rules: the same file written by an older campaign.
  Json stale = Json::makeObject();
  for (const Json::Member& m : file.members()) stale.set(m.first, m.first == "dataHash" ? Json::makeString("0000000000000000") : m.second);
  CHECK(store.write(2, stale, err));
  i = infoOf(store, 2, r);
  CHECK(i.state == SaveInfo::Unusable);
  CHECK_STR(i.reason, "Made with a different version of this game");
  SaveFile unused;
  CHECK(!store.read(2, r.g.db, "camp_player", unused, err));
  CHECK_STR(err, "Made with a different version of this game");

  // Without a phase the file is not a playthrough at all.
  Json broken = Json::makeObject();
  for (const Json::Member& m : file.members()) {
    if (m.first != "playthrough") { broken.set(m.first, m.second); continue; }
    Json p = Json::makeObject();
    for (const Json::Member& pm : m.second.members())
      if (pm.first != "phase") p.set(pm.first, pm.second);
    broken.set("playthrough", p);
  }
  CHECK(store.write(3, broken, err));
  i = infoOf(store, 3, r);
  CHECK(i.state == SaveInfo::Unusable);
  CHECK_STR(i.reason, "The file is damaged");

  // A game that no longer has a campaign refuses a story save.
  Run plain;
  CHECK(plain.g.load("camp_player_plain", ""));
  i = store.inspect(1, &plain.g.db, "camp_player");
  CHECK(i.state == SaveInfo::Unusable);
}
