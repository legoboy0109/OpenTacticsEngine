// Original-Xbox (nxdk) feasibility test: "hello screen" for the OpenTacticsEngine rules core.
// Not a port. It loads games/demo-srpg through the core's loader (from the disc: D:\games\demo-srpg), plays one whole
// AI-vs-AI battle like ote-sim does, measures time and memory, then draws the final map and the numbers straight into
// the NV2A framebuffer at 640x480 and again at 1280x720 (nxdk XVideoSetMode), timing the frames.
// Everything measured is also written with DbgPrint (debugger / serial) and shown on screen.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <windows.h>

#include "core/ai/ai.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "platform/common/bitmap_font.h"
#include "bench3d.h"
#include "battle_hash.h"

using namespace ote;

namespace {

double nowMs() {
  static const double freq = (double)KeQueryPerformanceFrequency();
  return (double)KeQueryPerformanceCounter() * 1000.0 / freq;
}

// Bytes of RAM the kernel says are in use (total physical pages minus available), as the test's "memory used".
long usedKb() {
  MM_STATISTICS st;
  std::memset(&st, 0, sizeof st);
  st.Length = sizeof st;
  if (MmQueryStatistics(&st) != 0) return -1;
  return (long)(((unsigned long long)(st.TotalPhysicalPages - st.AvailablePages) * 4096ull) / 1024ull);
}

// ---- tiny framebuffer drawing (32 bpp, write straight into the scan-out surface) ----
struct Fb {
  unsigned char* base = nullptr;
  int w = 0, h = 0, pitch = 0;
  void fill(int x, int y, int rw, int rh, unsigned rgb) const {
    if (x < 0) { rw += x; x = 0; }
    if (y < 0) { rh += y; y = 0; }
    if (x + rw > w) rw = w - x;
    if (y + rh > h) rh = h - y;
    for (int j = 0; j < rh; ++j) {
      unsigned* p = (unsigned*)(base + (size_t)(y + j) * pitch) + x;
      for (int i = 0; i < rw; ++i) p[i] = rgb;
    }
  }
  void text(int x, int y, const char* s, int scale, unsigned rgb) const {
    for (; *s; ++s, x += 8 * scale) {
      const unsigned char* g = bitmapGlyph(*s);
      for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
          if (g[r] & (1 << c)) fill(x + c * scale, y + r * scale, scale, scale, rgb);
    }
  }
};

bool setMode(Fb& fb, int w, int h) {
  if (!XVideoSetMode(w, h, 32, REFRESH_DEFAULT)) return false;
  VIDEO_MODE m = XVideoGetMode();
  fb.base = XVideoGetFB();
  fb.w = m.width;
  fb.h = m.height;
  fb.pitch = m.width * 4;
  return true;
}

// nxdk's printf has no floating-point support (prints nothing for %f), so format numbers by hand: "123.45".
const char* F(double v, int dec = 2) {
  static char ring[8][32];
  static int n = 0;
  char* b = ring[n++ & 7];
  long scale = dec == 0 ? 1 : (dec == 1 ? 10 : (dec == 2 ? 100 : 1000));
  long long x = (long long)(v * scale + 0.5);
  if (dec == 0) std::snprintf(b, 32, "%lld", x);
  else std::snprintf(b, 32, "%lld.%0*lld", x / scale, dec, x % scale);
  return b;
}

// Progress lines on screen (the only output channel in xemu: retail-kernel DbgPrint goes nowhere there).
std::vector<std::string> g_stages;
void drawStages(const Fb& fb) {
  fb.fill(0, fb.h - 10 * (int)g_stages.size() - 8, fb.w, 10 * (int)g_stages.size() + 8, 0x101830);
  for (size_t i = 0; i < g_stages.size(); ++i) fb.text(8, fb.h - 10 * (int)g_stages.size() - 4 + 10 * (int)i, g_stages[i].c_str(), 1, 0xffff00);
}
void stage(const Fb& fb, const char* msg) {
  g_stages.push_back(msg);
  drawStages(fb);
}

struct Result {
  bool loaded = false, finished = false;
  std::string error;
  long kbStart = 0, kbAfterLoad = 0, kbPeakBattle = 0, kbEnd = 0;
  double loadMs = 0, battleMs = 0, worstStepMs = 0;
  int steps = 0, rounds = 0;
  std::string winner;
};

}  // namespace

int main(void) {
  Fb fb;
  Result res;
  res.kbStart = usedKb();
  if (!setMode(fb, 640, 480)) {
    DbgPrint("OTE: XVideoSetMode 640x480 failed\n");
    return 1;
  }
  fb.fill(0, 0, fb.w, fb.h, 0x101830);
  fb.text(16, 16, "OpenTacticsEngine on original Xbox: loading demo-srpg...", 2, 0xffffff);
  DbgPrint("OTE: start used=%ld KB\n", res.kbStart);

  Database db;
  MapDef map;
  Formulas formulas;
  Battle battle;
  std::vector<Event> events;
  std::string err;
  double t0 = nowMs();
  stage(fb, "loading game");
  if (!loadGame("D:\\games\\demo-srpg", db, err) || !loadMap(db, "bridge", map, err) || !formulas.init(db, err)) {
    res.error = "load: " + err;
  } else {
    res.loaded = true;
    res.loadMs = nowMs() - t0;
    res.kbAfterLoad = usedKb();
    stage(fb, "loaded; battle starts");
    DbgPrint("OTE: loaded in %.1f ms, used=%ld KB\n", res.loadMs, res.kbAfterLoad);
    if (!battle.init(db, map, formulas, 1, err)) {
      res.error = "init: " + err;
    } else {
      battle.start(events);
      res.kbPeakBattle = usedKb();
      t0 = nowMs();
      while (!battle.over && battle.round <= 60) {
        long guard = 10L * (long)battle.units.size() + 100, n = 0;
        for (;;) {
          double s0 = nowMs();
          bool more = aiTakeStep(battle, events);
          double s = nowMs() - s0;
          if (s > res.worstStepMs) res.worstStepMs = s;
          ++res.steps;
          if (!more || ++n > guard) break;
        }
        long kb = usedKb();
        if (kb > res.kbPeakBattle) res.kbPeakBattle = kb;
        events.clear();
      }
      res.battleMs = nowMs() - t0;
      res.rounds = battle.round;
      res.finished = battle.over;
      res.kbEnd = usedKb();
      for (size_t s = 0; s < battle.sides.size(); ++s)
        if (battle.over && battle.sides[s].team == battle.winnerTeam && !battle.sides[s].defeated)
          res.winner += (res.winner.empty() ? "" : "+") + battle.sides[s].name;
      DbgPrint("OTE: battle %s rounds=%d steps=%d total=%.1f ms worstStep=%.2f ms peak=%ld KB\n",
               res.finished ? "over" : "NOT over", res.rounds, res.steps, res.battleMs, res.worstStepMs,
               res.kbPeakBattle);
    }
  }
  stage(fb, res.error.empty() ? "battle done" : res.error.c_str());
  if (!res.error.empty()) {
    // File-access diagnosis (shown on screen): which of the usual ways to open a disc file work?
    char b[160];
    const char* paths[] = {"D:\\games\\demo-srpg\\game.json", "D:\\default.xbe", "D:\\games\\demo-srpg\\maps\\bridge.json"};
    for (const char* p : paths) {
      std::FILE* f = std::fopen(p, "rb");
      std::snprintf(b, sizeof b, "fopen %s -> %s", p, f ? "OK" : "FAIL");
      if (f) {
        char rb[4096];
        size_t total = 0, n, calls = 0;
        while ((n = std::fread(rb, 1, sizeof rb, f)) > 0) { total += n; ++calls; }
        std::snprintf(b + std::strlen(b), sizeof b - std::strlen(b), " read %lu B in %lu calls ferror=%d feof=%d", (unsigned long)total,
                      (unsigned long)calls, std::ferror(f), std::feof(f));
        std::fclose(f);
      }
      stage(fb, b);
      HANDLE h = CreateFileA(p, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
      std::snprintf(b, sizeof b, "  CreateFile -> %s err=%lu", h != INVALID_HANDLE_VALUE ? "OK" : "FAIL", (unsigned long)GetLastError());
      if (h != INVALID_HANDLE_VALUE) {
        static char rbuf[8192];
        DWORD sz = GetFileSize(h, NULL), got = 0;
        BOOL ok1 = ReadFile(h, rbuf, 4096, &got, NULL);
        DWORD e1 = GetLastError(), got1 = got;
        SetFilePointer(h, 0, NULL, FILE_BEGIN);
        got = 0;
        BOOL ok2 = ReadFile(h, rbuf, sz, &got, NULL);
        std::snprintf(b, sizeof b, "  sz %lu R4096 ok=%d got=%lu e=%lu Rsz ok=%d got=%lu", (unsigned long)sz,
                      (int)ok1, (unsigned long)got1, (unsigned long)e1, (int)ok2, (unsigned long)got);
        stage(fb, b);
        CloseHandle(h);
      }
      stage(fb, b);
    }
  }
  if (!res.error.empty()) DbgPrint("OTE: ERROR %s\n", res.error.c_str());

  // Result summary comparable with `ote-sim <game> bridge --seed 1`: winner, rounds, survivors, plus an FNV-1a hash of the final
  // battle state (see battle_hash.h; tools/xbox-og/battle_host.cpp prints the same hash on Linux).
  std::string summary = "no battle";
  if (res.loaded && res.error.empty()) {
    char sb[200];
    std::string surv;
    for (size_t s = 0; s < battle.sides.size(); ++s) {
      int alive = 0;
      for (const Unit& u : battle.units)
        if (battle.exists(u) && u.side == (int)s) ++alive;
      surv += (surv.empty() ? "" : ", ") + battle.sides[s].name + "=" + std::to_string(alive);
    }
    std::snprintf(sb, sizeof sb, "winner %s rounds %d surv %s hash %08x", res.winner.c_str(), res.rounds, surv.c_str(),
                  battleHash(battle));
    summary = sb;
    DbgPrint("OTE: result %s\n", summary.c_str());
  }

  // Draw the map and the numbers in each resolution; time the frames. Text lines (all of them, wrapped) sit on top, the map below.
  static const int modes[2][2] = {{640, 480}, {1280, 720}};
  static std::string modeLine[2];
  {  // Probe: is 1280x720 accepted by the video code at all? Report it from the 480p screen afterwards.
    char pb[120];
    const bool ok = XVideoSetMode(1280, 720, 32, REFRESH_DEFAULT);
    if (ok) {
      VIDEO_MODE m = XVideoGetMode();
      std::snprintf(pb, sizeof pb, "720p probe: setMode OK, mode %dx%d refresh %d", m.width, m.height, m.refresh);
      fb.base = XVideoGetFB();
      fb.w = m.width; fb.h = m.height; fb.pitch = m.width * 4;
      fb.fill(0, 0, fb.w, fb.h, 0xff0000);
      fb.text(100, 100, "720p PROBE", 6, 0xffffff);
      Sleep(3000);
    } else {
      std::snprintf(pb, sizeof pb, "720p probe: setMode FAILED (mode not in XVideoListModes)");
    }
    if (!setMode(fb, 640, 480)) return 1;
    stage(fb, pb);
    modeLine[1] = pb;
    Sleep(4000);
  }
  for (int round = 0;; round ^= 1) {
    const int mw = modes[round][0], mh = modes[round][1];
    if (!setMode(fb, mw, mh)) {
      DbgPrint("OTE: XVideoSetMode %dx%d failed\n", mw, mh);
      modeLine[round] = "mode not accepted";
      round ^= 1;  // stay in the mode that works
      if (!setMode(fb, modes[round][0], modes[round][1])) return 1;
      Sleep(2000);
      continue;
    }
    const int scale = mh >= 720 ? 2 : 1;
    {  // show at once that the mode was accepted (the frame benchmark below is slow under emulation)
      char hdr[80];
      std::snprintf(hdr, sizeof hdr, "mode %dx%d accepted (fb %dx%d); benchmarking...", mw, mh, fb.w, fb.h);
      fb.fill(0, 0, fb.w, fb.h, 0x303030);
      fb.text(8, 8, hdr, scale, 0xffffff);
    }
    const int cells = res.loaded ? battle.grid.cellCount() : 0;
    const int gw = res.loaded ? battle.grid.width() : 1, gh = res.loaded ? battle.grid.height() : 1;
    const int textH = 16 * (scale * 8 + 2);  // reserve room for up to 16 text lines
    int tile = (mh - textH - 16) / (gh ? gh : 1);
    if (tile * gw > mw - 16) tile = (mw - 16) / gw;
    if (tile < 4) tile = 4;
    const int frames = 120;
    double f0 = nowMs();
    for (int f = 0; f < frames; ++f) {
      fb.fill(0, 0, fb.w, fb.h, 0x101830);
      for (int c = 0; c < cells; ++c) {
        const int x = battle.grid.x(c), y = battle.grid.y(c);
        unsigned col = db.terrains[battle.cells[c].terrain].color;
        fb.fill(8 + x * tile, textH + 8 + y * tile, tile - 1, tile - 1, col);
        const int occ = battle.occupant[c];
        if (occ >= 0) {
          const Unit& u = battle.units[occ];
          fb.fill(8 + x * tile + tile / 4, textH + 8 + y * tile + tile / 4, tile / 2, tile / 2, battle.sides[u.side].color);
        }
      }
    }
    const double perFrame = (nowMs() - f0) / frames;
    // The software 3D rasterizer on a fixed scene (docs question 4), measured once per mode: 3 frames.
    static double soft3dMs[2] = {-1, -1};
    static int soft3dTris[2] = {0, 0};
    static unsigned soft3dSum[2] = {0, 0};
    if (soft3dMs[round] < 0) {
      int tris = 0;
      unsigned sum = 0;
      stage(fb, "soft3d bench running");
      const double s0 = nowMs();
      bench3dRun(mw, mh, 3, tris, sum);
      soft3dMs[round] = (nowMs() - s0) / 3;
      soft3dTris[round] = tris;
      soft3dSum[round] = sum;
      DbgPrint("OTE: soft3d %dx%d %.1f ms/frame, %d triangles, checksum %u\n", mw, mh, soft3dMs[round], tris, sum);
    }
    char line[200];
    std::snprintf(line, sizeof line, "%dx%d: frame %s ms (map %dx%d solid, 120 frames) soft3d %s ms/frame %d tris sum %u", fb.w, fb.h,
                  F(perFrame), gw, gh, F(soft3dMs[round], 1), soft3dTris[round], soft3dSum[round]);
    modeLine[round] = line;
    DbgPrint("OTE: %s\n", line);

    std::vector<std::string> lines;
    lines.push_back("OTE core on OG Xbox (xemu is not cycle accurate)");
    if (!res.error.empty()) lines.push_back("ERROR " + res.error);
    std::snprintf(line, sizeof line, "load %s ms  battle %s  sim %s ms  %d steps  worst step %s ms", F(res.loadMs, 1),
                  res.finished ? "over" : "OPEN", F(res.battleMs, 1), res.steps, F(res.worstStepMs));
    lines.push_back(line);
    lines.push_back("result " + summary);
    std::snprintf(line, sizeof line, "RAM KB start %ld  loaded %ld  battle peak %ld  end %ld", res.kbStart, res.kbAfterLoad,
                  res.kbPeakBattle, res.kbEnd);
    lines.push_back(line);
    for (int m = 0; m < 2; ++m) if (!modeLine[m].empty()) lines.push_back(modeLine[m]);
    const size_t cols = (size_t)(mw - 16) / (8 * scale);
    int ty = 8;
    fb.fill(0, 0, fb.w, textH, 0x101830);  // wipe the progress lines
    for (const std::string& l : lines)
      for (size_t o = 0; o < l.size() && ty + 8 * scale < textH; o += cols, ty += 8 * scale + 2)
        fb.text(8, ty, l.substr(o, cols).c_str(), scale, 0xffffff);
    Sleep(6000);
  }
  return 0;
}
