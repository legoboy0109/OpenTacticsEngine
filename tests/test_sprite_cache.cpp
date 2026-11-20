// The lazily filled sheet cache (battle/sprites.h, sprite_cache.cpp): loading decodes nothing, a texture is made on first
// use, the mirrored copy only when asked for, least recently used sheets go when over budget (never one used this frame),
// and a sheet that cannot be decoded reports once and then behaves as missing.
#include <cstdio>
#include <cstring>
#include <map>

#include "battle/sprites.h"
#include "core/base/log.h"
#include "test.h"

using namespace ote;

namespace {

// ---- a PNG writer (stored deflate blocks), for sheets whose pixels the test knows ----
uint32_t crc32(const uint8_t* d, size_t n, uint32_t crc = 0xffffffffu) {
  for (size_t i = 0; i < n; ++i) {
    crc ^= d[i];
    for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
  }
  return crc;
}
void be32(std::vector<uint8_t>& v, uint32_t x) {
  for (int s = 24; s >= 0; s -= 8) v.push_back((uint8_t)(x >> s));
}
void chunk(std::vector<uint8_t>& out, const char* kind, const std::vector<uint8_t>& data) {
  be32(out, (uint32_t)data.size());
  std::vector<uint8_t> body(kind, kind + 4);
  body.insert(body.end(), data.begin(), data.end());
  out.insert(out.end(), body.begin(), body.end());
  be32(out, ~crc32(body.data(), body.size()));
}
// Pixel (x, y) of a test sheet: red = x, green = y, blue = sheet tag, opaque.
void pixelOf(int x, int y, int tag, uint8_t* p) {
  p[0] = (uint8_t)x;
  p[1] = (uint8_t)y;
  p[2] = (uint8_t)tag;
  p[3] = 255;
}
std::vector<uint8_t> makePng(int w, int h, int tag) {
  std::vector<uint8_t> raw;
  for (int y = 0; y < h; ++y) {
    raw.push_back(0);  // filter: none
    for (int x = 0; x < w; ++x) {
      uint8_t p[4];
      pixelOf(x, y, tag, p);
      raw.insert(raw.end(), p, p + 4);
    }
  }
  std::vector<uint8_t> z = {0x78, 0x01};
  size_t pos = 0;
  while (pos < raw.size() || pos == 0) {
    const size_t n = std::min<size_t>(65535, raw.size() - pos);
    z.push_back(pos + n >= raw.size() ? 1 : 0);
    z.push_back((uint8_t)(n & 0xff));
    z.push_back((uint8_t)(n >> 8));
    z.push_back((uint8_t)(~n & 0xff));
    z.push_back((uint8_t)((~n >> 8) & 0xff));
    z.insert(z.end(), raw.begin() + (long)pos, raw.begin() + (long)(pos + n));
    pos += n;
    if (n == 0) break;
  }
  uint32_t a = 1, b = 0;
  for (uint8_t c : raw) {
    a = (a + c) % 65521u;
    b = (b + a) % 65521u;
  }
  be32(z, (b << 16) | a);
  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  std::vector<uint8_t> ihdr;
  be32(ihdr, (uint32_t)w);
  be32(ihdr, (uint32_t)h);
  ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", z);
  chunk(png, "IEND", {});
  return png;
}
void writeFile(const std::string& path, const std::vector<uint8_t>& bytes) {
  FILE* f = std::fopen(path.c_str(), "wb");
  if (!f) return;
  std::fwrite(bytes.data(), 1, bytes.size(), f);
  std::fclose(f);
}

// ---- a renderer that only counts textures ----
class CountingRenderer : public Renderer {
 public:
  struct Tex { int w, h; std::vector<uint8_t> rgba; };
  std::map<TextureId, Tex> live;
  int created = 0, destroyed = 0;
  TextureId next = 1;
  int width() const override { return 480; }
  int height() const override { return 270; }
  void displaySize(int& w, int& h) const override { w = 480; h = 270; }
  void clear(Color) override {}
  void fillRect(int, int, int, int, Color) override {}
  void drawRect(int, int, int, int, Color) override {}
  void setLogicalSize(int, int) override {}
  bool setFont(const FontRequest&) override { return true; }
  void drawText(int, int, const char*, Color, int) override {}
  int textWidth(const char*, int) const override { return 0; }
  int lineHeight(int) const override { return 8; }
  TextureId createTexture(const uint8_t* rgba, int w, int h, bool) override {
    ++created;
    live[next] = Tex{w, h, std::vector<uint8_t>(rgba, rgba + (size_t)w * h * 4)};
    return next++;
  }
  void destroyTexture(TextureId id) override {
    ++destroyed;
    live.erase(id);
  }
  void updateTexture(TextureId, const uint8_t*) override {}
  void drawTexture(TextureId, int, int, int, int, int, int, int, int, Color) override {}
};

// Counts the error lines of a log (the sink is a plain function pointer).
int gErrors = 0, gInfos = 0;
std::string gLastError;
void sink(LogLevel l, const char* m) {
  if (l == LogLevel::Error) {
    ++gErrors;
    gLastError = m;
  }
  if (l == LogLevel::Info) ++gInfos;
}

SheetDef sheetDef(const std::string& id, int w, int h, int cell) {
  SheetDef s;
  s.id = id;
  s.file = id + ".png";
  s.cellW = s.cellH = cell;
  s.width = w;
  s.height = h;
  s.cols = w / cell;
  s.rows = h / cell;
  return s;
}

// A folder with `n` sheets "s0".."s<n-1>" (each 32x16, 16-pixel cells, tag = index) and the Graphics naming them.
struct Game {
  std::string dir;
  Graphics g;
  explicit Game(int n, const char* name) {
    dir = ote_test::tempDir() + "/" + name;
    makeDirForTest(dir);
    for (int i = 0; i < n; ++i) {
      writeFile(dir + "/s" + std::to_string(i) + ".png", makePng(32, 16, i));
      g.sheets.push_back(sheetDef("s" + std::to_string(i), 32, 16, 16));
    }
  }
  ~Game() {
    for (size_t i = 0; i < g.sheets.size(); ++i) std::remove((dir + "/" + g.sheets[i].file).c_str());
  }
  static void makeDirForTest(const std::string& d) { makeDir(d); }
};

const uint64_t kSheetBytes = 32 * 16 * 4;  // 2048

}  // namespace

TEST(sprite_cache_load_decodes_nothing_and_makes_textures_on_first_use) {
  setLogSink(sink);
  gErrors = 0;
  Game game(3, "sc_lazy");
  CountingRenderer r;
  Sprites sp;
  sp.load(r, game.dir, game.g);
  CHECK(sp.any());
  CHECK_EQ(r.created, 0);  // loading only checked the headers
  CHECK_EQ(sp.bytesHeld(), 0u);
  const TextureId t1 = sp.texture(1);
  CHECK(t1 != 0);
  CHECK_EQ(r.created, 1);
  CHECK_EQ(sp.texture(1), t1);  // the same texture again, nothing new made
  CHECK_EQ(r.created, 1);
  CHECK_EQ(sp.bytesHeld(), kSheetBytes);
  CHECK(r.live[t1].rgba[2] == 1);  // it is sheet 1's picture
  CHECK_EQ(sp.texture(-1), 0);
  CHECK_EQ(sp.texture(9), 0);
  CHECK_EQ(sp.mirrored(1), 0);  // no battle set uses it mirrored: never any
  CHECK_EQ(sp.pixels(1) == nullptr, true);  // and no tileset: the pixels are not kept
  CHECK_EQ(gErrors, 0);
  sp.release(r);
  CHECK(!sp.any());
  CHECK(r.live.empty());  // everything went with release
  setLogSink(nullptr);
}

TEST(sprite_cache_mirrored_copy_only_when_asked_and_pixels_only_for_tilesets) {
  Game game(3, "sc_mirror");
  BattleSet set;
  set.anims[0].sheet = 0;
  game.g.battle.sets.push_back(set);
  TilesetDef ts;
  ts.sheet = 2;
  game.g.tilesets.push_back(ts);
  CountingRenderer r;
  Sprites sp;
  sp.load(r, game.dir, game.g);
  const TextureId t = sp.texture(0);
  CHECK(t != 0);
  CHECK_EQ(r.created, 1);  // the mirrored copy is not made with the texture
  CHECK_EQ(sp.bytesHeld(), kSheetBytes);
  const TextureId m = sp.mirrored(0);
  CHECK(m != 0);
  CHECK(m != t);
  CHECK_EQ(r.created, 2);
  CHECK_EQ(sp.bytesHeld(), 2 * kSheetBytes);
  // Whole cells with every row reversed: pixel (x, y) of the copy is pixel (31 - x, y) of the sheet.
  const std::vector<uint8_t>& px = r.live[m].rgba;
  CHECK_EQ(r.live[m].w, 32);
  CHECK_EQ((int)px[(size_t)(3 * 4)], 28);          // x = 3 holds the colour of x = 28
  CHECK_EQ((int)px[(size_t)((5 * 32 + 0) * 4 + 1)], 5);  // rows keep their order
  CHECK_EQ(sp.mirrored(0), m);
  CHECK_EQ(r.created, 2);
  // A mirrored copy asked for first makes both with one go.
  CHECK_EQ(sp.mirrored(1), 0);  // (sheet 1 is not a mirrored one)
  // The tileset's sheet keeps its pixels, counted in the budget.
  const Image* img = sp.pixels(2);
  CHECK(img != nullptr);
  if (img) {
    CHECK_EQ(img->width, 32);
    CHECK_EQ((int)img->rgba[2], 2);
  }
  CHECK_EQ(sp.bytesHeld(), 2 * kSheetBytes + 2 * kSheetBytes);  // 0 + its copy, 2 + its pixels
  sp.release(r);
}

TEST(sprite_cache_evicts_the_least_recently_used_but_never_this_frames) {
  Game game(4, "sc_evict");
  CountingRenderer r;
  Sprites sp;
  sp.load(r, game.dir, game.g);
  sp.setBudgetBytes(2 * kSheetBytes + 100);  // room for two sheets
  const TextureId a = sp.texture(0);
  const TextureId b = sp.texture(1);
  const TextureId c = sp.texture(2);  // three in one frame: over budget, but all in use: nothing may go
  CHECK(a && b && c);
  CHECK_EQ(r.destroyed, 0);
  CHECK_EQ(r.live.size(), 3u);
  sp.beginFrame();
  sp.texture(1);  // 1 is the newest now, then 2, 0 is the oldest
  const TextureId d = sp.texture(3);  // makes room: 0 and 2 go (3 + 1 used this frame stay)
  CHECK(d != 0);
  CHECK_EQ(r.destroyed, 2);
  CHECK_EQ(r.live.count(a), 0u);
  CHECK_EQ(r.live.count(c), 0u);
  CHECK_EQ(r.live.count(b), 1u);
  CHECK_EQ(sp.bytesHeld(), 2 * kSheetBytes);
  // An evicted sheet comes back on demand, same picture.
  sp.beginFrame();
  const TextureId a2 = sp.texture(0);
  CHECK(a2 != 0);
  CHECK_EQ((int)r.live[a2].rgba[2], 0);
  CHECK_EQ(r.live.count(b), 0u);  // (1 was the oldest of the two that were held)
  // Low memory: everything not used this frame goes.
  sp.trimUnused();
  CHECK_EQ(r.live.size(), 0u);
  CHECK_EQ(sp.bytesHeld(), 0u);
  CHECK(sp.texture(3) != 0);
  sp.release(r);
  CHECK(r.live.empty());
}

TEST(sprite_cache_a_sheet_that_cannot_be_decoded_reports_once) {
  setLogSink(sink);
  gErrors = 0;
  Game game(2, "sc_bad");
  // Sheet 0: a good header, then the file ends (a damaged file): the header check passes, the decode fails.
  std::vector<uint8_t> png = makePng(32, 16, 0);
  png.resize(40);
  writeFile(game.dir + "/s0.png", png);
  // Sheet 1: not a PNG at all: reported at load.
  writeFile(game.dir + "/s1.png", std::vector<uint8_t>{'n', 'o', 'p', 'e', ' ', 'p', 'n', 'g', '!', '!'});
  CountingRenderer r;
  Sprites sp;
  sp.load(r, game.dir, game.g);
  CHECK_EQ(gErrors, 1);  // sheet 1, at load; the damaged one is not found out before its pixels are asked for
  CHECK(gLastError.find("not a PNG") != std::string::npos);
  for (int i = 0; i < 5; ++i) CHECK_EQ(sp.texture(0), 0);
  CHECK_EQ(gErrors, 2);  // once, not every frame
  CHECK(gLastError.find("\"s0\"") != std::string::npos);
  CHECK(gLastError.find("placeholder") != std::string::npos);
  for (int i = 0; i < 5; ++i) CHECK_EQ(sp.texture(1), 0);
  CHECK_EQ(gErrors, 2);
  CHECK_EQ(r.created, 0);
  sp.release(r);
  setLogSink(nullptr);
}
