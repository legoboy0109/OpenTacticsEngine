// Xbox 360 picture output through xsdk's GPU layer (xbox360_present.h). Modelled on xsdk's samples/cube.
#include "platform/xbox360/xbox360_present.h"

#include <cstdint>
#include <cstring>

#include "core/base/log.h"

extern "C" {
#include "xsdk/gpu.h"
#include "xsdk/kernel.h"
#include "xsdk/present.h"
#include "xsdk/shader.h"
#include "xsdk/texture.h"
}
// Written by xsdk_add_shaders (tools/build_shaders.py) from present.vert / present.frag: present_vert, present_frag.
#include "ote-player_shaders.h"

namespace ote {
namespace {

constexpr uint32_t kBuffers = 2;
constexpr uint32_t kSwapInterval = 1;
constexpr uint32_t kBeginTimeoutMs = 5000;  // the Guide's first opening held a frame up 2.7 s on the console (xsdk docs)
constexpr uint32_t kFenceTimeoutMs = 1000;
constexpr uint32_t kClearArgb = 0xFF000000;

// Position (clip space) then texture coordinates, 4 corners of a triangle strip.
constexpr uint32_t kVertexFloats = 4;
constexpr uint32_t kStrideBytes = kVertexFloats * 4;
// (xsdk's XsdkVertexElement has more members than these; a plain C++ initializer would warn about each.)
XsdkVertexElement kLayout[2];
void initLayout() {
  std::memset(kLayout, 0, sizeof kLayout);  // components all XSDK_COMPONENT_OWN (0): each reads its own
  kLayout[0].usage = XSDK_USAGE_POSITION;
  kLayout[0].format = XSDK_VERTEX_FLOAT2;
  kLayout[0].offset_bytes = 0;
  kLayout[1].usage = XSDK_USAGE_TEXCOORD;
  kLayout[1].format = XSDK_VERTEX_FLOAT2;
  kLayout[1].offset_bytes = 8;
}

struct State {
  bool open = false;
  uint32_t width = 0, height = 0;
  XsdkShader vertex, pixel;
  XsdkTexture frame;
  XsdkSamplerState sampler;
  volatile uint32_t* vertices = nullptr;
  bool presenting = false;
  uint32_t lastFence = 0;  // the fence after the previous frame's draw: the GPU reads the texture until then
  bool drewOnce = false;
};
State g;

// GPU memory is written with volatile 32-bit stores (xsdk docs/PITFALLS.md): a plain memcpy may merge them into 64-bit
// stores the console faults on.
void writeGpu(volatile uint32_t* memory, const void* data, uint32_t bytes) {
  for (uint32_t i = 0; i < bytes / 4; i++) {
    uint32_t word;
    std::memcpy(&word, static_cast<const uint8_t*>(data) + i * 4, 4);
    memory[i] = word;
  }
  xsdk_gpu_invalidate(xsdk_gpu_address(memory), bytes);
}

bool fail(const char* what, NTSTATUS status) {
  OTE_ERROR("xbox360 present: %s failed: 0x%08X", what, (unsigned)status);
  return false;
}

}  // namespace

bool xbox360PresentOpen(int& width, int& height) {
  NTSTATUS st = xsdk_gpu_open();
  if (!NT_SUCCESS(st)) return fail("xsdk_gpu_open", st);
  initLayout();
  st = xsdk_shader_load(&g.vertex, present_vert.container, present_vert.container_bytes);
  if (NT_SUCCESS(st)) st = xsdk_shader_load(&g.pixel, present_frag.container, present_frag.container_bytes);
  if (NT_SUCCESS(st))
    st = xsdk_shader_set_vertex_layout(&g.vertex, kLayout, sizeof kLayout / sizeof kLayout[0],
                                       (const uint32_t[]){kStrideBytes});
  if (!NT_SUCCESS(st)) return fail("loading the shaders", st);

  st = xsdk_present_open(XSDK_DISPLAY_720P, kBuffers, kSwapInterval, &g.width, &g.height);
  if (!NT_SUCCESS(st)) return fail("xsdk_present_open", st);
  g.presenting = true;
  OTE_INFO("xbox360 present: display %ux%u", (unsigned)g.width, (unsigned)g.height);

  // One texture the size of the screen, filled with the engine's frame each time.
  st = xsdk_texture_create(&g.frame, XSDK_FORMAT_A8R8G8B8, g.width, g.height, 1, true);
  if (!NT_SUCCESS(st)) return fail("xsdk_texture_create", st);
  XsdkSampler s;
  std::memset(&s, 0, sizeof s);
  s.mag_filter = XSDK_FILTER_POINT;  // the frame is exactly screen size: no filtering at all
  s.min_filter = XSDK_FILTER_POINT;
  s.mip_filter = XSDK_FILTER_POINT;
  s.address_u = XSDK_ADDRESS_CLAMP;
  s.address_v = XSDK_ADDRESS_CLAMP;
  xsdk_texture_sampler_state(&g.sampler, &s);

  // Clip space has y up; texture v 0 is the top row.
  const float quad[4][kVertexFloats] = {
      {-1.0f, 1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f, 0.0f}, {-1.0f, -1.0f, 0.0f, 1.0f}, {1.0f, -1.0f, 1.0f, 1.0f}};
  g.vertices = xsdk_gpu_alloc(sizeof quad);
  if (!g.vertices) {
    OTE_ERROR("xbox360 present: xsdk_gpu_alloc failed");
    return false;
  }
  writeGpu(g.vertices, quad, sizeof quad);
  g.open = true;
  width = (int)g.width;
  height = (int)g.height;
  return true;
}

bool xbox360PresentFrame(const void* pixels, int pitchBytes) {
  if (!g.open) return false;
  uint32_t address = 0;
  NTSTATUS st = xsdk_present_begin(&address, kBeginTimeoutMs);
  if (!NT_SUCCESS(st)) return fail("xsdk_present_begin", st);
  // The GPU may still be reading the texture for the previous frame: wait for its draw before overwriting it.
  if (g.drewOnce) {
    st = xsdk_gpu_wait(g.lastFence, kFenceTimeoutMs);
    if (!NT_SUCCESS(st)) return fail("xsdk_gpu_wait", st);
  }
  xsdk_texture_upload(&g.frame, 0, pixels, (uint32_t)pitchBytes);
  xsdk_gpu_invalidate(g.frame.address, g.frame.size_bytes);

  xsdk_gpu_set_target(g.width, g.height);
  xsdk_gpu_set_shaders(&g.vertex, &g.pixel);
  xsdk_built_shader_set_literals(&present_vert);
  xsdk_built_shader_set_literals(&present_frag);
  xsdk_gpu_set_texture(PRESENT_FRAG_SAMPLER_0_0, &g.frame, &g.sampler);
  xsdk_gpu_set_vertices(0, xsdk_gpu_address(g.vertices), 4 * kStrideBytes);
  xsdk_gpu_draw(XSDK_PRIMITIVE_TRIANGLE_STRIP, 4);
  xsdk_gpu_resolve(address, g.width, g.height, kClearArgb);
  g.lastFence = xsdk_gpu_fence();
  g.drewOnce = true;
  xsdk_present_end();
  return true;
}

void xbox360PresentClose() {
  if (g.presenting) xsdk_present_close();
  if (g.open) {
    xsdk_texture_free(&g.frame);
    xsdk_gpu_free(g.vertices);
    xsdk_shader_free(&g.pixel);
    xsdk_shader_free(&g.vertex);
    xsdk_gpu_close();
  }
  g = State();
}

}  // namespace ote
