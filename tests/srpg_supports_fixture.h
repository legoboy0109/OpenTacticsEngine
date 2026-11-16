// Helpers for the supports tests (docs/SRPG.md section 6, step S1).
//   SupWorld: the in-code miniature game of rescue_fixture.h with characters named after the units' story ids and
//             support pairs given in code (no files), for the battle tests.
//   kCharacters / kSupports: data/characters.json and data/supports.json for the campaign fixture (camptest::Game),
//             for the loader, campaign and save tests.
#pragma once
#include "rescue_fixture.h"

namespace suptest {

using namespace ote;
using fixture::UnitSpec;

// One level of a pair written in code: its points, its bonus and its conversation as "speaker: text" lines (a
// speaker that is not a character id is a free name).
struct Lv {
  int points = 10;
  SupportBonus bonus;
  std::vector<std::string> talk;
};

inline SupportBonus bonus(int hit, int avoid, int damage = 0) {
  SupportBonus b;
  b.hit = hit;
  b.avoid = avoid;
  b.damage = damage;
  return b;
}

struct SupWorld : rescuetest::RescueWorld {
  SupWorld(const std::vector<UnitSpec>& units, const std::vector<const char*>& ids, RescueMode mode = RescueMode::Off)
      : RescueWorld(mode, units, rescuetest::open(), ids) {
    for (size_t i = 0; i < ids.size() && i < units.size(); ++i) {
      CharacterDef c;
      c.id = ids[i];
      c.name = ids[i];
      c.type = db.findUnitType(units[i].type);
      db.characters.push_back(c);
    }
    db.supports.rules.range = 3;
  }

  // Adds the pair `a`, `b` (character ids) with these levels; returns its index.
  int pair(const char* a, const char* b, const std::vector<Lv>& levels, int start = 0) {
    SupportPairDef p;
    p.a = db.findCharacter(a);
    p.b = db.findCharacter(b);
    p.start = start;
    for (const Lv& l : levels) {
      SupportLevelDef d;
      d.points = l.points;
      d.bonus = l.bonus;
      for (const std::string& line : l.talk) {
        SupportLine s;
        const size_t colon = line.find(": ");
        const std::string who = colon == std::string::npos ? "" : line.substr(0, colon);
        s.text = (int)db.supports.texts.size();
        db.supports.texts.push_back(colon == std::string::npos ? line : line.substr(colon + 2));
        if (db.findCharacter(who) >= 0) {
          s.speaker = who;
          s.speakerName = (int)db.supports.texts.size();
          db.supports.texts.push_back(who);
        } else if (!who.empty()) {
          s.speakerName = (int)db.supports.texts.size();
          db.supports.texts.push_back(who);
        }
        d.talk.push_back(s);
      }
      p.levels.push_back(d);
    }
    db.supports.pairs.push_back(p);
    return (int)db.supports.pairs.size() - 1;
  }
  void points(int endTurnAdjacent, int fightTogether = 0, int heal = 0) {
    db.supports.rules.endTurnAdjacent = endTurnAdjacent;
    db.supports.rules.fightTogether = fightTogether;
    db.supports.rules.heal = heal;
  }

  int pts(int pair) const { return battle.supports.points[(size_t)pair]; }
  int level(int pair) const { return battle.supports.level[(size_t)pair]; }
  int unitOf(const char* id) const { return battle.unitByStoryId(id); }
  bool supportAction(const char* a, const char* b) { return act(Action::support(unitOf(a), unitOf(b))); }
  std::string whyNot(const char* a, const char* b) const {
    std::string why;
    battle.validate(Action::support(unitOf(a), unitOf(b)), &why);
    return why;
  }
  // The events since the last clear, of one type.
  std::vector<Event> of(EventType t) const {
    std::vector<Event> out;
    for (const Event& e : events)
      if (e.type == t) out.push_back(e);
    return out;
  }
  // Blue ends its turn and red ends its: one round.
  void round() { pass(1); }
  // The attack forecast of `a` on `d` from where `a` stands (the first weapon).
  Forecast forecastOf(const char* a, const char* d) const {
    Forecast f;
    const int ua = unitOf(a), ud = unitOf(d);
    battle.forecast(ua, ud, 0, battle.units[(size_t)ua].cell, f);
    return f;
  }
};

// Blue: eirik (1,1), lyra (2,1) next to it, gorm (4,2) apart; red: bram (4,0)... all soldiers.
inline SupWorld trio(RescueMode mode = RescueMode::Off) {
  return SupWorld({{"soldier", 0, 1, 1, false}, {"soldier", 0, 2, 1, false}, {"soldier", 0, 0, 2, false}, {"soldier", 1, 4, 1, false}},
                  {"eirik", "lyra", "gorm", "bram"}, mode);
}

// data/characters.json and data/supports.json for camptest::Game (units eirik, lyra, wanderer, scout of its maps).
inline const char* kCharacters = R"({ "characters": [
  { "id": "eirik", "name": "Eirik", "type": "hero" }, { "id": "lyra", "name": "Lyra", "type": "ranger" },
  { "id": "wanderer", "name": "Wanderer", "type": "grunt" }, { "id": "scout", "name": "Scout", "type": "ranger" } ] })";

inline const char* kSupports = R"({
  "rules": { "levelNames": ["C", "B", "A"], "points": { "endTurnAdjacent": 5, "fightTogether": 3, "heal": 4 },
             "range": 3, "conversations": "both" },
  "pairs": [
    { "a": "eirik", "b": "lyra", "start": 0, "levels": [
        { "points": 10, "bonus": { "hit": 10, "avoid": 5 },
          "talk": [ { "say": "You never miss.", "speaker": "eirik" }, { "say": "I miss plenty.", "speaker": "lyra" } ] },
        { "points": 30, "bonus": { "hit": 20, "damage": 1, "stats": { "def": 1 } } } ] },
    { "a": "eirik", "b": "wanderer", "levels": [ { "points": 5, "bonus": { "avoid": 7 } } ] },
    { "a": "lyra", "b": "scout", "start": 40, "levels": [
        { "points": 20, "talk": [ { "say": "Hello again.", "speakerName": "Old friend" } ] } ] }
  ] })";

}  // namespace suptest
