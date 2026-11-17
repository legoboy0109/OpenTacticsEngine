// Battle and defeat quotes: the loader's refusals, the data hash, mid-battle saves and a campaign's once-per-campaign
// quotes (docs/EVENTS.md "Battle and defeat quotes", docs/SAVES.md). Game: tests/fixtures/srpg-quotes.
#include "quotes_fixture.h"

using namespace ote;
using namespace quotestest;

namespace {

int counter = 0;

// The load message of the game whose characters.json has `from` replaced by `to` ("" = it loads).
std::string loadWith(const std::string& from, const std::string& to) {
  QCopy c("quotes_data_" + std::to_string(counter++));
  c.swap("data/characters.json", from, to);
  return c.gameError();
}

std::string hashOf(QCopy& c, const char* map = "ch1") {
  Database db;
  std::string err, out;
  if (!loadGame(c.dir, db, err) || !computeDataHash(db, map, out, err)) return "error: " + err;
  return out;
}

const std::string kQ = "character \"eirik\": battle quote ";

}  // namespace

TEST(quotes_loader_refuses_bad_quotes_in_plain_words) {
  CHECK_STR(loadWith("", ""), "");
  CHECK(rankstest::has(loadWith("\"against\": \"gorm\"", "\"against\": \"nobody\""), kQ + "1: \"against\" names unknown character \"nobody\""));
  CHECK(rankstest::has(loadWith("\"against\": \"gorm\"", "\"against\": \"eirik\""), "\"against\" names the character itself"));
  CHECK(rankstest::has(loadWith("{ \"against\": \"gorm\", \"lines\"", "{ \"against\": \"gorm\", \"lines\": [ { \"say\": \"x\" } ] }, { \"against\": \"gorm\", \"lines\""),
                       "a second quote against \"gorm\""));
  CHECK(rankstest::has(loadWith("{ \"lines\": [ { \"say\": \"For the valley!\" } ] } ],", "{ \"lines\": [ { \"say\": \"A\" } ] }, { \"lines\": [ { \"say\": \"B\" } ] } ],"),
                       "a second quote against anyone"));
  CHECK(rankstest::has(loadWith("\"lines\": [ { \"say\": \"For the valley!\" } ]", "\"lines\": []"), "\"lines\" must be a list with at least one say command"));
  CHECK(rankstest::has(loadWith("\"lines\": [ { \"say\": \"For the valley!\" } ]", "\"talk\": 1"), "unknown key \"talk\""));
  CHECK(rankstest::has(loadWith("{ \"lines\": [ { \"say\": \"For the valley!\" } ] }", "{ \"once\": \"twice\", \"lines\": [ { \"say\": \"A\" } ] }"),
                       "\"once\" must be \"map\", \"campaign\" or \"always\", not \"twice\""));
  CHECK(rankstest::has(loadWith("{ \"say\": \"For the valley!\" }", "{ \"say\": \"A\", \"speaker\": \"zed\" }"), "\"speaker\" names unknown character \"zed\""));
  CHECK(rankstest::has(loadWith("{ \"say\": \"For the valley!\" }", "{ \"say\": \"A\", \"speaker\": \"mira\", \"speakerName\": \"M\" }"), "give \"speaker\" or \"speakerName\", not both"));
  CHECK(rankstest::has(loadWith("{ \"say\": \"For the valley!\" }", "{ \"giveItem\": \"x\" }"), "only \"say\" is allowed in a quote"));
  CHECK(rankstest::has(loadWith("\"retreats\": [ { \"say\": \"I must pull back!\" } ]", "\"flees\": []"), "\"defeat\": unknown key \"flees\""));
  CHECK(rankstest::has(loadWith("\"falls\": [ { \"say\": \"Tell them I tried.\" } ]", "\"falls\": \"gone\""), "must be a list of say commands"));
  CHECK(rankstest::has(loadWith("\"quotes\": {\n      \"battle\"", "\"quotes\": {\n      \"talk\": [], \"battle\""), "\"quotes\": unknown key \"talk\""));
}

TEST(quotes_wording_is_free_but_the_rules_count_for_the_data_hash) {
  QCopy a("quotes_hash_a"), b("quotes_hash_b"), c("quotes_hash_c"), d("quotes_hash_d");
  b.swap("data/characters.json", "For the valley!", "For the hills, for the road, for home!");
  c.swap("data/characters.json", "\"once\": \"campaign\"", "\"once\": \"map\"");
  d.swap("data/characters.json", "{ \"say\": \"Tell them I tried.\" }", "{ \"say\": \"Tell them I tried.\" }, { \"say\": \"Go.\" }");
  CHECK_STR(hashOf(a), hashOf(b));    // what is said does not count
  CHECK(hashOf(a) != hashOf(c));      // how often does
  CHECK(hashOf(a) != hashOf(d));      // so does adding a line
}

TEST(quotes_mid_battle_save_keeps_what_was_already_said) {
  Q q("quotes_save");
  CHECK(q.attack("eirik", "gorm"));
  Json saved;
  saveBattleState(q.b(), saved);
  CHECK(saved.has("quotes"));  // eirik's quote against gorm is used up (and Gorm's campaign one was not said)
  // Load it into a new battle of the same game: Eirik's second fight goes straight to his general quote.
  Q r("quotes_save2", "ch1", nullptr, false);
  std::string err;
  CHECK(restoreBattleState(r.b(), saved, err));
  r.fresh("eirik");
  CHECK(r.attack("eirik", "gorm"));
  CHECK(is(r.said(), {"Gorm: Eirik. At last.", "Eirik: For the valley!"}));
  QCopy none("quotes_save3");
  none.put("data/characters.json", R"({ "characters": [ { "id": "eirik", "name": "Eirik", "type": "hero" }, { "id": "bram", "name": "Bram", "type": "brute" },
      { "id": "mira", "name": "Mira", "type": "rider" }, { "id": "gorm", "name": "Gorm", "type": "brute" }, { "id": "tolly", "name": "Tolly", "type": "grunt" } ] })");
  savetest::Rig plain;
  CHECK(plain.load(none.dir, "ch1"));
  CHECK(!restoreBattleState(plain.battle, saved, err));
  CHECK(rankstest::has(err, "this game has no quotes"));
}

namespace {

// A playthrough of the fixture's two chapters (loaded from `copy`).
struct Camp {
  Database db;
  Formulas formulas;
  Playthrough pt;
  Battle b;
  std::vector<Event> ev;
  bool begin(const QCopy& c) {
    std::string err;
    if (!loadGame(c.dir, db, err) || !formulas.init(db, err)) {
      std::printf("  %s\n", err.c_str());
      return false;
    }
    return pt.begin(db);
  }
  bool chapter() {
    std::vector<SceneLine> lines;
    if (pt.phase == Phase::Before) runScene(db, pt, SceneKind::Before, lines);
    std::string err;
    ev.clear();
    b = Battle();
    const bool ok = startChapterBattle(db, pt, formulas, 3, b, ev, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    return ok;
  }
  std::vector<std::string> say(const std::vector<Event>& e) {
    std::vector<std::string> out;
    for (const Event& x : e)
      if (x.type == EventType::Dialogue) out.push_back(b.storyText(x.b));
    return out;
  }
  // Bram next to Gorm, then a fight started by Bram; the lines said.
  std::vector<std::string> bramFights() {
    Unit& bram = b.units[(size_t)b.unitByStoryId("bram")];
    b.occupant[(size_t)bram.cell] = -1;
    bram.cell = b.grid.at(4, 0);
    b.occupant[(size_t)bram.cell] = bram.id;
    bram.done = bram.moved = bram.acted = false;
    std::vector<Event> e;
    CHECK(b.apply(Action::attack(bram.id, b.unitByStoryId("gorm"), db.findWeapon("sword")), e, nullptr));
    return say(e);
  }
  void win() {
    b.over = true;
    b.winnerTeam = b.humanTeam();
    CHECK(finishChapterBattle(db, pt, b) == ChapterResult::Won);
    std::vector<SceneLine> lines;
    CHECK(runScene(db, pt, SceneKind::After, lines));
  }
};

}  // namespace

TEST(quotes_once_per_campaign_carries_across_chapters_and_saves) {
  QCopy c("quotes_campaign");
  Camp k;
  CHECK(k.begin(c));
  CHECK(k.chapter());
  CHECK(is(k.bramFights(), {"Come, little ones.", "Do not run.", "Hah!"}));
  k.win();
  CHECK(k.pt.quotesSaid.size() == 6);
  // The playthrough saves and loads with it.
  Json saved;
  savePlaythrough(k.db, k.pt, saved);
  CHECK(saved.has("quotes"));
  Playthrough back;
  std::string err;
  CHECK(restorePlaythrough(k.db, saved, back, err));
  CHECK(back.quotesSaid == k.pt.quotesSaid);
  k.pt = back;
  // Chapter two: Gorm's campaign quote is spent; Bram's "always" one comes again.
  CHECK(k.chapter());
  CHECK(is(k.bramFights(), {"Hah!"}));
}

TEST(quotes_a_lost_chapter_does_not_use_up_a_campaign_quote) {
  QCopy c("quotes_campaign_lost");
  Camp k;
  CHECK(k.begin(c));
  CHECK(k.chapter());
  CHECK(is(k.bramFights(), {"Come, little ones.", "Do not run.", "Hah!"}));
  k.b.over = true;
  k.b.winnerTeam = 1 - k.b.humanTeam();
  CHECK(finishChapterBattle(k.db, k.pt, k.b) == ChapterResult::Retry);
  CHECK(k.chapter());  // the retry
  CHECK(is(k.bramFights(), {"Come, little ones.", "Do not run.", "Hah!"}));
}

TEST(quotes_a_game_without_quotes_has_no_playthrough_flags) {
  QCopy c("quotes_campaign_none");
  c.put("data/characters.json", R"({ "characters": [ { "id": "eirik", "name": "Eirik", "type": "hero" }, { "id": "bram", "name": "Bram", "type": "brute" },
      { "id": "mira", "name": "Mira", "type": "rider" }, { "id": "gorm", "name": "Gorm", "type": "brute" }, { "id": "tolly", "name": "Tolly", "type": "grunt" } ] })");
  Camp k;
  CHECK(k.begin(c));
  CHECK(k.pt.quotesSaid.empty());
  Json saved;
  savePlaythrough(k.db, k.pt, saved);
  CHECK(!saved.has("quotes"));
}
