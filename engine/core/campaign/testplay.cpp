// Test playthroughs: see testplay.h. Walking the main path reuses runScene, so parties, scene conditions and gotos behave as in a
// real game; only the battles are skipped (their units are read from the map files instead of played).
#include "core/campaign/testplay.h"

#include <algorithm>
#include <set>

#include "core/base/log.h"
#include "core/data/loader.h"

namespace ote {

namespace {

// One named unit the map places for the player becomes a roster entry (the first placement wins).
void addPlaced(const Database& db, const MapDef& map, Playthrough& pt, const MapUnitDef& ud) {
  if (ud.id.empty() || ud.side < 0 || ud.side >= (int)map.sides.size() || map.sides[(size_t)ud.side].ai) return;
  if (ud.type < 0 || ud.type >= (int)db.unitTypes.size()) return;
  if (pt.findUnit(ud.id)) return;
  for (const ParkedParty& p : pt.parked)
    for (const RosterUnit& r : p.roster)
      if (r.id == ud.id) return;  // already placed for another party
  RosterUnit r;
  r.id = ud.id;
  r.type = ud.type;
  r.name = ud.name.empty() ? db.unitTypes[(size_t)ud.type].name : ud.name;
  r.level = ud.level;
  r.stats = ud.stats;
  r.stats.resize(db.stats.size(), 0);
  r.items = ud.hasItems ? ud.items : db.unitTypes[(size_t)ud.type].items;
  r.weaponXp = ud.weaponXp;
  for (int& w : r.weaponXp) w = std::max(0, w);  // -1 = "not said" on a map unit; dressUnit raises a roster entry to its class
  r.jobs = ud.jobs;
  r.skills = ud.skills;
  r.side = map.sides[(size_t)ud.side].id;
  pt.roster.push_back(r);
}

// addUnit commands anywhere in a map's events (inside If and Choice answers too).
void addFromCommands(const Database& db, const MapDef& map, Playthrough& pt, const std::vector<Command>& list) {
  for (const Command& c : list) {
    if (c.kind == Command::Kind::AddUnit) addPlaced(db, map, pt, c.newUnit);
    addFromCommands(db, map, pt, c.thenDo);
    addFromCommands(db, map, pt, c.elseDo);
    for (const ChoiceAnswer& a : c.answers) addFromCommands(db, map, pt, a.commands);
  }
}

void placeFromMap(const Database& db, const ChapterDef& ch, Playthrough& pt) {
  MapDef map;
  std::string err;
  if (ch.map < 0 || ch.map >= (int)db.maps.size() || !loadMap(db, db.maps[(size_t)ch.map].id, map, err)) {
    OTE_WARN("test playthrough: chapter '%s': its map could not be read (%s); nobody joins from it", ch.id.c_str(), err.c_str());
    return;
  }
  for (const MapUnitDef& ud : map.units) addPlaced(db, map, pt, ud);
  if (map.story)
    for (const StoryEvent& e : map.story->events) addFromCommands(db, map, pt, e.commands);
}

// Walks one chapter without a battle: its scene, the units its map places, its after scene. Returns the chapter the story goes on
// with (-1 = it ended). `listOrder`: the fallback walk, which goes on with the next chapter of the list whatever the story says.
int walkChapter(const Database& db, Playthrough& pt, int chapter, bool listOrder) {
  std::vector<SceneLine> lines;
  pt.chapter = chapter;
  pt.phase = Phase::Before;
  runScene(db, pt, SceneKind::Before, lines);
  if (pt.phase == Phase::Before && !listOrder) return pt.chapter;  // a "goto" in the before scene skipped this chapter's battle
  placeFromMap(db, db.campaign->chapters[(size_t)chapter], pt);
  pt.chapter = chapter;
  pt.phase = Phase::After;
  lines.clear();
  runScene(db, pt, SceneKind::After, lines);
  if (listOrder) return chapter + 1;
  return pt.phase == Phase::Before ? pt.chapter : -1;
}

int raiseRoster(const Database& db, Battle& calc, Roster& roster, int level) {
  int raised = 0;
  for (RosterUnit& r : roster) {
    if (r.gone || r.type < 0 || r.type >= (int)db.unitTypes.size()) continue;
    Unit u;  // a stand-alone unit: levelUnitTo only needs the database (no battle around it)
    u.type = r.type;
    u.level = r.level;
    u.xp = r.xp;
    u.merit = r.merit;
    u.stats = r.stats;
    u.stats.resize(db.stats.size(), 0);
    u.learned = r.skills;
    u.character = db.findCharacter(r.id);
    if (calc.levelUnitTo(u, level) <= 0) continue;
    r.level = u.level;
    r.xp = u.xp;
    r.merit = u.merit;
    r.stats = u.stats;
    r.skills = u.learned;
    ++raised;
    OTE_INFO("test level: %s (roster) -> Lv %d", r.name.c_str(), r.level);
  }
  return raised;
}

}  // namespace

bool beginPlaythroughAt(const Database& db, Playthrough& pt, const std::string& chapterId, const std::string& difficulty, std::string& note,
                        std::string& err) {
  if (!db.campaign) {
    err = "this game has no campaign";
    return false;
  }
  const int target = db.campaign->findChapter(chapterId);
  if (target < 0) {
    std::string ids;
    for (const ChapterDef& c : db.campaign->chapters) ids += (ids.empty() ? "" : ", ") + c.id;
    err = "no chapter \"" + chapterId + "\" in this campaign (it has: " + ids + ")";
    return false;
  }
  pt.begin(db);
  if (!difficulty.empty()) pt.difficulty = difficulty;
  // The main path: from chapter 1, each chapter's own "next" / "goto", until the target (or a loop / the end: then it is not on it).
  std::set<int> seen;
  int cur = 0;
  while (cur >= 0 && cur != target && !seen.count(cur)) {
    seen.insert(cur);
    cur = walkChapter(db, pt, cur, false);
  }
  if (cur != target) {  // not on the main path: walk the list order instead
    note = "chapter \"" + chapterId + "\" is not on the campaign's main path from chapter 1; the chapters before it in the list were used";
    pt.begin(db);
    if (!difficulty.empty()) pt.difficulty = difficulty;
    for (int i = 0; i < target; ++i) walkChapter(db, pt, i, true);
  }
  pt.chapter = target;
  pt.phase = Phase::Before;
  return true;
}

int levelRoster(const Database& db, Playthrough& pt, int level) {
  Battle calc;
  calc.db = &db;
  int raised = raiseRoster(db, calc, pt.roster, level);
  for (ParkedParty& p : pt.parked) raised += raiseRoster(db, calc, p.roster, level);
  return raised;
}

int applyChapterTestLevel(const Database& db, Playthrough& pt, Formulas& formulas, uint32_t seed, int option, std::string& err) {
  if (option == 0) return 0;
  int level = option;
  if (option < 0) {
    Battle probe;  // the chapter's map as it starts: only its hostile units are looked at
    if (!prepareChapterBattle(db, pt, formulas, seed, probe, err)) return 0;
    level = probe.autoTestLevel();
    if (level <= 0) {
      OTE_INFO("test level: automatic, but this chapter's map has no hostile computer units: nothing changed");
      return 0;
    }
  }
  levelRoster(db, pt, level);
  return level;
}

}  // namespace ote
