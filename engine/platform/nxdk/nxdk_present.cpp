// See nxdk_present.h.
#include "platform/nxdk/nxdk_present.h"

#include <SDL.h>
#include <hal/video.h>
#include <string.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "core/base/fileio.h"
#include "core/base/log.h"
#include "platform/nxdk/hw3d_gpu.h"
#include "platform/nxdk/nxdk_prof.h"

namespace ote {

namespace {

SDL_Surface* g_canvas = nullptr;
NxdkFit g_fit;
void* g_layerMem[2] = {nullptr, nullptr};  // two textures the canvas is copied into (one per use in a frame, alternating)
size_t g_layerBytes = 0;                   // what each of them holds
int g_layers = 0;                          // layers the chip has drawn this frame

// The part of the surface that is the 2D layer: all of it, or (chip scaling) the game's canvas in the top-left corner.
int canvasWidth() { return g_fit.canvasW > 0 ? g_fit.canvasW : g_canvas->w; }
int canvasHeight() { return g_fit.canvasH > 0 ? g_fit.canvasH : g_canvas->h; }

bool ensureLayerMemory() {
  const size_t bytes = (size_t)canvasWidth() * canvasHeight() * 4;
  if (g_layerMem[0] && bytes <= g_layerBytes) return true;
  if (g_layerMem[0]) {  // the canvas grew (the game changed its logical size)
    hw3d::waitIdle();
    hw3d::release(g_layerMem[0]);
    hw3d::release(g_layerMem[1]);
    g_layerMem[0] = g_layerMem[1] = nullptr;
  }
  g_layerMem[0] = hw3d::alloc(bytes);
  g_layerMem[1] = hw3d::alloc(bytes);
  if (!g_layerMem[0] || !g_layerMem[1]) {
    OTE_ERROR("3D drawing: no memory for the 2D layer's textures (%u KB each)", (unsigned)(bytes >> 10));
    hw3d::release(g_layerMem[0]);
    hw3d::release(g_layerMem[1]);
    g_layerMem[0] = g_layerMem[1] = nullptr;
    return false;
  }
  g_layerBytes = bytes;
  return true;
}

// The canvas goes to the screen as one rectangle: copied into a texture, drawn at window pixel 1:1 or (chip scaling) stretched
// over the fitted rectangle. The first layer of a frame replaces what is there, later ones are blended on top (the canvas holds
// premultiplied colour: see nxdk_present.h).
void drawLayer(bool opaque) {
  if (!ensureLayerMemory()) return;
  prof::Span span(prof::kLayer);
  const int w = canvasWidth(), h = canvasHeight();
  if (g_layers >= 2) hw3d::waitIdle();  // the texture this layer would overwrite may still be read by an earlier one
  void* mem = g_layerMem[g_layers & 1];
  const uint8_t* src = (const uint8_t*)g_canvas->pixels;
  uint8_t* dst = (uint8_t*)mem;
  for (int y = 0; y < h; ++y) memcpy(dst + (size_t)y * w * 4, src + (size_t)y * g_canvas->pitch, (size_t)w * 4);
  hw3d::flushWrites();
  prof::count(prof::kLayerBytes, (double)w * h * 4);
  prof::count(prof::kLayers, 1);

  hw3d::HwTexture tex;
  tex.offset = hw3d::physical(mem);
  tex.format = (1u << 16) | (0x12u << 8) | 0x2Au;  // one level, linear A8R8G8B8, 2D
  tex.pitch = (uint32_t)w * 4;
  tex.rect = ((uint32_t)w << 16) | (uint32_t)h;
  tex.linear = true;
  hw3d::Pipe pipe;
  pipe.mix = opaque ? hw3d::Mix::Replace : hw3d::Mix::Premultiplied;
  pipe.depthTest = false;
  pipe.depthWrite = false;
  pipe.cull = false;
  pipe.sampling = g_fit.canvasW > 0 && g_fit.smooth ? hw3d::Sampling::Linear : hw3d::Sampling::Nearest;
  pipe.edge = hw3d::Edge::Clamp;
  hw3d::VsConstants c;
  hw3d::screenSpaceConstants(c, 1, 1, 1, 1);
  hw3d::Rect all;
  all.w = hw3d::width();
  all.h = hw3d::height();
  hw3d::setScissor(all);
  hw3d::setConstants(c);
  hw3d::setPipe(pipe, tex);
  if (!opaque) {
    // Debug: OTE_XBOX_CHIPLOAD=N (args.txt) draws N tiny rectangles (4 x 4 pixels in the corner) before the last layer, the HUD:
    // the chip is busy for a good part of the frame with the HUD still missing, so a frame shown while the chip is drawing it (a race
    // in the flip, hw3d_gpu.cpp beginFrame) is caught by many screenshots instead of one in a few hundred
    // (tools/xbox-og/flicker_check.py).
    static int load = -1;
    if (load < 0) {
      const char* e = getenv("OTE_XBOX_CHIPLOAD");
      load = e ? atoi(e) : 0;
    }
    for (int i = 0; i < load; ++i) hw3d::drawQuad(0, 0, 4, 4, 0, 0, 1, 1, 0xFFFFFFFFu);
  }
  if (g_fit.canvasW > 0) {
    // Chip scaling: the canvas covers only the fitted rectangle, so the first layer blacks out the margin around it first
    // (the 3D picture and the later layers only draw inside it).
    const bool margin = g_fit.x > 0 || g_fit.y > 0 || g_fit.x + g_fit.w < hw3d::width() || g_fit.y + g_fit.h < hw3d::height();
    if (opaque && margin) hw3d::drawQuad(0, 0, (float)hw3d::width(), (float)hw3d::height(), 0, 0, 1, 1, 0xFF000000u);
    hw3d::drawQuad((float)g_fit.x, (float)g_fit.y, (float)(g_fit.x + g_fit.w), (float)(g_fit.y + g_fit.h), 0, 0, (float)w, (float)h,
                   0xFFFFFFFFu);
  } else {
    hw3d::drawQuad(0, 0, (float)w, (float)h, 0, 0, (float)w, (float)h, 0xFFFFFFFFu);
  }
  ++g_layers;
}

// Debug: OTE_XBOX_FRAMEMARK=1 (args.txt) paints this frame's number as 16 white 8 x 8 squares (bit 15 first, x = 2 + 10 i, y = 2) in the
// screen's top-left corner, drawn last. tools/xbox-og/flicker_check.py --framemark reads them from emulator screenshots and says
// whether the frames reach the screen in order and without repeats beyond what the pacing allows.
void drawFrameMark(int number) {
  static int on = -1;
  static void* white = nullptr;
  if (on < 0) {
    const char* e = getenv("OTE_XBOX_FRAMEMARK");
    on = e && *e && *e != '0';
    if (on) {
      white = hw3d::alloc(4096);
      if (white) memset(white, 0xFF, 4096);
      hw3d::flushWrites();
    }
  }
  if (!on || !white) return;
  hw3d::HwTexture tex;
  tex.offset = hw3d::physical(white);
  tex.format = (1u << 16) | (0x12u << 8) | 0x2Au;
  tex.pitch = 4;
  tex.rect = (1u << 16) | 1u;
  tex.linear = true;
  hw3d::Pipe pipe;
  pipe.mix = hw3d::Mix::Replace;
  pipe.depthTest = false;
  pipe.depthWrite = false;
  pipe.cull = false;
  pipe.edge = hw3d::Edge::Clamp;
  hw3d::VsConstants c;
  hw3d::screenSpaceConstants(c, 1, 1, 1, 1);
  hw3d::Rect all;
  all.w = hw3d::width();
  all.h = hw3d::height();
  hw3d::setScissor(all);
  hw3d::setConstants(c);
  hw3d::setPipe(pipe, tex);
  for (int i = 0; i < 16; ++i)
    if (number & (0x8000 >> i)) hw3d::drawQuad((float)(2 + 10 * i), 2, (float)(10 + 10 * i), 10, 0, 0, 1, 1, 0xFFFFFFFFu);
}

bool saveBmp(const std::string& path, const uint8_t* pixels, int w, int h, int pitch, Uint32 format = SDL_PIXELFORMAT_ARGB8888) {
  SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom((void*)pixels, w, h, 32, pitch, format);
  if (!s) return false;
  FILE* fp = openFile(path, "wb");  // the kernel wants backslashes: openFile converts
  bool ok = fp && SDL_SaveBMP_RW(s, SDL_RWFromFP(fp, SDL_TRUE), 1) == 0;
  SDL_FreeSurface(s);
  if (!ok) OTE_ERROR("screenshot: cannot write '%s'", path.c_str());
  return ok;
}

void presentByCpu(const std::string& shot) {
  const double t0 = prof::now();
  if (!shot.empty()) saveBmp(shot, (const uint8_t*)g_canvas->pixels, g_canvas->w, g_canvas->h, g_canvas->pitch);
  VIDEO_MODE vm = XVideoGetMode();
  uint8_t* fb = (uint8_t*)XVideoGetFB();
  const uint8_t* src = (const uint8_t*)g_canvas->pixels;
  const size_t row = (size_t)g_canvas->w * 4;
  for (int y = 0; y < g_canvas->h; ++y) memcpy(fb + (size_t)y * vm.width * 4, src + (size_t)y * g_canvas->pitch, row);
  XVideoFlushFB();  // the frame buffer is write-combined memory
  prof::count(prof::kLayerBytes, (double)g_canvas->w * g_canvas->h * 4);
  prof::count(prof::kLayers, 1);
  prof::g_ticks[prof::kPresent] += prof::now() - t0;
  prof::Span vbl(prof::kVblank);
  XVideoWaitForVBlank();
}

}  // namespace

void nxdkSetCanvas(SDL_Surface* canvas) { g_canvas = canvas; }
void nxdkSetFit(const NxdkFit& fit) { g_fit = fit; }
bool nxdkChipPresents() { return hw3d::running(); }
void nxdkStartChip() {
  // OTE_XBOX_LEGACY=1 (args.txt, debug): the first version's behaviour, for before/after measurements: the chip starts with the
  // first 3D picture and the 2D layer is drawn at the screen's scale by the CPU.
  const char* legacy = getenv("OTE_XBOX_LEGACY");
  if (legacy && *legacy && *legacy != '0') return;
  std::string why;
  if (!hw3d::start(why)) OTE_WARN("the graphics chip could not be started (%s): the picture is copied by the CPU and 3D is off", why.c_str());
}

void nxdkPresent(const std::string& shotIn) {
  if (!g_canvas) return;
  const double presentStart = prof::now();
  // Debug: OTE_SHOT_EVERY=N saves the composed picture of every Nth frame (the 20th before it) as D:/userdata/dbgNNNN.bmp.
  static int every = -1, frame = 0;
  if (every < 0) {
    const char* e = getenv("OTE_SHOT_EVERY");
    every = e ? atoi(e) : 0;
  }
  ++frame;
  std::string shot = shotIn;
  if (every > 0 && shot.empty() && hw3d::running() && frame % every == every - 20) {
    char name[64];
    snprintf(name, sizeof(name), "D:/userdata/dbg%04d.bmp", frame);
    shot = name;
  }
  if (!hw3d::running()) {
    presentByCpu(shot);
    return;
  }
  hw3d::beginFrame();  // (already open when a 3D picture was drawn)
  drawLayer(g_layers == 0);
  drawFrameMark(frame);
  if (!shot.empty()) {
    hw3d::waitIdle();  // the picture is complete only when the chip is done
    int w = 0, h = 0, pitch = 0;
    const uint8_t* fb = hw3d::backBuffer(w, h, pitch);
    if (fb) {
      std::vector<uint8_t> copy((size_t)h * pitch);  // the frame buffer is slow to read in place
      memcpy(copy.data(), fb, copy.size());
      saveBmp(shot, copy.data(), w, h, pitch);
    }
  }
  prof::g_ticks[prof::kPresent] += prof::now() - presentStart;
  hw3d::endFrame();  // (counts its wait for the vertical blank itself; the time before that is the push-buffer hand-over)
  g_layers = 0;
}

namespace hw3d {

void canvasLayerDown() {
  if (!g_canvas) return;
  drawLayer(g_layers == 0);
  SDL_Rect canvas = {0, 0, canvasWidth(), canvasHeight()};  // (only the part that is the layer: the rest of the surface is unused)
  SDL_FillRect(g_canvas, &canvas, 0);  // transparent: what the 2D renderer draws next lies over the 3D picture
}

void canvasRect(int x, int y, int w, int h, int& sx, int& sy, int& sw, int& sh) {
  int x0 = g_fit.x + (int)std::floor((float)x * g_fit.kx + 0.5f);
  int y0 = g_fit.y + (int)std::floor((float)y * g_fit.ky + 0.5f);
  int x1 = g_fit.x + (int)std::floor((float)(x + w) * g_fit.kx + 0.5f);
  int y1 = g_fit.y + (int)std::floor((float)(y + h) * g_fit.ky + 0.5f);
  sx = x0;
  sy = y0;
  sw = x1 - x0;
  sh = y1 - y0;
}

}  // namespace hw3d
}  // namespace ote
