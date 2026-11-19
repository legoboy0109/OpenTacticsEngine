// The player's save-file module (engine/player/saves.*): paths, atomic
// writes, listing with reasons, reading, deleting.
#include "posix_compat.h"

#include <cstdio>

#include "player/saves.h"
#include "save_helpers.h"

using namespace ote;
using namespace savetest;

namespace {

struct Fixture {
  Rig rig;
  std::string folder;
  SaveStore store;
  Json file;

  bool make(const char* name) {
    if (!rig.loadDemo("demo-srpg", "bridge", 5)) return false;
    rig.start();
    std::vector<Event> ev;
    playAi(rig.battle, 3, ev);
    tempFolder("save_files_test");
    folder = tempFolder(std::string("save_files_test/") + name);
    store = SaveStore(folder);
    for (int s = 0; s <= SaveStore::kSlots; ++s) {
      std::string err;
      store.remove(s, err);
    }
    std::string err;
    bool ok = buildSaveFile(rig.battle, rig.db, "demo-srpg", "bridge", 1794651000, file, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    return ok;
  }
  bool exists(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f) std::fclose(f);
    return f != nullptr;
  }
};

}  // namespace

TEST(save_files_write_inspect_read_and_delete) {
  Fixture f;
  CHECK(f.make("basic"));
  CHECK_STR(f.store.path(0), f.folder + "/continue.json");
  CHECK_STR(f.store.path(3), f.folder + "/slot3.json");
  CHECK(f.store.inspect(0, &f.rig.db, "demo-srpg").state == SaveInfo::Empty);

  std::string err;
  CHECK(f.store.write(0, f.file, err));
  CHECK(f.store.write(2, f.file, err));
  CHECK(!f.exists(f.store.path(0) + ".tmp"));  // the temp file is gone after the rename
  SaveInfo info = f.store.inspect(2, &f.rig.db, "demo-srpg");
  CHECK(info.state == SaveInfo::Ok);
  CHECK_STR(info.mapId, "bridge");
  CHECK(!info.mapName.empty());
  CHECK_EQ(info.round, f.rig.battle.round);
  CHECK(info.savedAt == 1794651000);
  CHECK(f.store.inspect(1, &f.rig.db, "demo-srpg").state == SaveInfo::Empty);

  SaveFile read;
  CHECK(f.store.read(2, f.rig.db, "demo-srpg", read, err));
  Rig fresh;
  CHECK(fresh.loadDemo("demo-srpg", read.mapId.c_str(), 1));
  CHECK(restoreBattleState(fresh.battle, read.battle, err));
  CHECK_STR(stateText(fresh.battle), stateText(f.rig.battle));
  CHECK(!f.store.read(1, f.rig.db, "demo-srpg", read, err));  // empty slot: a reason, not a crash
  CHECK(!err.empty());

  // Writing over an existing save replaces it completely.
  Json other = f.file;
  other.set("round", Json::makeNumber(42));
  CHECK(f.store.write(2, other, err));
  CHECK_EQ(f.store.inspect(2, &f.rig.db, "demo-srpg").round, 42);

  CHECK(f.store.remove(2, err));
  CHECK(!f.exists(f.store.path(2)));
  CHECK(f.store.remove(2, err));  // deleting what is not there is fine
  CHECK(f.exists(f.store.path(0)));
  CHECK(f.store.remove(0, err));
}

TEST(save_files_unusable_saves_are_listed_with_a_reason_and_kept) {
  Fixture f;
  CHECK(f.make("unusable"));
  const std::string game = "demo-srpg";
  std::string err;
  auto put = [&](int slot, const Json& j) { CHECK(f.store.write(slot, j, err)); };
  auto with = [&](const char* key, const Json& v) { Json j = f.file; j.set(key, v); return j; };

  put(1, f.file);                                             // good
  put(2, with("saveVersion", Json::makeNumber(99)));          // newer format
  put(3, with("dataHash", Json::makeString("0123456789abcdef")));  // game changed
  put(4, with("map", Json::makeString("long_gone")));         // map removed
  put(5, with("game", Json::makeString("another-game")));     // copied from elsewhere
  CHECK(writeTextFile(f.store.path(0), "{ this is not json"));  // damaged continue

  SaveInfo i1 = f.store.inspect(1, &f.rig.db, game);
  CHECK(i1.state == SaveInfo::Ok);
  SaveInfo i2 = f.store.inspect(2, &f.rig.db, game);
  CHECK(i2.state == SaveInfo::Unusable);
  CHECK(i2.reason.find("different version") != std::string::npos);
  SaveInfo i3 = f.store.inspect(3, &f.rig.db, game);
  CHECK(i3.state == SaveInfo::Unusable);
  CHECK(i3.reason.find("different version") != std::string::npos);
  CHECK_STR(i3.mapName, f.rig.db.maps[(size_t)f.rig.db.findMap("bridge")].name);  // still shows what it was
  SaveInfo i4 = f.store.inspect(4, &f.rig.db, game);
  CHECK(i4.state == SaveInfo::Unusable);
  CHECK(i4.reason.find("map") != std::string::npos);
  SaveInfo i5 = f.store.inspect(5, &f.rig.db, game);
  CHECK(i5.state == SaveInfo::Unusable);
  CHECK(i5.reason.find("another game") != std::string::npos);
  SaveInfo i0 = f.store.inspect(0, &f.rig.db, game);
  CHECK(i0.state == SaveInfo::Unusable);
  CHECK(i0.reason.find("damaged") != std::string::npos);

  // Reading reports the same reason, and nothing is ever deleted.
  SaveFile sf;
  for (int slot : {0, 2, 3, 4, 5}) {
    CHECK(!f.store.read(slot, f.rig.db, game, sf, err));
    CHECK(!err.empty());
    CHECK(f.exists(f.store.path(slot)));
  }
  CHECK(f.store.read(1, f.rig.db, game, sf, err));

  // Without the loaded game only the file itself is judged (the launcher's quick look).
  CHECK(f.store.inspect(3, nullptr, game).state == SaveInfo::Ok);
  CHECK(f.store.inspect(2, nullptr, game).state == SaveInfo::Unusable);
}

TEST(save_files_cannot_be_written_into_a_blocked_place) {
  Fixture f;
  CHECK(f.make("blocked"));
  // A plain file where the saves folder should be.
  std::string blocker = f.folder + "/not_a_folder";
  CHECK(writeTextFile(blocker, "x"));
  SaveStore store(blocker + "/game");
  std::string err;
  CHECK(!store.write(0, f.file, err));
  CHECK(!err.empty());
  CHECK(store.inspect(0, &f.rig.db, "demo-srpg").state == SaveInfo::Empty);
  removeFile(blocker);

  // A failed write leaves the previous save untouched.
  CHECK(f.store.write(1, f.file, err));
  std::string before;
  CHECK(readTextFile(f.store.path(1), before));
  std::string tmp = f.store.path(1) + ".tmp";
  CHECK(makeDir(tmp));  // a folder named like the temp file: opening it for writing fails
  CHECK(!f.store.write(1, f.file, err));
  std::string after;
  CHECK(readTextFile(f.store.path(1), after));
  CHECK_STR(before, after);
  rmdir(tmp.c_str());

  // An unreadable file is listed as such (skipped when running as a user that ignores permissions).
  CHECK(f.store.write(2, f.file, err));
  chmod(f.store.path(2).c_str(), 0);
  std::FILE* probe = std::fopen(f.store.path(2).c_str(), "rb");
  if (probe) std::fclose(probe);
  else {
    CHECK(f.store.inspect(2, &f.rig.db, "demo-srpg").state == SaveInfo::Unusable);
    CHECK(f.store.exists(2));  // listed, not deleted
  }
  chmod(f.store.path(2).c_str(), 0600);
}

TEST(save_files_disabled_store_does_nothing) {
  SaveStore off;
  CHECK(!off.enabled());
  Json j = Json::makeObject();
  std::string err;
  CHECK(!off.write(0, j, err));
  CHECK(off.remove(0, err));
  CHECK(!off.exists(0));
  CHECK(off.inspect(1, nullptr, "x").state == SaveInfo::Empty);
}

TEST(save_files_time_is_shown_as_local_date_and_time) {
  std::string t = formatSavedAt(1794651000);
  CHECK_EQ((int)t.size(), 16);  // "YYYY-MM-DD HH:MM"
  CHECK(t[4] == '-' && t[7] == '-' && t[10] == ' ' && t[13] == ':');
  CHECK(formatSavedAt(0).empty());
  CHECK(nowSeconds() > 1700000000);
}
