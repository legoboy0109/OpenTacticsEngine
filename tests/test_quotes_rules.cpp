// Battle and defeat quotes in the rules core (docs/EVENTS.md "Battle and defeat quotes"): when each is said, in which
// order, how often, and how it sits in the event stream. Game: tests/fixtures/srpg-quotes.
#include "quotes_fixture.h"

using namespace ote;
using namespace quotestest;

TEST(quotes_general_quote_is_said_once_per_map_before_the_strikes) {
  Q q("quotes_general");
  q.put("eirik", 4, 2);
  // The nameless grunt stands at 5,2 beside him: it has no quote, so only Eirik's general one is said.
  const int grunt = q.b().occupant[(size_t)q.b().grid.at(5, 2)];
  CHECK(q.run(Action::attack(q.id("eirik"), grunt, q.sword())));
  CHECK(is(q.trace(), {"Eirik: For the valley!", "strike Eirik>Grunt", "strike Grunt>Eirik"}));  // before the first strike, once
  q.fresh("eirik");
  CHECK(q.run(Action::attack(q.id("eirik"), grunt, q.sword())));
  CHECK(is(q.said(), {}));  // already said on this map
}

TEST(quotes_a_quote_against_the_foe_beats_the_general_one_and_the_defender_speaks_first) {
  Q q("quotes_pair");
  CHECK(q.attack("eirik", "gorm"));
  // Gorm (attacked) first with his quote against Eirik, then Eirik's against Gorm; both before the first strike.
  CHECK(is(q.trace(), {"Gorm: Eirik. At last.", "Eirik: Gorm! Your raids end today.", "strike Eirik>Gorm", "strike Gorm>Eirik"}));
  // Second fight: Gorm's is "always" so it comes again; Eirik's against-Gorm quote is used up, so his general one follows.
  q.fresh("eirik");
  CHECK(q.attack("eirik", "gorm"));
  CHECK(is(q.said(), {"Gorm: Eirik. At last.", "Eirik: For the valley!"}));
  // Third: Gorm again, Eirik has nothing left.
  q.fresh("eirik");
  CHECK(q.attack("eirik", "gorm"));
  CHECK(is(q.said(), {"Gorm: Eirik. At last."}));
}

TEST(quotes_the_attacker_speaks_second_and_a_friend_can_say_a_line) {
  Q q("quotes_order");
  q.put("bram", 4, 0);
  CHECK(q.attack("bram", "gorm"));
  // Gorm has nothing against Bram: his general quote (two lines, the second by Mira), then Bram's "always" one.
  CHECK(is(q.said(), {"Gorm: Come, little ones.", "Mira: Do not run.", "Bram: Hah!"}));
  q.fresh("bram");
  CHECK(q.attack("bram", "gorm"));
  CHECK(is(q.said(), {"Bram: Hah!"}));  // Gorm's was once per campaign; Bram's is every time
}

TEST(quotes_the_computers_attack_gives_the_blue_defender_the_first_word) {
  Q q("quotes_enemy_turn");
  q.unit("bram").hp = 24;
  q.put("bram", 0, 2);  // out of the way: Gorm's only neighbour is Eirik
  q.unit("tolly").hp = 10;
  const std::vector<Event> all = q.redTurn();
  const std::vector<std::string> t = q.trace(all);
  // Whoever Gorm hit, the unit attacked speaks before its attacker; if it was Eirik the pair quotes show it.
  for (size_t i = 0; i + 1 < t.size(); ++i)
    if (t[i] == "Eirik: Gorm! Your raids end today.") CHECK(t[i + 1] == "Gorm: Eirik. At last.");
  bool gormAttacked = false;
  for (const std::string& s : t) gormAttacked = gormAttacked || s == "strike Gorm>Eirik";
  CHECK(gormAttacked);
}

TEST(quotes_defeat_quote_follows_the_kill_before_the_story_events) {
  Q q("quotes_defeat_player", "eventful");
  q.unit("gorm").hp = 1;
  CHECK(q.attack("eirik", "gorm"));
  // quotes, then the fight event's line, the strike, the fall, the last words, then the defeat event's line.
  CHECK(is(q.trace(), {"Gorm: Eirik. At last.", "Eirik: Gorm! Your raids end today.", "Narrator: The field falls quiet.", "strike Eirik>Gorm",
                       "died Gorm", "Gorm: Impossible...", "Mira: Brother!", "Narrator: The raiders scatter."}));
}

TEST(quotes_defeat_on_a_counter_and_falls_or_retreats_by_the_campaigns_rule) {
  Q q("quotes_counter");  // fallen: gone
  q.unit("eirik").hp = 1;
  CHECK(q.attack("eirik", "gorm"));
  CHECK(is(q.said(), {"Gorm: Eirik. At last.", "Eirik: Gorm! Your raids end today.", "Eirik: Tell them I tried."}));
  CHECK(q.trace().back() == "Eirik: Tell them I tried.");
  Q r("quotes_counter_return", "ch1", [](QCopy& c) { c.swap("data/campaign.json", "\"fallen\": \"gone\"", "\"fallen\": \"return\""); });
  r.unit("eirik").hp = 1;
  CHECK(r.attack("eirik", "gorm"));
  CHECK(r.said().back() == "Eirik: I must pull back!");  // one of ours, and fallen units return
  // The enemy leader still "falls": only the human side's units can come back.
  Q s("quotes_return_enemy", "ch1", [](QCopy& c) { c.swap("data/campaign.json", "\"fallen\": \"gone\"", "\"fallen\": \"return\""); });
  s.unit("gorm").hp = 1;
  CHECK(s.attack("eirik", "gorm"));
  CHECK(s.said().back() == "Mira: Brother!");
}

TEST(quotes_a_unit_struck_down_on_the_enemy_turn_says_its_last_words) {
  Q q("quotes_enemy_kill");
  q.unit("eirik").hp = 1;
  q.put("bram", 0, 2);
  const std::vector<Event> all = q.redTurn();
  const std::vector<std::string> t = q.trace(all);
  bool found = false;
  for (size_t i = 0; i + 1 < t.size(); ++i)
    if (t[i] == "died Eirik") {
      found = true;
      CHECK(t[i + 1] == "Eirik: Tell them I tried.");
    }
  CHECK(found);
}

TEST(quotes_a_bosss_defeat_quote_comes_before_the_victory) {
  Q q("quotes_boss");
  for (const char* id : {"tolly"}) {  // Gorm and the nameless grunt remain; take the grunt off the board too
    Unit& u = q.unit(id);
    q.b().occupant[(size_t)u.cell] = -1;
    u.alive = false;
    u.cell = kNoCell;
  }
  const int grunt = q.b().occupant[(size_t)q.b().grid.at(5, 2)];
  Unit& g = q.b().units[(size_t)grunt];
  q.b().occupant[(size_t)g.cell] = -1;
  g.alive = false;
  g.cell = kNoCell;
  q.unit("gorm").hp = 1;
  CHECK(q.attack("eirik", "gorm"));
  const std::vector<std::string> t = q.trace();
  CHECK(is(t, {"Gorm: Eirik. At last.", "Eirik: Gorm! Your raids end today.", "strike Eirik>Gorm", "died Gorm", "Gorm: Impossible...",
               "Mira: Brother!", "ended"}));
  CHECK(q.b().over);
}

TEST(quotes_units_without_a_character_have_none_and_a_story_removal_is_no_defeat) {
  Q q("quotes_generic");
  q.put("mira", 5, 1);  // beside the nameless grunt (5,2) and tolly (5,0)
  const int grunt = q.b().occupant[(size_t)q.b().grid.at(5, 2)];
  q.b().units[(size_t)grunt].hp = 1;
  CHECK(q.run(Action::attack(q.id("mira"), grunt, q.sword())));
  CHECK(is(q.trace(), {"strike Mira>Grunt", "died Grunt"}));  // neither has a quote
}

TEST(quotes_a_line_of_a_character_that_is_not_on_the_map_is_shown_by_name_with_its_portrait) {
  Q q("quotes_absent", "ch1", [](QCopy& c) {
    c.swap("maps/ch1.json", "    { \"character\": \"mira\", \"side\": \"blue\", \"x\": 0, \"y\": 0 },\n", "");
  });
  q.put("bram", 4, 0);
  CHECK(q.attack("bram", "gorm"));
  CHECK(is(q.said(), {"Gorm: Come, little ones.", "Mira: Do not run.", "Bram: Hah!"}));
  const Event* line = nullptr;
  for (const Event& e : q.events)
    if (e.type == EventType::Dialogue && q.b().storyText(e.b) == "Do not run.") line = &e;
  CHECK(line != nullptr && line->a == -1 && line->d >= 0 && q.b().storyText(line->d) == "mira");
}

TEST(quotes_a_game_without_quotes_makes_no_dialogue_and_keeps_its_saves) {
  Q q("quotes_none", "ch1", [](QCopy& c) {
    c.put("data/characters.json", R"({ "characters": [ { "id": "eirik", "name": "Eirik", "type": "hero" }, { "id": "bram", "name": "Bram", "type": "brute" },
      { "id": "mira", "name": "Mira", "type": "rider" }, { "id": "gorm", "name": "Gorm", "type": "brute" }, { "id": "tolly", "name": "Tolly", "type": "grunt" } ] })");
  });
  CHECK(q.attack("eirik", "gorm"));
  CHECK(is(q.said(), {}));
  Json j;
  saveBattleState(q.b(), j);
  CHECK(!j.has("quotes"));
}
