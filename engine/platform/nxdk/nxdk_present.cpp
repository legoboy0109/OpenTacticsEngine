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

namespace ote {

namespace {

SDL_Surface* g_canvas = nullptr;
NxdkFit g_fit;
void* g_layerMem[2] = {nullptr, nullptr};  // two textures the canvas is copied into (one per use in a frame, alternating)
int g_layers = 0;                          // layers the chip has drawn this frame

bool ensureLayerMemory() {
  if (g_layerMem[0]) return true;
  const size_t bytes = (size_t)g_canvas->w * g_canvas->h * 4;
  g_layerMem[0] = hw3d::alloc(bytes);
  g_layerMem[1] = hw3d::alloc(bytes);
  if (!g_layerMem[0] || !g_layerMem[1]) {
    OTE_ERROR("3D drawing: no memory for the 2D layer's textures (%u KB each)", (unsigned)(bytes >> 10));
    hw3d::release(g_layerMem[0]);
    hw3d::release(g_layerMem[1]);
    g_layerMem[0] = g_layerMem[1] = nullptr;
    return false;
  }
  return true;
}

// The canvas goes to the screen as one rectangle: copied into a texture, drawn at window pixel 1:1. The first layer of a frame
// replaces what is there, later ones are blended on top (the canvas holds premultiplied colour: see nxdk_present.h).
void drawLayer(bool opaque) {
  if (!ensureLayerMemory()) return;
  const int w = g_canvas->w, h = g_canvas->h;
  if (g_layers >= 2) hw3d::waitIdle();  // the texture this layer would overwrite may still be read by an earlier one
  void* mem = g_layerMem[g_layers & 1];
  const uint8_t* src = (const uint8_t*)g_canvas->pixels;
  uint8_t* dst = (uint8_t*)mem;
  for (int y = 0; y < h; ++y) memcpy(dst + (size_t)y * w * 4, src + (size_t)y * g_canvas->pitch, (size_t)w * 4);
  hw3d::flushWrites();

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
  pipe.sampling = hw3d::Sampling::Nearest;
  pipe.edge = hw3d::Edge::Clamp;
  hw3d::VsConstants c;
  hw3d::screenSpaceConstants(c, 1, 1, 1, 1);
  hw3d::Rect all;
  all.w = w;
  all.h = h;
  hw3d::setScissor(all);
  hw3d::setConstants(c);
  hw3d::setPipe(pipe, tex);
  hw3d::drawQuad(0, 0, (float)w, (float)h, 0, 0, (float)w, (float)h, 0xFFFFFFFFu);
  ++g_layers;
}

bool saveBmp(const std::string& path, const uint8_t* pixels, int w, int h, int pitch) {
  SDL_Surface* s = SDL_CreateRGBSurfaceWithFormatFrom((void*)pixels, w, h, 32, pitch, SDL_PIXELFORMAT_ARGB8888);
  if (!s) return false;
  FILE* fp = openFile(path, "wb");  // the kernel wants backslashes: openFile converts
  bool ok = fp && SDL_SaveBMP_RW(s, SDL_RWFromFP(fp, SDL_TRUE), 1) == 0;
  SDL_FreeSurface(s);
  if (!ok) OTE_ERROR("screenshot: cannot write '%s'", path.c_str());
  return ok;
}

void presentByCpu(const std::string& shot) {
  if (!shot.empty()) saveBmp(shot, (const uint8_t*)g_canvas->pixels, g_canvas->w, g_canvas->h, g_canvas->pitch);
  VIDEO_MODE vm = XVideoGetMode();
  uint8_t* fb = (uint8_t*)XVideoGetFB();
  const uint8_t* src = (const uint8_t*)g_canvas->pixels;
  const size_t row = (size_t)g_canvas->w * 4;
  for (int y = 0; y < g_canvas->h; ++y) memcpy(fb + (size_t)y * vm.width * 4, src + (size_t)y * g_canvas->pitch, row);
  XVideoFlushFB();  // the frame buffer is write-combined memory
  XVideoWaitForVBlank();
}

}  // namespace

void nxdkSetCanvas(SDL_Surface* canvas) { g_canvas = canvas; }
void nxdkSetFit(const NxdkFit& fit) { g_fit = fit; }
bool nxdkChipPresents() { return hw3d::running(); }

void nxdkPresent(const std::string& shotIn) {
  if (!g_canvas) return;
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
  hw3d::endFrame();
  g_layers = 0;
}

namespace hw3d {

void canvasLayerDown() {
  if (!g_canvas) return;
  drawLayer(g_layers == 0);
  SDL_FillRect(g_canvas, nullptr, 0);  // transparent: what the 2D renderer draws next lies over the 3D picture
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
