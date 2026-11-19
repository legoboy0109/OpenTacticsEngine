// Portrait facing, talk and blink frames (docs/GRAPHICS.md "Portraits"): the loader reads and refuses the keys, and portraitFrame() picks
// the frame to draw: talk frames cycle while the line is typed, a blink frame shows for a moment at times that depend only on the clock
// and the speaker (never random), and a portrait with neither is untouched.
#include "posix_compat.h"

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"
#include "ui/dialogue_portrait.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

std::string makeGame(const char* name, const std::string& portraits) {
  std::string root = ote_test::tempDir() + "/pframes_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}], \"formulas\": {\"damage\":\"1\"} }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"stick\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json", "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3}}] }"));
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 64, 0, 0, 0, 32, 8, 6};
  FILE* f = std::fopen((root + "/art/faces.png").c_str(), "wb");
  CHECK(f != nullptr);
  if (f) {
    std::fwrite(b, 1, sizeof b, f);
    std::fclose(f);
  }
  CHECK(writeTextFile(root + "/data/graphics.json",
                      "{ \"sheets\": [ {\"id\":\"faces\",\"file\":\"art/faces.png\",\"cellWidth\":16,\"cellHeight\":16} ], \"portraits\": " + portraits + " }"));
  return root;
}

std::string loadErr(const char* name, const std::string& portraits, Database& db) {
  std::string err;
  return loadGame(makeGame(name, portraits), db, err) ? std::string() : err;
}

}  // namespace

TEST(portrait_faces_talk_and_blink_load) {
  Database db;
  CHECK_STR(loadErr("ok", R"({ "eirik": {"sheet":"faces","cell":[0,0],"faces":"left","talk":[[1,0],[2,0]],"blink":[[3,0]]}, "lyra": {"sheet":"faces","cell":[0,1]} })", db), std::string());
  const Look* e = db.graphics.set(0)->portrait("eirik");
  CHECK(e != nullptr);
  if (e) {
    CHECK_EQ(e->faces, 1);
    CHECK_EQ(e->talk.size(), size_t(2));
    CHECK_EQ(e->blink.size(), size_t(1));
    CHECK(e->talk[1].col == 2 && e->blink[0].col == 3);
  }
  const Look* l = db.graphics.set(0)->portrait("lyra");
  CHECK(l != nullptr && l->faces == 0 && l->talk.empty() && l->blink.empty());  // a portrait that says nothing is as it was
}

TEST(portrait_keys_are_refused) {
  Database a, b, c, d, e;
  CHECK(has(loadErr("f1", R"({ "x": {"sheet":"faces","cell":[0,0],"faces":"up"} })", a), "\"faces\" must be \"left\" or \"right\""));
  CHECK(has(loadErr("f2", R"({ "x": {"sheet":"faces","cell":[0,0],"talk":[]} })", b), "\"talk\" must list between 1 and 8 cells"));
  CHECK(has(loadErr("f3", R"({ "x": {"sheet":"faces","cell":[0,0],"blink":[[9,9]]} })", c), "is outside sheet"));
  CHECK(has(loadErr("f4", R"({ "x": {"sheet":"faces","cell":[0,0],"talk":[[0,0],[1,0],[2,0],[3,0],[0,1],[1,1],[2,1],[3,1],[0,0]]} })", d), "between 1 and 8"));
  CHECK(has(loadErr("f5", R"({ "x": {"sheet":"faces","cell":[0,0],"blink":[1,0]} })", e), "must be [column, row]"));
}

TEST(portrait_frame_picks_talk_and_blink_frames) {
  PortraitArt art;
  art.tex = 1;
  art.sw = art.sh = 16;
  art.sx = 0;
  art.sy = 0;
  // A portrait with neither is returned as it is.
  PortraitArt plain = portraitFrame(art, true, 3.0, "eirik");
  CHECK(plain.sx == 0 && plain.sy == 0);
  art.talk = {{16, 0}, {32, 0}};
  art.blink = {{48, 0}};
  // Talking: the talk frames cycle (140 ms each); not talking: the base frame (outside a blink).
  CHECK_EQ(portraitFrame(art, true, 0.00, "eirik").sx, 16);
  CHECK_EQ(portraitFrame(art, true, 0.15, "eirik").sx, 32);
  CHECK_EQ(portraitFrame(art, true, 0.29, "eirik").sx, 16);
  CHECK_EQ(portraitFrame(art, false, 0.0, "eirik").sx, 0);
  // Blinks: never in the first 2.8 s, always the same moments for the same speaker, a few seconds apart, 130 ms long.
  int blinking = 0, first = -1, last = -1, count = 0;
  bool was = false;
  for (int ms = 0; ms < 30000; ++ms) {
    const bool b = portraitFrame(art, false, ms / 1000.0, "eirik").sx == 48;
    blinking += b ? 1 : 0;
    if (b && !was) {
      ++count;
      if (first < 0) first = ms;
      last = ms;
    }
    was = b;
  }
  CHECK(first >= 2800 - 140);
  CHECK(count >= 5 && count <= 11);  // between 2.8 and 5.4 s apart
  CHECK(blinking >= count * 125 && blinking <= count * 135);
  int again = 0;
  for (int ms = 0; ms < 30000; ++ms) again += portraitFrame(art, false, ms / 1000.0, "eirik").sx == 48 ? 1 : 0;
  CHECK_EQ(again, blinking);  // deterministic
  int other = 0, differs = 0;
  for (int ms = 0; ms < 30000; ++ms) {
    const bool a = portraitFrame(art, false, ms / 1000.0, "eirik").sx == 48, b = portraitFrame(art, false, ms / 1000.0, "gorm").sx == 48;
    other += b ? 1 : 0;
    differs += a != b ? 1 : 0;
  }
  CHECK(other > 0 && differs > 0);  // another speaker blinks at other moments
  (void)last;
  // While talking the blink does not interrupt the mouth.
  CHECK(portraitFrame(art, true, 3.0, "eirik").sx != 48);
}
