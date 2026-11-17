// Helpers of the battle and defeat quote tests (docs/EVENTS.md "Battle and defeat quotes"): a writable copy of
// tests/fixtures/srpg-quotes and a loaded battle on one of its maps.
//
// The fixture (original names): ch1 / ch2 (8 x 3, a wall column at x 3 with a gap in row 1): blue eirik (hero, leader, 3,1), bram
// (brute, 2,1), mira (rider, 0,0); red gorm (brute, leader, 4,1), tolly (grunt, 5,0), a nameless grunt (5,2). "eventful" is ch1
// with two story events (the fight of eirik and gorm, the defeat of gorm) and no tolly.
// Quotes: eirik: against gorm "Gorm! Your raids end today." (map), general "For the valley!" (map); defeat falls "Tell them I tried.",
// retreats "I must pull back!". bram: always "Hah!"; defeat falls "Bram goes down.", retreats "Bram backs off.". gorm: general "Come,
// little ones." then mira "Do not run." (campaign); against eirik "Eirik. At last." (always); defeat falls "Impossible..." then mira
// "Brother!". tolly: general "Tolly tries." (map). mira has none.
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/ai/ai.h"
#include "core/campaign/campaign_save.h"
#include "core/campaign/playthrough.h"
#include "srpg_ranks_fixture.h"

namespace quotestest {

using namespace ote;

struct QCopy : rankstest::Copy {
  explicit QCopy(const std::string& name) : Copy(name, "srpg-quotes") {
    static const char* const extra[] = {"data/campaign.json", "maps/ch1.json", "maps/ch2.json", "maps/eventful.json"};
    for (const char* f : extra) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/srpg-quotes/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
};

// A battle on the fixture (or a changed copy of it), started.
struct Q {
  QCopy copy;
  savetest::Rig rig;
  std::vector<Event> events;
  explicit Q(const char* name, const char* map = "ch1", const std::function<void(QCopy&)>& tweak = nullptr, bool start = true)
      : copy(name) {
    if (tweak) tweak(copy);
    CHECK(rig.load(copy.dir, map));
    if (start) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  int sword() { return rig.db.findWeapon("sword"); }
  bool run(const Action& a) {
    events.clear();
    std::string why;
    const bool ok = b().apply(a, events, &why);
    if (!ok) std::printf("  refused: %s\n", why.c_str());
    return ok;
  }
  // Puts a unit on a square by hand (a test arranging the board) and lets it act again.
  void put(const char* who, int x, int y) {
    Unit& u = unit(who);
    if (u.cell != kNoCell) b().occupant[(size_t)u.cell] = -1;
    u.cell = b().grid.at(x, y);
    b().occupant[(size_t)u.cell] = u.id;
    b().refreshSight();
    fresh(who);
  }
  void fresh(const char* who) {
    Unit& u = unit(who);
    u.done = u.moved = u.acted = false;
  }
  bool attack(const char* who, const char* target) { return run(Action::attack(id(who), id(target), sword())); }
  // Everything of `ev` that matters to a quote test, in order: "Name: text" for a line, "strike a>b", "died name", "ended".
  std::vector<std::string> trace(const std::vector<Event>& ev) {
    std::vector<std::string> out;
    for (const Event& e : ev) {
      if (e.type == EventType::Dialogue) {
        std::string who = e.a >= 0 ? b().units[(size_t)e.a].name : (e.c >= 0 ? b().storyText(e.c) : "");
        out.push_back(who + ": " + b().storyText(e.b));
      } else if (e.type == EventType::Attack) {
        out.push_back("strike " + b().units[(size_t)e.a].name + ">" + b().units[(size_t)e.b].name);
      } else if (e.type == EventType::UnitDied) {
        out.push_back("died " + b().units[(size_t)e.a].name);
      } else if (e.type == EventType::BattleEnded) {
        out.push_back("ended");
      }
    }
    return out;
  }
  std::vector<std::string> trace() { return trace(events); }
  // Only the dialogue lines.
  std::vector<std::string> said(const std::vector<Event>& ev) {
    std::vector<std::string> out;
    for (const std::string& s : trace(ev))
      if (s.compare(0, 6, "strike") && s.compare(0, 4, "died") && s != "ended") out.push_back(s);
    return out;
  }
  std::vector<std::string> said() { return said(events); }
  // The computer plays red's whole turn after blue ends its own; returns every event of both.
  std::vector<Event> redTurn() {
    std::vector<Event> all;
    run(Action::endTurn());
    all.insert(all.end(), events.begin(), events.end());
    for (int i = 0; i < 40 && !b().over && b().activeSide == 1; ++i) {
      events.clear();
      aiTakeStep(b(), events);
      all.insert(all.end(), events.begin(), events.end());
    }
    return all;
  }
};

inline bool is(const std::vector<std::string>& got, const std::vector<std::string>& want) {
  if (got == want) return true;
  std::printf("  got:");
  for (const std::string& s : got) std::printf(" [%s]", s.c_str());
  std::printf("\n  want:");
  for (const std::string& s : want) std::printf(" [%s]", s.c_str());
  std::printf("\n");
  return false;
}

}  // namespace quotestest
