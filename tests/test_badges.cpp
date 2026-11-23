// Three small engine keys (docs/GRAPHICS.md "Unit badges", docs/BATTLE_SCENE.md "Quotes in the scene", docs/EVENTS.md "Battle and defeat quotes"):
//   data/graphics.json "badges", "battle" > "scene" > "quote", game.json "quotesPerFight"; and the timeline's defeat stop.
#include "posix_compat.h"

#include <cmath>
#include <cstdio>

#include "battle/fight_timeline.h"
#include "core/base/json.h"
#include "core/data/loader.h"
#include "quotes_fixture.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// A small game: unit types knight (tag "boss" when `bossTag`) and grunt, a character "hero" with the tag "hero", a 16 x 16 sheet "marks" of 2 x 1 cells of 8 pixels.
std::string makeGame(const char* name, const std::string& graphicsExtra, const std::string& gameKeys = std::string(), bool bossTag = true) {
  std::string root = ote_test::tempDir() + "/badges_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\"," + gameKeys + " \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}], \"rules\": {\"items\": {\"weapons\": \"carried\"}},"
                      " \"formulas\": {\"damage\":\"1\"}, \"maps\": [{\"id\":\"m\",\"name\":\"M\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      std::string("{ \"units\": [{\"id\":\"knight\",\"moveType\":\"foot\",") + (bossTag ? "\"tags\":[\"boss\"]," : "") +
                          "\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"sword\"]},"
                          "{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"sword\"]}] }"));
  CHECK(writeTextFile(root + "/data/characters.json", "{ \"characters\": [{\"id\":\"hero\",\"name\":\"Hero\",\"type\":\"grunt\",\"tags\":[\"star\"]}] }"));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"name\": \"M\", \"grid\": {\"type\":\"square\",\"width\":2,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"..\"],"
                      " \"sides\": [{\"id\":\"a\",\"controller\":\"human\"},{\"id\":\"b\",\"controller\":\"ai\"}],"
                      " \"units\": [{\"type\":\"knight\",\"side\":\"a\",\"x\":0,\"y\":0},{\"type\":\"grunt\",\"side\":\"b\",\"x\":1,\"y\":0}] }"));
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 16, 0, 0, 0, 8, 8, 6};
  FILE* f = std::fopen((root + "/art/marks.png").c_str(), "wb");
  CHECK(f != nullptr);
  if (f) {
    std::fwrite(b, 1, sizeof b, f);
    std::fclose(f);
  }
  CHECK(writeTextFile(root + "/data/graphics.json", "{ \"sheets\": [ {\"id\":\"marks\",\"file\":\"art/marks.png\",\"cellWidth\":8,\"cellHeight\":8} ]" + graphicsExtra + " }"));
  return root;
}

std::string loadError(const char* name, const std::string& graphicsExtra, const std::string& gameKeys = std::string(), bool bossTag = true) {
  Database db;
  std::string err;
  if (loadGame(makeGame(name, graphicsExtra, gameKeys, bossTag), db, err)) return "";
  return err;
}

}  // namespace

TEST(badges_load_with_every_key_and_their_defaults) {
  Database db;
  std::string err;
  const std::string root = makeGame("ok", ", \"badges\": [ { \"id\": \"boss\", \"sheet\": \"marks\", \"tag\": \"boss\" },"
                                          " { \"id\": \"star\", \"sheet\": \"marks\", \"cell\": [1, 0], \"tag\": \"star\", \"sides\": [\"a\", \"b\"], \"anchor\": \"topLeft\", \"offset\": [-2, 3] } ]");
  CHECK(loadGame(root, db, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK_EQ(db.graphics.badges.size(), (size_t)2);
  if (db.graphics.badges.size() != 2) return;
  const BadgeDef& a = db.graphics.badges[0];
  CHECK(a.id == "boss" && a.whole && a.tag == "boss" && a.sides == BadgeDef::Sides::All && a.anchor == BadgeDef::Anchor::BottomRight && a.dx == 0 && a.dy == 0);
  const BadgeDef& b = db.graphics.badges[1];
  CHECK(!b.whole && b.cell.col == 1 && b.cell.row == 0 && b.sides == BadgeDef::Sides::List && b.sideIds.size() == 2 && b.anchor == BadgeDef::Anchor::TopLeft && b.dx == -2 &&
        b.dy == 3);  // (a character's tag "star" is accepted as well as a unit type's "boss")
  for (const char* w : {"all", "computer", "player"}) {
    Database d2;
    CHECK(loadGame(makeGame((std::string("w_") + w).c_str(), std::string(", \"badges\": [ { \"id\": \"b\", \"sheet\": \"marks\", \"tag\": \"boss\", \"sides\": \"") + w + "\" } ]"), d2, err));
  }
  Database none;  // no key: no badges
  CHECK(loadGame(makeGame("none", ""), none, err));
  CHECK(none.graphics.badges.empty());
}

TEST(badges_loader_refusals) {
  const std::string head = ", \"badges\": [ { \"id\": \"b\", \"sheet\": \"marks\", \"tag\": \"boss\"";
  CHECK(has(loadError("r1", ", \"badges\": 5"), "\"badges\" must be a list"));
  CHECK(has(loadError("r2", ", \"badges\": [ 5 ]"), "badge 1: must be an object"));
  CHECK(has(loadError("r3", head + ", \"anchr\": 1 } ]"), "unknown key \"anchr\"") && has(loadError("r3b", head + ", \"anchr\": 1 } ]"), "\"anchor\""));
  CHECK(has(loadError("r4", ", \"badges\": [ { \"id\": \"b\", \"sheet\": \"nope\", \"tag\": \"boss\" } ]"), "unknown sheet \"nope\""));
  CHECK(has(loadError("r5", ", \"badges\": [ { \"id\": \"b\", \"sheet\": \"marks\" } ]"), "\"tag\""));
  CHECK(has(loadError("r6", ", \"badges\": [ { \"id\": \"b\", \"sheet\": \"marks\", \"tag\": \"ghost\" } ]"), "no unit type") &&
        has(loadError("r6b", ", \"badges\": [ { \"id\": \"b\", \"sheet\": \"marks\", \"tag\": \"ghost\" } ]"), "no character"));
  CHECK(has(loadError("r7", ", \"badges\": [ { \"id\": \"b\", \"sheet\": \"marks\", \"tag\": \"boss\" }, { \"id\": \"b\", \"sheet\": \"marks\", \"tag\": \"boss\" } ]"), "duplicate id"));
  CHECK(has(loadError("r8", ", \"badges\": [ { \"id\": \"a b\", \"sheet\": \"marks\", \"tag\": \"boss\" } ]"), "letters, digits"));
  CHECK(has(loadError("r9", head + ", \"cell\": [2, 0] } ]"), "outside sheet \"marks\" (2 x 2 cells)") || has(loadError("r9", head + ", \"cell\": [2, 0] } ]"), "outside sheet"));
  CHECK(has(loadError("r10", head + ", \"sides\": \"everyone\" } ]"), "\"sides\" must be \"all\", \"computer\", \"player\""));
  CHECK(has(loadError("r11", head + ", \"sides\": [] } ]"), "empty list"));
  CHECK(has(loadError("r12", head + ", \"anchor\": \"middle\" } ]"), "\"anchor\" must be"));
  CHECK(has(loadError("r13", head + ", \"offset\": [1] } ]"), "\"offset\" must be [x, y]"));
  CHECK(has(loadError("r14", head + ", \"offset\": [300, 0] } ]"), "-256 to 256"));
  CHECK(has(loadError("r15", head + " } ]", "", false), "no unit type"));  // the unit type lost its tag
}

TEST(scene_quote_timing_loads_and_is_refused_when_wrong) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("q0", ", \"battle\": { \"sets\": {}, \"scene\": { \"quote\": { \"leadMs\": 0, \"slideOutMs\": 500 } } }"), db, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  const QuoteTiming& q = db.graphics.battle.timing.quote;
  CHECK(q.leadMs == 0 && q.slideOutMs == 500 && q.slideInMs == 300);  // left out = the default
  Database d2;
  CHECK(loadGame(makeGame("q1", ", \"battle\": { \"sets\": {} }"), d2, err));
  CHECK(d2.graphics.battle.timing.quote.leadMs == 150 && d2.graphics.battle.timing.quote.slideOutMs == 300 && d2.graphics.battle.timing.quote.slideInMs == 300);
  auto bad = [&](const char* name, const char* quote) { return loadError(name, std::string(", \"battle\": { \"sets\": {}, \"scene\": { \"quote\": ") + quote + " } }"); };
  CHECK(has(bad("q2", "5"), "battle scene quote") && has(bad("q2b", "5"), "must be an object"));
  CHECK(has(bad("q3", "{ \"lead\": 5 }"), "unknown key \"lead\"") && has(bad("q3b", "{ \"lead\": 5 }"), "\"leadMs\""));
  CHECK(has(bad("q4", "{ \"slideInMs\": 5001 }"), "slideInMs"));
  CHECK(has(bad("q5", "{ \"slideOutMs\": -1 }"), "slideOutMs"));
  CHECK(has(bad("q6", "{ \"leadMs\": 1.5 }"), "leadMs"));
}

TEST(game_json_quotes_per_fight_loads_and_is_refused_when_wrong) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("p1", "", " \"quotesPerFight\": \"one\","), db, err) && db.quotesOnePerFight);
  Database d2;
  CHECK(loadGame(makeGame("p2", "", " \"quotesPerFight\": \"both\","), d2, err) && !d2.quotesOnePerFight);
  Database d3;
  CHECK(loadGame(makeGame("p3", ""), d3, err) && !d3.quotesOnePerFight);
  const std::string bad = loadError("p4", "", " \"quotesPerFight\": \"two\",");
  CHECK(has(bad, "\"quotesPerFight\" must be \"both\"") && has(bad, "\"one\""));
}

TEST(quotes_one_per_fight_only_the_first_speaker_and_the_other_keeps_its_quote) {
  using namespace quotestest;
  Q q("quotes_one");
  q.rig.db.quotesOnePerFight = true;
  CHECK(q.attack("eirik", "gorm"));
  CHECK(is(q.said(), {"Gorm: Eirik. At last."}));  // the attacked unit spoke; Eirik says nothing this time
  q.fresh("eirik");
  CHECK(q.attack("eirik", "gorm"));
  CHECK(is(q.said(), {"Gorm: Eirik. At last."}));  // Gorm's quote is "always": it speaks first again, Eirik's pair quote was never used up
  Q w("quotes_both");  // the default: both
  CHECK(w.attack("eirik", "gorm"));
  CHECK(is(w.said(), {"Gorm: Eirik. At last.", "Eirik: Gorm! Your raids end today."}));
}

TEST(quote_lines_carry_the_flags_the_scene_reads) {
  using namespace quotestest;
  Q q("quotes_flags", "ch1");
  q.unit("gorm").hp = 1;
  CHECK(q.attack("eirik", "gorm"));
  int battle = 0, defeat = 0, plain = 0;
  for (const Event& e : q.events) {
    if (e.type != EventType::Dialogue) continue;
    if (e.flags & kDialogueQuote) ++battle;
    else if (e.flags & kDialogueDefeat) ++defeat;
    else ++plain;
  }
  CHECK(battle == 2 && defeat >= 1 && plain == 0);
}

// ---- the timeline's defeat stop ----

namespace {

Event strikeEv(int a, int b, int flags, int damage) {
  Event e;
  e.type = EventType::Attack;
  e.a = a;
  e.b = b;
  e.d = damage;
  e.flags = flags;
  return e;
}
FightInfo infoOf() {
  FightInfo f;
  f.units[1] = {1, "Eirik", 3, 20, 24, 2};
  f.units[2] = {2, "Gorm", 5, 10, 30, 3};
  f.distance = 1;
  return f;
}
FighterTimes times() {
  FighterTimes t;
  t.has[BaAttack] = t.has[BaHurt] = t.has[BaDefeat] = true;
  t.total[BaAttack] = 430;
  t.hitAt[BaAttack] = 200;
  t.total[BaHurt] = 180;
  t.total[BaDefeat] = 500;
  for (int& v : t.spawnAt) v = -1;
  return t;
}
const FightBeat* beatOf(const FightTimeline& tl, BeatKind k) {
  for (const FightBeat& b : tl.beats)
    if (b.kind == k) return &b;
  return nullptr;
}

}  // namespace

TEST(timeline_defeat_stop_delays_the_fall_to_after_the_bar_has_emptied) {
  FightScript s = buildFightScript({strikeEv(1, 2, kHit | kKill, 10)}, infoOf());
  FighterTimes t[2] = {times(), times()};
  const FightTimeline plain = buildTimelineList(s, t, {});
  const FightTimeline stop = buildTimelineList(s, t, {}, -1, -1, 533, 0, HitHold(), true);
  CHECK(plain.defeatStopAt < 0);  // without quotes: nothing changes
  CHECK(stop.defeatStopAt > 0);
  const FightBeat* hit = beatOf(stop, BeatKind::Hit);
  CHECK(hit != nullptr);
  if (!hit) return;
  CHECK(stop.hp.size() == 1);
  if (stop.hp.empty()) return;
  CHECK(stop.defeatStopAt >= stop.hp[0].t1 - 1e-9);    // the bar has emptied
  CHECK(stop.defeatStopAt >= hit->t + 0.18 - 1e-9);    // and the hurt pose is over
  CHECK(stop.poses[1].size() == 2 && stop.poses[1][0].anim == BaHurt && stop.poses[1][1].anim == BaDefeat);
  if (stop.poses[1].size() == 2) CHECK(std::fabs(stop.poses[1][1].start - stop.defeatStopAt) < 1e-9);  // the fall starts at the stop
  const FightBeat* fell = beatOf(stop, BeatKind::Defeated);
  CHECK(fell && std::fabs(fell->t - (stop.defeatStopAt + 0.5)) < 1e-9);
  CHECK(stop.total > plain.total);  // the fall comes later
  // The plain timeline is exactly what it always was: the fall starts at the blow.
  const FightBeat* fell0 = beatOf(plain, BeatKind::Defeated);
  CHECK(plain.poses[1].size() == 1 && plain.poses[1][0].anim == BaDefeat && fell0 && std::fabs(fell0->t - (hit->t + 0.5)) < 1e-9);
  // A fight with no fall has no stop, whatever the flag says.
  FightScript s2 = buildFightScript({strikeEv(1, 2, kHit, 3)}, infoOf());
  const FightTimeline same = buildTimelineList(s2, t, {}, -1, -1, 533, 0, HitHold(), true);
  const FightTimeline base = buildTimelineList(s2, t, {});
  CHECK(same.defeatStopAt < 0 && same.total == base.total && same.beats.size() == base.beats.size() && same.poses[1].size() == base.poses[1].size());
}
